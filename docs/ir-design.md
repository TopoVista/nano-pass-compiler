# NanoPass Compiler — Intermediate Representation Design

Phase 3 design document.  Covers CoreIR, ANF, CFG, and the lowering pipeline.

---

## 1. Audit of Existing IR Code (Phase 3 starting point)

### Existing ANF pass (`compiler/passes/anf_pass.h`)

The repository contained an older `ANFPass` struct operating on the AST:

- Returns `vector<unique_ptr<Stmt>>` (flattened AST), not a separate IR
- Handles: blocks, ExprStmt, PrintStmt, IfStmt, WhileStmt, ReturnStmt
- **Does NOT handle FunctionStmt** — function bodies not recursed
- **Does NOT handle break/continue**
- Temporaries named `_t0`, `_t1` (global counter, not reset per function)
- Does NOT preserve types on new nodes
- Does NOT handle `&&` / `||` short-circuit correctly (treats as plain binary)
- NOT wired into the main compilation pipeline

**Decision:** Do not extend or wire this pass. Build new ANF IR in `compiler/ir/`.

### Existing IR printer (`compiler/ir/ir_printer.h`)

A flat 3-address `IRInstr`/`IRPrinter` from earlier days. Not used in production.
Retained as historical artifact.

### Existing CPS (`compiler/ir/cps.h`, `cps_pass.h`)

CPS IR with `CPSExpr`, `CPSLet`, `CPSIf`, `CPSCall`, `CPSReturn`. Disconnected
from the main pipeline. Retained as experimental/educational code.

### New IR code discovered in `compiler/ir/` (already present at Phase 3 start)

The repository contained complete implementations of:
- `core_ir.h`, `ast_to_core.h`, `core_printer.h`, `core_verifier.h`
- `anf_ir.h`, `core_to_anf.h`, `anf_printer.h`, `anf_verifier.h`
- `cfg_ir.h`, `cfg_builder.h`, `anf_to_cfg.h`, `cfg_printer.h`, `cfg_verifier.h`,
  `cfg_graphviz.h`
- `analysis/reachability_analysis.h`, `analysis/reverse_postorder_analysis.h`,
  `analysis/cfg_stats_analysis.h`
- `pipeline.h` — `IRPipeline::runAll()` wiring all stages together

Phase 3 work consisted of:
1. Completing the CLI wiring (all IR formats were stubs)
2. Adding `break`/`continue` keywords to lexer and parser (previously absent)
3. Fixing `nano explain` to show real desugar telemetry
4. Writing `docs/ir-design.md` and `docs/ir.md`
5. Adding 28 IR unit tests covering the full pipeline
6. Adding 27 CLI integration tests for the new formats
7. Adding `examples/errors.nano`, `examples/short_circuit.nano`,
   `examples/control_flow.nano`
8. Fixing two test programs that triggered `bool`-assigned-to-`int` errors

---

## 2. Representation Pipeline

```
Source
  ↓ Lexer + Parser
AST (vector<unique_ptr<Stmt>>)
  ↓ ResolveScopesPass + TypeCheckPass
Typed AST
  ↓ PassManager [DesugarFor, DesugarIncDec, DesugarPlusAssign]
Desugared Typed AST    (no ForStmt, no ++/--, no +=)
  ↓ ASTToCoreLowering
CoreIR (CoreProgram)
  ↓ CoreToANFLowering
ANFIR (ANFProgram)
  ↓ ANFToCFGLowering
CFG IR (CFGProgram)
  ↓ [Phase 4: SSA construction]
SSA CFG
  ↓ [LLVM lowering — currently still from AST]
LLVM IR
```

**Canonical LLVM compilation** still uses the AST → LLVM path (unchanged from
Phase 2) for robustness.  The new CoreIR → ANF → CFG path is fully wired for
inspection (`--format=core/anf/cfg`, `nano graph --cfg`, `nano explain`,
`nano verify`).  Phase 4 SSA will build on top of the CFG.

---

## 3. CoreIR Design

### Purpose

CoreIR represents simplified language semantics after all surface-syntax sugar
has been eliminated.  It is a tree-structured IR, similar in shape to AST but:
- All types are explicitly known on every expression node
- `ForStmt`, `++`, `--`, `+=` are absent (eliminated by desugaring)
- `print` is modelled as a call to `@print`
- Every identifier has a stable `LocalId` (name + disambiguation index)
- Source provenance (`SourceOrigin`) is preserved from AST nodes

