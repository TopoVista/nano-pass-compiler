# NanoPass Compiler Lab

A transparent compiler laboratory that lowers a small C-like language through
explicit typed intermediate representations, verifies compiler invariants, and
visually explains every transformation.

> Every pass is small, named, inspectable, measurable, verifiable, and
> observable.  A developer should always be able to answer:
> "What happened to my program?"

```
Source → Lex → Parse → Resolve → TypeCheck
       → PassManager[ DesugarFor | DesugarIncDec | DesugarPlusAssign ]
       → [CoreIR] → [ANF] → [CFG] → [SSA] → [Optimizer]
       → LLVM IR → Native / JIT                     (native/JIT planned)
```

Items in `[brackets]` are planned and under active development.

The CFG/SSA pipeline powers verified middle-end analysis and inspection.
Native LLVM emission still uses the established desugared-AST backend until
SSA-to-LLVM lowering is introduced in a later phase.

---

## Quick start

Install a C++20 compiler and a CMake-discoverable LLVM 14+ development package,
then:

```sh
git clone https://github.com/TopoVista/nano-pass-compiler
cd nano-pass-compiler
cmake -S . -B build -G Ninja -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm
cmake --build build -j
ctest --test-dir build
```

On WSL with the system LLVM:
```sh
cmake -S . -B build -G Ninja -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm
```

The produced binary is `build/compiler/nano`.

---

## 30-second example

```sh
# Tokenize
./build/compiler/nano emit examples/factorial.nano --format=tokens

# Print AST
./build/compiler/nano emit examples/factorial.nano --format=ast

# Desugar and print AST (for / ++ / += eliminated)
./build/compiler/nano emit examples/factorial.nano --format=desugar

# Emit CoreIR — typed tree IR with LocalId disambiguation
./build/compiler/nano emit examples/factorial.nano --format=core

# Emit ANF — evaluation order explicit, temporaries named
./build/compiler/nano emit examples/factorial.nano --format=anf

# Emit CFG — basic blocks, explicit jumps, no structured control flow
./build/compiler/nano emit examples/loops.nano --format=cfg

# Emit CFG as Graphviz DOT
./build/compiler/nano graph examples/loops.nano --cfg

# Emit SSA and the dominator tree
./build/compiler/nano emit examples/control_flow.nano --format=ssa
./build/compiler/nano graph examples/control_flow.nano --dominator-tree

# Explain the full compilation journey
./build/compiler/nano explain examples/factorial.nano

# Emit LLVM IR
./build/compiler/nano emit examples/factorial.nano --format=llvm

# Verify all available compiler invariants, including SSA
./build/compiler/nano verify examples/factorial.nano
```

Run the generated IR with LLVM's interpreter:
```sh
./build/compiler/nano emit examples/factorial.nano --format=llvm -o factorial.ll
lli factorial.ll
```

---

## Compiler passes

NanoPass uses a real `PassManager` that runs each pass **once over the full
program** and tracks analysis caching and invalidation.

```
$ nano passes

Available compiler passes

Desugaring
  desugar-for
    Unit:  program
    Lower for-loops to equivalent while-loops.

  desugar-inc-dec
    Unit:  program
    Lower ++ and -- operators to explicit additions/subtractions.

  desugar-plus-assign
    Unit:  program
    Lower += compound assignment to simple assignment.
```

```
$ nano analyses

Available compiler analyses

Ast
  ast-node-count
    Unit:  program
    Count total AST nodes (statements, expressions, functions).

  ast-depth
    Unit:  program
    Compute maximum nesting depth of the AST.
```

### Pass pipeline debugging

```sh
./build/compiler/nano emit examples/loops.nano --format=desugar \
    --debug-pass-manager
```

Output:

```
[pass-manager] starting pipeline "desugar"

[pass] DesugarFor  (desugar-for)
  changed:  no
  duration: 5.0 us

[pass] DesugarIncDec  (desugar-inc-dec)
  changed:  no
  duration: 8.6 us

[pass] DesugarPlusAssign  (desugar-plus-assign)
  changed:  no
  duration: 7.9 us

[pass-manager] completed "desugar"
  total duration: 21.5 us
  any changed:    no
```

