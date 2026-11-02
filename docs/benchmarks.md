# Benchmarks

`benchmarks/corpus/` contains 13 readable Nano workloads: arithmetic, branching, calls, recursion, arrays, accumulation, nested loops, LICM/SCCP/CSE/inlining targets, and PGO/adaptive-JIT shapes.

Run a local server first, then use the reproducible standard-library harness:

```sh
python3 tools/run_benchmarks.py \
  --server http://127.0.0.1:8080 \
  --warmup 2 --iterations 7 --modes ssa \
  --policy heuristic \
  --output benchmarks/results/latest.json
```

The result is schema-v1 JSON, ignored by Git. It records machine/LLVM/compiler-policy metadata, measured frontend, aggregate lowering, **CFG→SSA construction plus SSA verification**, Nano optimization, SSA→LLVM lowering, and LLVM-verification timings separately. It also records execution warmup samples, median/p25/p75, and deterministic structural counters. Compile stages are separate from execution. The present service records JIT as end-to-end execution only; it intentionally labels separate JIT materialization timing unavailable rather than estimating it.

The final local Phase 11 run on 2026-11-03 used WSL LLVM 18.1.3, a
`local-trusted` service, the full 13-program corpus, O0–O3, one warmup, and
three measured samples for each of the SSA-interpreter, JIT, and adaptive-JIT
modes. It produced **156 rows per policy** (52 compilation configurations ×
3 execution modes). Across the 52 unique compilation configurations,
heuristic inlined 14 calls and learned inlined 12; both emitted 22 factual
policy records. The aggregate compiler telemetry was 89,007 μs for heuristic
and 149,773 μs for learned in that single run. Of 156 paired runtime medians,
learned was lower in 37, higher in 118, and equal in one. These three-sample,
machine-local figures are recorded as an honest smoke/evaluation result—not a
claim that learned policy improves runtime. Generated JSON is deliberately not
checked in because timings are machine-specific.