### Stable identifiers

```cpp
struct LocalId {
    std::string name;   // source name
    int index;          // disambiguation: x#0 and x#1 for two 'x' locals
};
```

Two variables named `x` in different scopes get `x#0` and `x#1`.
Function parameters get index 0 by default; locals increment per name.
Printers display: `x#0`, `total#1`, etc.

### Type system

Reuses existing `LangType` directly — no wrapper needed.  Every `CoreExpr`
carries a `LangType type` field.  Operator enums:

```
BinaryOpKind: Add Sub Mul Div Mod Eq Ne Lt Le Gt Ge LogicalAnd LogicalOr
UnaryOpKind: Negate LogicalNot
```

### Constant representation

```cpp
using CoreConstantVal = std::variant<int64_t, double, bool, std::string>;
```

No stringly-typed constants.

### Statement hierarchy

```
CoreStmt
  CoreVarDeclStmt(LocalId, LangType, CoreExpr?)
  CoreAssignStmt(LocalId target, CoreExpr)
  CoreIndexAssignStmt(LocalId array, CoreExpr index, CoreExpr value)
  CoreExprStmt(CoreExpr)
  CoreIfStmt(CoreExpr cond, body[], else[])
  CoreWhileStmt(CoreExpr cond, body[])
  CoreReturnStmt(CoreExpr?)
  CoreBreakStmt
  CoreContinueStmt
```

### Expression hierarchy

```
CoreExpr
  CoreConstantExpr(CoreConstantVal)
  CoreVariableRefExpr(LocalId)
  CoreBinaryExpr(BinaryOpKind, left, right)
  CoreUnaryExpr(UnaryOpKind, operand)
  CoreCallExpr(callee, args[])
  CoreIndexExpr(array, index)
```

### Ownership

`unique_ptr` throughout.  `CoreProgram` cannot be accidentally copied
(`unique_ptr` fields make copy impossible).

### Verifier invariants

`verifyCoreIR()` checks:
- No duplicate function names
- No duplicate `LocalId` within a single function scope
- All referenced variables declared before use
- All calls target known functions
- `break`/`continue` only inside loop bodies
- All expression nodes verifiable recursively

---

## 4. ANF IR Design

### Purpose

ANF (Administrative Normal Form) makes evaluation order explicit by naming
every non-trivial intermediate computation.  After ANF lowering:
- Every argument to a computation is an **atom** (constant, variable, or temporary)
- No nested function calls
- Evaluation order is determined by the linear sequence of `ANFComputeStmt`s

### Atomic values

```
ANFAtom
  Constant(CoreConstantVal)
  Variable(LocalId)
  Temporary(TemporaryId)
```

### Temporary IDs

```cpp
struct TemporaryId { int index; };  // %t0, %t1, ...
```

Reset to 0 at the start of each function.  Global state is never used.
Deterministic output guaranteed for golden tests.

### Computation statement

```cpp
struct ANFComputation {
  ANFComputationKind kind;  // Binary | Unary | Call | IndexLoad
  optional<TemporaryId> dst;
  LangType type;
  // ... operands
};
```

### Short-circuit strategy (&&, ||)

**Choice: lower to structured control flow in ANF, not at CFG time.**

Rationale: ANF's `if` statement can already represent conditional evaluation.
Lowering `&&` and `||` to control flow at ANF time means:
- CFG lowering is a straightforward mechanical translation
- No special `&&`/`||` case in the CFG builder
- Short-circuit semantics provably correct at ANF level

Implementation:

```
a && b   →   var _sc#N : bool = false
             if (a) { _sc#N = b }
             (result = _sc#N)

a || b   →   var _sc#N : bool = true
             t = not(a)
             if (t) { _sc#N = b }
             (result = _sc#N)
```

This pattern ensures `b` is never evaluated when short-circuit applies.

### Evaluation order guarantee

Arguments to every computation are atomized **left to right** before the
computation is emitted.  For `foo(bar(), baz())`:
```
%t0 = call @bar()
%t1 = call @baz()
%t2 = call @foo(%t0, %t1)
```

### Statement hierarchy

