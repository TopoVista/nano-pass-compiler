import { useEffect, useState } from 'react';
import { ArrowRight, Braces, Check, GitBranch, Network, Pause, Play, RotateCcw, ShieldCheck, Workflow } from 'lucide-react';
import { BuildAtlas } from './BuildAtlas';

type Lens = 'pipeline' | 'graphs' | 'evidence';
type Props = { onExample: (name: string) => void; onStage: (stage: string) => void; onGraph: (kind: string) => void; };
type GuideStage = { id: string; title: string; short: string; stage: string; source: string; output: string; takeaway: string; question: string; facts: string[]; };

const guideStages: GuideStage[] = [
  { id: 'source', title: 'Source', short: 'what you wrote', stage: 'source', source: 'int score(int x) {\n  int bias = 4;\n  if (x > 0) return x * bias;\n  return bias;\n}', output: 'tokens\nint · score · ( · int · x · )\n{ · int · bias · = · 4 · ;\nif · ( · x · > · 0 · ) …', takeaway: 'The language begins as readable C-like source. Nothing has been optimized or guessed yet.', question: 'Can the compiler understand every name and operator?', facts: ['Lexical tokens preserve source spans', 'Parser builds a structural AST', 'Diagnostics point back to source'] },
  { id: 'typed-ast', title: 'Typed AST', short: 'what it means', stage: 'ast', source: 'score(x)\n  ├─ declare bias : int\n  └─ if (x > 0)\n       ├─ return x * bias\n       └─ return bias', output: 'Function score : (int) → int\n  Local bias : int\n  Compare gt : bool\n  Multiply int × int : int', takeaway: 'Names resolve to symbols and every expression receives a type before lowering continues.', question: 'Is this program meaningful before we transform it?', facts: ['Scopes resolve identifiers', 'Types make invalid operations visible', 'Errors remain user-facing diagnostics'] },
  { id: 'core', title: 'Core + ANF', short: 'make order explicit', stage: 'anf', source: 'return x * bias;\n\n// convenient surface syntax', output: '%0 = load x\n%1 = load bias\n%2 = mul %0, %1\nreturn %2', takeaway: 'The compiler reduces convenient syntax and gives every non-trivial calculation a named, ordered result.', question: 'What must happen first, and what value does it produce?', facts: ['Surface shortcuts disappear', 'Evaluation order is explicit', 'Temporary values are typed'] },
  { id: 'cfg', title: 'CFG', short: 'show possible paths', stage: 'cfg', source: 'if (x > 0)\n  return x * bias;\nreturn bias;', output: 'bb0.entry\n  branch %cond, bb1, bb2\n\nbb1.positive → return %2\nbb2.fallback → return %1', takeaway: 'Basic blocks and arrows make every route through the function inspectable.', question: 'Which blocks can execute, and where can control go next?', facts: ['Every block ends in a terminator', 'Predecessors and successors agree', 'Reachability becomes measurable'] },
  { id: 'ssa', title: 'SSA', short: 'trace each value once', stage: 'ssa', source: 'bias = 4;\nif (x > 0) return x * bias;\nreturn bias;', output: '%0 = const 4\n%1 = cmp.gt %arg0, 0\nbranch %1, bb1, bb2\n\nbb1: %2 = mul %arg0, %0\nbb2: return %0', takeaway: 'Each SSA value is defined once. Uses, dominance, and merge points can now be checked mechanically.', question: 'Where did this value come from, and is it valid on this path?', facts: ['Unique value definitions', 'Phi nodes merge branch values', 'Verifier checks dominance'] },
  { id: 'optimize', title: 'Optimize + verify', short: 'rewrite with evidence', stage: 'optimized-ssa', source: '%7 = add %x, 0\n%8 = mul 2, 3\nbranch true, bb1, bb2', output: '%8 = const 6\njump bb1\n\nremarks\nAPPLIED constant-fold\nAPPLIED simplify-cfg', takeaway: 'Passes apply only legal rewrites and record what changed. The verifier runs after structural work.', question: 'Is a smaller program still the same program?', facts: ['Passes record applied and missed reasons', 'SSA/CFG invariants are rechecked', 'No LLM decides correctness'] },
];

