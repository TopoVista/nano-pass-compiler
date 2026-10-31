# Project Status

Last updated: Phase 8 foundation — verified SSA-to-LLVM lowering, optional ORC
JIT execution, host object emission, and reproducible native differential
testing are implemented. Standalone AOT linking and runtime safety traps remain
planned.

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
  telemetry, and factual optimization remarks. Optimized SSA is the explicit
  input to the optional SSA LLVM backend.

### LLVM backend and JIT (Phase 8 foundation)
- **SSA → LLVM lowering** — direct lowering of verified SSA functions,
  predeclared functions for forward calls/recursion, deterministic block and
  phi creation, typed memory-backed array locals, and mandatory LLVM function
  and module verifier gates.
- **ORC JIT** — `nano jit` / `nano run` execute optimized SSA through ORCv2
  LLJIT when built with `NANO_BUILD_JIT=ON`; C-ABI print shims capture output.
- **Host object emission** — `nano compile --backend=ssa --emit-obj -o file.o`
  emits a target-machine-verified host object file.
- **Native differential runner** — `nano_jit_correctness fuzz` replays
  generated CoreIR-vs-O3-SSA/JIT cases. Recorded local campaign: seed
  `12648430`, 1,000 generated programs, 1,000 comparisons, 0 mismatches.

### Correctness testing (Phase 7)
- **CoreIR reference interpreter** — scalar expressions, fixed-size arrays,
  control flow, calls, recursion limits, print, and explicit runtime errors.
- **SSA interpreter** — independent frames/registers; predecessor-sensitive,
  parallel phi assignment; optimized O0–O3 execution.
- **Differential runner and fuzzing** — Core-vs-SSA O0–O3 execution,
  deterministic typed generation including nested calls, malformed-input smoke fuzzing,
  JSON summaries, persisted failures, and AST-guided semantic reduction.

### Advanced optimizer (Phase 6 complete)
- **LoopInfo** — dominance-qualified natural loops, latches, exits, stable
  nesting/depth, and reusable preheader discovery.
- **LICM / strength reduction** — conservative scalar LICM and integer
  multiply-by-two reduction; unsafe/speculative operations are rejected.
- **Interprocedural analysis** — direct call graph and recursive-SCC detection.
- **Inlining** — verifier-gated direct CFG cloning with deterministic block/value
  remapping, caller splitting, successor-phi repair, and multi-/void-return
  continuations under explicit cost/budget rejections.

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
| Pass manager assertions | 90 |
| IR/SSA/optimizer/correctness assertions | 5,426 |
| LLVM backend/JIT assertions | 496 |
| Per-example emit/desugar/verify (8 examples × 3) | 24 |
| Phase 3 core/anf/cfg/graph/explain (4 examples × 5) | 20 |
| Short-circuit format tests (3 formats) | 3 |
| Control-flow format tests (2 formats) | 2 |
| Pass-manager CLI smoke tests | 5 |
| Errors.nano rejection | 1 |

Local WSL validation with LLVM 18.1.3: **80/80 CTest** passed; ASan+UBSan
passed the 5,426-assertion IR suite and 496-assertion LLVM backend suite. The
Windows LLVM package remains invalid because its imported targets reference a
missing `C:\llvm\bin\llvm-tblgen.exe`; it was not modified or bypassed.

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

### Phase 6 — Advanced optimization ✅
- [x] LoopInfo, canonical preheaders, LICM, and conservative strength reduction
- [x] Direct CallGraph/SCC recursion analysis and scoped SSA analysis caching
- [x] Deterministic general CFG inlining with multi-return/void continuations,
  edge rebuilding, analysis invalidation, and verifier gates

### Phase 7 — Correctness, differential testing, and fuzzing ✅
- [x] CoreIR and SSA interpreters with shared limits, fixed-size arrays, and parallel phi execution
- [x] Differential O0–O3 semantic corpus and fixed-seed 1,000-case / 4,000-comparison campaign
- [x] Mismatch persistence, AST-guided semantic reduction, and malformed-input smoke fuzzing
- [x] LLVM-independent GitHub Actions correctness job with bounded fuzz smoke
- [x] WSL LLVM 18.1.3 ASan+UBSan execution: 5,426 IR assertions and 496
  backend/JIT assertions passed. The Windows package remains independently
  broken (`C:\llvm\bin\llvm-tblgen.exe` missing).

Latest recorded LLVM-independent campaign: seed `42`, 1,000 generated and
valid programs, 4,000 O0–O3 comparisons, 0 mismatches/crashes/timeouts, and
3,192,709 µs elapsed. All generated cases exercised arithmetic, comparisons,
conditionals, bounded loops, direct/nested calls, mutable locals, and returns.
Separate malformed seed `8128` rejected 256/256 inputs with 0 crashes.

### Phase 8 — LLVM/JIT (in progress)
- [x] Verified SSA → LLVM IR lowering, including CFG, phi, call, recursion,
  in-bounds fixed-array, and short-circuit coverage
- [x] Optional ORCv2 JIT and captured typed print runtime
- [x] Host object-file emission and replayable native differential runner
- [ ] Standalone executable linking with a distributable runtime library
- [ ] Native array bounds traps, execution limits, profiling, and adaptive JIT

### Phase 9 — Observability
- `nano explain` full polished UX
- JSON telemetry (`--pass-telemetry=json`)
- Optimization remarks

### Phase 10 — Web lab
- JSON API server (`nano-server`)
- Next.js compiler lab with Monaco, pipeline timeline, CFG explorer

---

## Known limitations

- **Canonical default LLVM path** remains AST → LLVM for compatibility;
  `--backend=ssa` selects the verified SSA path explicitly.
- **`bool` not assignable to `int`** in the type checker — short-circuit
  expressions produce `bool`; assigning to `int` is a type error.  Use
  `if/else` to convert.
- **No anonymous block scopes** — variable shadowing only across functions.
- **LLVM 14+ required** for build.
- **No standalone native executable linking** yet — host object emission is
  available, but it requires a future distributable Nano runtime to link.
- **Native bounds/runtime isolation** are not implemented. The JIT test claim
  covers in-bounds generated programs only; it does not claim a sandbox.
