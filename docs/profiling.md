# SSA runtime profiling

`nano profile input.nano -o input.nprof.json` executes verified Tier-0/O0 SSA
in the independent SSA interpreter and emits deterministic schema-v1 JSON. The
profile identity is a canonical SSA FNV-1a fingerprint, not a source path or
memory address. Its `optimization_level` and `execution_mode` make collection
conditions explicit.

For each invoked function it records calls, executed block entries, CFG-edge
counts, and natural-loop entries/backedges/exits. Blocks and loop headers use
stable SSA `BlockId` numbers. The loop average is an explicitly conservative
edge-frequency estimate: `backedges / outside-to-header entries`.

```text
nano profile examples/loops.nano -o loops.nprof.json
nano profile examples/loops.nano -O2 -o loops.observed-o2.nprof.json
nano profile show loops.nprof.json
```

Profiles are accepted for optimization only when their schema, kind,
fingerprint, and status are valid. A profile from another SSA program produces
`PROFILE_PROGRAM_MISMATCH`; a `partial` or `failed` profile is never consumed
as PGO input. Counter collection obeys the interpreter's existing execution
limits. Failed execution produces a failed in-memory profile, never a profile
reported as complete.

The current PGO consumer intentionally accepts only Tier-0/O0 profiles. A
higher-level profile is useful for deterministic inspection but is rejected as
`PROFILE_OPTIMIZATION_LEVEL_MISMATCH`, avoiding accidental use against a
different SSA shape.

`nano explain input.nano --backend=ssa --profile=input.nprof.json` includes a compact summary
of the validated runtime facts consumed by the compilation journey. It reports
profile metadata and aggregate observed function, call, block-entry, edge, and
loop counts; it never invents a runtime measurement.

For a reproducible profile-guided loop-effort demonstration, use
`examples/pgo_hot_loop.nano`: its `step` function reaches the default hot
function threshold during O0 profile collection. `nano optimize ... -O2
--profile=... --remarks` then records `PROFILE_LOOP_EFFORT_ENABLED`; cold
functions record `PROFILE_LOOP_EFFORT_SKIPPED` through
`--why-not-optimized` instead.
