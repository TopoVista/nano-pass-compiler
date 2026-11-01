# Nano execution profiles

`nano profile source.nano -o source.nprof` executes the verified CoreIR
reference interpreter and writes a deterministic JSON profile. The current
schema is version `1` and records observed function calls, branch outcomes,
and loop iterations.

Profile site keys include the owning function, construct kind, source location
when available, and a deterministic per-function ordinal. The ordinal prevents
desugared constructs with unavailable source locations from colliding.

This is interpreter-collected profile data. It is not yet JIT instrumentation,
native sampling, PGO consumption, or proof of an optimization's profitability.
The profile command inherits the reference interpreter's execution limits and
reports a runtime failure instead of writing a successful profile when the
program cannot complete.
