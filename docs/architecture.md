# Architecture audit (2026-09-14)

The active target is `compiler/main.cpp`, configured by `compiler/CMakeLists.txt`.
It directly drives `Lexer`, `Parser`, `ResolveScopesPass`, `TypeCheckPass`, and
the LLVM lowering helpers. `lower_stmt.cpp` and `lower_expr.cpp` lower the AST
straight to LLVM IR with allocas for local variables.

```mermaid
flowchart LR
  S[Source] --> L[Lexer]
  L --> P[Parser]
  P --> A[AST]
  A --> R[Scope resolver]
  R --> T[Type checker]
  T --> G[LLVM lowering]
  G --> V[LLVM verifier]
```

The parser is recursive descent and owns a vector of tokens. The AST is a
polymorphic `Stmt`/`Expr` hierarchy held through `std::unique_ptr`; semantic
information is attached to expressions through `LangType` and resolved
`Symbol*` pointers. Source locations exist but are only partially populated.

`passes/` contains AST-mutating desugaring and ANF prototypes. `ir/cps.h`
contains a separate CPS teaching IR. None is invoked by the production driver.
`ir/ir_printer.h` is a separate, unused three-address instruction sketch.

Historical `day*` directories are not added to CMake. Build products and IDE
metadata currently present in Git are legacy artifacts and are ignored for new
work; removing them from Git history/index should be a separate reviewable
cleanup commit.

## Audit findings to address before IR work

- The lexer reports token columns at the end of a lexeme, while parser-created
  AST nodes generally never receive those locations.
- Functions are resolved and lowered in source order, so forward calls and
  mutual recursion were not safely supported; this was corrected with explicit
  declaration passes in the frontend and LLVM lowering.
- The type checker accepts operators and types that the LLVM lowering does not
  cover (for example modulo, comparisons, logical operators, unary operators,
  strings, and several declared integer spellings).
- A `void` return currently follows an integer-return fallback in the backend.
- The legacy ANF, CPS, desugaring, and Graphviz headers contain stale field
  references and are not compiled by the active target. They are source
  material, not working pipeline stages.
