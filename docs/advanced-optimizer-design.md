# Advanced SSA optimizer design

Phase 6 extends the verified SSA optimizer with analyses and transformations
that remain independent from the AST-to-LLVM backend. Every advanced pass uses
the same post-pass SSA verifier gate as Phase 5.

`LoopInfoAnalysis` identifies a natural loop only from a CFG edge `latch ->
header` where the header dominates the latch. A reverse predecessor walk from
the latch constructs the loop. Loops are sorted by stable header/block IDs;
proper set containment builds the nesting tree. Exit edges are explicit.

`loop-simplify` creates a single preheader when a loop header has multiple
outside predecessors. Header phi inputs are merged in a preheader phi only
when their external values differ. LICM hoists only pure, non-trapping unary
or add/sub/mul/compare instructions whose operands are constants, parameters,
outside-loop definitions, or previously hoisted invariants. Calls, memory,
division, and modulo are never speculated.

The call graph records direct call sites by function name and detects recursive
strongly connected components. The initial inliner is deliberately limited to
single-block, single-return callees; it remaps every ValueId and reconnects the
caller continuation. A centralized budget/cost model rejects recursion, large
callees, caller-growth overflow, and unsupported shapes.

Advanced passes emit structured applied/missed decisions with reason codes.
Those records are the source of `--why-not-optimized`; human wording is only a
rendering of the machine-generated legality decision. Phase 7 can consume the
same SSA snapshots and decision records for differential testing.
