# Roadmap

## Phase 4 — CFG analyses + SSA ✅
- [x] Deterministic dominators, dominance frontier, and dominator graph output
- [x] Iterated-DF phi placement and dominator-tree renaming
- [x] Def-use index and `replaceAllUses`
- [x] SSA verifier, printer, `nano emit --format=ssa`, verify/explain integration
- [ ] LLVM-backed CTest and sanitizer validation (local LLVM SDK unavailable)

## Phase 5 — SSA optimizer
- [x] Function-level optimizer infrastructure and post-pass verification
- [x] Folding, algebraic simplification, trivial phis, copy propagation, DCE
- [x] Executable-edge SCCP, CFG cleanup, and dominance-aware CSE
- [x] O0–O3 and custom pipelines; optimized SSA CLI inspection
- [ ] LLVM-backed CTest and sanitizer validation (local LLVM SDK unavailable)

## Phase 6 — Advanced optimization (planned)
- [x] Natural-loop discovery, nesting, exits, deterministic preheader synthesis, and phi repair
- [x] Conservative LICM and scalar strength reduction
- [x] Direct call graph, recursive SCC detection, and tiny SSA inlining
- [ ] Multi-block/multi-return inlining
- [ ] SSA-to-LLVM lowering and differential runtime validation

## Phase 1 — Stabilize existing compiler ✅
- [x] Root CMake with LLVM detection and clear error messages
- [x] Professional CLI (`emit`, `compile`, `verify`, stubs for later commands)
- [x] Fix `lower_expr.cpp`: comparison ops, logical `&&`/`||`, unary `!`, strings
- [x] Fix `ASTGraphviz`: all node types, label escaping
- [x] Wire `DesugarFor → DesugarIncDec → DesugarPlusAssign` into pipeline
- [x] Add `ForStmt` / `BreakStmt` / `ContinueStmt` to `TypeCheckPass`
- [x] 8 sample programs in `examples/`
- [x] Remove generated artifacts from git; comprehensive `.gitignore`
- [x] 26 CTest targets; 100% passing
- [x] `docs/STATUS.md`, `docs/ROADMAP.md`, `README.md`

## Phase 2 — PassManager + AnalysisManager ✅
- [x] `nanopass::Pass` base class with stable ID / `PassInfo`
- [x] `nanopass::PassManager`: once-per-program execution, stop-after,
      disable-pass, instrumentation, analysis invalidation
- [x] `nanopass::AnalysisManager`: type-safe cache, preservation checks,
      debug callbacks
- [x] `nanopass::PreservedAnalyses` / `PassResult` / `PassStatistics`
- [x] `nanopass::PassInstrumentation` observer hooks + `PassTelemetry`
- [x] `nanopass::PassRegistry` + `PipelineBuilder`
- [x] `NodeCountAnalysis` + `ASTDepthAnalysis` (prove cache/invalidation works)
- [x] Migrate `DesugarForPass`, `DesugarIncDecPass`, `DesugarPlusAssignPass`
      to `Pass` base; fix `FunctionStmt` recursion gap; add rewrite counters
- [x] Structured `--debug-pass-manager` output with timing
- [x] `--debug-analyses`, `--print-pipeline`, `--stop-after`, `--disable-pass`
- [x] `nano passes` / `nano analyses` CLI commands
- [x] 22 pass-manager unit tests + 9 CLI smoke tests = 35 total CTest targets
- [x] `docs/pass-manager-design.md`

