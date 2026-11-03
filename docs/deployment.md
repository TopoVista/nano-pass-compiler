# Deployment

The supported container deployment is a local demonstration environment, not a public sandbox.

```sh
docker compose up --build
# Lab: http://127.0.0.1:3000
# Compiler API: http://127.0.0.1:8080/api/v1/readiness
```

`docker/server.Dockerfile` builds against Ubuntu 24.04 and LLVM 18, then copies only the installed server and LLVM runtime into the final image. `docker/web.Dockerfile` uses pinned Node 22.14.0 to build the static Lab and serves it with pinned nginx 1.27.4. The compose configuration runs the compiler as a non-root user, read-only with a temporary `/tmp`, a 768 MiB memory limit, a process limit, and `no-new-privileges`.

The container starts the server with `--execution-policy=interpreter-only`; JIT/native modes are not exposed. It binds inside the container to all interfaces solely so Docker can publish the explicitly loopback-bound host port. For local developer use, the default server is loopback-only and `local-trusted`:

```sh
./build/compiler/nano-server --port=8080 --execution-policy=local-trusted
```

`disabled` rejects all execution, and `interpreter-only` admits only bounded CoreIR/SSA interpreter execution. The readiness endpoint verifies the initialized compiler service and reports LLVM version and policy. See [the threat model](threat-model.md) before changing these defaults.

## Local container evidence

On 2026-11-03, Docker Desktop 28.1.1 built both pinned images from this
repository. The server image passed its `/api/v1/readiness` probe in
`interpreter-only` mode and compiled a schema-v1 learned-policy request with
zero diagnostics. The web image served its root document with HTTP 200. This
is local smoke evidence, not a claim that hosted CI or public deployment has
completed.

The images do not make compilation safe for hostile multi-tenant input. Production hosting needs a request worker boundary, OS-enforced CPU/memory quotas, request cancellation, authentication/rate limiting, and an explicit CORS/origin policy.
