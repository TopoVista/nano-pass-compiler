# Local web execution security

`nano-server` is a local development component. Its current security boundary
is deliberately narrow:

- It listens only on loopback (`127.0.0.1`).
- Requests contain source text and whitelisted compiler options only; no input
  field is a path, executable name, or shell fragment.
- The HTTP decoder rejects duplicate fields, malformed JSON, unsupported JSON
  escapes, non-boolean artifact flags, and non-string custom-pass values.
- The request reader caps HTTP input at 512 KiB, applies a five-second receive
  timeout, and the compiler service centrally caps source (256 KiB), tokens,
  AST depth, functions, CFG blocks, SSA values, and optimization iterations.
- Compiler timeout checks occur at safe phase boundaries. Reference execution
  has explicit wall-clock, step, recursion-depth, output-size, and array-size
  limits. Resource exhaustion is reported as such.
- Custom optimization pipelines go through the C++ pass allowlist.
- API policy selection is a closed `default|heuristic|learned` set. The service
  never accepts a model file/path; learned inference uses a reviewed built-in,
  schema-compatible coefficient table and may only decide an already legal
  inline candidate.
- Native JIT/adaptive execution is controlled by `--execution-policy`:
  `disabled`, `interpreter-only`, or `local-trusted`. The default server binds
  loopback; `--bind=all` is an explicit container/deployment choice.

This is not a hardened remote execution service. Compilation remains in-process
and its timeout is cooperative rather than preemptive; local-trusted JIT has
no process sandbox. The container compose demo defaults to interpreter-only,
non-root, read-only operation, but a deployed multi-user service must still
place compilation in a dedicated worker with OS-level CPU/memory limits,
authentication, origin policy, and rate limiting. CORS is origin-allowlisted:
local development defaults to Vite origins on port 5173, while deployments
must set `NANOPASS_ALLOWED_ORIGINS` to an explicit comma-separated frontend
list. `NANO_ALLOWED_ORIGINS` remains a backwards-compatible development alias.
The server does not emit a wildcard CORS response. See [the threat model](threat-model.md).
