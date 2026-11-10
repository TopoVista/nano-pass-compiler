# Deploy NanoPass on Render and Vercel

This repository is configured for a two-service deployment:

```text
Browser ──HTTPS──> Vercel static React app ──HTTPS + JSON──> Render nano-server
                              │                                  │
                              └──── VITE_NANO_SERVER_URL ─────────┘
```

The frontend is static Vite output. The backend is a Dockerized C++20/LLVM 18
compiler service. They are deliberately separate: the browser never receives
compiler binaries, and the API never accepts a file path, shell command, or
arbitrary optimization implementation.

## Before you start

- Push the desired `main` commit to GitHub.
- Reserve the production frontend hostname before the API’s first boot. Use a
  custom hostname such as `https://nanopass.example.com`, or decide the Vercel
  project slug so its stable origin is known as
  `https://YOUR_PROJECT.vercel.app`.
- Deploy the Render API **before** the Vercel frontend. Its HTTPS URL is a
  build-time input to Vite.
- Treat the hosted API as a bounded demonstration service, not a multi-tenant
  sandbox. It runs with `interpreter-only` execution; native JIT is not
  exposed.

## 1. Create the Render compiler API

1. In Render, choose **New → Blueprint** and connect this GitHub repository.
2. Let Render discover the root [`render.yaml`](../render.yaml). It builds
   [`docker/server.Dockerfile`](../docker/server.Dockerfile) with the repository
   root as the Docker context.
3. Keep the generated service name or rename it in Render. The blueprint pins
   these production-safe defaults:

   | Setting | Value |
   | --- | --- |
   | Runtime | Docker / Ubuntu 24.04 / LLVM 18 |
   | Bound interface | `0.0.0.0` inside the Render container |
   | Port | `PORT=10000` |
   | Health check | `/api/v1/readiness` |
   | Execution | `interpreter-only` |
   | Source limit | 262,144 bytes |
   | Deploys | commits to `main` |

4. When Render prompts for `NANOPASS_ALLOWED_ORIGINS`, enter the exact Vercel
   production origin reserved above—for example `https://nanopass.vercel.app`.
   Do not add a trailing slash, do not use `*`, and do not paste a preview URL
   unless that preview is intended to access the API.
5. Deploy. Copy the public URL Render assigns, such as
   `https://nanopass-compiler-api.onrender.com`.

Render starts the server with `--bind=all`; the server reads `PORT` and the
container health check reads that same value. `NANOPASS_ENV=production` means
startup fails closed if the CORS allowlist is missing.

Verify the API before creating the frontend:

```sh
curl -fsS https://YOUR_RENDER_HOST/api/v1/readiness
# Expected JSON includes: "ready":true and "executionPolicy":"interpreter-only"

curl -i -X OPTIONS https://YOUR_RENDER_HOST/api/v1/compile \
  -H 'Origin: https://YOUR_VERCEL_HOST' \
  -H 'Access-Control-Request-Method: POST' \
  -H 'Access-Control-Request-Headers: Content-Type'
# Expected: HTTP 200 and Access-Control-Allow-Origin with the exact Vercel origin
```

## 2. Create the Vercel frontend

1. In Vercel, import this same GitHub repository.
2. Leave **Root Directory** at the repository root. The root
   [`vercel.json`](../vercel.json) runs `npm ci --prefix web`, builds the Vite
   app, and serves `web/dist`. If you intentionally set Root Directory to
   `web`, the matching [`web/vercel.json`](../web/vercel.json) supplies the
   equivalent configuration.
3. Set these **Production** environment variables before the first deployment:

   | Variable | Example | Secret? |
   | --- | --- | --- |
   | `VITE_SITE_URL` | `https://nanopass.vercel.app` | No — public canonical URL |
   | `VITE_NANO_SERVER_URL` | `https://nanopass-compiler-api.onrender.com` | No — browser API origin |

   The values are embedded at build time because they are consumed by browser
   code. Never store credentials, API keys, or private values in `VITE_*`.
   The client accepts either form of the API URL, but normalizes a trailing
   slash before it constructs API paths.

4. Deploy. The Vercel configuration includes the SPA fallback for `/lab`,
   `/guide`, `/graphs`, `/optimizations`, and `/verification`, long-lived cache
   headers for hashed Vite assets, and baseline browser hardening headers.

## 3. Connect the two deployed services

Open the Vercel URL and load `/lab`. Start with a small program, then check
that the compiler produces representations rather than the “service could not
be reached” message.

```sh
curl -fsS -X POST https://YOUR_RENDER_HOST/api/v1/compile \
  -H 'Content-Type: application/json' \
  -H 'Origin: https://YOUR_VERCEL_HOST' \
  --data '{"schemaVersion":1,"source":"int main(){ return 0; }","optimizationLevel":0}'
```

The response should be HTTP 200 with `"success":true`. A CORS error means the
value of `NANOPASS_ALLOWED_ORIGINS` does not exactly match the Vercel origin.
Update that Render environment variable and redeploy the API. If a custom
domain replaces the initial Vercel URL, update **both** `VITE_SITE_URL` in
Vercel and `NANOPASS_ALLOWED_ORIGINS` in Render, then redeploy both services.

## Preview deployments

Vercel preview domains are intentionally not implicitly trusted by the API.
The service enforces an exact origin allowlist, so preview builds cannot access
the production compiler API unless their stable origin is deliberately added
to `NANOPASS_ALLOWED_ORIGINS`. This prevents arbitrary pull-request previews
from becoming a public compiler client by accident.

For a separate preview environment, deploy a separate Render API and give it a
separate, explicit preview frontend origin. Do not weaken the production API to
`*` or a broad wildcard merely to make previews work.

## Operational checks and rollback

- Render logs include a request ID, endpoint, status, and duration but never
  user source. Monitor `/api/v1/readiness` and 4xx/5xx rates.
- Keep Render’s execution policy at `interpreter-only` for public deployment.
- Deploy rollback is provider-native: promote a prior Vercel deployment, then
  roll Render back to the matching prior image/commit if needed.
- The API is intentionally not an authenticated or per-request-isolated
  multi-tenant execution system. Add authentication, rate limits, and an
  OS-enforced worker boundary before expanding beyond a bounded public demo.

## Local parity check

The checked-in container configuration remains useful before pushing:

```sh
docker compose config
docker compose up --build
curl -fsS http://127.0.0.1:8080/api/v1/readiness
```

For Vercel-style static output, copy [`web/.env.example`](../web/.env.example)
to `web/.env.local`, set local origins, and run `npm run build` from `web/`.
`.env.local` is ignored by Git.
