# Project Status

Last updated: Phase 2 — PassManager + AnalysisManager complete.

---

## Stable

### Frontend
- **Lexer** — full Nano surface syntax; correct line/column tracking; unterminated
  string/comment errors with source locations.
- **Parser** — recursive-descent; produces typed AST for all language constructs:
  variables, arrays, functions, if/else, while, for, return, break, continue,
  print, and all expression forms including array indexing.
- **Scope resolution** (`ResolveScopesPass`) — lexical scopes, forward function
  predeclaration (mutual recursion and forward calls work), stable symbol
  ownership (no dangling pointers), source-location-attached errors.
- **Type checker** (`TypeCheckPass`) — arithmetic, comparison, logical, assignment,
  array indexing, function calls with arity/type checking, if/while/for
  conditions, void/non-void return checking.  All statement kinds handled.

### Pass infrastructure (`compiler/passes/`)
- **`nanopass::Pass`** — abstract base class; `PassInfo` with stable string IDs;
  `run(Program &, PassContext &)` interface.
- **`nanopass::PassManager`** — sequential pipeline; each pass runs **once over the
  full program**; `--stop-after` / `--disable-pass` support; instrumentation hooks;
  analysis invalidation after each pass.
- **`nanopass::AnalysisManager`** — type-safe `get<T>()` with caching by
  `std::type_index`; `invalidate(PreservedAnalyses)`; preservation-check
  registration; debug callbacks.
- **`nanopass::PreservedAnalyses`** — `all()` / `none()` / `preserve<T>()`;
  used by passes to declare which analyses remain valid.
- **`nanopass::PassResult`** — `changed` flag, `PreservedAnalyses`, `PassStatistics`
  (open named-value map).
- **`nanopass::PassInstrumentation`** — observer hooks: `onPassStart`, `onPassEnd`,
  `onAnalysisAccess`, `onAnalysisInvalidated`, `onPipelineStart`, `onPipelineEnd`.
- **`nanopass::PassTelemetry`** — per-pass structured record: pass ID/name, unit,
  `changed`, `durationNs`, statistics, invalidated analysis IDs.
- **`nanopass::PassRegistry`** — static registry; lookup by stable ID; category-
  grouped `printPasses()` / `printAnalyses()` for CLI output; `validatePassIds()`.
- **`nanopass::PipelineBuilder`** — single authoritative pipeline factory;
  `buildDesugarPipeline()`; `initialise()` (idempotent registry population).

### Analyses
- **`NodeCountAnalysis`** (`ast-node-count`) — total/statement/expression/function
  counts via full AST traversal.  No raw AST pointers stored; safe to cache.
- **`ASTDepthAnalysis`** (`ast-depth`) — maximum nesting depth; same guarantees.

### Desugaring passes (wired into PassManager)
- **`DesugarForPass`** (`desugar-for`) — `for(init;cond;inc) body` →
  `{ init; while(cond){ body; inc } }`.  Recurses into `FunctionStmt` bodies
  (Phase 1 bug fixed).  Source locations preserved on synthesised nodes.
  Returns `changed=false` when no `ForStmt` present.
- **`DesugarIncDecPass`** (`desugar-inc-dec`) — `x++`/`x--` →
  `x = x ± 1`.  Recurses into `FunctionStmt` bodies.
- **`DesugarPlusAssignPass`** (`desugar-plus-assign`) — `x += e` →
  `x = x + e`.  Recurses into `FunctionStmt` bodies.

All three passes report `rewrites` counts and return accurate `changed` flags.

### LLVM code generation
- Lowers desugared AST to LLVM IR.  Supported: all integer/float/bool
  arithmetic, comparisons, logical `&&`/`||` (short-circuit phi), unary
  `-`/`!`, string literals, arrays with bounds checks, function declarations
  and calls (including forward calls), if/else, while, variable declarations
  and assignment, print (int, float, bool, string), void and non-void returns.
- LLVM module verifier runs after codegen.

