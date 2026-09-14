# Roadmap

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
- [ ] Define `CoreIR` — reduced typed IR after all desugaring
- [ ] `CoreIR` verifier: all names resolved, all expressions typed,
      no high-level syntax remaining
- [ ] `CoreIR` printer and `nano emit --format=core`
- [ ] Lower `CoreIR` to `CFG` IR: `BasicBlock`, `Terminator`, `Branch`,
      `ConditionalBranch`, `Return`
- [ ] CFG verifier: every block ends in terminator, predecessor/successor
      consistency, no unreachable blocks
- [ ] CFG Graphviz output + `nano graph --cfg`
- [ ] ANF integration into the production pipeline
- [ ] `nano emit --format=anf` / `--format=cfg`
- [ ] Wire new IR stages through `PassManager`
- [ ] Update all 35 existing tests; add CFG-specific tests

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
- [ ] Constant folding
- [ ] Constant propagation / SCCP with lattice states
- [ ] Dead-code elimination
- [ ] Copy propagation
- [ ] Common subexpression elimination
- [ ] Algebraic simplification
- [ ] `SimplifyCFG` (constant branch folding, trivial block elimination)
- [ ] Function inlining (small-function heuristic)
- [ ] Loop invariant code motion (LICM)
- [ ] Optimization levels `-O0` through `-O3`
- [ ] `nano optimize --passes=sccp,dce,licm`
- [ ] `--print-pipeline` for optimization pipelines

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
