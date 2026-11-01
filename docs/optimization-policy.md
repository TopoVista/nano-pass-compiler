# Optimization-policy infrastructure

`OptimizationFeaturesV1` is a versioned, deterministic feature vector for a
function. It contains static CFG/SSA counts (instructions, blocks, edges,
phis, branches, calls, memory operations, loops and call-graph degrees) plus
optional complete-profile facts (calls, dynamic block execution, hot blocks,
dynamic edge execution, largest branch probability, loop backedges and largest
trip estimate). It never serializes pointers or
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

The default `-O` pipelines remain the normal compiler behavior. To execute a
policy-selected, function-local pipeline instead, opt in explicitly:

```text
nano optimize app.nano --profile=app.nprof.json --policy=heuristic --remarks
```

The compiler records the selected hotness class and ordered pass list as an
analysis remark. The policy cannot emit arbitrary transformations: every name
is resolved through the compiler's registered pass allowlist, and ordinary
post-pass SSA verification remains in force. Global inlining is intentionally
left to the default/profile-guided optimizer pipeline for now.

`--decision-log=decisions.jsonl` exports one deterministic schema-v1 record per
function. A record contains the source-program fingerprint, the fixed
heuristic threshold configuration, static/dynamic features, selected action,
hotness reason, and the observed before/after instruction and block counts
after the requested compiler pipeline. The log is a dataset export, not a
training loop: it makes future learned-policy experiments reproducible without
letting a model choose unregistered transforms. Decision replay and measured
reward collection remain planned.

Hotness derives only from counters and centralized thresholds, never wall-clock
time. Its structured reasons distinguish `COLD_FUNCTION`, `WARM_FUNCTION`,
hot function/block/loop criteria, and `INSUFFICIENT_PROFILE_DATA`. A future learned policy may implement the same interface but may only
choose among these legal, verifier-gated actions; it may not waive SSA, type,
recursion, memory-safety, or LLVM verifier checks.