### CLI
- `nano --help` / `nano --version`
- `nano emit <file> --format=tokens|ast|desugar|llvm`
- `nano compile <file> --emit-llvm [-o <file>]`
- `nano verify <file>` — parse → resolve → typecheck → desugar → LLVM verify
- `nano passes` — list all registered passes with category and description
- `nano analyses` — list all registered analyses
- `--print-pipeline` — print the active pass pipeline before execution
- `--stop-after=<id>` — halt pipeline after the named pass
- `--disable-pass=<id>` — skip a named pass
- `--debug-pass-manager` — structured per-pass timing, changed state, stats
- `--debug-analyses` — analysis cache hit/miss events
- Stub commands with clear "not yet implemented" messages: `run`, `explain`,
  `graph`, `optimize`

### Repository hygiene
- Generated artifacts removed from git; comprehensive `.gitignore`.
- 8 sample programs in `examples/`.
- 35 CTest targets; 100% passing.

### Tests — 35 total
| Category | Count |
|---|---|
| CLI smoke (help, version, passes, analyses) | 4 |
| Frontend unit (forward calls, errors, source locations) | 1 |
| Pass manager unit (22 focused tests) | 1 |
| Per-example emit/desugar/verify (8 examples × 3) | 24 |
| CLI pass-manager smoke (debug, print-pipeline, stop-after, disable, invalid) | 5 |

---

## Experimental / not wired into production pipeline

- **`ANFPass`** — AST-level ANF prototype. Not connected to main pipeline.
- **`CPSPass` / CPS IR** — teaching/research CPS transformation.
- **`IRPrinter`** — flat 3-address IR printer from earlier experiments.
- **`DesugarBoolPass`** / **`DesugarIfElsePass`** — exist as headers; not enabled
  (semantics and tests incomplete).

---

## Planned (not yet implemented)

### IR pipeline
- Typed `CoreIR` — reduced IR after desugaring
- ANF integration into the real pipeline
- `CFG` IR — basic blocks, terminators, predecessors/successors
- CFG verifier and Graphviz output

### Analyses
- Dominator tree (Cooper/Harvey/Kennedy algorithm)
- Dominance frontier
- Loop nesting (`LoopInfo`)
- Liveness

### SSA
- SSA construction via dominance frontiers
- Phi placement and variable renaming
- SSA verifier and printer
- `nano emit --format=ssa`

### Optimizer
- Constant folding / propagation / SCCP
- Dead-code elimination
- Copy propagation, CSE
- Algebraic simplification, SimplifyCFG
- Function inlining
- LICM
- Optimization levels `-O0` through `-O3`

### Runtime
- Tree-walking interpreter (differential testing)
- LLVM AOT object/executable emission
- ORC JIT (`nano jit`)
- Optional adaptive JIT

### Observability
- `nano explain` — full transformation journey
- Structured diagnostics with rich source spans and fix-it hints
- Pass telemetry JSON (`--pass-telemetry=json`)
- Optimization remarks (`OptimizationApplied` / `OptimizationMissed`)
- "Why didn't this optimize?" structured reasons

### Web & API
- JSON compiler API server (`nano-server`)
- Next.js compiler lab

### Research extensions
- Profile-guided optimization
- Learned optimization policy
- Verifier-gated GenAI advisor
- Optional Alive2 translation validation
- Optional e-graph optimizer
- Optional RISC-V backend

---

## Known limitations

- **LLVM required** — cannot configure without CMake-discoverable LLVM 14+.
- **No native executable emission** — only LLVM textual IR; link with `clang`
  or run via `lli`.
- **Source spans incomplete** — nodes carry locations; spans are not yet
  propagated through LLVM IR metadata.
- **No SSA, CFG, or optimization** — LLVM backend relies on LLVM's own
  alloca/mem2reg.  Nano-level SSA is planned for Phase 4.
- **`break`/`continue` not lowered** — parsed and type-checked but LLVM
  backend does not emit correct branch targets for these.
- **String type not fully supported** — string literals print correctly, but
  string variables and operations are not implemented.
- **ANF/CPS prototypes are disconnected** — exist as headers but are not part
  of the active compilation path.
- **`namespace nanopass`** — the compiler pass infrastructure uses the
  `nanopass` namespace (not `nano`) to avoid collision with `std::nano` from
  `<ratio>`.  This is intentional and permanent.
