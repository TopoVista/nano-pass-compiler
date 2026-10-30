# Deterministic semantic fuzzing

`NanoProgramGenerator` owns an `mt19937_64` seeded explicitly through
`GeneratorOptions`. Its initial typed grammar subset generates initialized
integer locals, arithmetic, bounded while loops, if/else, and print/return.
Function-call behavior is covered by dedicated differential regressions while
the generator stays inside the most broadly validated scalar subset. Every
source is reparsed and type-checked by the real
frontend; rejection is never counted as a successful comparison.

The generator deliberately excludes arrays, unbounded recursion, division by
unknown values, and overflow-dependent arithmetic until both interpreters have
defined matching semantics. A seed plus case index reproduces the source.
