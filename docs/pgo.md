# Profile-guided optimization

Use a complete SSA runtime profile with the normal verified optimizer:

```text
nano profile app.nano -o app.nprof.json
nano optimize app.nano --profile=app.nprof.json -O3
```

Collect the profile at Tier-0/O0 (the default, or pass `-O0` explicitly).
Higher-level profile runs are labelled observation artifacts and are rejected
by the current PGO consumer because their SSA fingerprint describes a
different input shape.

A successful Tier-0 → Tier-1 native run can produce an equivalent complete
profile for the same input source:

```text
nano jit app.nano --adaptive --profile-output=app.native.nprof.json
nano optimize app.nano --profile=app.native.nprof.json -O3
```

Tier 0 deliberately starts from the original SSA, so the saved fingerprint is
validated against the normal optimizer's input before any transformation runs.

Current PGO changes profitability only. Measured function calls can raise the
bounds for direct-callee inlining (callee cost plus finite caller/program
growth budgets). A complete runtime profile also controls optional loop effort:
with a supplied profile, `-O2`/`-O3` runs loop canonicalization and LICM only
for functions classified `hot`; without a profile, the normal static pipeline
is unchanged. The profile never changes LICM legality. It cannot override recursive-SCC
rejection, type checking, CFG constraints, growth budgets, inline-depth limits,
or post-pass SSA verification. Profile mismatch and incomplete-profile checks
occur before any optimization decision.

Use `--remarks` to inspect a concrete profile decision. When a callee crosses
the hot-call threshold, the inliner records `PROFILE_HOT_CALLEE` with its
observed calls and the actual threshold change. It does not claim that the
inline is profitable or that it improved runtime; it only reports the bounded
cost-model input that was used.

When a profile was supplied but a callee remains below the hot threshold, the
inliner records `PROFILE_COLD_CALLEE` before any ordinary
`INLINE_COST_TOO_HIGH` or budget rejection. This keeps "why not" output
grounded in the actual counter and preserves the static legality/profitability
gate.

The static/observed feature and hotness infrastructure is available now for
future advanced-policy work, including deliberately unimplemented branch layout
and decision replay. `--why-not-optimized` reports
`PROFILE_LOOP_EFFORT_SKIPPED` with a machine-generated hotness reason when a
profile intentionally leaves a function at the smaller loop-effort budget. It
does not claim measured speedups and it does not make LICM or speculative
transformations legal.
