# Adaptive ORC JIT design

## Baseline ORC path

`executeWithLLJIT` remains the simple, one-shot path: it creates one `LLJIT`,
lowers the whole verified SSA program into one `ThreadSafeModule`, resolves the
Nano print runtime from the current process, looks up `main`, and invokes it.
It has no persistent function identity and cannot safely replace a function.

## Adaptive path

`executeAdaptivelyWithLLJIT` owns one `LLJIT` for the run and creates an LLVM
18 `IndirectStubsManager`. Every Nano function gets a stable public stub named
after the source function. Tier implementations have distinct private symbols:

```text
caller → @helper stub → __nano_adaptive_helper_tier0_0
                         ↓ promotion
caller → @helper stub → __nano_adaptive_helper_tier1_1
```

Each function body is lowered in its own module. Other Nano functions are
declarations and resolve to the already-defined public stubs. This is the
smallest extension of the existing whole-program lowering path: the selected
body is retained, other lowered bodies are converted to declarations, and the
module is verified again before ORC accepts it.

Tier 0 installs verified SSA with no expensive middle-end work. Tier 1 starts
from a fresh SSA copy, supplies observed call counters to the ordinary inliner
only as its documented hot-callee cost input, then runs the bounded O3
pipeline plus loop canonicalization, LICM, and strength reduction. Recursion,
growth budgets, CFG legality, and SSA verification remain mandatory. The
implementation verifies SSA before lowering; `SSALLVMLowering` verifies the
function/module before the new address is installed.

## Promotion and safety

Generated Tier-0/Tier-1 entry blocks call the C ABI
`nano_adaptive_function_entry`. Its thread-local session records the function
call and promotes a non-recursive Tier-0 function once its deterministic call
counter reaches `--hot-threshold`. Promotion compiles Tier 1, materializes its
private symbol, then calls `IndirectStubsManager::updatePointer`.

An already-active Tier-0 frame is never migrated or patched. It returns using
Tier 0. Only later calls through the stable stub reach Tier 1. Old modules stay
owned by LLJIT for the run so no active code can be reclaimed prematurely.

Recursive SCCs are profiled but deliberately remain Tier 0 in this initial
policy. Force modes exist for deterministic tests:

```text
nano jit file.nano --force-tier0
nano jit file.nano --force-tier1
nano jit file.nano --adaptive --hot-threshold=100
```

Tier modules instrument stable function names and SSA `BlockId`s at function
entry, basic-block entry, and every emitted CFG edge. The resulting
`--profile-output` profile is complete when execution completes, can be shown
with `nano profile show`, and has the same schema/fingerprint contract as
interpreter-collected profiles. `--jit-events=json` emits a separate
schema-v1 machine-readable event record without mixing terminal text.
