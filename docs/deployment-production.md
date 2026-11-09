# Production deployment

NanoPass has two deployable concerns: a static React client and a bounded C++ compiler API. They may be deployed separately. A deployment configuration in this repository is not evidence of a live hosted service.

## Frontend: Vercel

`web/vercel.json` provides an SPA fallback for `/lab`, `/guide`, `/graphs`, and `/optimizations` and caches fingerprinted Vite assets. Set the frontend environment variable before building:

```sh
VITE_SITE_URL=https://your-frontend.example.com
VITE_NANO_SERVER_URL=https://your-api.example.com
```

`VITE_SITE_URL` sets the built canonical, OpenGraph, `robots.txt`, and sitemap URLs. It must be the actual HTTPS frontend origin; it is intentionally not hard-coded in the repository. The client has no secret environment variables. It sends source text only to that configured API URL.

The public landing route is code-split from the compiler cockpit. Monaco and its
editor worker are emitted as local, lazy-loaded assets, so the lab does not
depend on a third-party CDN at runtime.

## API: Render or a compatible Docker host

`render.yaml` and `docker/server.Dockerfile` build `nano-server` with LLVM 18, then run the non-root runtime image. The service binds `0.0.0.0` only because the platform routes requests to it; set:

```sh
NANOPASS_ENV=production
PORT=8080
NANOPASS_ALLOWED_ORIGINS=https://your-frontend.example.com
NANOPASS_EXECUTION_POLICY=interpreter-only
NANOPASS_MAX_SOURCE_BYTES=262144
```

Health checks use `/api/v1/readiness`. The server logs a request ID, endpoint, status, duration, bind address, execution policy, and allowlist count—but never user source. Error envelopes carry the correlation ID. It accepts a 512 KiB HTTP request maximum and delegates source, token, AST, CFG, SSA, runtime-step, output, and optimizer limits to the compiler service. `NANO_ALLOWED_ORIGINS` remains accepted as a backwards-compatible alias. In `NANOPASS_ENV=production`, startup requires `NANOPASS_ALLOWED_ORIGINS`; it will not fall back to local Vite origins.

`PORT`, `NANOPASS_EXECUTION_POLICY`, `NANOPASS_BIND`, and
`NANOPASS_MAX_SOURCE_BYTES` are validated at startup. Explicit server CLI
flags take precedence over their environment equivalents. `NANOPASS_MAX_SOURCE_BYTES`
must be between 1 and 524288 and is additionally bounded by the HTTP request cap.

## Public execution boundary

Use `--execution-policy=interpreter-only` for an internet-facing demonstration. It disables native JIT execution and keeps the API's supported execution modes bounded to the interpreters. This is still not sufficient isolation for arbitrary hostile multi-tenant workloads: production should add authentication, rate limits, a dedicated worker boundary, CPU and memory cgroups, and audited observability before exposing compilation broadly.

## Local container smoke test

```sh
docker compose up --build
curl http://127.0.0.1:8080/api/v1/readiness
```

The client is then available at `http://127.0.0.1:3000`. Compose is local-only: its published ports bind loopback and its service uses a read-only filesystem, tmpfs scratch space, PID limit, memory limit, non-root process, and `no-new-privileges`.

## Release checklist

- Run the C++ unit, integration, and correctness targets available in the target environment.
- Run `npm run typecheck`, `npm run test`, `npm run build`, and `npm run test:e2e` in `web/`.
- Set an exact CORS allowlist; do not use `*`.
- Confirm `/api/v1/readiness` reports the expected compiler revision and interpreter-only execution policy.
- Run the container health check and a preflight request from the actual frontend origin.
- Record benchmark and fuzz results separately with the machine/environment metadata; do not add marketing claims from a one-off run.
