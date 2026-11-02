# 7-minute demo script

1. Open the Lab with **SCCP branch**. Show Source, AST/Core/ANF/CFG/SSA and the pass timeline.
2. Inspect the CFG and SSA provenance for a selected value; show dominance-aware uses.
3. Use **LICM** and **Why not LICM** to contrast an applied hoist with the compiler-recorded speculative-safety rejection.
4. Choose **Inlining**, select Heuristic then Learned policy, and show the structured decision rather than AI prose.
5. Run SSA, inspect the bounded runtime profile, then run correctness to compare CoreIR and SSA modes.
6. Use the adaptive-JIT example only in a local-trusted build; show Tier 0/Tier 1 events and profile facts.
7. Close with `tools/run_benchmarks.py`, [the threat model](threat-model.md), and the statement that policy choices never bypass legality or verification.
