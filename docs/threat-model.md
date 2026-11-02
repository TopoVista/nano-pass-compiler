# Threat model

NanoPass is a local compiler laboratory, not a public multi-tenant code-execution platform. The browser is untrusted; the HTTP service accepts untrusted source text and allowlisted compiler options; compiler, interpreters, and optional JIT execute in the service process. Profiles and shared-session fragments are data, not authority.

## Boundaries and controls

| Boundary | Relevant threats | Current control |
| --- | --- | --- |
| Browser → HTTP service | malformed JSON, unknown options, source bombs | schema v1, duplicate/unknown-field rejection, 512 KiB HTTP cap, 256 KiB source cap, 50,000-token cap |
| Frontend → IR pipeline | deeply nested AST, many functions, pathological CFG/SSA | AST-depth, function, CFG-block, SSA-value, and optimization-iteration budgets |
| Interpreter | infinite loops, recursion, allocation and output exhaustion | wall-clock, step, recursion, array-size, and output budgets |
| JIT/native execution | arbitrary native code / process compromise | disabled outside `local-trusted`; no public JIT claim |
| Service process | slow client / request abuse | loopback default, 5-second receive timeout, fixed request cap, no request paths or shell fragments |
| Profiles / policy data | corrupt or incompatible data | versioned schemas, fingerprints where applicable, model feature-schema validation, no HTTP model upload |

The compiler never forms a shell command from source, API JSON, profile content, or model output. The learned policy only chooses `inline` or `do-not-inline` for an already legal direct-call candidate. Deterministic compiler code performs the clone and `verifySSA` gates the result.

## Deliberate limitations

Compile timeout checks are cooperative phase-boundary checks, not preemptive cancellation of a C++ compilation. Interpreter timeout checks are cooperative per execution step. The local-trusted ORC JIT has no process sandbox, OS memory cgroup, authentication, rate limiting, or tenant isolation. Do not expose it to hostile users. A public deployment must use an isolated worker process/container per request with operating-system CPU, memory, network, filesystem, and rate limits.
