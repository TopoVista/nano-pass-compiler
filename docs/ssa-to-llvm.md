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
store. Runtime bounds checking is not yet part of this initial backend; native
array differential equivalence is therefore intentionally not claimed.

The AST LLVM backend remains the default reference backend. The new path is
explicitly selected with:

```text
nano emit example.nano --format=llvm --backend=ssa
```

The SSA backend will become the default only after deterministic and generated
native differential validation covers the supported runtime semantics.
