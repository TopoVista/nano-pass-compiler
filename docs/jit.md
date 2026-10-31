# ORC JIT

`nano jit file.nano` lowers optimized SSA through the LLVM backend and executes
`main` using ORCv2 `LLJIT`. The JIT owns a fresh LLVM context through a
`ThreadSafeModule`; it never borrows the CLI's temporary context.

Before creating LLJIT, Nano initializes the native target, asm printer, and
asm parser. Runtime symbols are explicit C ABI functions such as
`nano_print_i32` and `nano_print_f64`. The current ORC implementation uses a
current-process symbol generator and Unix builds export the executable's
symbols with `-rdynamic`.

Initial JIT execution supports a parameterless `main` returning `void` or a
Nano integer width. It reports ORC and lowering errors rather than discarding
LLVM `Error` values. Captured output, bounded execution, native differential
testing, and runtime bounds traps remain Phase 8 work before this command is
promoted from experimental status.
