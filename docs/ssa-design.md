# CFG, Dominance, and SSA design

This document defines the Phase 4 middle-end boundary. The canonical LLVM
backend still lowers the desugared AST directly; CFG/SSA is an independently
verified inspection and optimization path until that lowering is mature.

## CFG assumptions

`CFGFunction` owns basic blocks and refers to them only by stable `BlockId`.
Each block has one terminator and bidirectional predecessor/successor lists.
Mutable source variables are represented as `LocalId` values through
`LoadLocal` and `StoreLocal`; temporary computation results use the separate,
function-scoped `ValueId` domain. Arrays remain memory operations in Phase 4.

Reachability begins at `entryBlock`. Analyses do not assign semantic facts to
unreachable blocks: an unreachable block has no immediate dominator and is not
dominated by entry.

## Loop continuation semantics

The desugaring pass rewrites a `continue` belonging to a `for` body into a
block that first executes a clone of the increment expression and then
continues. It never descends into nested loops, whose continues have their own
targets. The CFG lowering can therefore keep the simple while-loop convention:
while-loop continuation jumps to its header.

## Dominators

NanoPass uses the Cooper-Harvey-Kennedy iterative immediate-dominator
algorithm over the existing reverse-postorder traversal. Entry is an internal
iteration sentinel, then exposed with no immediate dominator. The result stores
only `BlockId` values and constructs sorted dominator-tree child lists.

The algorithm is iterative rather than a set-intersection formulation; it is
well suited to the small and medium CFGs expected here and avoids cubic set
operations. A verifier checks that every reachable non-entry block has one
strict, dominating immediate dominator.

## Dominance frontier and planned SSA

Dominance frontiers are computed from immediate dominators using the standard
join-block walk. Their sets are sorted by `BlockId`. The SSA builder will use
iterated dominance frontiers per promotable `LocalId` to decide phi placement:
definition blocks seed a worklist; each discovered frontier gets one phi per
local and may itself become a worklist member.

SSA will be a separate representation, not a CFG object that silently changes
meaning. It will preserve the CFG shape and terminators, add phi instructions
at the start of blocks, and replace eligible scalar `LoadLocal`/`StoreLocal`
pairs with SSA values. Eligible locals are scalars without address-taking or
memory-specific semantics. Arrays and index operations remain in memory form.

Renaming will walk dominator-tree DFS with a stack of current `ValueId`s per
`LocalId`: phi results are pushed on entry; stores define and push a new value;
loads use the stack top and disappear; successor phis receive edge-specific
incoming values; all pushes are popped when leaving the block. Parameters will
be explicit entry definitions.

The SSA verifier will check CFG structure, unique definitions, type-consistent
uses, same-block ordering, ordinary-use dominance, edge-based phi dominance,
and exact phi-predecessor coverage. It will also build deterministic, rebuildable
definition and use indices for Phase 5 optimizations.
