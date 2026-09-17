# Nano SSA optimizer

NanoPass optimizes verified SSA for inspection; it does not yet lower optimized
SSA into LLVM. Use `nano optimize examples/optimizer.nano -O2` or
`nano emit examples/optimizer.nano --format=optimized`.

O0 verifies only. O1 performs constant folding, algebraic simplification,
copy/trivial-phi elimination, and DCE. O2 adds SCCP, CFG cleanup, and
dominance-aware CSE. O3 currently repeats the existing cleanup sequence rather
than claiming new advanced optimizations.

SCCP tracks known SSA constants and executable control-flow. A known branch
becomes a jump; subsequent CFG simplification removes the unreachable path.
Arithmetic whose result is unused is removable, while calls and memory stores
remain because Nano conservatively treats them as observable. CSE only reuses
pure binary expressions whose earlier definition dominates the later use;
identical sibling expressions are therefore not merged.
