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
