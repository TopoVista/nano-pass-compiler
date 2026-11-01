# Nano execution profiles

`nano profile source.nano -o source.nprof.json` executes the verified SSA
interpreter and writes deterministic schema-v1 JSON. It records function calls,
SSA block entries, CFG edges, and natural-loop entry/backedge/exit counts.

Profiles contain a canonical SSA fingerprint and stable function names and
numeric `BlockId`s, never pointer identities or machine paths.

This is complete profile data, not proof of an optimization's profitability.
The interpreter and successful adaptive ORC executions both produce the same
schema-v1 function/block/edge/loop counter model.  A native adaptive profile
is only written as `complete` after the run returns and loop summaries have
been finalized; failed or interrupted runs are never accepted as PGO input.

The optional SSA optimization consumer is deliberately narrow:

```text
nano optimize source.nano --profile=source.nprof.json -O2
```

Measured calls at or above the current hot threshold may raise the direct
callee-size threshold from 12 to 24 instructions. This only changes a cost
choice; recursion rejection, growth limits, type checks, CFG restrictions, and
post-inline SSA verification remain mandatory. Branch and loop counts are
recorded for future layout/loop decisions but are not consumed yet.
Use `nano profile show source.nprof.json` to inspect a profile. The profile
command inherits the SSA interpreter's execution limits and
reports a runtime failure instead of writing a successful profile when the
program cannot complete.
