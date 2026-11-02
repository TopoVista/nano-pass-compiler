# NanoPass Compiler Lab

The `web/` client is a Vite + React + TypeScript application. The repository
did not contain a prior web application, so the lab uses a small modern React
client rather than introducing a second server-side application framework.

```sh
cd web
npm ci
npm run dev
```

By default it calls `http://127.0.0.1:8080`. Set
`VITE_NANO_SERVER_URL` when the local server listens elsewhere.

The lab is intentionally an inspector rather than a mock dashboard:

- Monaco edits Nano source with Nano keywords, types, comments, and operators.
- The stage selector shows compiler-generated AST, CoreIR, ANF, CFG, SSA,
  optimized SSA, and LLVM snapshots when requested.
- The timeline uses service pass telemetry; each status is an actual verifier
  gate, duration, and IR-size observation.
- The graph pane renders structured CFG, dominator-tree, loop, and call-graph
  JSON with React Flow. Selecting a basic block highlights matching IR lines.
- SSA `%value` buttons open the compiler-provided definition/type/source
  inspector. Source locations are shown only when retained by the IR.
- The remarks pane reports compiler-recorded applied/missed optimization
  reasons. It does not synthesize an explanation.
- Clicking an optimizer timeline entry displays the compiler-captured SSA
  before/after snapshot. The O0–O3 comparison uses four actual service
  compilations and reports only measured structural/compiler-time fields.
- CoreIR, SSA interpreter, JIT, and adaptive-JIT controls show actual output,
  bounded runtime profile data, and adaptive events where the build supports
  them.
- The correctness pane calls the bounded `/api/v1/correctness` service route;
  it reports CoreIR versus SSA O0–O3 and includes native/adaptive checks only
  in a JIT-capable service build.
- Runtime counts overlay CFG nodes/edges numerically; hot nodes additionally
  carry a text marker, so hotness is not conveyed only by color. The policy
  pane renders compiler-extracted static/dynamic feature vectors and the
  corresponding hotness reason.
- The inline-policy selector offers only Default, Heuristic, and the reviewed
  Learned model. Its decision pane renders compiler-recorded candidate facts
  (model version, confidence, structural features, legality, and post-clone
  verifier gate); it never accepts a browser model upload or synthesizes an
  explanation.

The client debounces live compile requests, caches identical successful
compile inputs, preserves explicit custom-pass order, and embeds source,
optimization level, selected pass sequence, selected IR stage, and graph view
in the share URL fragment.
The fragment remains client-side; source is only sent to the configured local
service for compilation.

The design reference used for this implementation is
`web/design/compiler-lab-concept.png`. The shipped UI intentionally keeps its
graphite, compact developer-tooling hierarchy while using the real local API
instead of placeholder cards.

`npm run test:e2e` is a bounded Playwright journey that starts the real local
`nano-server` plus Vite, loads a source example, switches IR/graphs, opens a
transformation, runs/profile the program, and inspects adaptive events. Install
Chromium once with `npm run test:e2e:install`.