Each pass runs **once** — not once per top-level statement.

### Other pass options

```sh
# Print the active pass pipeline
--print-pipeline

# Stop after a named pass (by stable ID)
--stop-after=desugar-for

# Skip a named pass
--disable-pass=desugar-inc-dec

# Show analysis cache events
--debug-analyses
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
| PassManager + AnalysisManager | ✅ stable | |
| PassRegistry + PipelineBuilder | ✅ stable | `nano passes` |
| **CoreIR** | ✅ **stable** | `--format=core` |
| **ANFIR** | ✅ **stable** | `--format=anf` |
| **CFG IR + Graphviz** | ✅ **stable** | `--format=cfg`, `nano graph --cfg` |
| **`nano explain`** | ✅ **stable** | `nano explain` |
| **`nano verify` (10 stages)** | ✅ **stable** | `nano verify` |
| LLVM IR emission | ✅ stable | `--format=llvm` |
| **SSA IR + verifier** | ✅ **stable** | `--format=ssa`, `nano verify` |
| Optimizer | 🗓 planned | Phase 5 |
| JIT | 🗓 planned | Phase 7 |
| Web lab | 🗓 planned | Phase 9 |

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

## Pass infrastructure architecture

The pass infrastructure lives in `compiler/passes/` and uses the `nanopass`
namespace (not `nano`, which collides with `std::nano` from `<ratio>`).

Key types:

| Type | Purpose |
|---|---|
| `nanopass::Pass` | Abstract base; `PassInfo info()` + `run(Program&, PassContext&)` |
| `nanopass::PassManager` | Sequential pipeline; once-per-program execution |
| `nanopass::AnalysisManager` | Type-safe analysis cache with `get<T>()` |
| `nanopass::PreservedAnalyses` | Declares which analyses a pass leaves valid |
| `nanopass::PassResult` | `changed` + `preserved` + `PassStatistics` |
| `nanopass::PassInstrumentation` | Observer hooks around pass/analysis lifecycle |
| `nanopass::PassRegistry` | Static registry of known passes and analyses |
| `nanopass::PipelineBuilder` | Pipeline factory; registry initialisation |

See [docs/pass-manager-design.md](docs/pass-manager-design.md) for the full
architecture description including the pass lifecycle diagram.

---

## Build options

| Option | Default | Description |
|---|---|---|
| `NANO_BUILD_TESTS` | `ON` | Build the test suite |
| `NANO_BUILD_JIT` | `OFF` | Build ORC JIT support (planned) |
| `NANO_BUILD_WEB_SERVER` | `OFF` | Build JSON API server (planned) |
| `NANO_ENABLE_SANITIZERS` | `OFF` | AddressSanitizer + UBSan |
| `NANO_ENABLE_FUZZING` | `OFF` | libFuzzer targets (planned) |
| `NANO_ENABLE_ALIVE2` | `OFF` | Alive2 integration (planned) |

---

## Testing

```sh
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Current: **35 tests, 100% passing.**

Test categories:
- CLI smoke (`nano_help`, `nano_version`, `nano_passes_command`, `nano_analyses_command`)
- Frontend unit (`nano_frontend_smoke`: forward calls, errors, source locations)
- Pass manager unit (`nano_pass_manager_unit`: 22 tests covering PassManager,
  AnalysisManager, analysis cache/invalidation, rewrite counts, stop-after,
  disable-pass, registry lookups, PreservedAnalyses semantics)
- Per-example integration (`emit_*`, `desugar_*`, `verify_*` for 8 examples)
- CLI pass-manager smoke (`debug_pass_manager_smoke`, `print_pipeline_smoke`,
  `stop_after_smoke`, `disable_pass_smoke`, `stop_after_invalid_pass`)

---

## Architecture

See [docs/architecture.md](docs/architecture.md) for component descriptions.  
See [docs/pass-manager-design.md](docs/pass-manager-design.md) for the pass
infrastructure design and lifecycle.  
See [docs/STATUS.md](docs/STATUS.md) for an accurate feature inventory.  
See [docs/ROADMAP.md](docs/ROADMAP.md) for the phased development plan.
