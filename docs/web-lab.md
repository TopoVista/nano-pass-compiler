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
- CoreIR, SSA interpreter, JIT, and adaptive-JIT controls show actual output,
  bounded runtime profile data, and adaptive events where the build supports
  them.

The client debounces live compile requests, caches identical successful
compile inputs, and embeds source/options/stage in the share URL fragment.
The fragment remains client-side; source is only sent to the configured local
service for compilation.

The design reference used for this implementation is
`web/design/compiler-lab-concept.png`. The shipped UI intentionally keeps its
graphite, compact developer-tooling hierarchy while using the real local API
instead of placeholder cards.