```
ANFStmt
  ANFVarDeclStmt(LocalId, LangType, ANFAtom?)
  ANFAssignStmt(LocalId, ANFAtom)
  ANFIndexAssignStmt(LocalId array, ANFAtom index, ANFAtom value)
  ANFComputeStmt(ANFComputation)
  ANFIfStmt(ANFAtom cond, then[], else[])
  ANFWhileStmt(condHeader[], ANFAtom cond, body[])
  ANFReturnStmt(ANFAtom?)
  ANFBreakStmt
  ANFContinueStmt
```

`ANFWhileStmt.condHeader` holds the `ANFComputeStmt`s needed to evaluate
the condition (these must re-execute every iteration).

### Verifier invariants

`verifyANF()` checks:
- Every temporary defined exactly once per function
- All referenced temporaries are defined before use
- All computation arguments are atoms
- `break`/`continue` only inside loop bodies

---

## 5. CFG IR Design

### Purpose

The Control-Flow Graph makes control flow explicit through basic blocks and
terminators.  After CFG lowering:
- No structured `if`/`while`/`break`/`continue` — all replaced by jumps and branches
- Every basic block ends in exactly one terminator
- Predecessor/successor edges are explicit and consistent
- Mutable locals use load/store; computed values are named by `ValueId`

This design is intentionally SSA-ready: Phase 4 will promote `load`/`store`
patterns to phi nodes and rename values.

### Stable IDs

```cpp
struct BlockId  { int index; };   // bb0, bb1, bb2, ...
struct ValueId  { int index; };   // %0, %1, %2, ...
struct LocalId  { string name; int index; };  // x#0, x#1, ...
```

`ValueId`s are allocated monotonically per function by `CFGBuilder`.
`LocalId`s are carried from CoreIR/ANF.

### Instruction set

```
CFGInstruction (kind)
  Constant   dst = const <val>
  Copy       dst = copy <operand>
  Unary      dst = <op> <operand>
  Binary     dst = <op> <left>, <right>
  Call       [dst =] call @<name>(<args>)
  LoadLocal  dst = load <LocalId>
  StoreLocal store <operand> -> <LocalId>
  LoadIndex  dst = load <LocalId>[<idx>]
  StoreIndex store <val> -> <LocalId>[<idx>]
```

`ValueId` is the SSA-candidate value.  `LocalId` is the mutable storage slot.
In Phase 4, `LoadLocal`/`StoreLocal` pairs over the same `LocalId` will be
converted to SSA `phi` nodes via the standard mem2reg algorithm.

### Terminator model

```cpp
using Terminator = variant<
    JumpTerminator{BlockId target},
    BranchTerminator{CFGOperand cond, BlockId then, BlockId else},
    ReturnTerminator{optional<CFGOperand> value},
    UnreachableTerminator{}
>;
```

Every `BasicBlock` has exactly one terminator.  The builder refuses to emit
instructions after a terminator is placed.

### Loop context and break/continue

`CFGBuilder` maintains a `vector<LoopContext>` stack:

```cpp
struct LoopContext {
    BlockId continueTarget;  // loop.header (re-evaluates condition)
    BlockId breakTarget;     // loop.exit
};
```

`break` → `terminateWithJump(currentLoop()->breakTarget)`
`continue` → `terminateWithJump(currentLoop()->continueTarget)`

Nested loops: each `ANFWhileStmt` pushes a `LoopContext` before lowering its
body and pops it after.  `break`/`continue` always target the innermost loop.

**`continue` and desugared for-loops:** After `DesugarForPass`, for-loops
become `while`-loops where the increment is placed at the end of the body
block.  A `continue` inside this body jumps to `loop.header`, which
re-evaluates the condition — this skips the increment.  This is a known
limitation: `continue` in a desugared for-loop does not execute the original
increment.  Programs using `continue` inside for-loops should rewrite as
explicit while-loops where the increment is placed before `continue`.  This
limitation is documented in `docs/STATUS.md`.

### Block naming convention

Blocks are named by the CFGBuilder at creation time:

| Block name | Meaning |
|---|---|
| `entry` | Function entry |
| `loop.header` | While condition evaluation |
| `loop.body` | While body |
| `loop.exit` | After while |
| `then` | If true branch |
| `else` | If false branch |
| `if.merge` | After if/else join |

### Predecessor/successor consistency

