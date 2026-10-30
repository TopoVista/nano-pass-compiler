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
  is bounded. Strings print their contents; arrays print as `<array>`.
- Fixed-size local arrays are allocated with zero values for their declared
  element type. Indexed loads and stores check signed bounds in both engines;
  an invalid index reports `array-bounds`. Array allocation is capped by the
  execution limit. Aggregate parameter passing and arrays of arrays are not
  currently part of the supported execution subset.
- `break`, `continue`, `return`, if/else, while, calls, scalar arithmetic,
  fixed-size arrays, indexed memory, and residual memory-backed SSA locals
  are supported. The typed generator remains scalar-only so its reported
  coverage is precise rather than aspirational.
- Execution uses explicit step, call-depth, output-byte, and array-element
  budgets. Division/remainder by zero and bounds failures have defined shared
  runtime categories. Signed-overflow-dependent programs, NaN-sensitive
  floating comparisons, aggregate parameters, and arrays of arrays are not
  claimed as differential-equivalence inputs.

The installed LLVM backend remains a separate oracle candidate once its local
toolchain is repaired. Optimized SSA is not claimed to execute natively.
