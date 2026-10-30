# Differential testing

`DifferentialRunner` compiles source through the real frontend/desugaring/IR
pipeline, executes CoreIR as the reference, then rebuilds an independent SSA
program for each requested O0/O1/O2/O3 level. Comparison includes execution
status, return value, stdout, and defined runtime-error category; matching
timeouts are not equivalence proof.

For example, a scalar loop program can be checked as CoreIR versus each O level
without requiring LLVM. Verifier success is a prerequisite, not the semantic
oracle. LLVM execution will become an optional additional oracle only after the
local LLVM installation is usable.

`DifferentialReducer` preserves a mismatch only after every candidate has been
reparsed, type-checked, rebuilt through the IR pipeline, and executed through
the supplied differential adapter. The current bounded reducer removes
complete source lines/statements; its syntax/semantic validity is enforced by
that real compiler pipeline, and test-only faulty adapters never alter a
production optimization pass.
