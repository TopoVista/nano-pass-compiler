# Learned inline policy

The first learned decision is direct-call inlining profitability. It is an interpretable deterministic logistic-regression inference implementation in C++; no Python runtime, GPU library, LLM, or arbitrary pass selection is involved during compilation.

`models/inline-imitation-v1.json` records model kind/version, feature-schema version, ordered feature names, `log1p` transform, coefficients, threshold, and training metadata. The loader rejects missing, reordered, non-finite, or schema-incompatible fields. `tools/build_policy_dataset.py` exports actual candidate/heuristic records together with the caller's measured before/after SSA instruction and block counts; each outer dataset row also carries the source-controlled compiler revision that emitted it. `tools/train_policy.py` uses reproducible program-level splits and writes the portable model JSON. Structural outcomes are retained for analysis, not misrepresented as performance labels.

The bootstrap dataset has 41 actual heuristic-imitation records. It is intentionally small and its reported evaluation is **heuristic-imitation accuracy**, not a claim of runtime optimization quality. Its recorded split is 28 train / 4 validation / 9 test rows with seed `20261103`.

```sh
nano optimize examples/functions.nano -O2 \
  --policy=learned --policy-model=models/inline-imitation-v1.json --remarks
```

At service level the public API exposes only `policy: default|heuristic|learned`; the reviewed built-in coefficient table is used for `learned`, and no client path/model upload is accepted. The order is fixed:

```text
direct call candidate → recursion/cost/budget/CFG legality → features
→ inline | do-not-inline policy → deterministic clone → verifySSA
```

Model confidence estimates profitability only. It is not a correctness confidence. The service and web Lab retain factual structured records with model version, feature schema, decision, confidence, heuristic action, legality, and post-clone verification state.
