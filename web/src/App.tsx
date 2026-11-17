import { lazy, Suspense, useCallback, useEffect, useMemo, useRef, useState } from "react";
import { CodeView } from "./components/CodeView";
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
  type BenchmarkFailure,
  type BenchmarkMeasurement,
} from "./components/BenchmarkPanel";
import { PolicyFeatures } from "./components/PolicyFeatures";
import { PolicyDecisions } from "./components/PolicyDecisions";
import { FeatureTour } from "./components/FeatureTour";
import { TerminalPane, type TerminalReply } from "./components/TerminalPane";
import { IRExplainer } from "./components/IRExplainer";
import { LandingPage } from "./components/LandingPage";
import {
  allowedPasses,
  checkCorrectness,
  compile,
  getCapabilities,
  run,
  type Compilation,
  type CorrectnessResult,
  type RunMode,
  type RunResult,
  type ServiceCapabilities,
} from "./lib/api";
import { formatNano } from "./lib/formatNano";
import { generateNanoProgram } from "./lib/randomNano";
import "./styles.css";

const SourceEditor = lazy(async () => ({
  default: (await import("./components/SourceEditor")).SourceEditor,
}));
const GraphPane = lazy(async () => ({
  default: (await import("./components/GraphPane")).GraphPane,
}));

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
type ConnectionState = "connecting" | "ready" | "unavailable" | "recovering";
const executionModeLabel: Record<RunMode, string> = {
  core: "CoreIR interpreter",
  ssa: "SSA interpreter",
  jit: "LLVM ORC JIT",
  "adaptive-jit": "Adaptive JIT",
};
type SharedSession = {
  source?: string;
  level?: number;
  policy?: "default" | "heuristic" | "learned";
  stage?: string;
  passes?: string[];
  graph?: string;
};
const policies = ["default", "heuristic", "learned"] as const;
const stages = ["source", "ast", "desugared-ast", "core", "anf", "cfg", "ssa", "optimized-ssa", "llvm"];
const graphs = ["cfg", "dominator-tree", "loops", "call-graph"];
const maxSharedSourceBytes = 256 * 1024;
const sessionFromHash = (): SharedSession => {
  try {
    const raw = window.location.hash.replace(/^#session=/, "");
    if (!raw || raw.length > maxSharedSourceBytes * 2) return {};
    const candidate = JSON.parse(
      decodeURIComponent(atob(raw.replace(/-/g, "+").replace(/_/g, "/"))),
    ) as SharedSession;
    const source = typeof candidate.source === "string" && candidate.source.length <= maxSharedSourceBytes ? candidate.source : undefined;
    const level = Number.isInteger(candidate.level) && candidate.level! >= 0 && candidate.level! <= 3 ? candidate.level : undefined;
    const passes = Array.isArray(candidate.passes) ? candidate.passes.filter((pass): pass is string => typeof pass === "string" && allowedPasses.includes(pass)).filter((pass, index, values) => values.indexOf(pass) === index) : undefined;
    const policy = policies.includes(candidate.policy as typeof policies[number]) ? candidate.policy : undefined;
    return {
      source,
      level,
      // A custom pipeline always takes precedence when restoring a share. It
      // is the only representation the service accepts for this combination.
      policy: passes?.length ? "default" : policy,
      passes,
      stage: stages.includes(candidate.stage ?? "") ? candidate.stage : undefined,
      graph: graphs.includes(candidate.graph ?? "") ? candidate.graph : undefined,
    };
  } catch {
    return {};
  }
};
export default function App() {
  const [session] = useState(sessionFromHash);
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
  const [benchmarkError, setBenchmarkError] = useState<BenchmarkFailure>();
  const [busy, setBusy] = useState(false);
  const [serviceError, setServiceError] = useState<string>();
  const [capabilities, setCapabilities] = useState<ServiceCapabilities>();
  const [connectionState, setConnectionState] = useState<ConnectionState>("connecting");
  const [executionStep, setExecutionStep] = useState(0);
  const [editorExpanded, setEditorExpanded] = useState(false);
  const latestCompile = useRef(0);
  const compileController = useRef<AbortController>();
  const capabilityController = useRef<AbortController>();
  const capabilityRequest = useRef<Promise<boolean>>();
  const latestCapabilityRequest = useRef(0);
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
  const optionsKey = JSON.stringify(options);
  const latestOptionsKey = useRef(optionsKey);
  latestOptionsKey.current = optionsKey;
  const serviceReady = connectionState === "ready" && capabilities?.ready === true;
  const supportsMode = (mode: RunMode) => serviceReady && Boolean(capabilities?.capabilities.allowedExecutionModes.includes(mode));
  const executionCapabilityNote = !serviceReady
    ? "Compiler service unavailable. Execution controls remain disabled until readiness succeeds."
    : `Available modes: ${capabilities?.capabilities.allowedExecutionModes.map((mode) => executionModeLabel[mode]).join(", ") ?? "none"}. Disabled modes are not provided by this service policy.`;
  const unavailableModeMessage = (mode: RunMode) => capabilities
    ? `${mode} is unavailable under ${capabilities.executionPolicy}.`
    : "The compiler service is unavailable. Retry after it finishes starting.";
  const markServiceUnavailable = useCallback((message: string) => {
    setCapabilities(undefined);
    setConnectionState("unavailable");
    setServiceError(message);
  }, []);
  const refreshCapabilities = useCallback((recovering = false, replace = false): Promise<boolean> => {
    if (capabilityRequest.current && !replace) return capabilityRequest.current;
    if (replace) capabilityController.current?.abort();
    const requestId = ++latestCapabilityRequest.current;
    const controller = new AbortController();
    capabilityController.current = controller;
    setConnectionState(recovering ? "recovering" : "connecting");
    const request = getCapabilities(controller.signal)
      .then((next) => {
        if (requestId !== latestCapabilityRequest.current || controller.signal.aborted) return false;
        setCapabilities(next);
        setConnectionState(next.ready ? "ready" : "unavailable");
        setServiceError(undefined);
        return next.ready;
      })
      .catch((error) => {
        if (requestId !== latestCapabilityRequest.current || controller.signal.aborted) return false;
        markServiceUnavailable(error instanceof Error ? error.message : "The compiler service could not be reached.");
        return false;
      })
      .finally(() => {
        if (requestId === latestCapabilityRequest.current) capabilityRequest.current = undefined;
      });
    capabilityRequest.current = request;
    return request;
  }, [markServiceUnavailable]);
  useEffect(() => {
    void refreshCapabilities();
    return () => {
      ++latestCapabilityRequest.current;
      capabilityController.current?.abort();
    };
  }, [refreshCapabilities]);
  const doCompile = async () => {
    const requestId = ++latestCompile.current;
    compileController.current?.abort();
    const controller = new AbortController();
    compileController.current = controller;
    setBusy(true);
    setResult(undefined);
    setRunResult(undefined);
    setCorrectness(undefined);
    setComparison(undefined);
    setBenchmark(undefined);
    setBenchmarkError(undefined);
    setServiceError(undefined);
    try {
      const response = await compile(options, "/api/v1/compile", controller.signal);
      if (requestId !== latestCompile.current) return;
      setResult(response);
      if (
        response.snapshots.length &&
        !response.snapshots.some((snapshot) => snapshot.stageId === selectedStage)
      )
        setSelectedStage(response.snapshots.at(-1)!.stageId);
    } catch (error) {
      if (controller.signal.aborted || requestId !== latestCompile.current) return;
      markServiceUnavailable(error instanceof Error ? error.message : "The compiler service could not be reached.");
    } finally {
      if (requestId === latestCompile.current) setBusy(false);
    }
  };
  useEffect(() => {
    const handle = window.setTimeout(() => {
      void doCompile();
    }, 450);
    return () => {
      window.clearTimeout(handle);
      compileController.current?.abort();
    };
  }, [source, level, policy, custom.join(",")]);
  const doRun = async (mode: RunMode) => {
    const requestKey = optionsKey;
    if (!supportsMode(mode)) {
      setServiceError(unavailableModeMessage(mode));
      return;
    }
    setBusy(true);
    setExecutionStep(1);
    setServiceError(undefined);
    try {
      const response = await run(options, mode);
      if (requestKey !== latestOptionsKey.current) return;
      setRunResult(response);
      [2, 3, 4].forEach((step, index) =>
        window.setTimeout(() => setExecutionStep(step), 260 * (index + 1)),
      );
    } catch {
      if (requestKey !== latestOptionsKey.current) return;
      setExecutionStep(0);
      markServiceUnavailable('Compiler service unavailable. Retry after it finishes starting.');
    } finally {
      setBusy(false);
    }
  };
  const doCorrectness = async () => {
    const requestKey = optionsKey;
    setBusy(true);
    setServiceError(undefined);
    try {
      const response = await checkCorrectness(options);
      if (requestKey !== latestOptionsKey.current) return;
      setCorrectness(response);
    } catch {
      if (requestKey !== latestOptionsKey.current) return;
      markServiceUnavailable('Compiler service unavailable. Retry after it finishes starting.');
    } finally {
      setBusy(false);
    }
  };
  const doCompare = async () => {
    const requestKey = optionsKey;
    setBusy(true);
    setServiceError(undefined);
    try {
      const results = await Promise.all(
          [0, 1, 2, 3].map((optimizationLevel) =>
            compile({ ...options, optimizationLevel }),
          ),
        );
      if (requestKey !== latestOptionsKey.current) return;
      setComparison(results);
    } catch {
      if (requestKey !== latestOptionsKey.current) return;
      markServiceUnavailable('Compiler service unavailable. Retry after it finishes starting.');
    } finally {
      setBusy(false);
    }
  };
  const doBenchmark = async () => {
    const requestKey = optionsKey;
    setBusy(true);
    setBenchmark(undefined);
    setBenchmarkError(undefined);
    setServiceError(undefined);
    try {
      const compilation = await compile(options);
      if (requestKey !== latestOptionsKey.current) return;
      if (!compilation.success) {
        setBenchmarkError({ phase: "compilation", message: compilation.diagnostics[0]?.message ?? "The compiler could not prepare this program for measurement." });
        return;
      }
      const warmup = await run(options, "ssa");
      if (requestKey !== latestOptionsKey.current) return;
      if (!warmup.success) {
        setBenchmarkError({ phase: "warmup", message: warmup.stderr || warmup.errorKind || "The SSA interpreter could not complete the warmup." });
        return;
      }
      const samples: number[] = [];
      for (let index = 0; index < 5; ++index) {
        const measured = await run(options, "ssa");
        if (requestKey !== latestOptionsKey.current) return;
        if (!measured.success || measured.executionDurationUs === undefined) {
          setBenchmarkError({ phase: "measurement", message: measured.stderr || measured.errorKind || `Sample ${index + 1} did not produce an execution duration.` });
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
        compilationCached: compilation.clientCached === true,
      });
    } catch {
      if (requestKey !== latestOptionsKey.current) return;
      setBenchmarkError({ phase: "service", message: "The compiler service could not complete the benchmark request." });
      markServiceUnavailable('Compiler service unavailable. Retry after it finishes starting.');
    } finally {
      if (requestKey === latestOptionsKey.current) setBusy(false);
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
        selectPolicy(argument);
        return { tone: "success", lines: [`Selected ${argument} policy. ${argument === "default" ? "The compiler will re-evaluate the current source." : "Custom passes were cleared before the compiler re-evaluates the current source."}`, argument === "learned" ? "Scope is bounded to reviewed inline profitability decisions; the verifier remains mandatory." : ""] .filter(Boolean) };
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
        if (!supportsMode(mode)) return { tone: "error", lines: [unavailableModeMessage(mode)] };
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
        return { tone: response.equivalent ? "success" : "error", lines: response.checks.map((check) => `${check.status === "passed" ? "✓" : check.status === "failed" ? "×" : "-"} ${check.stage} [${check.status}]: ${check.detail}`) };
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
        const measurement = { compileUs: compilation.telemetry.totalCompileUs, samplesUs: samples, medianUs: ordered[Math.round((ordered.length - 1) * .5)], p25Us: ordered[Math.round((ordered.length - 1) * .25)], p75Us: ordered[Math.round((ordered.length - 1) * .75)], compilationCached: compilation.clientCached === true };
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
    // Persisting to the URL is the source of truth. Clipboard access is a
    // convenience and can be denied in embedded or headless environments.
    try {
      await navigator.clipboard?.writeText(window.location.href);
    } catch {
      // The session remains shareable through the updated URL.
    }
  };
  const movePass = (index: number, direction: -1 | 1) =>
    setCustom((current) => {
      const target = index + direction;
      if (target < 0 || target >= current.length) return current;
      const next = [...current];
      [next[index], next[target]] = [next[target], next[index]];
      return next;
    });
  const selectPolicy = (next: "default" | "heuristic" | "learned") => {
    if (next !== "default") setCustom([]);
    setPolicy(next);
  };
  const toggleCustomPass = (pass: string) => {
    if (!custom.includes(pass)) setPolicy("default");
    setCustom((current) => current.includes(pass) ? current.filter((item) => item !== pass) : [...current, pass]);
  };
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
          onChange={(event) => selectPolicy(event.target.value as typeof policy)}
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
                onChange={() => toggleCustomPass(pass)}
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
        {connectionState === "recovering"
          ? "recovering compiler service…"
          : connectionState === "unavailable"
          ? "compiler service unavailable"
          : busy
          ? result
            ? "updating compiler facts…"
            : "starting compiler service…"
          : result
          ? result.success
            ? `verified · ${result.telemetry.totalCompileUs} μs`
            : "diagnostics reported"
          : capabilities?.ready
          ? "ready to compile"
          : "waiting for service"}
      </span>
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
            <Suspense fallback={<div className="editor-loading" role="status">Preparing source editor…</div>}>
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
            </Suspense>
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
          <Suspense fallback={<div className="graph-loading" role="status">Preparing graph explorer…</div>}>
            <GraphPane
              graph={graph}
              profile={runResult?.runtimeProfile}
              selectedBlock={selectedBlock}
              onSelect={(id) => {
                const match = /bb(\d+)$/.exec(id);
                setSelectedBlock(match ? Number(match[1]) : undefined);
              }}
            />
          </Suspense>
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
          <span className="execution-capability-note" id="execution-capability-note">{executionCapabilityNote}</span>
        </div>
        <div>
          {(["core", "ssa", "jit", "adaptive-jit"] as RunMode[]).map((mode) => (
            <button key={mode} aria-describedby="execution-capability-note" disabled={busy || !supportsMode(mode)} title={!supportsMode(mode) ? unavailableModeMessage(mode) : undefined} onClick={() => void doRun(mode)}>{mode}</button>
          ))}
          <button disabled={busy || !serviceReady} onClick={() => void doCompare()}>Compare O-levels</button>
          <button disabled={busy || !serviceReady} onClick={() => void doBenchmark()}>Benchmark</button>
          <button className="verification-primary" disabled={busy || !serviceReady} onClick={() => void doCorrectness()}>check correctness</button>
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
            {runResult ? <pre className="output">{[`Execution mode: ${executionModeLabel[runResult.executionMode]}${runResult.executionModeReported === false ? ' (requested; legacy service did not echo mode)' : ''}`, `Status: ${runResult.status}`, runResult.executionDurationUs !== undefined ? `Execution duration: ${runResult.executionDurationUs} μs` : "", runResult.stdout ? `Standard output:\n${runResult.stdout.trimEnd()}` : "Standard output: (no output)", runResult.stderr ? `Diagnostic: ${runResult.stderr}` : "", runResult.errorKind !== "none" ? `Error category: ${runResult.errorKind}` : "", runResult.returnValue ? `Return value: ${runResult.returnValue}` : "", ...((runResult.adaptiveEvents ?? []).map((event) => `${event.kind} @${event.function}${event.detail ? ` · ${event.detail}` : ""}`))].filter(Boolean).join("\n\n")}</pre> : <p className="quiet">Use the terminal command <code>run ssa</code>, or choose an available engine above. Execution details appear after a real run.</p>}
          </div>
        </div>
        <Correctness result={correctness} disabled={busy || !serviceReady} onRun={() => void doCorrectness()} />
        <OptimizationComparison results={comparison} />
        <BenchmarkPanel result={benchmark} error={benchmarkError} disabled={busy || !serviceReady} onMeasure={() => void doBenchmark()} />
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
          <span><strong>NanoPass</strong><small>Interactive compiler</small></span>
        </a>
        <nav className="rail-nav" aria-label="Lab views">
          <span className="rail-label">Navigate</span>
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
          <b><i /> {result?.success ? "Verifier passed" : serviceError ? "Service disconnected" : capabilities?.ready ? "Compiler ready" : "Connecting"}</b>
          <small>{result ? `${result.telemetry.functions} function${result.telemetry.functions === 1 ? "" : "s"} · ${result.telemetry.blocks} blocks` : capabilities?.executionPolicy ? capabilities.executionPolicy : "Awaiting service"}</small>
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
          {connectionState === "unavailable" && <button className="service-retry topbar-retry" onClick={() => void refreshCapabilities(true, true)}>Retry service</button>}
          <button
            className="run"
            disabled={busy || !supportsMode("ssa")}
            title={!supportsMode("ssa") ? unavailableModeMessage("ssa") : undefined}
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
