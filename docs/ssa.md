# SSA middle end

NanoPass lowers inspected programs through `CoreIR -> ANF -> CFG -> SSA`.
The established AST-to-LLVM backend remains the execution backend for now;
SSA is the verified middle-end representation used for analysis, inspection,
and future optimizations.

## Dominance and phi placement

`DominatorAnalysis` uses a deterministic reverse-postorder, iterative
immediate-dominator algorithm (the Cooper-Harvey-Kennedy formulation) over
reachable CFG blocks. `DominanceFrontierAnalysis` computes sorted frontier
sets. For each promotable local, the builder starts with its definition
blocks, walks the iterated dominance frontier, and inserts one phi per local
at each discovered join.

## Rename traversal

Parameters receive definitions before entry instructions. The builder walks
the dominator tree and maintains a stack of current `ValueId`s per `LocalId`.
Entering a block pushes its phis; promoted stores push a new definition and
promoted loads read the stack top. Phi incoming values are added on outgoing
CFG edges. Every push count is recorded and popped after recursively visiting
that dominator subtree, so a definition in one sibling cannot leak into
another sibling.

`LocalId` includes a disambiguating index: source-name shadowing does not
merge definitions. Arrays and locals without a safe entry lifetime remain
memory-backed; NanoPass deliberately does not claim MemorySSA.

## Example

CFG before promotion:

```text
entry: store 0 -> i; jump header
header: %0 = load i; %1 = lt %0, 10; branch %1, body, exit
body: %2 = load i; %3 = add %2, 1; store %3 -> i; jump header
```

SSA after promotion:

```text
entry: %0 = const 0; jump header
header: %1 = phi int [bb0: %0], [bb2: %3] ; i#0
        %2 = lt %1, 10; branch %2, bb2, bb3
body:   %3 = add %1, 1; jump bb1
```

A phi use belongs to its predecessor edge: an incoming definition must
dominate the corresponding predecessor, not necessarily the phi block.

## Def-use and verification

`SSADefUseIndex` is rebuildable and stores stable block/value IDs and vector
indices rather than pointers. It indexes instruction, phi, branch, return,
call, copy, and index operands. `replaceAllUses` covers those same mutable
locations.

`verifySSA` checks unique definitions, defined uses, ordinary and same-block
dominance, exact phi predecessor coverage, known type compatibility, branch
and return types, and that promoted locals have no residual load/store.
Use `nano emit input.nano --format=ssa`, `nano verify input.nano`, or
`nano explain input.nano` to inspect the real pipeline.
