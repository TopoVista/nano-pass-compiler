# Deterministic semantic fuzzing

`NanoProgramGenerator` owns an `mt19937_64` seeded explicitly through
`GeneratorOptions` controls seed, expression depth, statement count, function
count, call depth, loop nesting, and source-byte budget. Its typed subset
generates initialized integer locals, arithmetic, comparisons, conditionals,
bounded while loops, print/return, direct calls, and a bounded nested-call pair
(`relay` → `increment`). Every source is reparsed and type-checked by the real
frontend; rejection is never counted as a successful comparison.

The generator deliberately excludes arrays, unbounded recursion, division by
unknown values, and overflow-dependent arithmetic even though fixed-size
arrays are executed by both engines. This keeps generated coverage accurately
reported as scalar. A seed plus case index reproduces the source.

`nano-correctness fuzz --seed=42 --cases=1000 --max-depth=3
--max-statements=8 --max-functions=3 --max-call-depth=2
--max-loop-nesting=1 --max-source-bytes=8192
--save-failures=tests/regressions/generated` emits measured JSON counts
for generated, valid, rejected, compared, unsupported, mismatch, crash,
timeout, distinct-failure, elapsed-time, and per-feature categories. Failed
sources and metadata are content-hashed in the requested directory. Metadata
contains seed/case/options, compiler revision when supplied at build time,
the named optimization pipeline for every candidate, reference execution, and
every candidate execution. Positional `fuzz <seed> <cases> [save-dir]` remains
available for compatibility. `nano-correctness malformed-fuzz --seed=8128
--cases=256` is separate deterministic lexer/parser smoke fuzzing: parser
rejection is expected and is never a semantic comparison.
