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
LLVM `Error` values, and captures the C-ABI print runtime's output in the
returned `JITExecutionResult`. `nano run` is an alias for `nano jit` in a
JIT-enabled build.

`nano_jit_correctness` is the reproducible native differential runner. It
builds a CoreIR reference execution and an independently optimized O3 SSA JIT
execution for each grammar-generated program:

```text
nano_jit_correctness fuzz --seed=12648430 --cases=1000
```

`--start-case` splits a campaign into replayable intervals, and
`--save-failures=DIR` persists the exact source for a mismatch. This is
measured differential testing, not formal translation validation. There are
no JIT timeouts, memory limits, or native runtime bounds traps yet; those are
known limitations rather than silent guarantees.
