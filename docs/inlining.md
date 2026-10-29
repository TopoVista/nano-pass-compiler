# Inlining

The SSA inliner accepts direct, non-recursive CFG callees under an explicit
cost/growth budget. It splits the caller at the callsite, maps parameters to
arguments, preallocates fresh BlockIds and ValueIds, clones phis/instructions
and terminators, and rebuilds CFG predecessor information.

Each cloned return jumps to a continuation. A single value return directly
replaces the call result; multiple value returns feed a deterministic
continuation phi. Void return paths simply reconnect to that continuation.
Original successor phi predecessors are rewritten from the caller block to
the continuation. Nested calls are cloned structurally and remain subject to
subsequent inliner policy; SSA verification runs after every inline.
