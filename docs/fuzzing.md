# Deterministic semantic fuzzing

`NanoProgramGenerator` owns an `mt19937_64` seeded explicitly through
`GeneratorOptions`. Its initial typed grammar subset generates initialized
integer locals, arithmetic, bounded while loops, if/else, print/return, direct
calls, and a bounded nested-call pair (`relay` → `increment`). Every source is
reparsed and type-checked by the real frontend; rejection is never counted as
a successful comparison.

The generator deliberately excludes arrays, unbounded recursion, division by
unknown values, and overflow-dependent arithmetic even though fixed-size
arrays are executed by both engines. This keeps generated coverage accurately
reported as scalar. A seed plus case index reproduces the source.

`nano-correctness fuzz <seed> <cases> [save-dir]` emits measured JSON counts
for generated, valid, rejected, compared, unsupported, mismatch, crash,
timeout, and distinct-failure categories. Failed sources and metadata are
content-hashed in the requested directory. `nano-correctness malformed-fuzz
<seed> <cases>` is separate deterministic lexer/parser smoke fuzzing: parser
rejection is expected and is never a semantic comparison.
