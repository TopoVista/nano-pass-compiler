# Profile-guided optimization

Use a complete SSA runtime profile with the normal verified optimizer:

```text
nano profile app.nano -o app.nprof.json
nano optimize app.nano --profile=app.nprof.json -O3
```

Current PGO changes profitability only. Measured function calls can raise the
bounded direct-callee inline threshold; they cannot override recursive-SCC
rejection, type checking, CFG constraints, growth budgets, inline-depth limits,
or post-pass SSA verification. Profile mismatch and incomplete-profile checks
occur before any optimization decision.

The static/observed feature and hotness infrastructure is available now for
future loop-policy and layout work. It does not claim measured speedups and it
does not make LICM or speculative transformations legal.