const lenses: Record<Lens, { eyebrow: string; title: string; body: string; action: string; stage?: string; graph?: string; example: string }> = {
  pipeline: { eyebrow: 'IR journey', title: 'See every representation.', body: 'Open a compiler-generated snapshot and follow source through ANF, CFG and SSA instead of trusting an illustration.', action: 'Open SSA', stage: 'ssa', example: 'SCCP branch' },
  graphs: { eyebrow: 'Control flow', title: 'Make flow visible.', body: 'Inspect real basic blocks, dominance and loop structure. Selecting a node focuses the matching IR block.', action: 'Open CFG', graph: 'cfg', example: 'Nested loops' },
  evidence: { eyebrow: 'Proof layer', title: 'Ask why a pass changed code.', body: 'Pass events, verifier gates and optimization remarks stay tied to facts emitted by the compiler service.', action: 'Inspect optimization', stage: 'optimized-ssa', example: 'Why not LICM' },
};

const architectureNodes = [
  { id: 'browser', label: 'Browser workbench', detail: 'The editor sends source plus allowlisted options. It only displays structured compiler facts.', emits: 'POST /compile · source text · optimization options', guarantee: 'No shell command is constructed from source text.' },
  { id: 'gateway', label: 'Request guard', detail: 'The local service bounds input, output, timeout and the option set before the compiler sees a request.', emits: 'bounded request · explicit capability state', guarantee: 'Untrusted source never becomes a host command.' },
  { id: 'frontend', label: 'Frontend + typed IR', detail: 'Lexer, parser, resolver and type checker turn syntax into a meaningful, typed program.', emits: 'AST · typed AST · diagnostics', guarantee: 'Names and types are checked before middle-end lowering.' },
  { id: 'middle', label: 'Middle end', detail: 'CoreIR, ANF, CFG, analyses and SSA make control flow and values explicit.', emits: 'CFG · dominators · SSA · remarks', guarantee: 'Implemented stages expose explicit invariants and stable textual forms.' },
  { id: 'backend', label: 'Backend boundary', detail: 'LLVM lowering is available to AOT/JIT only when the current build enables it.', emits: 'LLVM IR · execution result · profile', guarantee: 'Unavailable engines report unavailable; they are never simulated.' },
] as const;

