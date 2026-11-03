# NanoPass Compiler Lab UX Contract

## Product context

- **Audience:** Compiler engineers, students, and users debugging Nano programs.
- **Primary jobs:** Edit source, inspect deterministic compiler artifacts, execute bounded modes, and understand optimization/verification facts.
- **Active locale:** English.
- **Accessibility target:** WCAG 2.2 AA.

## Business-context sources

| Domain / scope | Authoritative source | Reviewed date |
|---|---|---|
| Local compiler API and artifacts | `docs/compiler-api.md` | 2026-11-03 |
| Browser execution boundary | `docs/web-security.md` | 2026-11-03 |
| Lab behavior and browser journey | `docs/web-lab.md` | 2026-11-03 |

## Visual contract

- **Project visual system:** `DESIGN.md`.
- **Token ownership:** Existing runtime CSS is canonical; `DESIGN.md` mirrors `web/src/styles.css` variables.
- **Scrollbar:** The global application stylesheet owns visible tokenized scrollbars.

## Canonical UI Map

| Capability | Canonical owner | Source of truth | Allowed variants | Verification |
|---|---|---|---|---|
| Public introduction | `LandingPage` | static, compiler-capability copy | public route `/` | unit + browser |
| Product navigation | `App.tsx` dashboard rail | route path | persistent desktop rail / compact mobile rail | unit + browser |
| Select/Listbox | Native selects in `App.tsx` | browser control + accessible label | stage, graph, policy, example | unit + E2E |
| Source input | Monaco `SourceEditor` | component props/API | code editing | unit + browser |
| Source tools | Workbench source header | deterministic local formatter / Nano generator | format source, generate program | unit + browser |
| Focus mode | Workbench source header | local workbench state | expand/restore source + terminal | unit + browser |
| Compiler terminal | `TerminalPane` | compile/run response | verifier, diagnostics, program output | unit + browser |
| Terminal command reference | `TerminalPane` tabs | documented CLI surface + compile response | verify, emit SSA, explain, optimize, learned policy | unit + browser |
| Graph | `GraphPane` / React Flow | structured compiler graph JSON | cfg, dominator-tree, loops, call-graph | unit + E2E |
| Diagnostics/remarks | `Panels.tsx` | compiler response | applied/missed/analysis | unit |
| Scrollbar | `web/src/styles.css` | runtime CSS tokens | geometry only | browser |

## Interaction behavior

| Operation | Trigger | Pending | Success feedback | Failure recovery |
|---|---|---|---|---|
| Live compile | Debounced source/options change | status text and disabled run controls | verified compiler duration when available | structured diagnostics remain visible |
| Run mode | Named native button | busy controls | runtime/profile output | API result shows explicit unavailable/error status |
| IR/graph inspection | Labeled select or graph node | none | selected IR/block is highlighted | retain last valid compilation |
| Explain representation | Plain-language guide beneath IR | selected compiler snapshot and SSA value | explanation updates with stage/value | state remains useful with no selection |
| Generate program | Generate program button | none | a deterministic language-valid program is inserted | user can edit/regenerate it |
| Focus editor | Focus / Restore button | none | source editor and terminal occupy the workbench | restores the prior artifact layout without losing source |
| Terminal command | command prompt or suggestion | command is evaluated against a strict compiler-command allowlist | compiler response is appended to terminal history | unknown commands are rejected; no shell is available |
| Visual evidence | correctness, benchmark, and policy panels | checked results / local measurements / compiler records | run or measure CTA when data is absent | never display invented metrics |
| Share session | Share session button | none | URL fragment + clipboard when available | URL fragment remains the durable fallback |

## Navigation and responsive behavior

Home (`/`) is a public introduction to the observable compiler pipeline and links directly to the workbench. Workbench (`/lab`), Guide (`/guide`), Graphs (`/graphs`), and Optimizations (`/optimizations`) are route-level views. Desktop product routes place those views in a persistent left navigation rail; the active route is text-labelled and visually selected. At narrow widths the same rail is compressed into labelled icon navigation before the workbench panels stack in compiler-flow order. Every visible action is a semantic button or labeled native select, with a keyboard focus ring.

## Async and resilience

Live compilation is debounced by the app and identical successful work is cached by the client API layer. The UI never presents synthetic stage, timing, policy, or runtime output. The security and capability boundary remains exactly as defined in `docs/web-security.md` and `docs/compiler-api.md`.

## Verification

- Static: TypeScript, Vitest, Vite production build, and the project E2E browser journey.
- Browser: source selection, IR selection, graph selection, SSA execution, adaptive JIT facts, plus desktop and narrow visual inspection.
- Canonical sibling comparison: `web/e2e/lab.spec.ts` describes the production interaction baseline.
