# Interview guide

- **Why ANF then SSA?** ANF makes evaluation order explicit before CFG lowering; SSA gives one-definition values and enables sparse data-flow optimization.
- **How are phis placed?** The builder uses iterated dominance frontiers then dominator-tree renaming; the verifier checks edge coverage and edge dominance.
- **Why is LICM conservative?** It requires a preheader, pure/safe-to-speculate instructions, and loop-invariant operands; division/modulo and memory are not speculated.
- **How does the inliner remain safe?** It splits the caller, deterministically maps blocks/values/phis, repairs successor phis, rebuilds edges, rejects recursion/cost/budget violations, then verifies SSA.
- **What does learned mean here?** A portable logistic model makes only a profitability choice for an already legal inlining candidate. It cannot emit IR, name a pass, waive budgets, or override verification.
- **Why not call the web server a sandbox?** Source compilation is in-process and local-trusted JIT is native execution. Public deployment needs an isolated worker with OS limits; the checked-in container defaults to interpreter-only.
