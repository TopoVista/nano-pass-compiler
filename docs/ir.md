# NanoPass Intermediate Representations

This document describes each IR in the NanoPass compiler pipeline, what it
represents, which constructs are legal, invariants maintained, and example
transformations.

---

## Pipeline overview

```
Source
  ↓  Lex + Parse
AST
  ↓  Scope resolution + Type checking
Typed AST
  ↓  Desugaring (ForStmt, ++/--, +=)
Desugared Typed AST
  ↓  ASTToCoreLowering
CoreIR                   ← structured, fully typed, no surface sugar
  ↓  CoreToANFLowering
ANFIR                    ← evaluation order explicit, temporaries named
  ↓  ANFToCFGLowering
CFG IR                   ← basic blocks, explicit jumps, no structured cf
  ↓  [Phase 4: SSA]
SSA CFG
  ↓  LLVM lowering
LLVM IR
```

---

## AST

**Purpose:** Represents the source language structure as parsed.

**Legal constructs:** All surface-syntax forms including `for`, `++`, `--`,
`+=`, `print`, `if/else`, `while`, `break`, `continue`, `return`, variable
declarations, function definitions, arrays.

**Ownership:** `vector<unique_ptr<Stmt>>` at the top level; `unique_ptr` for
all child nodes.

**Invariants after type-checking:**
- Every `Expr` node has `type` field set (not `Unknown`)
- Every `VariableExpr`/`CallExpr` has non-null `symbol` pointer
- All names resolved; no undeclared identifiers

**Not legal after desugaring:**
- `ForStmt` (lowered to `while`)
- `UnaryExpr` with `++`/`--`
- `BinaryExpr` with `+=`

---

## CoreIR

**Purpose:** Simplified, fully typed, tree-structured IR.  Separates source
structure from optimization infrastructure.

**Legal constructs:**

Statements:
- `CoreVarDeclStmt` — typed variable declaration with optional initializer
- `CoreAssignStmt` — assignment to a named local
- `CoreIndexAssignStmt` — array element assignment
- `CoreExprStmt` — expression evaluated for side effects
- `CoreIfStmt` — conditional with then/else branch lists
- `CoreWhileStmt` — loop with condition and body
- `CoreReturnStmt` — return with optional value
- `CoreBreakStmt` — loop exit (must be inside a `CoreWhileStmt`)
- `CoreContinueStmt` — loop continuation (must be inside a `CoreWhileStmt`)

Expressions:
- `CoreConstantExpr` — typed constant (`int64_t`, `double`, `bool`, `string`)
- `CoreVariableRefExpr` — reference to a named local by `LocalId`
- `CoreBinaryExpr` — typed binary operation using `BinaryOpKind` enum
- `CoreUnaryExpr` — typed unary operation using `UnaryOpKind` enum
- `CoreCallExpr` — function call with argument list
- `CoreIndexExpr` — array element read

**NOT legal in CoreIR:**
- `ForStmt` — must be desugared before lowering
- `++`/`--` operators — must be desugared before lowering
- `+=` operator — must be desugared before lowering
- Unresolved identifier references
- Untyped expression nodes

**Invariants (enforced by `verifyCoreIR()`):**
- All expression types are not `Unknown`
- All variable references are declared in enclosing scope
- All function calls target known functions
- `break`/`continue` only inside loop bodies
- No duplicate function names
- No duplicate `LocalId` within a scope

**Example:**
```
// Source (after desugaring):
int i = 0;
while (i < 10) { i = i + 1; }

// CoreIR:
var i#0 : int = 0
while (lt(i#0, 10)) {
  i#0 = add(i#0, 1)
}
```

---

## ANFIR (Administrative Normal Form)

**Purpose:** Makes evaluation order explicit by naming every non-trivial
intermediate computation.  Simplifies CFG lowering by ensuring all
computation arguments are atomic.

**Key property:** Every argument to a computation (`ANFComputation`) is an
`ANFAtom` — either a constant, a named local, or a previously computed
temporary.  No nested expressions.

**Atomic values:**
```
ANFAtom
  Constant(CoreConstantVal, LangType)
  Variable(LocalId, LangType)
  Temporary(TemporaryId, LangType)   — %t0, %t1, ...
```

Temporary IDs are reset to 0 at the start of each function.

**Legal constructs:**

Statements:
- `ANFVarDeclStmt` — declares a local; optional atomic initializer
- `ANFAssignStmt` — assigns an atom to a local
- `ANFIndexAssignStmt` — assigns an atom to an array element
- `ANFComputeStmt` — names the result of a computation
- `ANFIfStmt` — conditional with atomic condition; body is list of `ANFStmt`
- `ANFWhileStmt` — loop; condition header (list of stmts that compute cond),
  atomic condition, body list
- `ANFReturnStmt` — optional atomic return value
- `ANFBreakStmt` / `ANFContinueStmt`

**Short-circuit &&/|| lowering:**

`&&` is NOT a binary computation in ANF.  It is lowered to:
```
var _sc#N : bool = false
if (left_atom) {
    right_atom = <evaluate right>
    _sc#N = right_atom
}
// _sc#N holds the result
```

`||` is lowered similarly with `_sc#N = true` and the negation of the left.

This guarantees that the right operand is never evaluated when short-circuit
applies.

