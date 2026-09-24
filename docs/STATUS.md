# Project Status

Last updated: Phase 5 — verified SSA optimizer complete; LLVM-backed validation pending local tooling.

---

## Stable

### SSA middle end
- **Dominator analysis** — deterministic reachable-block RPO plus iterative
  immediate dominators, dominance frontier, and DOT output.
- **SSA IR and builder** — explicit parameter definitions, iterated-DF phi
  placement, dominator-tree renaming, stable ValueIds, and scalar promotion.
- **Verifier and def-use** — dominance/order and phi-edge checks, known-type
  compatibility, promotion-residue checks, use indexing, and replacement.
- **CLI** — `nano emit --format=ssa`; `nano verify` and `nano explain` report SSA.

### SSA optimizer
- **Verified structural optimizer** — constant folding, conservative algebraic
  simplification, executable-edge SCCP, copy/trivial-phi elimination, DCE,
  dominance-aware CSE, CFG cleanup, bounded O3 fixpoint iteration, pass
  telemetry, and factual optimization remarks. Optimized SSA is not yet the
  native LLVM input.

### Frontend
- **Lexer** — full Nano surface syntax; correct line/column tracking; `break`
  and `continue` keywords now supported.
- **Parser** — all language constructs including `break;` and `continue;`.
- **Scope resolution** — forward function predeclaration, stable symbol
  ownership, source-location diagnostics.
- **Type checker** — all statement kinds including `ForStmt`, `BreakStmt`,
  `ContinueStmt`; function call type checking; return type checking.

### Desugaring passes
- **`DesugarForPass`** (`desugar-for`) — `for` → `while`; recurses into
  `FunctionStmt` bodies.
- **`DesugarIncDecPass`** (`desugar-inc-dec`) — `x++`/`x--` → `x = x ± 1`.
- **`DesugarPlusAssignPass`** (`desugar-plus-assign`) — `x += e` → `x = x + e`.

### Pass infrastructure
- **`nanopass::PassManager`** — once-per-program; stop-after; disable-pass;
  instrumentation; analysis invalidation.
- **`nanopass::AnalysisManager`** — type-safe cache; invalidation.
- **`nanopass::PassRegistry`** / **`PipelineBuilder`** — all passes and
  analyses registered; `nano passes` / `nano analyses` CLI commands.
- **`NodeCountAnalysis`** / **`ASTDepthAnalysis`** — AST-level analysis.

### CoreIR (`compiler/ir/core/`)
- **`core_ir.h`** — typed tree IR; `LocalId` with disambiguation index;
  `BinaryOpKind`/`UnaryOpKind` enums; `CoreConstantVal` variant.
- **`ast_to_core.h`** — AST → CoreIR lowering; `print` → `@print` call;
  all statement/expression kinds including `break`/`continue`.
- **`core_printer.h`** — deterministic text output.
- **`core_verifier.h`** — structural invariant checking.

### ANFIR (`compiler/ir/anf/`)
- **`anf_ir.h`** — `ANFAtom` (Constant/Variable/Temporary); `TemporaryId`;
  `ANFComputation`; full statement hierarchy.
- **`core_to_anf.h`** — CoreIR → ANF lowering; correct short-circuit
  `&&`/`||` via control-flow patterns (not binary operations); evaluation
  order preserved left-to-right.
- **`anf_printer.h`** — deterministic text output with `%t0`/`%t1` temporaries.
- **`anf_verifier.h`** — single-assignment, defined-before-use,
  loop-context checking.

### CFG IR (`compiler/ir/cfg/`)
- **`cfg_ir.h`** — `BasicBlock`, `CFGInstruction`, `Terminator` variant
  (Jump/Branch/Return/Unreachable); `BlockId`/`ValueId`/`LocalId`.
- **`cfg_builder.h`** — explicit builder; loop context stack for
  `break`/`continue`; `finalizeEdges()` for predecessor/successor consistency.
- **`anf_to_cfg.h`** — ANF → CFG lowering; all control flow constructs.
- **`cfg_printer.h`** — deterministic block-by-block text output.
- **`cfg_verifier.h`** — entry block, terminator, edge consistency,
  reachability; unreachable blocks reported as warnings.
- **`cfg_graphviz.h`** — DOT output with subgraphs per function; coloured edges.

