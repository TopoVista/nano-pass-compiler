# Interpreters

`CoreInterpreter` is the correctness-first reference engine: each call owns a
fresh `LocalId`-keyed frame. `SSAInterpreter` is independent: it owns a fresh
`ValueId` register map and aggregate-local memory environment per invocation.
Both return `ExecutionResult`: status, optional return value, stdout/stderr,
typed runtime error, and executed step count.

Supported execution includes scalar constants, copies, unary/binary operations,
comparisons, short-circuit-derived control flow, calls/recursion, residual
memory locals, fixed-size local arrays, indexed loads/stores, branches, jumps,
and returns. Unsupported aggregate parameters and arrays-of-arrays produce an
explicit unsupported result rather than guessed behavior.

Limits cover steps, call depth, output bytes, and fixed-array element
allocation. Step/output limits are reported as bounded execution outcomes and
are excluded from successful fuzz equivalence counts.

SSA block entry uses predecessor-sensitive, parallel phi evaluation: all phi
operands are read from the pre-entry register environment, collected, then all
phi results are assigned together. This preserves loop-carried and swapped phi
semantics. The interpreter therefore directly tests cloned Phase 6 blocks,
continuations, and return phis rather than relying on SSA verification alone.

Fixed-size local arrays use independent memory environments in CoreIR and SSA.
CFG records aggregate-local types and SSA carries that metadata, so both
engines allocate source-declared bounds rather than inferring bounds from a
first store. Bounds failures use the shared `array-bounds` category.

The interpreter is a test oracle, not a native backend. Optimized SSA is
executed by the SSA interpreter; it is not represented as native execution.
