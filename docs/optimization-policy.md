# Optimization-policy infrastructure

`OptimizationFeaturesV1` is a versioned, deterministic feature vector for a
function. It contains static CFG/SSA counts (instructions, blocks, edges,
phis, branches, calls, memory operations, loops and call-graph degrees) plus
optional complete-profile facts (calls, dynamic block execution, hot blocks,
loop backedges and largest trip estimate). It never serializes pointers or
unstable SSA values.

```text
nano features app.nano --profile=app.nprof.json
```

`OptimizationPolicy` separates legal candidates from profitability decisions.
The current `HeuristicOptimizationPolicy` selects only allowlisted pipelines:

```text
cold: constant-fold,dce
warm: sccp,simplify-cfg,cse,dce
hot:  sccp,simplify-cfg,loop-simplify,licm,cse,dce
```

Hotness derives only from counters and centralized thresholds, never wall-clock
time. A future learned policy may implement the same interface but may only
choose among these legal, verifier-gated actions; it may not waive SSA, type,
recursion, memory-safety, or LLVM verifier checks.
