# Local web execution security

`nano-server` is a local development component. Its current security boundary
is deliberately narrow:

- It listens only on loopback (`127.0.0.1`).
- Requests contain source text and whitelisted compiler options only; no input
  field is a path, executable name, or shell fragment.
- The HTTP decoder rejects duplicate fields, malformed JSON, unsupported JSON
  escapes, non-boolean artifact flags, and non-string custom-pass values.
- The request reader caps HTTP input at 512 KiB and the compiler service caps
  source at 256 KiB.
- Reference execution has explicit step, recursion-depth, output-size, and
  array-size limits. Resource exhaustion is reported as such.
- Custom optimization pipelines go through the C++ pass allowlist.

This is not yet a hardened remote execution service. Compilation is in-process
and therefore has no independent wall-clock timeout or OS memory cgroup. A
deployed multi-user service must place the compiler in a dedicated worker with
process isolation, OS-level time/memory limits, authentication, origin policy,
and rate limiting. The local browser's permissive CORS response is intentional
for development and must be tightened before any non-loopback deployment.
