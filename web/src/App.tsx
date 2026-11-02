import { useEffect, useMemo, useState } from 'react';
import { CodeView } from './components/CodeView';
import { GraphPane } from './components/GraphPane';
import { Diagnostics, Profile, Remarks, Timeline, ValueInspector } from './components/Panels';
import { Correctness } from './components/Correctness';
import { OptimizationComparison } from './components/Comparison';
import { PolicyFeatures } from './components/PolicyFeatures';
import { SourceEditor } from './components/SourceEditor';
import { allowedPasses, checkCorrectness, compile, run, type Compilation, type CorrectnessResult, type RunResult } from './lib/api';
import './styles.css';

const examples: Record<string, string> = {
  'Factorial': 'int fact(int n) { if (n <= 1) { return 1; } return n * fact(n - 1); }\nint main() { int n = 5; print(fact(n)); return 0; }',
  'SCCP branch': 'int main() { int x = 4; if (true) { x = x + 3; } else { x = 99; } print(x); return x; }',
  'LICM': 'int main() { int i = 0; int total = 0; while (i < 4) { total = total + 2 * 3; i = i + 1; } print(total); return total; }',
  'Inlining': 'int choose(int x) { if (x > 0) { return 7; } return 3; }\nint main() { int y = choose(1); print(y); return y; }',
  'Arrays': 'int main() { int values[3]; values[0] = 2; values[1] = 3; values[2] = values[0] + values[1]; print(values[2]); return values[2]; }',
  'Nested loops': 'int main() { int i = 0; int total = 0; while (i < 3) { int j = 0; while (j < 2) { total = total + i + j; j = j + 1; } i = i + 1; } print(total); return total; }',
  'Why not LICM': 'int main() { int i = 0; int total = 0; while (i < 3) { total = total + i / (i + 1); i = i + 1; } print(total); return total; }',
  'PGO hot loop': 'int work(int x) { return x * 2 + 1; } int main() { int i = 0; int total = 0; while (i < 20) { total = work(total); i = i + 1; } print(total); return total; }',
  'Adaptive JIT': 'int step(int x) { return x + 1; }\nint main() { int i = 0; int total = 0; while (i < 12) { total = step(total); i = i + 1; } print(total); return total; }'
};
const allArtifacts = { includeSource: true, includeAst: true, includeDesugaredAst: true, includeCore: true, includeAnf: true, includeCfg: true, includeSsa: true, includeOptimizedSsa: true, includeLLVM: true, includeGraphs: true, includeRemarks: true, includeTelemetry: true, includeProvenance: true };
type RunMode = 'core' | 'ssa' | 'jit' | 'adaptive-jit';
type SharedSession = { source?: string; level?: number; stage?: string; passes?: string[]; graph?: string };
const sessionFromHash = (): SharedSession => { try { const raw = window.location.hash.replace(/^#session=/, ''); return raw ? JSON.parse(decodeURIComponent(atob(raw.replace(/-/g, '+').replace(/_/g, '/')))) as SharedSession : {}; } catch { return {}; } };
export default function App() {
  const session = sessionFromHash();
  // Start with a small executable optimization example so the first-run
  // profile view demonstrates a complete compiler/runtime round trip.
  const [source, setSource] = useState(session.source ?? examples['SCCP branch']);
  const [exampleName, setExampleName] = useState(session.source ? '' : 'SCCP branch');
  const [level, setLevel] = useState(session.level ?? 2);
  const [selectedStage, setSelectedStage] = useState(session.stage ?? 'optimized-ssa');
  const [selectedPass, setSelectedPass] = useState<string>();
  const [graphKind, setGraphKind] = useState(session.graph ?? 'cfg');
  const [selectedBlock, setSelectedBlock] = useState<number>();
  const [selectedValue, setSelectedValue] = useState<number>();
  const [focusLine, setFocusLine] = useState<number>();
  const [custom, setCustom] = useState<string[]>(() => (session.passes ?? []).filter((pass) => allowedPasses.includes(pass)));
  const [result, setResult] = useState<Compilation>();
  const [runResult, setRunResult] = useState<RunResult>();
  const [correctness, setCorrectness] = useState<CorrectnessResult>();
  const [comparison, setComparison] = useState<Compilation[]>();
  const [busy, setBusy] = useState(false);
  const options = useMemo(() => ({ source, optimizationLevel: level, ...(custom.length ? { passes: custom } : {}), ...allArtifacts }), [source, level, custom]);
  const doCompile = async () => { setBusy(true); setRunResult(undefined); setCorrectness(undefined); const response = await compile(options); setResult(response); if (response.snapshots.length && !response.snapshots.some((snapshot) => snapshot.stageId === selectedStage)) setSelectedStage(response.snapshots.at(-1)!.stageId); setBusy(false); };
  useEffect(() => { const handle = window.setTimeout(() => { void doCompile(); }, 450); return () => window.clearTimeout(handle); }, [source, level, custom.join(',')]);
  const doRun = async (mode: RunMode) => { setBusy(true); setRunResult(await run(options, mode)); setBusy(false); };
  const doCorrectness = async () => { setBusy(true); setCorrectness(await checkCorrectness(options)); setBusy(false); };
  const doCompare = async () => { setBusy(true); setComparison(await Promise.all([0, 1, 2, 3].map((optimizationLevel) => compile({ ...options, optimizationLevel })))); setBusy(false); };
  const share = async () => { const encoded = btoa(encodeURIComponent(JSON.stringify({ source, level, stage: selectedStage, passes: custom, graph: graphKind }))).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, ''); window.location.hash = `session=${encoded}`; await navigator.clipboard?.writeText(window.location.href); };
  const movePass = (index: number, direction: -1 | 1) => setCustom((current) => {
    const target = index + direction; if (target < 0 || target >= current.length) return current;
    const next = [...current]; [next[index], next[target]] = [next[target], next[index]]; return next;
  });
  const snapshot = result?.snapshots.find((item) => item.stageId === selectedStage) ?? result?.snapshots.at(-1);
  const graph = result?.graphs.find((item) => item.kind === graphKind) ?? result?.graphs[0];
  const value = result?.values.find((item) => item.value === selectedValue);
  useEffect(() => { if (value && value.line > 0) setFocusLine(value.line); }, [value]);
  const transformKey = selectedPass?.replace(/@[^@]+$/, '');
  const transform = transformKey ? result?.transformations.find((item) => item.passId === transformKey) : undefined;
  return <main className="lab-shell">
    <header className="topbar"><div className="brand"><span className="brand-mark">⊞</span><strong>NanoPass</strong><span>Compiler Lab</span></div><nav aria-label="Lab views"><button className="active">Lab</button><button onClick={() => setGraphKind('cfg')}>Graphs</button><button onClick={() => setSelectedStage('optimized-ssa')}>Optimizations</button></nav><div className="top-actions"><label>O-level <select aria-label="Optimization level" value={level} onChange={(event) => setLevel(Number(event.target.value))}>{[0,1,2,3].map((item) => <option key={item} value={item}>O{item}</option>)}</select></label><button onClick={share}>Share session</button><button className="run" disabled={busy} onClick={() => void doRun('ssa')}>{busy ? 'Compiling…' : 'Run SSA'}</button></div></header>
    <section className="toolbar"><label>Example <select aria-label="Example program" value={exampleName} onChange={(event) => { if (event.target.value) { setExampleName(event.target.value); setSource(examples[event.target.value]); } }}>{exampleName === '' && <option value="">Custom source</option>}{Object.keys(examples).map((name) => <option key={name} value={name}>{name}</option>)}</select></label><div className="pipeline-picker"><span>Custom pipeline</span>{allowedPasses.map((pass) => <label key={pass}><input type="checkbox" checked={custom.includes(pass)} onChange={() => setCustom((current) => current.includes(pass) ? current.filter((item) => item !== pass) : [...current, pass])} />{pass}</label>)}{custom.length > 1 && <div className="pipeline-order" aria-label="Custom pipeline order">{custom.map((pass, index) => <span key={pass}>{pass}<button aria-label={`Move ${pass} earlier`} disabled={index === 0} onClick={() => movePass(index, -1)}>↑</button><button aria-label={`Move ${pass} later`} disabled={index === custom.length - 1} onClick={() => movePass(index, 1)}>↓</button></span>)}</div>}</div><button disabled={busy} onClick={() => void doCompare()}>Compare O0–O3</button><span className={result?.success ? 'compile-status good' : 'compile-status'}>{result ? result.success ? `verified · ${result.telemetry.totalCompileUs} μs` : 'diagnostics reported' : 'waiting'}</span></section>
    <section className="workspace">
      <div className="panel source-panel"><header><strong>Source</strong><span>Nano</span></header><div className="editor"><SourceEditor value={source} focusLine={focusLine} onCursorLine={(line) => { const related = result?.values.find((candidate) => candidate.line === line); if (related) setSelectedValue(related.value); }} onChange={(next) => { setSource(next); setExampleName(''); }} /></div></div>
      <div className="panel ir-panel"><header><div><strong>Representation</strong><span>{snapshot?.displayName ?? 'waiting'}</span></div><select aria-label="IR stage" value={snapshot?.stageId ?? ''} onChange={(event) => setSelectedStage(event.target.value)}>{result?.snapshots.map((item) => <option key={item.stageId} value={item.stageId}>{item.displayName}</option>)}</select></header><CodeView snapshot={snapshot} selectedBlock={selectedBlock} selectedValue={value} onValue={setSelectedValue} onBlock={setSelectedBlock} /></div>
      <div className="panel timeline-panel"><header><strong>Pass timeline</strong><span>compiler facts</span></header><Timeline passes={result?.passes ?? []} selected={selectedPass} onSelect={setSelectedPass} /></div>
      <div className="panel graph-panel"><header><div><strong>Graph explorer</strong><span>{graph?.function ? `@${graph.function}` : 'no graph'}</span></div><select aria-label="Graph kind" value={graphKind} onChange={(event) => setGraphKind(event.target.value)}>{['cfg','dominator-tree','loops','call-graph'].map((kind) => <option key={kind}>{kind}</option>)}</select></header><GraphPane graph={graph} profile={runResult?.runtimeProfile} selectedBlock={selectedBlock} onSelect={(id) => { const match = /bb(\d+)$/.exec(id); setSelectedBlock(match ? Number(match[1]) : undefined); }} /></div>
      <aside className="inspector"><ValueInspector value={value} /><Remarks remarks={result?.remarks ?? []} /></aside>
      <div className="panel diagnostics-panel"><Diagnostics diagnostics={result?.diagnostics ?? []} onSelect={setFocusLine} /></div>
      <div className="panel runtime-panel"><div className="run-controls"><strong>Execution</strong>{(['core','ssa','jit','adaptive-jit'] as RunMode[]).map((mode) => <button key={mode} disabled={busy} onClick={() => void doRun(mode)}>{mode}</button>)}<button disabled={busy} onClick={() => void doCorrectness()}>check correctness</button></div><Profile profile={runResult?.runtimeProfile} stdout={runResult?.stdout} mode={runResult ? runResult.status : 'not run'} status={runResult?.errorKind} events={runResult?.adaptiveEvents} /></div>
      <div className="panel correctness-wrap"><Correctness result={correctness} /></div>
      <div className="panel comparison-wrap"><OptimizationComparison results={comparison} /></div>
      <div className="panel policy-wrap"><PolicyFeatures features={runResult?.policyFeatures ?? result?.policyFeatures} /></div>
      {transform && selectedPass && <div className="panel diff-panel"><header><strong>Before / after</strong><span>{selectedPass}</span></header><div className="diff"><pre>{transform.before}</pre><pre>{transform.after}</pre></div></div>}
    </section>
  </main>;
}