**Invariants (enforced by `verifyANF()`):**
- Every temporary defined exactly once per function
- Every referenced temporary is defined before use
- All computation operands are atoms
- `break`/`continue` only inside `ANFWhileStmt`

**Example:**
```
// CoreIR:
return mul(n, fact(sub(n, 1)))

// ANF:
%t0 : int = sub(n#0, 1)
%t1 : int = call @fact(%t0)
%t2 : int = mul(n#0, %t1)
return %t2
```

---

## CFG IR (Control-Flow Graph)

**Purpose:** Makes control flow explicit through basic blocks and terminators.
No structured control flow remains — all `if`/`while`/`break`/`continue`
are replaced by jumps and branches.

**Key property:** Every `BasicBlock` ends in exactly one `Terminator`.  No
implicit fallthrough.

**Instruction set:**
```
Constant   %N = const <val>
Copy       %N = copy <operand>
Unary      %N = <op> <operand>         (neg, not)
Binary     %N = <op> <left>, <right>   (add, sub, mul, div, mod, eq, ne, lt, ...)
Call       [%N =] call @<name>(<args>)
LoadLocal  %N = load <x#M>
StoreLocal store <operand> -> <x#M>
LoadIndex  %N = load <x#M>[<idx>]
StoreIndex store <val> -> <x#M>[<idx>]
```

`ValueId` (`%N`) names a computed value.  `LocalId` (`x#M`) names a mutable
storage slot.  The distinction is deliberate: Phase 4 SSA construction will
promote `LoadLocal`/`StoreLocal` pairs to SSA `phi` nodes.

**Terminator model:**
```
Jump(target: BlockId)
Branch(cond: CFGOperand, then: BlockId, else: BlockId)
Return(value: optional<CFGOperand>)
Unreachable
```

**Block structure for control flow:**

*if/else:*
```
entry:  branch cond, then, else
then:   <body>  jump merge
else:   <body>  jump merge
merge:  ...
```

*while:*
```
preheader: jump loop.header
loop.header: <cond computation>  branch cond, loop.body, loop.exit
loop.body:  <body>  jump loop.header
loop.exit: ...
```

*break:* `jump loop.exit`

*continue:* `jump loop.header`

**Invariants (enforced by `verifyCFG()`):**
- Entry block exists; has no predecessors
- All block IDs unique within a function
- Every block has exactly one terminator
- All jump/branch targets are existing blocks
- Predecessor/successor lists are bidirectionally consistent
- Unreachable blocks: reported as warnings, not errors

**Example:**
```
// Source:  while (i <= n) { total += i; i++; }

func @sumTo(n#0 : int) -> int {
bb0.entry:
  store 0 -> total#0
  store 1 -> i#0
  jump bb1
bb1.loop.header:
  %1 = load i#0
  %2 = load n#0
  %0 = le %1, %2
  branch %0, bb2, bb3
bb2.loop.body:
  %4 = load total#0
  %5 = load i#0
  %3 = add %4, %5
  store %3 -> total#0
  %7 = load i#0
  %6 = add %7, 1
  store %6 -> i#0
  jump bb1
bb3.loop.exit:
  %8 = load total#0
  return %8
}
```

**CFG Graphviz (DOT):**
```
digraph CFG {
  node [shape=box, fontname="Courier", style="rounded"];
  subgraph cluster_sumTo {
    label="@sumTo";
    sumTo_bb0 [label="bb0 (entry)"];
    sumTo_bb1 [label="bb1 (loop.header)"];
    sumTo_bb2 [label="bb2 (loop.body)"];
    sumTo_bb3 [label="bb3 (loop.exit)"];
    sumTo_bb0 -> sumTo_bb1;
    sumTo_bb1 -> sumTo_bb2 [label="true", color="green"];
    sumTo_bb1 -> sumTo_bb3 [label="false", color="red"];
    sumTo_bb2 -> sumTo_bb1;
  }
}
```

---

## Analyses

### ReachabilityAnalysis

BFS from entry block.  Returns `reachable[]` and `unreachable[]` block lists.

### ReversePostOrderAnalysis

DFS postorder, reversed.  Entry block appears first.
Will feed the Cooper/Harvey/Kennedy dominator algorithm in Phase 4.

### CFGStatsAnalysis

Counts: functions, basic blocks, edges, instructions, branches, calls, returns.

---

## Known limitations

- **`continue` inside desugared for-loops**: After `DesugarForPass` converts
  `for(init;cond;inc)` to `{init; while(cond){body; inc}}`, a `continue`
  inside the while body jumps to `loop.header`, skipping the increment.
  Programs that require `continue` inside for-loops should use explicit
  while-loops with increment before the continue.

- **No block-scope shadowing**: The Nano language does not have anonymous
  block scopes (`{}`).  Variable shadowing can only occur across function
  boundaries.  Two variables with the same name in the same function are
  a redeclaration error.

- **`&&`/`||` not in LLVM path via CFG**: The canonical LLVM compilation still
  uses the AST → LLVM path.  The LLVM lowering already handles short-circuit
  correctly via phi nodes.  The CFG path implements short-circuit via the ANF
  `if`-based pattern.

- **Arrays in CoreIR/ANF/CFG**: Array declarations and index expressions are
  represented, but array operations in the CFG lowering path are not yet
  verified end-to-end with the LLVM backend.
