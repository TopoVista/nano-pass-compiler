# Nano execution profiles

`nano profile source.nano -o source.nprof` executes the verified CoreIR
reference interpreter and writes a deterministic JSON profile. The current
schema is version `1` and records observed function calls, branch outcomes,
and loop iterations.

Profile site keys include the owning function, construct kind, source location
when available, and a deterministic per-function ordinal. The ordinal prevents
desugared constructs with unavailable source locations from colliding.

This is interpreter-collected profile data. It is not yet JIT instrumentation,
native sampling, or proof of an optimization's profitability.

The optional SSA optimization consumer is deliberately narrow:

```text
nano compile source.nano --backend=ssa --profile=source.nprof -O2 -o source
```

Measured calls at or above the current hot threshold may raise the direct
callee-size threshold from 12 to 24 instructions. This only changes a cost
choice; recursion rejection, growth limits, type checks, CFG restrictions, and
post-inline SSA verification remain mandatory. Branch and loop counts are
recorded for future layout/loop decisions but are not consumed yet.
The profile command inherits the reference interpreter's execution limits and
reports a runtime failure instead of writing a successful profile when the
program cannot complete.
