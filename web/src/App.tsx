import { useEffect, useMemo, useState } from "react";
import { CodeView } from "./components/CodeView";
import { GraphPane } from "./components/GraphPane";
import {
  Diagnostics,
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
import { TerminalPane, type TerminalReply } from "./components/TerminalPane";
import { IRExplainer } from "./components/IRExplainer";
import { LandingPage } from "./components/LandingPage";
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
    `int main() {
  int x = 4;
  if (true) {
    x = x + 3;
  } else {
    x = 99;
  }
  print(x);
  return x;
}`,
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
  "NanoPass Dijkstra": `int main() {
  int graph[36];
  int distance[6];
  bool visited[6];
  int i = 0;
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
  int round = 0;
  while (round < 6) {
    int best = 1000000;
    int current = -1;
    i = 0;
    while (i < 6) {
      if (!visited[i] && distance[i] < best) { best = distance[i]; current = i; }
      i = i + 1;
    }
    if (current == -1) { break; }
    visited[current] = true;
    int neighbor = 0;
    while (neighbor < 6) {
      int edge = graph[current * 6 + neighbor];
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
  const [route, setRoute] = useState(() => window.location.pathname);
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
  const [serviceError, setServiceError] = useState<string>();
  const [executionStep, setExecutionStep] = useState(0);
  const [editorExpanded, setEditorExpanded] = useState(false);
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
    setServiceError(undefined);
    try {
      const response = await compile(options);
      setResult(response);
      if (
        response.snapshots.length &&
        !response.snapshots.some((snapshot) => snapshot.stageId === selectedStage)
      )
        setSelectedStage(response.snapshots.at(-1)!.stageId);
    } catch {
      setServiceError("The compiler service is waking up or unavailable. Check its URL, then retry.");
    } finally {
      setBusy(false);
    }
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
    setServiceError(undefined);
    try {
      const response = await run(options, mode);
      setRunResult(response);
      [2, 3, 4].forEach((step, index) =>
        window.setTimeout(() => setExecutionStep(step), 260 * (index + 1)),
      );
    } catch {
      setExecutionStep(0);
      setServiceError('Compiler service unavailable. Retry after it finishes starting.');
    } finally {
      setBusy(false);
    }
  };
  const doCorrectness = async () => {
    setBusy(true);
    setServiceError(undefined);
    try {
      setCorrectness(await checkCorrectness(options));
    } catch {
      setServiceError('Compiler service unavailable. Retry after it finishes starting.');
    } finally {
      setBusy(false);
    }
  };
  const doCompare = async () => {
    setBusy(true);
    setServiceError(undefined);
    try {
      setComparison(
        await Promise.all(
          [0, 1, 2, 3].map((optimizationLevel) =>
            compile({ ...options, optimizationLevel }),
          ),
        ),
      );
    } catch {
      setServiceError('Compiler service unavailable. Retry after it finishes starting.');
    } finally {
      setBusy(false);
    }
  };
  const doBenchmark = async () => {
    setBusy(true);
    setBenchmark(undefined);
    setServiceError(undefined);
    try {
      const compilation = await compile(options);
      await run(options, "ssa");
      const samples: number[] = [];
      for (let index = 0; index < 5; ++index) {
        const measured = await run(options, "ssa");
        if (!measured.success || measured.executionDurationUs === undefined) return;
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
    } catch {
      setServiceError('Compiler service unavailable. Retry after it finishes starting.');
    } finally {
      setBusy(false);
    }
  };
  const runTerminalCommand = async (rawCommand: string): Promise<TerminalReply> => {
    const [verb = "", ...arguments_] = rawCommand.trim().toLowerCase().split(/\s+/);
    const argument = arguments_.join(" ");
    const help: TerminalReply = { tone: "info", lines: [
      "Available in-app CLI commands (no shell access):",
      "  verify                         verify the compiled pipeline",
      "  run [core|ssa|jit|adaptive-jit] execute with a supported engine",
      "  emit [ast|core|anf|cfg|ssa|llvm] print an emitted representation",
      "  explain                        show factual pass timing",
      "  optimize                       show recorded optimizer remarks",
      "  graph [cfg|dominator-tree|loops|call-graph] switch graph view",
      "  correctness                    run differential checks",
      "  benchmark                      collect five local SSA samples",
      "  policy [default|heuristic|learned] select a reviewed policy",
      "  clear                          clear terminal history",
    ] };
    if (verb === "help") return help;
    if (verb === "clear") return { lines: [], clear: true };
    if (verb === "policy") {
      if (argument === "default" || argument === "heuristic" || argument === "learned") {
        setPolicy(argument);
        return { tone: "success", lines: [`Selected ${argument} policy. The compiler will re-evaluate the current source.`, argument === "learned" ? "Scope is bounded to reviewed inline profitability decisions; the verifier remains mandatory." : ""] .filter(Boolean) };
      }
      return { tone: "error", lines: ["policy expects: default, heuristic, or learned"] };
    }
    if (verb === "graph") {
      if (["cfg", "dominator-tree", "loops", "call-graph"].includes(argument)) {
        setGraphKind(argument);
        return { tone: "success", lines: [`Selected ${argument} graph. Open Graphs to inspect its compiler-generated nodes and edges.`] };
      }
      return { tone: "error", lines: ["graph expects: cfg, dominator-tree, loops, or call-graph"] };
    }
    setBusy(true);
    try {
      if (verb === "verify") {
        const response = await compile(options);
        setResult(response);
        return response.success ? { tone: "success", lines: [`PASS · ${response.telemetry.functions} function(s), ${response.telemetry.blocks} blocks, ${response.telemetry.values} values`, "Compiler snapshots and verifier results are available in the workbench."] } : { tone: "error", lines: response.diagnostics.map((diagnostic) => `${diagnostic.severity}[${diagnostic.code}] ${diagnostic.message}`) };
      }
      if (verb === "run") {
        const mode = (argument || "ssa") as RunMode;
        if (!(["core", "ssa", "jit", "adaptive-jit"] as string[]).includes(mode)) return { tone: "error", lines: ["run expects: core, ssa, jit, or adaptive-jit"] };
        setExecutionStep(1);
        const response = await run(options, mode);
        setRunResult(response);
        [2, 3, 4].forEach((step, index) => window.setTimeout(() => setExecutionStep(step), 260 * (index + 1)));
        return response.success ? { tone: "success", lines: [...(response.stdout ? response.stdout.trimEnd().split("\n").map((line) => `› ${line}`) : ["(program produced no output)"]), response.returnValue ? `return ${response.returnValue}` : ""] .filter(Boolean) } : { tone: "error", lines: [response.stderr || response.errorKind] };
      }
      if (verb === "emit") {
        const stageMap: Record<string, string> = { source: "source", ast: "ast", core: "core", anf: "anf", cfg: "cfg", ssa: "ssa", llvm: "llvm" };
        const stage = stageMap[argument];
        if (!stage) return { tone: "error", lines: ["emit expects: ast, core, anf, cfg, ssa, or llvm"] };
        const response = await compile(options);
        setResult(response);
        const emitted = response.snapshots.find((snapshot) => snapshot.stageId === stage);
        if (!emitted) return { tone: "error", lines: [`${stage} is unavailable for this source.`] };
        setSelectedStage(stage);
        return { tone: "success", lines: emitted.text.split("\n").slice(0, 14) };
      }
      if (verb === "explain") {
        const response = await compile(options);
        setResult(response);
        return { tone: "success", lines: response.passes.map((pass) => `${pass.verified ? "✓" : "!"} ${pass.displayName} · ${pass.durationUs} μs${pass.changed ? " · rewrote IR" : ""}`).slice(0, 14) };
      }
      if (verb === "optimize") {
        const response = await compile(options);
        setResult(response);
        return { tone: "success", lines: response.remarks.length ? response.remarks.slice(0, 12).map((remark) => `${remark.kind.toUpperCase()} ${remark.passId}: ${remark.message}`) : ["No optimizer remarks were recorded for this source."] };
      }
      if (verb === "correctness") {
        const response = await checkCorrectness(options);
        setCorrectness(response);
        return { tone: response.equivalent ? "success" : "error", lines: response.checks.map((check) => `${check.passed ? "✓" : "×"} ${check.stage}: ${check.detail}`) };
      }
      if (verb === "benchmark") {
        const compilation = await compile(options);
        const samples: number[] = [];
        for (let index = 0; index < 5; ++index) {
          const measured = await run(options, "ssa");
          if (!measured.success || measured.executionDurationUs === undefined) return { tone: "error", lines: [measured.stderr || measured.errorKind || "benchmark run failed"] };
          samples.push(measured.executionDurationUs);
        }
        const ordered = [...samples].sort((left, right) => left - right);
        const measurement = { compileUs: compilation.telemetry.totalCompileUs, samplesUs: samples, medianUs: ordered[Math.round((ordered.length - 1) * .5)], p25Us: ordered[Math.round((ordered.length - 1) * .25)], p75Us: ordered[Math.round((ordered.length - 1) * .75)] };
        setBenchmark(measurement);
        return { tone: "success", lines: [`compile ${measurement.compileUs} μs`, `samples ${samples.join(", ")} μs`, `median ${measurement.medianUs} μs · p25/p75 ${measurement.p25Us}/${measurement.p75Us} μs`] };
      }
      return { tone: "error", lines: [`Unknown command: ${verb}`, "Type help to list safe in-app commands."] };
    } catch {
      return {
        tone: "error",
        lines: [
          "Compiler service unavailable.",
          "Check the local service URL, then run the command again.",
        ],
      };
    } finally { setBusy(false); }
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
  useEffect(() => {
    const syncRoute = () => setRoute(window.location.pathname);
    window.addEventListener("popstate", syncRoute);
    return () => window.removeEventListener("popstate", syncRoute);
  }, []);
  const navigate = (event: React.MouseEvent<HTMLAnchorElement>) => {
    const target = event.currentTarget.getAttribute("href");
    if (!target || event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return;
    event.preventDefault();
    window.history.pushState({}, "", target);
    setRoute(window.location.pathname);
  };
  const navigation = [
    { href: "/lab", label: "Workbench", detail: "Code + output" },
    { href: "/graphs", label: "Graphs", detail: "Control flow" },
    { href: "/optimizations", label: "Optimize", detail: "Pass evidence" },
    { href: "/verification", label: "Verify", detail: "Runtime checks" },
    { href: "/guide", label: "Guide", detail: "Learn NanoPass" },
  ];
  const routeLabel = navigation.find((item) => item.href === route)?.label ?? "Workbench";
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
      <span
        className={result?.success ? "compile-status good" : "compile-status"}
      >
        {busy
          ? result
            ? "updating compiler facts…"
            : "starting compiler service…"
          : result
          ? result.success
            ? `verified · ${result.telemetry.totalCompileUs} μs`
            : "diagnostics reported"
          : "waiting"}
      </span>
      {serviceError && <button className="service-retry" onClick={() => void doCompile()}>Retry compiler service</button>}
      <span className="compiler-revision" aria-label="Compiler revision">
        {result?.compilerRevision ?? "waiting"}
      </span>
    </section>
  );
  const workspace = (
    <section className={`workspace${editorExpanded ? " editor-focus" : ""}`} id="compiler-workspace">
      <div className="panel source-panel">
        <header>
          <div>
            <strong>Source editor</strong>
            <span>NanoPass C</span>
          </div>
          <div className="source-actions">
            <button className="focus-editor" onClick={() => setEditorExpanded((current) => !current)}>
              {editorExpanded ? "Restore" : "Focus"}
            </button>
            <button
              aria-label="Generate program"
              onClick={() => {
                setSource(generateNanoProgram());
                setExampleName("");
              }}
            >
              Generate
            </button>
            <button
              className="format-source"
              aria-label="Format source"
              onClick={() => setSource(formatNano(source))}
            >
              Format
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
          <TerminalPane compilation={result} run={runResult} onCommand={runTerminalCommand} />
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
      <section className="route-grid optimization-evidence-grid">
        <PolicyFeatures
          features={runResult?.policyFeatures ?? result?.policyFeatures}
        />
        <PolicyDecisions records={result?.policyRecords} />
      </section>
      {transform && selectedPass && (
        <section className="panel diff-panel">
          <header>
            <strong>Before / after</strong>
            <span>{selectedPass}</span>
          </header>
          <div className="diff">
            <pre>{transform.before}</pre>
            <pre>{transform.after}</pre>
          </div>
        </section>
      )}
    </section>
  );
  const focusedVerification = (
    <section className="route-page verification-page">
      <header className="route-intro">
        <span>VERIFICATION CONSOLE</span>
        <h1>Measure what the compiler can actually prove.</h1>
        <p>
          Execute safe interpreter modes, compare observable results, and keep
          performance measurements separate from correctness evidence.
        </p>
      </header>
      <section className="verification-actionbar" aria-label="Execution controls">
        <div>
          <strong>Run current source</strong>
          <span>Each mode uses the compiler service; unavailable engines report their real state.</span>
        </div>
        <div>
          {(["core", "ssa", "jit", "adaptive-jit"] as RunMode[]).map((mode) => (
            <button key={mode} disabled={busy} onClick={() => void doRun(mode)}>{mode}</button>
          ))}
          <button disabled={busy} onClick={() => void doCompare()}>Compare O-levels</button>
          <button disabled={busy} onClick={() => void doBenchmark()}>Benchmark</button>
          <button className="verification-primary" disabled={busy} onClick={() => void doCorrectness()}>check correctness</button>
        </div>
      </section>
      <section className="verification-grid">
        <div className="panel verification-runtime">
          <header><strong>Latest execution</strong><span>{runResult?.status ?? "not run"}</span></header>
          <div className="execution-content">
            <div className="execution-trace" aria-label="Execution progress">
              {["Lower", "Verify", "Execute", "Report"].map((label, index) => (
                <span key={label} className={executionStep > index ? "complete" : executionStep === index ? "active" : ""}><i>{index + 1}</i>{label}</span>
              ))}
            </div>
            {runResult ? <pre className="output">{[runResult.stdout || "(no output)", runResult.returnValue ? `return ${runResult.returnValue}` : "", ...((runResult.adaptiveEvents ?? []).map((event) => `${event.kind} @${event.function}${event.detail ? ` · ${event.detail}` : ""}`))].filter(Boolean).join("\n")}</pre> : <p className="quiet">Use the terminal command <code>run ssa</code>, or choose an engine above. Execution traces appear here after a real run.</p>}
          </div>
        </div>
        <Correctness result={correctness} onRun={() => void doCorrectness()} />
        <OptimizationComparison results={comparison} />
        <BenchmarkPanel result={benchmark} onMeasure={() => void doBenchmark()} />
      </section>
      <section className="panel verification-diagnostics"><Diagnostics diagnostics={result?.diagnostics ?? []} onSelect={setFocusLine} /></section>
    </section>
  );
  if (route === "/") return <LandingPage />;

  return (
    <main className="lab-shell dashboard-shell">
      <aside className="app-rail">
        <a className="brand rail-brand" href="/">
          <span className="brand-mark" aria-hidden="true">
            <i />
            <i />
            <i />
          </span>
          <span><strong>NanoPass</strong><small>Compiler Lab</small></span>
        </a>
        <nav className="rail-nav" aria-label="Lab views">
          <span className="rail-label">Compiler lab</span>
          {navigation.map((item) => (
            <span className="rail-link-wrap" key={item.href}>
              <a
                className={route === item.href ? "active" : ""}
                href={item.href}
                onClick={navigate}
                aria-current={route === item.href ? "page" : undefined}
              >
                <i aria-hidden="true" />
                <span><b>{item.label}</b><small>{item.detail}</small></span>
              </a>
            </span>
          ))}
        </nav>
        <div className="rail-status">
          <span>LOCAL COMPILER</span>
          <b><i /> {result?.success ? "Verifier passed" : "Compiler ready"}</b>
          <small>{result ? `${result.telemetry.functions} function${result.telemetry.functions === 1 ? "" : "s"} · ${result.telemetry.blocks} blocks` : "Awaiting source"}</small>
        </div>
      </aside>
      <section className="app-main">
      <header className="topbar dashboard-topbar">
        <div className="dashboard-context">
          <span>LOCAL SESSION / NANO C</span>
          <strong>{routeLabel}<i aria-hidden="true">/</i><b>{snapshot?.displayName ?? "Compile view"}</b></strong>
        </div>
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
      ) : route === "/verification" ? (
        focusedVerification
      ) : (
        <>
          {toolbar}
          {workspace}
        </>
      )}
      </section>
    </main>
  );
}
