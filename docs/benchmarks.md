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

The result is schema-v1 JSON, ignored by Git. It records machine/LLVM/compiler-policy metadata, one compile measurement from compiler telemetry, execution warmup samples, median/p25/p75, and deterministic structural counters. Compile stages are separate from execution. The present service records JIT as end-to-end execution only; it intentionally labels separate JIT materialization timing unavailable rather than estimating it.

An actual Phase 11 local smoke run on 2026-11-03 used WSL LLVM 18.1.3,
interpreter-only service, the full 13-program corpus, O0–O3, one warmup and
three samples. It produced **52 rows per policy** (heuristic and learned):
heuristic cloned 14 calls and learned cloned 12. The small three-sample timing
comparison varied by workload (including learned losses on `branch_heavy`,
`calls`, and `licm`); it is recorded as a smoke/evaluation result, not a claim
that learned policy improves runtime. Generated JSON is deliberately not
checked in because timings are machine-specific.
