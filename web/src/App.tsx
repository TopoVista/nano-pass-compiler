import { useEffect, useMemo, useState } from "react";
import { CodeView } from "./components/CodeView";
import { GraphPane } from "./components/GraphPane";
import {
  Diagnostics,
  Profile,
  Remarks,
  Timeline,
  ValueInspector,
} from "./components/Panels";
import { Correctness } from "./components/Correctness";
import { OptimizationComparison } from "./components/Comparison";
import {
  BenchmarkPanel,
  type BenchmarkMeasurement,
} from "./components/BenchmarkPanel";
import { PolicyFeatures } from "./components/PolicyFeatures";
import { PolicyDecisions } from "./components/PolicyDecisions";
import { SourceEditor } from "./components/SourceEditor";
import { FeatureTour } from "./components/FeatureTour";
import { TerminalPane } from "./components/TerminalPane";
import { IRExplainer } from "./components/IRExplainer";
import {
  allowedPasses,
  checkCorrectness,
  compile,
  run,
  type Compilation,
  type CorrectnessResult,
  type RunResult,
} from "./lib/api";
import { formatNano } from "./lib/formatNano";
import { generateNanoProgram } from "./lib/randomNano";
import "./styles.css";

const examples: Record<string, string> = {
  Factorial:
    "int fact(int n) { if (n <= 1) { return 1; } return n * fact(n - 1); }\nint main() { int n = 5; print(fact(n)); return 0; }",
  "SCCP branch":
    "int main() { int x = 4; if (true) { x = x + 3; } else { x = 99; } print(x); return x; }",
  LICM: "int main() { int i = 0; int total = 0; while (i < 4) { total = total + 2 * 3; i = i + 1; } print(total); return total; }",
  Inlining:
    "int choose(int x) { if (x > 0) { return 7; } return 3; }\nint main() { int y = choose(1); print(y); return y; }",
  Arrays:
    "int main() { int values[3]; values[0] = 2; values[1] = 3; values[2] = values[0] + values[1]; print(values[2]); return values[2]; }",
  "Nested loops":
    "int main() { int i = 0; int total = 0; while (i < 3) { int j = 0; while (j < 2) { total = total + i + j; j = j + 1; } i = i + 1; } print(total); return total; }",
  "Why not LICM":
    "int main() { int i = 0; int total = 0; while (i < 3) { total = total + i / (i + 1); i = i + 1; } print(total); return total; }",
  "PGO hot loop":
    "int work(int x) { return x * 2 + 1; } int main() { int i = 0; int total = 0; while (i < 20) { total = work(total); i = i + 1; } print(total); return total; }",
  "Adaptive JIT":
    "int step(int x) { return x + 1; }\nint main() { int i = 0; int total = 0; while (i < 12) { total = step(total); i = i + 1; } print(total); return total; }",
  "NanoPass Dijkstra": `i32 main() {
  i32 graph[36];
  i32 distance[6];
  truth visited[6];
  i32 i = 0;
  while (i < 36) { graph[i] = 0; i = i + 1; }
  graph[1] = 7; graph[6] = 7;
  graph[2] = 9; graph[12] = 9;
  graph[5] = 14; graph[30] = 14;
  graph[7] = 10; graph[13] = 10;
  graph[9] = 15; graph[19] = 15;
  graph[15] = 11; graph[20] = 11;
  graph[17] = 2; graph[32] = 2;
  graph[22] = 6; graph[27] = 6;
  graph[29] = 9; graph[34] = 9;
  i = 0;
  while (i < 6) { distance[i] = 1000000; visited[i] = false; i = i + 1; }
  distance[0] = 0;
  i32 round = 0;
  while (round < 6) {
    i32 best = 1000000;
    i32 current = -1;
    i = 0;
    while (i < 6) {
      if (!visited[i] && distance[i] < best) { best = distance[i]; current = i; }
      i = i + 1;
    }
    if (current == -1) { break; }
    visited[current] = true;
    i32 neighbor = 0;
    while (neighbor < 6) {
      i32 edge = graph[current * 6 + neighbor];
      if (edge > 0 && !visited[neighbor] && distance[current] + edge < distance[neighbor]) {
        distance[neighbor] = distance[current] + edge;
      }
      neighbor = neighbor + 1;
    }
    round = round + 1;
  }
  i = 0;
  while (i < 6) { print(distance[i]); i = i + 1; }
  return 0;
}`,
};
const allArtifacts = {
  includeSource: true,
  includeAst: true,
  includeDesugaredAst: true,
  includeCore: true,
  includeAnf: true,
  includeCfg: true,
  includeSsa: true,
  includeOptimizedSsa: true,
  includeLLVM: true,
  includeGraphs: true,
  includeRemarks: true,
  includeTelemetry: true,
  includeProvenance: true,
};
type RunMode = "core" | "ssa" | "jit" | "adaptive-jit";
type SharedSession = {
  source?: string;
  level?: number;
  policy?: "default" | "heuristic" | "learned";
  stage?: string;
  passes?: string[];
  graph?: string;
};
const sessionFromHash = (): SharedSession => {
  try {
    const raw = window.location.hash.replace(/^#session=/, "");
    return raw
      ? (JSON.parse(
          decodeURIComponent(atob(raw.replace(/-/g, "+").replace(/_/g, "/"))),
        ) as SharedSession)
      : {};
  } catch {
    return {};
  }
};
export default function App() {
  const session = sessionFromHash();
  // Start with a small executable optimization example so the first-run
  // profile view demonstrates a complete compiler/runtime round trip.
  const [source, setSource] = useState(
    session.source ?? examples["SCCP branch"],
  );
  const [exampleName, setExampleName] = useState(
    session.source ? "" : "SCCP branch",
  );
  const [level, setLevel] = useState(session.level ?? 2);
  const [policy, setPolicy] = useState<"default" | "heuristic" | "learned">(
    session.policy ?? "default",
  );
  const [selectedStage, setSelectedStage] = useState(
    session.stage ?? "optimized-ssa",
  );
  const [selectedPass, setSelectedPass] = useState<string>();
  const [graphKind, setGraphKind] = useState(session.graph ?? "cfg");
  const [selectedBlock, setSelectedBlock] = useState<number>();
  const [selectedValue, setSelectedValue] = useState<number>();
  const [focusLine, setFocusLine] = useState<number>();
  const [custom, setCustom] = useState<string[]>(() =>
    (session.passes ?? []).filter((pass) => allowedPasses.includes(pass)),
  );
  const [result, setResult] = useState<Compilation>();
  const [runResult, setRunResult] = useState<RunResult>();
  const [correctness, setCorrectness] = useState<CorrectnessResult>();
  const [comparison, setComparison] = useState<Compilation[]>();
  const [benchmark, setBenchmark] = useState<BenchmarkMeasurement>();
  const [busy, setBusy] = useState(false);
  const [executionStep, setExecutionStep] = useState(0);
  const options = useMemo(
    () => ({
      source,
      optimizationLevel: level,
      policy,
      ...(custom.length ? { passes: custom } : {}),
      ...allArtifacts,
    }),
    [source, level, policy, custom],
  );
  const doCompile = async () => {
    setBusy(true);
    setRunResult(undefined);
    setCorrectness(undefined);
    const response = await compile(options);
    setResult(response);
    if (
      response.snapshots.length &&
      !response.snapshots.some((snapshot) => snapshot.stageId === selectedStage)
    )
      setSelectedStage(response.snapshots.at(-1)!.stageId);
    setBusy(false);
  };
  useEffect(() => {
    const handle = window.setTimeout(() => {
      void doCompile();
    }, 450);
    return () => window.clearTimeout(handle);
  }, [source, level, policy, custom.join(",")]);
  const doRun = async (mode: RunMode) => {
    setBusy(true);
    setExecutionStep(1);
    const response = await run(options, mode);
    setRunResult(response);
    setBusy(false);
    [2, 3, 4].forEach((step, index) =>
      window.setTimeout(() => setExecutionStep(step), 260 * (index + 1)),
    );
  };
  const doCorrectness = async () => {
    setBusy(true);
    setCorrectness(await checkCorrectness(options));
    setBusy(false);
  };
  const doCompare = async () => {
    setBusy(true);
    setComparison(
      await Promise.all(
        [0, 1, 2, 3].map((optimizationLevel) =>
          compile({ ...options, optimizationLevel }),
        ),
      ),
    );
    setBusy(false);
  };
  const doBenchmark = async () => {
    setBusy(true);
    setBenchmark(undefined);
    const compilation = await compile(options);
    await run(options, "ssa");
    const samples: number[] = [];
    for (let index = 0; index < 5; ++index) {
      const measured = await run(options, "ssa");
      if (!measured.success || measured.executionDurationUs === undefined) {
        setBusy(false);
        return;
      }
      samples.push(measured.executionDurationUs);
    }
    const ordered = [...samples].sort((left, right) => left - right);
    const percentile = (fraction: number) =>
      ordered[Math.round((ordered.length - 1) * fraction)];
    setBenchmark({
      compileUs: compilation.telemetry.totalCompileUs,
      samplesUs: samples,
      medianUs: percentile(0.5),
      p25Us: percentile(0.25),
      p75Us: percentile(0.75),
    });
    setBusy(false);
  };
  const share = async () => {
    const encoded = btoa(
      encodeURIComponent(
        JSON.stringify({
          source,
          level,
          policy,
          stage: selectedStage,
          passes: custom,
          graph: graphKind,
        }),
      ),
    )
      .replace(/\+/g, "-")
      .replace(/\//g, "_")
      .replace(/=+$/, "");
    window.location.hash = `session=${encoded}`;
    await navigator.clipboard?.writeText(window.location.href);
  };
  const movePass = (index: number, direction: -1 | 1) =>
    setCustom((current) => {
      const target = index + direction;
      if (target < 0 || target >= current.length) return current;
      const next = [...current];
      [next[index], next[target]] = [next[target], next[index]];
      return next;
    });
  const snapshot =
    result?.snapshots.find((item) => item.stageId === selectedStage) ??
    result?.snapshots.at(-1);
  const graph =
    result?.graphs.find((item) => item.kind === graphKind) ?? result?.graphs[0];
  const value = result?.values.find((item) => item.value === selectedValue);
  useEffect(() => {
    if (value && value.line > 0) setFocusLine(value.line);
  }, [value]);
  const transformKey = selectedPass?.replace(/@[^@]+$/, "");
  const transform = transformKey
    ? result?.transformations.find((item) => item.passId === transformKey)
    : undefined;
  const route =
    window.location.pathname === "/" ? "/lab" : window.location.pathname;
  const navigation = [
    { href: "/lab", label: "Workbench" },
    { href: "/guide", label: "Guide" },
    { href: "/graphs", label: "Graphs" },
    { href: "/optimizations", label: "Optimizations" },
  ];
  const toolbar = (
    <section className="toolbar" aria-label="Compiler controls">
      <div className="control-step">
      <small>1 · Program</small><label>
        Example{" "}
        <select
          aria-label="Example program"
          value={exampleName}
          onChange={(event) => {
            if (event.target.value) {
              setExampleName(event.target.value);
              setSource(examples[event.target.value]);
            }
          }}
        >
          {exampleName === "" && <option value="">Custom source</option>}
          {Object.keys(examples).map((name) => (
            <option key={name} value={name}>
              {name}
            </option>
          ))}
        </select>
      </label></div>
      <div className="control-step">
      <small>2 · Strategy</small><label>
        Pipeline{" "}
        <select
          aria-label="Inline policy"
          value={policy}
          disabled={custom.length > 0}
          onChange={(event) => setPolicy(event.target.value as typeof policy)}
        >
          <option value="default">Default pipeline</option>
          <option value="heuristic">Heuristic</option>
          <option value="learned">Learned (reviewed model)</option>
        </select>
      </label>
      <details className="pipeline-picker">
        <summary>
          Passes{" "}
          <small>
            {custom.length ? `${custom.length} selected` : "optional"}
          </small>
        </summary>
        <div className="pipeline-options">
          {allowedPasses.map((pass) => (
            <label key={pass}>
              <input
                type="checkbox"
                checked={custom.includes(pass)}
                onChange={() =>
                  setCustom((current) =>
                    current.includes(pass)
                      ? current.filter((item) => item !== pass)
                      : [...current, pass],
                  )
                }
              />
              {pass}
            </label>
          ))}
        </div>
        {custom.length > 1 && (
          <div className="pipeline-order" aria-label="Custom pipeline order">
            {custom.map((pass, index) => (
              <span key={pass}>
                {pass}
                <button
                  aria-label={`Move ${pass} earlier`}
                  disabled={index === 0}
                  onClick={() => movePass(index, -1)}
                >
                  ↑
                </button>
                <button
                  aria-label={`Move ${pass} later`}
                  disabled={index === custom.length - 1}
                  onClick={() => movePass(index, 1)}
                >
                  ↓
                </button>
              </span>
            ))}
          </div>
        )}
      </details>
      </div>
      <div className="toolbar-actions"><small>3 · Measure</small><button disabled={busy} onClick={() => void doCompare()}>
        Compare O-levels
      </button>
      <button disabled={busy} onClick={() => void doBenchmark()}>
        Benchmark
      </button></div>
      <span
        className={result?.success ? "compile-status good" : "compile-status"}
      >
        {result
          ? result.success
            ? `verified · ${result.telemetry.totalCompileUs} μs`
            : "diagnostics reported"
          : "waiting"}
      </span>
      <span className="compiler-revision" aria-label="Compiler revision">
        {result?.compilerRevision ?? "waiting"}
      </span>
    </section>
  );
  const workspace = (
    <section className="workspace" id="compiler-workspace">
      <div className="panel source-panel">
        <header>
          <div>
            <strong>Source editor</strong>
            <span>NanoPass C</span>
          </div>
          <div className="source-actions">
            <button
              onClick={() => {
                setSource(generateNanoProgram());
                setExampleName("");
              }}
            >
              Generate program
            </button>
            <button
              className="format-source"
              onClick={() => setSource(formatNano(source))}
            >
              Format source
            </button>
          </div>
        </header>
        <div className="source-stack">
          <div className="editor">
            <SourceEditor
              value={source}
              focusLine={focusLine}
              onCursorLine={(line) => {
                const related = result?.values.find(
                  (candidate) => candidate.line === line,
                );
                if (related) setSelectedValue(related.value);
              }}
              onChange={(next) => {
                setSource(next);
                setExampleName("");
              }}
            />
          </div>
          <TerminalPane compilation={result} run={runResult} />
        </div>
      </div>
      <div className="panel ir-panel">
        <header>
          <div>
            <strong>Representation</strong>
            <span>{snapshot?.displayName ?? "waiting"}</span>
          </div>
          <select
            aria-label="IR stage"
            value={snapshot?.stageId ?? ""}
            onChange={(event) => setSelectedStage(event.target.value)}
          >
            {result?.snapshots.map((item) => (
              <option key={item.stageId} value={item.stageId}>
                {item.displayName}
              </option>
            ))}
          </select>
        </header>
        <CodeView
          snapshot={snapshot}
          selectedBlock={selectedBlock}
          selectedValue={value}
          onValue={setSelectedValue}
          onBlock={setSelectedBlock}
        />
        <IRExplainer snapshot={snapshot} value={value} />
      </div>
      <div className="panel timeline-panel">
        <header>
          <strong>Pass timeline</strong>
          <span>compiler facts</span>
        </header>
        <Timeline
          passes={result?.passes ?? []}
          selected={selectedPass}
          onSelect={setSelectedPass}
        />
      </div>
      <div className="panel graph-panel">
        <header>
          <div>
            <strong>Graph explorer</strong>
            <span>{graph?.function ? `@${graph.function}` : "no graph"}</span>
          </div>
          <select
            aria-label="Graph kind"
            value={graphKind}
            onChange={(event) => setGraphKind(event.target.value)}
          >
            {["cfg", "dominator-tree", "loops", "call-graph"].map((kind) => (
              <option key={kind}>{kind}</option>
            ))}
          </select>
        </header>
        <GraphPane
          graph={graph}
          profile={runResult?.runtimeProfile}
          selectedBlock={selectedBlock}
          onSelect={(id) => {
            const match = /bb(\d+)$/.exec(id);
            setSelectedBlock(match ? Number(match[1]) : undefined);
          }}
        />
      </div>
      <aside className="inspector">
        <ValueInspector value={value} />
        <Remarks remarks={result?.remarks ?? []} />
      </aside>
      <div className="panel diagnostics-panel">
        <Diagnostics
          diagnostics={result?.diagnostics ?? []}
          onSelect={setFocusLine}
        />
      </div>
      <div className="panel runtime-panel">
        <div className="run-controls">
          <strong>Execution</strong>
          {(["core", "ssa", "jit", "adaptive-jit"] as RunMode[]).map((mode) => (
            <button key={mode} disabled={busy} onClick={() => void doRun(mode)}>
              {mode}
            </button>
          ))}
          <button disabled={busy} onClick={() => void doCorrectness()}>
            check correctness
          </button>
        </div>
        <div className="execution-content">
          <div className="execution-trace" aria-label="Execution progress">
            {["Lower", "Verify", "Execute", "Report"].map((label, index) => (
              <span
                key={label}
                className={
                  executionStep > index
                    ? "complete"
                    : executionStep === index
                      ? "active"
                      : ""
                }
              >
                <i>{index + 1}</i>
                {label}
              </span>
            ))}
          </div>
          <Profile
            profile={runResult?.runtimeProfile}
            stdout={runResult?.stdout}
            mode={runResult ? runResult.status : "not run"}
            status={runResult?.errorKind}
            events={runResult?.adaptiveEvents}
          />
        </div>
      </div>
      <div className="panel correctness-wrap">
        <Correctness result={correctness} onRun={() => void doCorrectness()} />
      </div>
      <div className="panel comparison-wrap">
        <OptimizationComparison results={comparison} />
      </div>
      <div className="panel comparison-wrap">
        <BenchmarkPanel result={benchmark} onMeasure={() => void doBenchmark()} />
      </div>
      <div className="panel policy-wrap">
        <PolicyFeatures
          features={runResult?.policyFeatures ?? result?.policyFeatures}
        />
      </div>
      <div className="panel policy-wrap">
        <PolicyDecisions records={result?.policyRecords} />
      </div>
      {transform && selectedPass && (
        <div className="panel diff-panel">
          <header>
            <strong>Before / after</strong>
            <span>{selectedPass}</span>
          </header>
          <div className="diff">
            <pre>{transform.before}</pre>
            <pre>{transform.after}</pre>
          </div>
        </div>
      )}
    </section>
  );
  const focusedGraphs = (
    <section className="route-page">
      <header className="route-intro">
        <span>GRAPH EXPLORER</span>
        <h1>Follow control, not decoration.</h1>
        <p>
          Choose a compiler graph and inspect the actual blocks and edges
          emitted for the current program.
        </p>
      </header>
      <section className="route-grid">
        <div className="panel graph-panel route-graph">
          <header>
            <div>
              <strong>Graph explorer</strong>
              <span>{graph?.function ? `@${graph.function}` : "no graph"}</span>
            </div>
            <select
              aria-label="Graph kind"
              value={graphKind}
              onChange={(event) => setGraphKind(event.target.value)}
            >
              {["cfg", "dominator-tree", "loops", "call-graph"].map((kind) => (
                <option key={kind}>{kind}</option>
              ))}
            </select>
          </header>
          <GraphPane
            graph={graph}
            profile={runResult?.runtimeProfile}
            selectedBlock={selectedBlock}
            onSelect={(id) => {
              const match = /bb(\d+)$/.exec(id);
              setSelectedBlock(match ? Number(match[1]) : undefined);
            }}
          />
        </div>
        <ValueInspector value={value} />
      </section>
    </section>
  );
  const focusedOptimizations = (
    <section className="route-page">
      <header className="route-intro">
        <span>OPTIMIZATION EVIDENCE</span>
        <h1>Every rewrite has a reason.</h1>
        <p>
          Read a visual sequence of compiler passes, then inspect the facts
          behind applied and rejected transformations.
        </p>
      </header>
      <section className="panel route-timeline">
        <Timeline
          passes={result?.passes ?? []}
          selected={selectedPass}
          onSelect={setSelectedPass}
        />
      </section>
      <section className="route-grid">
        <Remarks remarks={result?.remarks ?? []} />
        <IRExplainer snapshot={snapshot} value={value} />
      </section>
    </section>
  );
  return (
    <main className="lab-shell">
      <header className="topbar">
        <a className="brand" href="/lab">
          <span className="brand-mark" aria-hidden="true">
            <i />
            <i />
            <i />
          </span>
          <strong>NanoPass</strong>
          <span>Compiler Lab</span>
        </a>
        <nav aria-label="Lab views">
          {navigation.map((item) => (
            <a
              key={item.href}
              className={route === item.href ? "active" : ""}
              href={item.href}
            >
              {item.label}
            </a>
          ))}
        </nav>
        <div className="top-actions">
          <label>
            O-level{" "}
            <select
              aria-label="Optimization level"
              value={level}
              onChange={(event) => setLevel(Number(event.target.value))}
            >
              {[0, 1, 2, 3].map((item) => (
                <option key={item} value={item}>
                  O{item}
                </option>
              ))}
            </select>
          </label>
          <button onClick={share}>Share session</button>
          <button
            className="run"
            disabled={busy}
            onClick={() => void doRun("ssa")}
          >
            {busy ? "Compiling…" : "Run SSA"}
          </button>
        </div>
      </header>
      {route === "/guide" ? (
        <FeatureTour
          onExample={(name) => {
            setExampleName(name);
            setSource(examples[name]);
          }}
          onStage={setSelectedStage}
          onGraph={setGraphKind}
        />
      ) : route === "/graphs" ? (
        focusedGraphs
      ) : route === "/optimizations" ? (
        focusedOptimizations
      ) : (
        <>
          {toolbar}
          {workspace}
        </>
      )}
    </main>
  );
}
