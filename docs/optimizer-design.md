# Nano SSA optimizer design

The optimizer consumes verified `nanopass::SSAFunction` objects and does not
alter the established AST-to-LLVM execution path. Every function pass runs
under an `SSAOptimizationContext`; when verification is enabled, `verifySSA`
runs after that pass and turns an invariant break into an internal compiler
error.

The common side-effect model is deliberately conservative: constants, copies,
unary operations, binary operations, and phis are pure; indexed loads read
memory; indexed stores write memory; local memory operations and calls may
have observable effects. CSE and DCE never remove or reorder impure work.

SCCP uses `Unknown`, `Constant`, and `Overdefined` lattice states together
with executable CFG edges. Phi nodes meet only executable predecessor inputs.
The analysis uses value and edge worklists; after convergence it replaces
known values, folds known branches, and delegates structural cleanup to CFG
simplification.

Initial pipelines are O0 (verification only), O1 (local simplification and
DCE), O2 (SCCP, CFG cleanup, local simplification, CSE, DCE), and O3 (O2 plus
a bounded cleanup fixpoint). CFG-changing passes invalidate reachability,
RPO, dominators, and dominance frontiers; value-only passes keep CFG analyses
valid but rebuild def-use data as needed.
