# SSA runtime profiling

`nano profile input.nano -o input.nprof.json` executes verified SSA in the
independent SSA interpreter and emits deterministic schema-v1 JSON. The profile
identity is a canonical SSA FNV-1a fingerprint, not a source path or memory
address.

For each invoked function it records calls, executed block entries, CFG-edge
counts, and natural-loop entries/backedges/exits. Blocks and loop headers use
stable SSA `BlockId` numbers. The loop average is an explicitly conservative
edge-frequency estimate: `backedges / outside-to-header entries`.

```text
nano profile examples/loops.nano -o loops.nprof.json
nano profile show loops.nprof.json
```

Profiles are accepted for optimization only when their schema, kind,
fingerprint, and status are valid. A profile from another SSA program produces
`PROFILE_PROGRAM_MISMATCH`; a `partial` or `failed` profile is never consumed
as PGO input. Counter collection obeys the interpreter's existing execution
limits. Failed execution produces a failed in-memory profile, never a profile
reported as complete.
