# Differential testing

`DifferentialRunner` compiles source through the real frontend/desugaring/IR
pipeline, executes CoreIR as the reference, then starts every requested O0,
O1, O2, and O3 evaluation from an independent copy of unoptimized SSA.
Comparison includes execution status, return value, stdout, and defined
runtime-error category; matching step/output limits are not equivalence proof.

For example, a scalar loop program can be checked as CoreIR versus each O level
without requiring LLVM. Verifier success is a prerequisite, not the semantic
oracle. LLVM execution will become an optional additional oracle only after the
local LLVM installation is usable.

Custom legal pipelines are also available through `runCustom`, for example
`sccp,dce`. Every custom result is verified by `verifySSA` before execution.

Example source:

```nano
int main() { int x = 2 + 3; print x; return x; }
```

The runner checks CoreIR against O0, O2, and O3 by actual execution. This is
differential testing on supported programs, not formal proof.

`DifferentialReducer` preserves a mismatch only after every candidate has been
reparsed, type-checked, rebuilt through the IR pipeline, and executed through
the supplied differential adapter. Its AST-guided reduction uses parsed
statement locations (the legacy AST lacks byte spans) to remove statements,
then deterministically shrinks integer literals. Syntax/semantic validity is
enforced by the real compiler pipeline. Test-only faulty adapters never alter
production optimization passes.