`CFGBuilder::finalizeEdges()` is called after all blocks are populated.  It
rebuilds predecessor/successor lists from the terminator targets, ensuring
bidirectional consistency.  The CFG verifier then validates this invariant.

### CFG Verifier invariants

`verifyCFG()` checks per function:
1. Entry block exists and has no predecessors
2. All block IDs unique
3. Every block has exactly one terminator
4. All jump/branch targets are existing blocks
5. Predecessor/successor bidirectional consistency
6. Unreachable blocks reported as warnings (not errors)

### Graph output

`emitCFGGraphviz()` produces Graphviz DOT with:
- One `subgraph cluster_<funcname>` per function
- Nodes labeled `bbN (label)`
- Edges labeled `true`/`false` for branches, unlabeled for jumps
- Green edges for true branches, red for false branches

---

## 6. CFG Analyses

### ReachabilityAnalysis

BFS from entry block.  Returns `reachable` and `unreachable` block lists.
Deterministic order (BFS traversal order, successor list order).

### ReversePostOrderAnalysis

DFS postorder, then reversed.  Entry block guaranteed first.
Used in Phase 4 for dominator computation.  Deterministic successor traversal.

### CFGStatsAnalysis

Counts: functions, basic blocks, edges, instructions, branches, calls, returns.
Used by `nano explain` and pass telemetry.

---

## 7. Source Origin Tracking

```cpp
struct SourceOrigin {
    SourceLocation loc;
    std::string transformation;  // e.g. "ast_to_core"
};
```

Every `CoreStmt`, `CoreExpr`, `ANFStmt`, `ANFComputation`, and `CFGInstruction`
carries a `SourceOrigin`.  During lowering, origin is propagated from the
originating AST node.  Synthesised nodes (e.g., temporary declarations for
short-circuit) carry the origin of the expression they came from.

---

## 8. Ownership Safety

| Layer | Ownership |
|---|---|
| `CoreProgram` | owns all `CoreFunction`s and statements (unique_ptr) |
| `ANFProgram` | owns all `ANFFunction`s and statements (unique_ptr) |
| `CFGProgram` | owns all `CFGFunction`s; `CFGFunction` owns `BasicBlock`s (unique_ptr) |
| `BlockId` references | stable integer IDs; never raw `BasicBlock*` across collections |
| `LocalId` | value type (string + int); safe to copy |
| `ValueId` | value type (int); safe to copy |

Passing `CFGOperand` (which contains `CFGOperandKind`, `CoreConstantVal`,
`ValueId`, or `LocalId`) is safe to copy because all members are value types.
No raw pointers into BasicBlock or instruction arrays are retained across
block creation.

---

## 9. Canonical Compilation Path

**Current (Phase 3):**
- `nano compile/emit --format=llvm` → AST → LLVM (unchanged from Phase 2)
- `nano emit --format=core/anf/cfg` → AST → CoreIR → ANF → CFG (inspection only)

**Phase 4 target:**
- Add SSA construction on top of the CFG
- Eventually: CFG → LLVM lowering (via `alloca`/`load`/`store` → `phi` after mem2reg)

The canonical path is NOT switched until the new path produces identical
output to the old path for all existing examples.

---

## 10. Phase 4 SSA Considerations

The CFG is designed to make SSA construction straightforward:

1. **LocalId as variable identity** — every mutable local has a stable `LocalId`.
   Two variables named `x` in different scopes have different indices.
   No string-name collision possible.

2. **LoadLocal/StoreLocal pattern** — mutable locals are accessed via explicit
   load/store.  The standard mem2reg SSA construction algorithm converts:
   ```
   store v -> x#0
   ...
   %n = load x#0
   ```
   into SSA form with phi nodes at join points.

3. **Predecessor/successor edges** — already computed and verified.
   Dominator tree construction can begin immediately from these.

4. **RPO order** — `ReversePostOrderAnalysis` is already implemented and
   tested.  It will feed directly into the Cooper/Harvey/Kennedy dominator
   algorithm.

5. **Unreachable blocks** — identified by the verifier.  Can be pruned before
   SSA construction to simplify the dominator tree.

6. **ValueId namespace** — computed values (`%0`, `%1`) are already separate
   from local variable names (`x#0`).  SSA renaming only needs to handle
   `LocalId` → `ValueId` promotion for mutable locals.
