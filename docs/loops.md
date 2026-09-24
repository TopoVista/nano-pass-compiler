# Loops

NanoPass identifies natural loops from dominance-qualified backedges. A loop
header dominates every latch; a reverse predecessor walk constructs the loop
body. LoopInfo records stable IDs, headers, latches, exits, parent/children,
depth, and a reusable preheader when one already exists.

Use `nano graph examples/nested_loops.nano --loops` to inspect detected loop
headers and nesting depth. The implementation deliberately does not claim to
canonicalize irreducible control flow.