export function FeatureTour({ onExample, onStage, onGraph }: Props) {
  const [lens, setLens] = useState<Lens>('pipeline');
  const [activeStep, setActiveStep] = useState(0);
  const [playing, setPlaying] = useState(false);
  const [architectureFocus, setArchitectureFocus] = useState<(typeof architectureNodes)[number]['id']>('middle');
  const selected = lenses[lens];
  const stage = guideStages[activeStep];
  const focusedArchitecture = architectureNodes.find((item) => item.id === architectureFocus) ?? architectureNodes[0];

  useEffect(() => {
    if (!playing) return undefined;
    const timer = window.setTimeout(() => setActiveStep((current) => {
      if (current === guideStages.length - 1) { setPlaying(false); return current; }
      return current + 1;
    }), 2400);
    return () => window.clearTimeout(timer);
  }, [activeStep, playing]);

  const open = () => {
    onExample(selected.example);
    if (selected.stage) onStage(selected.stage);
    if (selected.graph) onGraph(selected.graph);
    window.location.assign('/lab');
  };
  const openStage = () => { onExample('SCCP branch'); onStage(stage.stage); window.location.assign('/lab'); };
  const replay = () => { setActiveStep(0); setPlaying(true); };

  return <section className="feature-tour guide-deck" id="feature-tour" aria-labelledby="feature-tour-title">
    <header className="guide-hero">
      <div><p className="eyebrow">NanoPass / animated field guide</p><h2 id="feature-tour-title">A compiler lab,<br /><em>presented as a system.</em></h2><p>Use this walkthrough as a demo: the program changes form, the graph becomes visible, and each verification claim says exactly what it checks.</p></div>
      <div className="guide-hero-facts" aria-label="Guide contents"><span><Braces aria-hidden="true" /> representation journey</span><span><GitBranch aria-hidden="true" /> control-flow diagram</span><span><ShieldCheck aria-hidden="true" /> verifier evidence</span></div>
    </header>

    <section className="guide-presentation" aria-labelledby="guide-demo-title">
      <header className="guide-section-header"><div><span>01 / live walkthrough</span><h3 id="guide-demo-title">Watch a program acquire structure.</h3></div><div className="guide-play-controls"><button type="button" className="guide-icon-button" onClick={replay} aria-label="Replay compiler walkthrough"><RotateCcw aria-hidden="true" /></button><button type="button" className="guide-play-button" onClick={() => setPlaying((current) => !current)}>{playing ? <Pause aria-hidden="true" /> : <Play aria-hidden="true" />}{playing ? 'Pause walkthrough' : 'Play walkthrough'}</button></div></header>
      <div className="guide-stage-rail" role="tablist" aria-label="Compiler walkthrough stages">{guideStages.map((item, index) => <button type="button" key={item.id} role="tab" aria-selected={index === activeStep} className={index === activeStep ? 'active' : index < activeStep ? 'complete' : ''} onClick={() => { setActiveStep(index); setPlaying(false); }}><span>{String(index + 1).padStart(2, '0')}</span><b>{item.title}</b><i>{item.short}</i></button>)}</div>
      <div className="guide-demo-board" key={stage.id}>
        <article className="guide-code-panel guide-code-input"><header><span>INPUT</span><b>{stage.title}</b></header><pre>{stage.source}</pre></article>
        <div className="guide-process-column" aria-label="Current transformation"><div className="guide-pulse" aria-hidden="true"><i /><i /><i /></div><strong>{stage.title}</strong><span>compiler stage</span><ArrowRight aria-hidden="true" /></div>
        <article className="guide-code-panel guide-code-output"><header><span>OUTPUT</span><b>{stage.stage}</b></header><pre>{stage.output}</pre></article>
        <aside className="guide-stage-inspector" aria-live="polite"><span>WHAT THIS STAGE ANSWERS</span><h4>{stage.question}</h4><p>{stage.takeaway}</p><ul>{stage.facts.map((fact) => <li key={fact}><Check aria-hidden="true" />{fact}</li>)}</ul><button type="button" onClick={openStage}>Inspect this in the lab <ArrowRight aria-hidden="true" /></button></aside>
      </div>
    </section>

    <section className="guide-architecture" aria-labelledby="guide-architecture-title">
      <header className="guide-section-header"><div><span>02 / system architecture</span><h3 id="guide-architecture-title">Every boundary says what crosses it.</h3><p>The browser is a presentation layer. The compiler service owns compiler facts. Execution remains a feature-gated boundary.</p></div></header>
      <div className="guide-architecture-layout">
        <div className="guide-system-map" aria-label="Detailed NanoPass request and execution architecture">
          <div className="guide-system-lane browser"><span>DEVELOPER SPACE</span><button type="button" className={architectureFocus === 'browser' ? 'selected' : ''} onClick={() => setArchitectureFocus('browser')}><Braces aria-hidden="true" /><b>Browser workbench</b><small>source · controls · inspection</small></button><i className="guide-system-arrow">allowlisted request <ArrowRight aria-hidden="true" /></i></div>
          <div className="guide-system-lane service"><span>COMPILER SERVICE</span><button type="button" className={architectureFocus === 'gateway' ? 'selected' : ''} onClick={() => setArchitectureFocus('gateway')}><ShieldCheck aria-hidden="true" /><b>Request guard</b><small>limits · timeout · options</small></button><i className="guide-system-arrow"><ArrowRight aria-hidden="true" /></i><button type="button" className={architectureFocus === 'frontend' ? 'selected' : ''} onClick={() => setArchitectureFocus('frontend')}><Network aria-hidden="true" /><b>Frontend + typed IR</b><small>parse · resolve · type</small></button><i className="guide-system-arrow"><ArrowRight aria-hidden="true" /></i><button type="button" className={architectureFocus === 'middle' ? 'selected' : ''} onClick={() => setArchitectureFocus('middle')}><Workflow aria-hidden="true" /><b>Middle end</b><small>Core · ANF · CFG · SSA</small></button></div>
          <div className="guide-system-lane execution"><span>BACKEND / RUNTIME</span><button type="button" className={architectureFocus === 'backend' ? 'selected' : ''} onClick={() => setArchitectureFocus('backend')}><GitBranch aria-hidden="true" /><b>LLVM + execution</b><small>lowering · AOT / ORC when enabled</small></button><i className="guide-return-path">diagnostics · graphs · remarks · telemetry <ArrowRight aria-hidden="true" /></i></div>
        </div>
        <aside className="guide-architecture-inspector" aria-live="polite"><span>SELECTED BOUNDARY</span><h4>{focusedArchitecture.label}</h4><p>{focusedArchitecture.detail}</p><dl><div><dt>EMITS</dt><dd>{focusedArchitecture.emits}</dd></div><div><dt>GUARANTEE</dt><dd>{focusedArchitecture.guarantee}</dd></div></dl></aside>
      </div>
      <ol className="guide-contract-list"><li><span>01</span><b>Input contract</b><p>Source is data, never a host command.</p></li><li><span>02</span><b>IR contract</b><p>Each representation has an explicit invariant.</p></li><li><span>03</span><b>Proof contract</b><p>Remarks are recorded compiler decisions.</p></li><li><span>04</span><b>Execution contract</b><p>Capabilities report their real availability.</p></li></ol>
    </section>

    <BuildAtlas mode="tour" />

    <section className="guide-proof-board" aria-labelledby="guide-proof-title">
      <header className="guide-section-header"><div><span>04 / optimization evidence</span><h3 id="guide-proof-title">A rewrite is a claim with receipts.</h3></div></header>
      <div className="guide-proof-layout"><article className="guide-rewrite-card"><header><span>BEFORE</span><b>constant-fold</b></header><pre>%4 = mul 2, 3\n%5 = add %x, 0\nbranch true, bb1, bb2</pre></article><div className="guide-rewrite-arrow"><ArrowRight aria-hidden="true" /><span>pass runs</span></div><article className="guide-rewrite-card approved"><header><span>AFTER</span><b>SSA preserved</b></header><pre>%4 = const 6\njump bb1</pre><footer><Check aria-hidden="true" />CFG simplified · values still valid</footer></article><aside className="guide-proof-notes"><span>HOW TO EXPLAIN THIS</span><p>“The compiler saw arithmetic with known inputs and a branch that can only go one way. It rewrote both, then checked that the remaining graph and SSA values still obey their rules.”</p><button type="button" onClick={() => { setLens('evidence'); document.querySelector('.guide-deep-dive')?.scrollIntoView({ behavior: 'smooth', block: 'start' }); }}>Show me a recorded remark <ArrowRight aria-hidden="true" /></button></aside></div>
    </section>

    <section className="guide-deep-dive" aria-labelledby="guide-deep-dive-title">
      <header className="guide-section-header"><div><span>05 / choose a live lens</span><h3 id="guide-deep-dive-title">Move from explanation to evidence.</h3></div></header>
      <div className="feature-console"><div className="feature-tabs" role="tablist" aria-label="Compiler lab features">{(Object.keys(lenses) as Lens[]).map((key) => <button type="button" key={key} role="tab" aria-selected={lens === key} className={lens === key ? 'selected' : ''} onClick={() => setLens(key)}>{lenses[key].eyebrow}</button>)}</div><article className="feature-detail" aria-live="polite"><span className="feature-index">{selected.eyebrow}</span><h3>{selected.title}</h3><p>{selected.body}</p><button type="button" className="feature-action" onClick={open}>{selected.action}<ArrowRight aria-hidden="true" /></button></article><div className="feature-trace" aria-label="Compiler pipeline overview">{['Source', 'Typed AST', 'CoreIR', 'ANF', 'CFG', 'SSA', 'Optimized SSA', 'LLVM'].map((stageName, index) => <div key={stageName} className={index === 5 || index === 6 ? 'trace-node active' : 'trace-node'}><span>{String(index + 1).padStart(2, '0')}</span><b>{stageName}</b></div>)}</div></div>
    </section>
  </section>;
}

