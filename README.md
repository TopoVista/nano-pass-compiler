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
       → LLVM IR → Host object / ORC JIT            (SSA backend optional)
```

Items in `[brackets]` are planned and under active development.

The CFG/SSA pipeline powers verified middle-end analysis and inspection. The
AST LLVM backend remains the compatibility default, while the explicit
`--backend=ssa` path lowers verified optimized SSA through LLVM, can emit a
host object file, and powers the optional ORC JIT.

The inspected SSA optimizer includes deterministic CFG-cloning direct-call
inlining: multi-block, loop, phi, multi-return, and void callees are expanded
only after cost, growth, recursion, and verifier checks succeed.

NanoPass also differentially tests supported programs: CoreIR execution is
compared with independently executed SSA O0–O3 programs. This is empirical
semantic testing, not formal proof. LLVM execution is optional and requires a
healthy CMake-discoverable LLVM development package.

## Differential correctness testing

The LLVM-independent correctness tool executes CoreIR against fresh SSA O0–O3
copies, rather than comparing textual IR. It is useful even when the LLVM CMake
package is unavailable:

```sh
g++ -std=c++20 -Icompiler tools/nano-correctness.cpp -o nano-correctness
./nano-correctness differential examples/factorial.nano
./nano-correctness differential-custom examples/optimizer.nano sccp,dce
./nano-correctness fuzz --seed=42 --cases=1000 --save-failures=tests/regressions/generated
./nano-correctness malformed-fuzz --seed=8128 --cases=256
```

The generator is seed- and case-index-reproducible, uses bounded typed source
generation, and reports measured JSON counts. These commands provide
differential testing, not formal verification.

When built with `-DNANO_BUILD_JIT=ON`, the native differential runner checks
the same generated language subset through CoreIR and optimized SSA/LLJIT:

```sh
./build/compiler/nano_jit_correctness fuzz --seed=12648430 --cases=1000
```

This is a measured comparison, not a formal proof. It currently covers the
in-bounds scalar/fixed-array subset; native bounds traps and sandboxing are
intentionally not claimed.

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

# Inspect Nano's verified SSA optimizer
./build/compiler/nano optimize examples/optimizer.nano -O2 --show-before-after
./build/compiler/nano optimize examples/optimizer.nano --passes=sccp,dce
./build/compiler/nano emit examples/optimizer.nano --format=optimized
./build/compiler/nano optimize examples/licm.nano -O3 --why-not-optimized
./build/compiler/nano graph examples/nested_loops.nano --loops

# Explain the full compilation journey
./build/compiler/nano explain examples/factorial.nano

# Emit LLVM IR
./build/compiler/nano emit examples/factorial.nano --format=llvm

# Lower verified optimized SSA through LLVM, then execute it in ORC
./build/compiler/nano emit examples/factorial.nano --format=llvm --backend=ssa -O3
./build/compiler/nano jit examples/factorial.nano -O3
./build/compiler/nano run examples/factorial.nano -O3

# `compile -o` links a host executable through the configured C++ driver.
# Use --emit-llvm to retain textual LLVM output with -o.
./build/compiler/nano compile examples/factorial.nano --backend=ssa -O2 -o factorial

# Emit a host object, or make the executable choice explicit.
./build/compiler/nano compile examples/factorial.nano --backend=ssa --emit-obj -o factorial.o
./build/compiler/nano compile examples/factorial.nano --backend=ssa --emit-exe -o factorial

# Package the driver, its host-AOT runtime archive, and the runtime ABI header.
cmake --install build --prefix "$HOME/.local"
$HOME/.local/bin/nano compile examples/factorial.nano --backend=ssa -O2 -o factorial

# Collect deterministic interpreter execution facts in versioned JSON.
./build/compiler/nano profile examples/loops.nano -o loops.nprof

# Feed measured function-call counts into the bounded inliner. This only
# relaxes a cost choice for hot direct callees; correctness gates remain on.
./build/compiler/nano emit examples/loops.nano --format=llvm --backend=ssa \
  --profile=loops.nprof -O2

# Inspect complete SSA block/edge/loop counters and export policy features.
./build/compiler/nano profile show loops.nprof
./build/compiler/nano features examples/loops.nano --profile=loops.nprof

# Promote deterministic hot calls through LLVM ORC dispatch stubs.
./build/compiler/nano profile examples/hot_loop.nano -o hot.nprof.json
./build/compiler/nano optimize examples/hot_loop.nano --profile=hot.nprof.json -O3 --why-not-optimized
./build/compiler/nano jit examples/hot_loop.nano --adaptive --hot-threshold=2

# Persist measured desugaring-pass events in a versioned JSON schema.
./build/compiler/nano emit examples/desugaring.nano --format=core \
  --telemetry=desugar-telemetry.json

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
| LLVM IR emission | ✅ stable | `--format=llvm`, `--backend=ssa` |
| **SSA IR + verifier** | ✅ **stable** | `--format=ssa`, `nano verify` |
| Optimizer | ✅ stable | `nano optimize`, `-O0`–`-O3` |
| ORC JIT | 🧪 optional | `nano jit`, `nano run` with `NANO_BUILD_JIT=ON` |
| Host AOT emission | 🧪 optional | `nano compile --backend=ssa -o file` |
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
| `NANO_BUILD_JIT` | `OFF` | Build ORCv2 JIT, native backend tests, and `nano_jit_correctness` |
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

The LLVM-independent suites always build; JIT/backend tests are added only
when `NANO_BUILD_JIT=ON` and a healthy LLVM SDK is found.

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
See [docs/analysis-manager.md](docs/analysis-manager.md) for AST/SSA analysis
cache scopes and invalidation.
See [docs/interpreter.md](docs/interpreter.md),
[docs/differential-testing.md](docs/differential-testing.md), and
[docs/fuzzing.md](docs/fuzzing.md) for execution and reliability testing.
See [docs/STATUS.md](docs/STATUS.md) for an accurate feature inventory.
See [docs/ROADMAP.md](docs/ROADMAP.md) for the phased development plan.
