# Compiler service API (v1)

`nano-server` exposes a local, versioned JSON boundary around the real
NanoPass frontend, IR pipeline, optimizer, verifier, and execution engines.
It is not a wrapper around terminal output and never accepts a filename or a
shell command from a client.

## Start locally

```sh
cmake -S . -B build -G Ninja \
  -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm \
  -DNANO_BUILD_JIT=ON -DNANO_BUILD_WEB_SERVER=ON
cmake --build build --target nano-server
./build/compiler/nano-server --port=8080
```

The service binds only to `127.0.0.1`. It is intended for the local Compiler
Lab, not as a multi-tenant deployment service.

## Versioning

Every request and response has `schemaVersion`. Version 1 requests use:

```json
{
  "schemaVersion": 1,
  "source": "int main() { print(7); return 7; }",
  "optimizationLevel": 2,
  "passes": ["sccp", "dce"],
  "includeSsa": true,
  "includeOptimizedSsa": true,
  "includeLLVM": true,
  "includeGraphs": true,
  "includeRemarks": true,
  "includeTelemetry": true,
  "includeProvenance": true
}
```

`optimizationLevel` is `0` through `3`. A custom `passes` array is validated
against the compiler's finite SSA-pass allowlist; it is never interpreted as a
command. Requested artifacts are opt-in to keep small live-edit requests
cheap.

## Routes

| Method | Route | Result |
| --- | --- | --- |
| `GET` | `/api/v1/health` | local service and schema status |
| `POST` | `/api/v1/compile` | checked pipeline and requested artifacts |
| `POST` | `/api/v1/analyze` | compile plus CFG, dominator, loop, call-graph, and provenance artifacts |
| `POST` | `/api/v1/optimize` | compile plus optimized SSA, telemetry, and optimization remarks |
| `POST` | `/api/v1/run` | compile and execute with `core`, `ssa`, `jit`, or `adaptive-jit` mode |
| `POST` | `/api/v1/profile` | compile and execute the SSA interpreter with runtime counters |
| `POST` | `/api/v1/correctness` | bounded CoreIR-vs-SSA O0–O3 and, when built, JIT/adaptive equivalence checks |

A successful compile response includes structured diagnostics, named IR
snapshots (including source when requested), transformation before/after
snapshots, pass events, optimization remarks, graph nodes/edges, SSA value
provenance, policy feature vectors, and aggregate telemetry. A run response
additionally reports stdout, stderr, return value, resource/runtime status,
optional runtime profile, structured adaptive-JIT events, and dynamic policy
features.

The pass timeline begins with measured `lex`, `parse`, `resolve`, and
`type-check` frontend events, followed by the measured Core→ANF→CFG→SSA
lowering event and the actual desugaring/optimization events. The service does
not synthesize a per-pass result when the compiler only exposes a pipeline-wide
measurement.

Graph IDs are stable strings such as `main:bb3`; they are not memory
addresses. SSA value records include definition function/block, type, all
uses, source location when retained by the compiler, and are emitted in
deterministic function/value order. Remarks retain their optional value/block
candidate and source coordinates so the UI can distinguish a compiler fact
from a frontend explanation.

## Capability boundary

LLVM textual IR is available whenever `nano-server` is built with a healthy
LLVM package. `jit` and `adaptive-jit` additionally require
`NANO_BUILD_JIT=ON`. If unavailable, execution returns an explicit
`capability_unavailable` result; it does not pretend to have run native code.

## Error model

Transport decoding errors return HTTP 400 with `MALFORMED_REQUEST`.
Compilation responses distinguish user diagnostics, resource-limit failures,
runtime failures, and compiler internal failures. Internal failures are never
relabelled as source-program errors.
