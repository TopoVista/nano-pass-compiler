# Inlining

The SSA inliner accepts direct, non-recursive CFG callees under explicit
per-callee, per-caller, and whole-program growth budgets. It supports arbitrary
reachable callee CFGs: branches, internal phis, multiple returns, void returns,
and natural loops. Indirect calls are not represented by Nano SSA and remain
outside this transform.

For a selected call, the transform first makes every rejection decision without
mutating the caller. It then allocates clone BlockIds in callee block order,
followed by a continuation BlockId. Parameter ValueIds map directly to actual
call operands. All callee phi/instruction result ValueIds are preallocated in
the same stable order, so forward references need no special case. The caller
is split: instructions before the call stay in place; later instructions and
the original terminator move to the continuation; the original block jumps to
the cloned entry. Any phi in an old successor has its predecessor changed from
the original caller block to the continuation.

Each cloned return jumps to the continuation. A single value return directly
replaces the call result; multiple value returns feed a continuation phi with
incoming edges sorted by cloned BlockId. Constants returned on a merged path
are materialized as fresh SSA constants before the phi. Void return paths have
no result phi and simply reconnect to the continuation. Nested direct calls are
cloned unchanged; they are deliberately not recursively expanded during the
same inliner run. CFG predecessor/successor lists are rebuilt from terminators,
the mutated caller and program analyses are invalidated, and `verifySSA()`
gates every successful inline.

The cost is `instructionCount + 2 * blockCount + (returnCount - 1)`. The
inliner records factual counters for considered/inlined/rejected calls, cloned
blocks/instructions/phis, merged return sites, and continuation phis. Rejection
codes are `INLINE_RECURSIVE`, `INLINE_COST_TOO_HIGH`,
`INLINE_GROWTH_BUDGET`, `INLINE_DEPTH_LIMIT`,
`INLINE_NORETURN_UNSUPPORTED`, and `INLINE_UNSUPPORTED_CFG`.