## Phase 3 — Explicit IR pipeline (CoreIR → CFG)
## Phase 3 — CoreIR + ANF + CFG ✅
- [x] `CoreIR` — typed tree IR; `LocalId` disambiguation; operator enums
- [x] `ASTToCoreLowering` — all AST constructs including break/continue
- [x] `CorePrinter` / `CoreVerifier`
- [x] `ANFIR` — atoms, temporaries, short-circuit via control flow
- [x] `CoreToANFLowering` — evaluation order preserved; no binary &&/||
- [x] `ANFPrinter` / `ANFVerifier`
- [x] `CFG IR` — BasicBlock, Terminator variant, BlockId/ValueId/LocalId
- [x] `CFGBuilder` — loop context stack; break/continue; edge finalization
- [x] `ANFToCFGLowering` — all structured control flow
- [x] `CFGPrinter` / `CFGVerifier` / `CFGGraphviz`
- [x] `ReachabilityAnalysis`, `ReversePostOrderAnalysis`, `CFGStatsAnalysis`
- [x] `IRPipeline::runAll()` — verified Core → ANF → CFG pipeline
- [x] break/continue keywords in lexer and parser
- [x] CLI: `--format=core/anf/cfg`, `nano graph --cfg`, real `nano explain`
- [x] `nano verify` — 10 stages including Core/ANF/CFG
- [x] 62/62 tests (normal + ASan+UBSan)
- [x] `docs/ir.md`, `docs/ir-design.md`

## Phase 4 — SSA
- [ ] Implement dominator tree (Cooper/Harvey/Kennedy algorithm)
- [ ] Implement dominance frontier
- [ ] Implement `LoopInfo` (natural loop detection)
- [ ] SSA construction: phi placement via dominance frontiers
- [ ] Variable renaming
- [ ] `SSAVerifier`: single assignment, dominance, phi consistency
- [ ] `nano emit --format=ssa`
- [ ] `nano verify --stage=ssa`
- [ ] Extensive graph algorithm tests (linear, diamond, nested loops,
      multiple exits, unreachable blocks)

## Phase 5 — Optimizer
- [x] Constant folding and conservative algebraic simplification
- [x] Sparse conditional constant propagation with executable edges
- [x] Copy propagation, trivial phi elimination, and iterative DCE
- [x] Dominance-aware pure-expression CSE and conservative CFG cleanup
- [x] O0–O3 Nano pipelines, bounded O3 fixpoint behavior, custom pass lists
- [x] Optimizer verification, telemetry, remarks, CLI/explain/verify integration

## Phase 6 — Reliability
- [ ] Tree-walking reference interpreter
- [ ] Differential testing: interpreter vs O0 vs O2
- [ ] Grammar-aware fuzzing with program generator
- [ ] Failure minimization (delta-debugging)
- [ ] Sanitizer CI (AddressSanitizer + UBSan on Linux/Clang)
- [ ] GitHub Actions CI (Linux build + tests + sanitizers)

## Phase 7 — LLVM/JIT
- [ ] LLVM AOT object and executable emission
- [ ] LLVM module verification gating (already done; strengthen)
- [ ] Optional ORC JIT (`nano jit`) under `NANO_BUILD_JIT`
- [ ] Optional adaptive JIT (tiered recompilation on hot functions)
- [ ] Profile collection (`nano profile`)

## Phase 8 — Observability
- [ ] `nano explain` — full transformation journey with before/after diffs
- [ ] Structured diagnostics: rich source spans, labels, fix-it hints
- [ ] Pass telemetry JSON (`--pass-telemetry=json`)
- [ ] Optimization remarks (`OptimizationApplied` / `OptimizationMissed`)
- [ ] "Why didn't this optimize?" structured rejection reasons
- [ ] JSON API server (`nano-server`) for web frontend

## Phase 9 — Web compiler lab
- [ ] `nano-server` HTTP API: `/compile`, `/explain`, `/verify`, `/optimize`
- [ ] Next.js + TypeScript + Tailwind frontend
- [ ] Monaco editor with Nano language support
- [ ] Pass timeline visualization
- [ ] CFG / dominator tree / SSA explorer
- [ ] Optimization inspector (toggle passes, see before/after)
- [ ] Diagnostics panel
- [ ] Benchmark dashboard (real measured numbers only)

## Phase 10 — Advanced research
- [ ] Profile-guided optimization (branch layout, inlining decisions)
- [ ] Learned optimization policy (`HeuristicPolicy` + optional `LearnedPolicy`)
- [ ] Verifier-gated GenAI advisor (LLM suggests known passes; compiler validates)
- [ ] Optional Alive2 translation validation (`NANO_ENABLE_ALIVE2`)
- [ ] Optional e-graph optimizer for pure arithmetic
- [ ] Optional RISC-V backend (RV64I subset, linear scan register allocation)
