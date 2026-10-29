# Interpreters

`CoreInterpreter` executes verified CoreIR with explicit call frames keyed by
`LocalId`. `SSAInterpreter` executes verified SSA with a fresh register map per
function invocation. Both return the common `ExecutionResult` contract:
status, optional return value, stdout/stderr, typed runtime error, and executed
step count. Limits cover steps, call depth, and output bytes.

SSA block entry uses predecessor-sensitive, parallel phi evaluation: all phi
operands are read from the pre-entry register environment, collected, then all
phi results are assigned together. This preserves loop-carried and swapped phi
semantics. The interpreter therefore directly tests cloned Phase 6 blocks,
continuations, and return phis rather than relying on SSA verification alone.
