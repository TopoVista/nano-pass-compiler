# Adaptive JIT

NanoPass supports deterministic single-threaded Tier-0 → Tier-1 execution:

```text
nano jit app.nano --adaptive --hot-threshold=1000
nano jit app.nano --force-tier0
nano jit app.nano --force-tier1
```

Tier-0 functions execute through stable LLVM ORC indirect stubs. A hot,
non-recursive function is rebuilt from immutable SSA, receives profile-guided
inline-cost input plus the bounded Tier-1 pipeline, and is verifier-gated
before ORC updates the stub pointer for future calls. Existing frames always
finish at their original tier. `--profile-output=run.nprof.json` records
complete stable function/block/edge/loop counters; `--jit-events=json`
produces a schema-v1 event record without human-output interleaving.

The detailed ownership, replacement, and correctness model is in
[`adaptive-jit-design.md`](adaptive-jit-design.md).
