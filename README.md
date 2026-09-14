# NanoPass Compiler Lab

A transparent compiler laboratory that lowers a small C-like language through
explicit typed intermediate representations, verifies compiler invariants,
performs real SSA-based optimizations, and visually explains every
transformation.

> Every pass is small, named, inspectable, measurable, and verifiable.
> A developer should always be able to answer: "What happened to my program?"

```
Source → Lex → Parse → Resolve → TypeCheck
       → DesugarFor → DesugarIncDec → DesugarPlusAssign
       → [ANF] → [CFG] → [SSA] → [Optimizer]
       → LLVM IR → Native / JIT
```

Items in `[brackets]` are planned and under active development.

---

## Quick start

Install a C++20 compiler and a CMake-discoverable LLVM 14+ development package,
then:

```sh
git clone https://github.com/TopoVista/nano-pass-compiler
cd nano-pass-compiler
cmake -S . -B build
cmake --build build -j
```

The produced binary is `build/nano` (`build/nano.exe` on Windows).

---

## 30-second example

```sh
# Tokenize
./build/nano emit examples/factorial.nano --format=tokens

# Print AST
./build/nano emit examples/factorial.nano --format=ast

# Print AST after desugaring (for/++/+= eliminated)
./build/nano emit examples/factorial.nano --format=desugar

# Emit LLVM IR
./build/nano emit examples/factorial.nano --format=llvm

# Verify all available compiler invariants
./build/nano verify examples/factorial.nano

# Show full CLI help
./build/nano --help
```

Run the generated IR with LLVM's interpreter:

```sh
./build/nano emit examples/factorial.nano --format=llvm -o factorial.ll
lli factorial.ll
```

---

## Current pipeline

| Stage | Status | Command |
|---|---|---|
| Lexer | ✅ stable | `--format=tokens` |
| Parser | ✅ stable | `--format=ast` |
| Scope resolution | ✅ stable | |
| Type checking | ✅ stable | |
| DesugarFor | ✅ stable | `--format=desugar` |
| DesugarIncDec | ✅ stable | |
| DesugarPlusAssign | ✅ stable | |
| LLVM IR emission | ✅ stable | `--format=llvm` |
| ANF | 🔬 experimental | |
| CPS | 🔬 experimental | |
| CFG / SSA | 🗓 planned | |
| Optimizer | 🗓 planned | |
| JIT | 🗓 planned | |
| Web lab | 🗓 planned | |

---

## Sample programs

| File | Demonstrates |
|---|---|
| `examples/hello.nano` | Basic function, print, string literal |
| `examples/factorial.nano` | Recursion, conditionals |
| `examples/fibonacci.nano` | Forward calls, while loop |
| `examples/fizzbuzz.nano` | Modulo, nested conditionals |
| `examples/loops.nano` | Nested while loops, accumulator patterns |
| `examples/branching.nano` | Boolean expressions, logical `&&`/`||` |
| `examples/functions.nano` | Multiple functions, parameter passing |
| `examples/desugaring.nano` | For/++/+= desugaring demonstration |

---

## Language reference

The Nano language is a small C-like language. See [docs/language.md](docs/language.md) (planned).

Current supported constructs:

```
// Types
int  float  double  bool  char  void
int[N]  (fixed-size arrays)

// Declarations
int x = 42;
int arr[5];

// Functions
int add(int a, int b) { return a + b; }

// Control flow
if (cond) { ... } else { ... }
while (cond) { ... }

// Print
print expr;

// Operators
+  -  *  /  %
<  <=  >  >=  ==  !=
&&  ||  !
=
```

---

## Build options

| Option | Default | Description |
|---|---|---|
| `NANO_BUILD_TESTS` | `ON` | Build the test suite |
| `NANO_BUILD_JIT` | `OFF` | Build ORC JIT support (planned) |
| `NANO_BUILD_WEB_SERVER` | `OFF` | Build the JSON API server (planned) |
| `NANO_ENABLE_SANITIZERS` | `OFF` | AddressSanitizer + UBSan |
| `NANO_ENABLE_FUZZING` | `OFF` | Build libFuzzer targets (planned) |
| `NANO_ENABLE_ALIVE2` | `OFF` | Optional Alive2 integration (planned) |

Example with sanitizers:

```sh
cmake -S . -B build -DNANO_ENABLE_SANITIZERS=ON
cmake --build build -j
```

---

## Testing

```sh
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Current tests:
- `nano_help` — verifies the CLI exits cleanly with `--help`
- `nano_frontend_smoke` — forward calls, unterminated comments, source-location
  diagnostics for undeclared variables

---

## Debug pass manager

```sh
./build/nano emit examples/loops.nano --format=llvm --debug-pass-manager
```

Output:

```
[pass] Lex
[pass] Parse
[pass] Resolve
[pass] TypeCheck
[pass] DesugarFor
[pass] DesugarIncDec
[pass] DesugarPlusAssign
[pass] LLVMCodegen
```

---

## Architecture

See [docs/architecture.md](docs/architecture.md) for a detailed description of
the compiler's components and design principles.

See [docs/STATUS.md](docs/STATUS.md) for an accurate, up-to-date inventory of
what is stable, experimental, and planned.

See [docs/ROADMAP.md](docs/ROADMAP.md) for the phased development plan.

---

## Contributing

This project is being built incrementally as a research-grade compiler
platform. Each phase leaves the repository compiling, tested, and documented.

- Build is gated on LLVM being discoverable by CMake.
- Do not add features that fake compiler correctness, optimization speedups,
  or benchmark numbers.
- All AI/ML features sit on top of verified deterministic compiler
  infrastructure — the compiler is never dependent on an LLM.
