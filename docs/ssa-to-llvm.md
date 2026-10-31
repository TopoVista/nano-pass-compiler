# SSA to LLVM lowering

The SSA backend lives in `compiler/backend/llvm/` and deliberately depends on
SSA plus LLVM only. The frontend, CoreIR, CFG, SSA, optimizer, and reference
interpreters remain LLVM-independent.

`SSALLVMLowering` first verifies every input `SSAFunction`. It then declares
all LLVM functions before lowering a body, which permits forward calls and
recursion. Each body pre-creates every LLVM basic block, maps parameters by
`ValueId`, creates all phi nodes, lowers ordinary instructions and terminators,
then adds phi incoming pairs after every definition has a mapping. LLVM
function and module verification are mandatory gates.

Nano scalar types map directly from `LangType`: integer width to an LLVM
integer, `bool` to `i1`, `char` to `i8`, float widths to `float`/`double`,
string to `i8*`, and fixed arrays to LLVM fixed arrays. Memory-backed SSA
locals become entry-block allocas. Index operations use typed GEP plus load or
store. A few legacy CFG instructions leave their result type unknown; lowering
derives that type from a mapped SSA operand, never from a pointer address or a
textual printer representation. Runtime bounds checking is not yet part of
this backend, so native array differential equivalence is claimed only for
in-bounds programs.

The AST LLVM backend remains the default reference backend. The new path is
explicitly selected with:

```text
nano emit example.nano --format=llvm --backend=ssa
```

The default AST backend remains the compatibility reference path.  The SSA
path additionally supports host object emission:

```text
nano compile example.nano --backend=ssa --emit-obj -o example.o
```

Object emission sets the LLVM module triple and data layout from a native
`TargetMachine` and uses LLVM's object-file emitter. The optional executable
mode links that object against the build's compact C-ABI print runtime through
the configured C++ driver:

```text
nano compile example.nano --backend=ssa --emit-exe -o example
```

For `compile`, `-o example` selects this executable mode by default; pass
`--emit-llvm` when `-o` should receive textual LLVM IR instead.

The compiler passes a fixed argument vector to the configured host driver; it
does not construct a shell command from Nano source. This is host-only AOT
linking, not cross compilation or a packaged/installable runtime yet.
