# Resume notes (factual)

- Built a C++20 Nano compiler laboratory with explicit CoreIR, ANF, CFG, SSA, deterministic data-flow analyses, verifier-gated SSA optimization, and LLVM/ORC optional backend paths.
- Implemented CoreIR-versus-SSA differential execution, deterministic grammar-aware fuzzing/reduction, runtime profiles, adaptive JIT telemetry, and structured compiler-service artifacts for a TypeScript inspection UI.
- Added a reproducible, schema-checked logistic-regression **heuristic-imitation** inline policy. It selects only inline/do-not-inline after deterministic legality gates; compiler code performs the transform and SSA verification checks the result.
- Built bounded local service/container configurations and documented the non-sandboxed JIT boundary rather than claiming public multi-tenant execution safety.
