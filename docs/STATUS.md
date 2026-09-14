# Project Status

Last updated: Phase 1 stabilization complete.

---

## Stable

### Frontend
- **Lexer** — tokenizes the full Nano language surface syntax; correct
  line/column tracking; unterminated string/comment errors with source
  locations.
- **Parser** — recursive-descent; produces typed AST nodes for all language
  constructs: variables, arrays, functions, if/else, while, for, return,
  break, continue, print, and all expression forms.
- **Scope resolution** (`ResolveScopesPass`) — lexical scopes, forward
  function predeclaration (mutual recursion and forward calls work), stable
  symbol ownership (no dangling pointers), source-location-attached errors.
- **Type checker** (`TypeCheckPass`) — arithmetic, comparison, logical,
  assignment, array indexing, function calls with arity/type checking,
  if/while/for conditions, void/non-void return checking. All statement kinds
  including `ForStmt`, `BreakStmt`, `ContinueStmt` handled.

### Desugaring passes (wired into main pipeline)
- **`DesugarForPass`** — `for(init;cond;inc) body` → `{ init; while(cond){ body; inc } }`
- **`DesugarIncDecPass`** — `x++` / `x--` → `x = x ± 1`
- **`DesugarPlusAssignPass`** — `x += e` → `x = x + e`

All three run in dependency order after type checking, before LLVM lowering.

### LLVM code generation
- **`LLVMCodegen` / `lower_expr` / `lower_stmt`** — lowers desugared AST to
  LLVM IR. Supported: all integer/float/bool arithmetic, comparisons (`<`,
  `<=`, `>`, `>=`, `==`, `!=`), logical operators (`&&`, `||`) with correct
  short-circuit phi nodes, unary `-` and `!`, string literals, array
  declarations and indexed access (with bounds check), function
  declarations/calls (forward calls included), if/else, while, for (after
  desugaring), variable declarations and assignment, print (int, float, bool,
  string), void and non-void returns.
- **LLVM module verifier** runs after codegen; emits `internal compiler error`
  on invariant violation.

### CLI
- `nano --help` / `nano --version`
- `nano emit <file> --format=tokens|ast|desugar|llvm`
- `nano compile <file> --emit-llvm [-o <file>]`
- `nano verify <file>` — runs parse → resolve → typecheck → desugar → LLVM
  verify; reports per-stage PASS.
- `nano --debug-pass-manager` — prints pass execution log to stderr.
- Stub commands with clear `not yet implemented` messages: `run`, `explain`,
  `graph`, `optimize`.

### Repository hygiene
- Generated artifacts (`.ll`, `.s`, `.exe`, `build/`, `.idea/`, `.vscode/`)
  removed from git history and excluded by `.gitignore`.
- Examples directory with 8 sample programs.

### Visualization
- **`ASTGraphviz`** — emits Graphviz DOT for the full AST including all
  statement and expression node types. Access via
  `nano emit <file> --format=dot -o out.dot`.

### Tests
- `nano_frontend_smoke` — covers: forward calls, mutual recursion, unterminated
  comments, undeclared variable errors with source locations.

---

## Experimental / not wired into production pipeline

- **`ANFPass`** — AST-level ANF normalization prototype. Not yet connected to
  the main pipeline or LLVM lowering.
- **`CPSPass` / CPS IR** — teaching/research CPS transformation. Retained as
  an experimental pipeline; does not affect production compilation.
- **`IRPrinter`** — flat 3-address IR printer from earlier days; superseded
  by LLVM IR emission.

---

## Planned (not yet implemented)

### IR pipeline
- Typed `CoreIR` — reduced IR after desugaring, before ANF
- ANF integration into the real pipeline
- `CFG` IR — basic blocks, terminators, predecessors/successors
- CFG verifier and Graphviz output

### Analyses
- Dominator tree (Cooper/Harvey/Kennedy algorithm)
- Dominance frontier
- Loop nesting (`LoopInfo`)
- Liveness

### SSA
- SSA construction (phi placement via dominance frontiers)
- SSA value renaming
- SSA verifier
- `nano emit --format=ssa`

### Optimizer
- Pass manager with analysis caching and invalidation
- Constant folding / propagation / SCCP
- Dead-code elimination
- Copy propagation, CSE
- Algebraic simplification, SimplifyCFG
- Function inlining
- LICM
- Optimization levels `-O0` through `-O3`
- `nano optimize` with custom pass pipelines

### Runtime
- Tree-walking interpreter (for differential testing)
- LLVM AOT object/executable emission
- ORC JIT (`nano jit`)
- Optional tiered adaptive JIT

### Observability
- `nano explain` — full transformation journey
- Structured diagnostics (rich source spans, fix-it hints, JSON output)
- Pass telemetry JSON
- Optimization remarks (`OptimizationApplied` / `OptimizationMissed`)
- "Why didn't this optimize?" structured reasons

### Web & API
- JSON compiler API server (`nano-server`)
- Next.js compiler lab (Monaco editor, pipeline timeline, CFG/SSA explorer)

### Research extensions
- Profile-guided optimization
- Learned optimization policy
- Verifier-gated GenAI advisor
- Optional Alive2 translation validation
- Optional e-graph optimizer
- Optional RISC-V backend

---

## Known limitations

- **LLVM required** — the project cannot configure without a CMake-discoverable
  LLVM 14+ development installation.
- **No native executable emission** — only LLVM textual IR is emitted;
  `nano compile` requires `--emit-llvm`. Link with `clang` or run via `lli`.
- **Source spans incomplete** — many AST nodes carry locations; however spans
  are not yet propagated through desugaring or into LLVM IR metadata.
- **No SSA, CFG, or optimization** — the LLVM backend relies on LLVM's own
  alloca/mem2reg promotion. Nano-level SSA is planned for Phase 4.
- **ANF/CPS prototypes are disconnected** — they exist as headers but are not
  part of the active compilation path.
- **`break` / `continue` not lowered** — parsed and type-checked but the LLVM
  backend does not yet emit correct branch targets for these.
- **String type not fully supported** — string literals can be printed, but
  variables of string type and string operations are not implemented.
