# Analysis caching scopes

NanoPass has two deliberately small analysis-cache adapters with a shared
invalidation vocabulary. `AnalysisManager` serves the existing AST `Program`
pass pipeline without changing its public `get<Analysis>(program)` API.
`SSAAnalysisManager` serves SSA analyses whose subjects are more granular than
an AST program.

## SSA cache identity

An SSA cache key contains the analysis type, the owning `SSAProgram` lifetime
token, and a subject discriminator:

- program analysis: `(type, SSAProgram address, empty function name)`
- function analysis: `(type, SSAProgram address, stable function name)`

The program address is used only while the caller owns that in-memory SSA
program; cache entries are explicitly invalidated before the program is
discarded. Function cache identity never uses a `SSAFunction*`, so vector
reallocation cannot accidentally redirect a cached result to another function.
Distinct SSA programs cannot collide, even when they use the same function or
block/value names.

## Analyses and dependencies

`CachedLoopInfoAnalysis` is a function-scoped result and computes `LoopInfo`
from the function CFG. `CachedCallGraphAnalysis` is a program-scoped result and
computes direct edges plus recursive SCC membership. A client requests each
through a type-safe template call:

```cpp
analyses.get<CachedLoopInfoAnalysis>(program, function);
analyses.get<CachedCallGraphAnalysis>(program);
```

Repeated requests for the same subject reuse the stored value. Requests for a
different function or program compute an independent value. The design keeps
analysis computations ordinary value objects; it never exposes `void*` or
stores pointers into `SSAFunction::blocks`.

## Invalidation

`invalidateFunction(program, function)` removes every cached function result
for that subject. `invalidateProgram(program)` removes both program-scoped and
all function-scoped entries for that SSA program. The CFG inliner calls both
after each successful caller mutation: function analyses such as loop/dominator
information cannot describe the old CFG, and the program call graph no longer
describes the new direct edges.

Function-local transformations that leave CFG topology unchanged may preserve
topological analyses, but callers must invalidate any cached def-use or value
analysis they alter. CFG-changing transformations are conservative and
invalidate their function subject. The AST manager's `PreservedAnalyses`
behavior remains unchanged for Phase 2 passes.
