# Inlining

The initial SSA inliner accepts direct, non-recursive, single-block,
single-return callees under a small explicit cost/growth budget. It clones
instructions, maps parameters to call operands, allocates fresh ValueIds, and
replaces the original call result. Multi-block and multi-return callees are
rejected with `UNSUPPORTED_CONTROL_FLOW`; no broader support is implied.