### CFG Analyses
- **`ReachabilityAnalysis`** — BFS from entry; deterministic order.
- **`ReversePostOrderAnalysis`** — DFS postorder reversed; entry first.
- **`CFGStatsAnalysis`** — block/edge/instruction/branch/call/return counts.

### IR Pipeline
- **`pipeline.h`** — `IRPipeline::runAll()` wires AST → Core → ANF → CFG with
  verification at each stage; throws `std::runtime_error` on internal failure.

### CLI
- `nano emit <file> --format=core|anf|cfg|ssa|llvm|tokens|ast|desugar`
- `nano graph <file> --cfg|--dominator-tree [-o out.dot]` — no longer a stub
- `nano explain <file>` — real output with per-stage stats and desugar rewrites
- `nano verify <file>` — 11 stages: Parse/Resolve/TypeCheck/3×Desugar/Core/ANF/CFG/SSA/LLVM
- `nano passes` / `nano analyses`
- `--debug-pass-manager`, `--debug-analyses`, `--print-pipeline`,
  `--stop-after`, `--disable-pass`

### Examples
- `hello`, `factorial`, `fibonacci`, `fizzbuzz`, `loops`, `branching`,
  `functions`, `desugaring`, `short_circuit`, `control_flow`, `errors`

### Tests
| Category | Count |
|---|---|
| CLI smoke (help, version, passes, analyses) | 4 |
| Frontend unit | 1 |
| Pass manager unit (22 tests) | 1 |
| IR/SSA/optimizer assertions | 188 |
| Per-example emit/desugar/verify (8 examples × 3) | 24 |
| Phase 3 core/anf/cfg/graph/explain (4 examples × 5) | 20 |
| Short-circuit format tests (3 formats) | 3 |
| Control-flow format tests (2 formats) | 2 |
| Pass-manager CLI smoke tests | 5 |
| Errors.nano rejection | 1 |

LLVM-independent suites are validated locally. Full CTest and sanitizer runs
remain pending a local LLVM development installation.

---

## Experimental / disconnected

- **`ANFPass`** (`passes/anf_pass.h`) — old AST-level ANF transformer; lacks
  FunctionStmt recursion and short-circuit correctness; superseded by
  `compiler/ir/anf/` pipeline.
- **`CPSPass` / CPS IR** — teaching/research CPS transformation; retained.
- **`IRPrinter`** — flat 3-address IR from earlier days; retained.
- **`DesugarBoolPass`** / **`DesugarIfElsePass`** — exist as headers; not enabled.

---

## Planned (not yet implemented)

### Phase 4 — SSA ✅
- [x] Dominator tree (Cooper/Harvey/Kennedy)
- [x] Dominance frontier
- `LoopInfo` (natural loop detection)
- [x] SSA construction: phi placement via dominance frontiers
- [x] Variable renaming
- [x] SSA verifier and def-use index
- [x] `nano emit --format=ssa`

### Phase 5 — Optimizer ✅
- [x] Constant folding, algebraic simplification, executable-edge SCCP,
  copy propagation, trivial phis, DCE, CSE, and CFG cleanup
- [x] Verified O0/O1/O2/O3 and custom SSA pass pipelines
- [x] Optimized SSA CLI emission, `nano optimize`, explain/verify integration,
  remarks, and per-pass structural telemetry

### Phase 6 — Reliability
- Tree-walking reference interpreter (for differential testing)
- Grammar-aware fuzzing
- GitHub Actions CI

### Phase 7 — LLVM/JIT
- CFG → LLVM lowering (currently AST → LLVM is canonical)
- ORC JIT (`nano jit`)
- Optional adaptive JIT

### Phase 8 — Observability
- `nano explain` full polished UX
- JSON telemetry (`--pass-telemetry=json`)
- Optimization remarks

### Phase 9 — Web lab
- JSON API server (`nano-server`)
- Next.js compiler lab with Monaco, pipeline timeline, CFG explorer

---

## Known limitations

- **Canonical LLVM path** still uses AST → LLVM (not via CFG).  CFG → LLVM
  lowering is planned for Phase 7.
- **`bool` not assignable to `int`** in the type checker — short-circuit
  expressions produce `bool`; assigning to `int` is a type error.  Use
  `if/else` to convert.
- **No anonymous block scopes** — variable shadowing only across functions.
- **LLVM 14+ required** for build.
- **No native executable emission** yet — use `lli` with emitted IR.
