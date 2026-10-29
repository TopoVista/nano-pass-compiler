# Runtime semantics

Phase 7 execution is a reference model for the well-defined scalar CoreIR/SSA
subset. It is intentionally separate from the AST-to-LLVM backend.

- Integer literals currently enter CoreIR as signed `int64_t`; interpreter
  arithmetic uses C++ signed 64-bit operations. Differential generation avoids
  overflowing arithmetic, so overflow is not an asserted portability contract.
- Integer division and remainder follow C++ truncation toward zero. Division or
  remainder by zero returns the shared `division-by-zero` runtime error.
- Floating values use host `double`; generated tests do not rely on NaN or
  implementation-specific formatting.
- Conditions accept `bool` and integer truthiness. `&&` and `||` are evaluated
  left-to-right with short-circuiting in CoreIR; SSA receives the already
  explicit control-flow lowering.
- Declarations without initializers are reported as `uninitialized-local` by
  the Core interpreter. Generated programs always initialize locals.
- Locals use `LocalId(name,index)`, so execution never aliases shadowed locals
  merely because their spelling matches. Parameters are passed by value in a
  fresh frame; recursion uses fresh frames and a configured call-depth limit.
- `print` appends the value's deterministic text followed by a newline. Output
  is bounded. Strings print their contents; arrays print as `<array>` if a
  future supported runtime path produces one.
- `break`, `continue`, `return`, if/else, while, calls, and scalar arithmetic
  are supported. Arrays/indexed memory and residual SSA local memory are
  currently reported as `UnsupportedFeature`; they are excluded from semantic
  fuzzing rather than given guessed semantics.

The installed LLVM backend remains a separate oracle candidate once its local
toolchain is repaired. Optimized SSA is not claimed to execute natively.
