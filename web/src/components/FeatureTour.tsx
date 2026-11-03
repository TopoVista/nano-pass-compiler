import { useState } from 'react';

type Lens = 'pipeline' | 'graphs' | 'evidence';
type Props = {
  onExample: (name: string) => void;
  onStage: (stage: string) => void;
  onGraph: (kind: string) => void;
};

const lenses: Record<Lens, { eyebrow: string; title: string; body: string; action: string; stage?: string; graph?: string; example: string }> = {
  pipeline: { eyebrow: '01 / IR journey', title: 'See every representation.', body: 'Move from source through ANF, CFG and SSA using compiler-generated snapshots—not illustrations.', action: 'Open SSA', stage: 'ssa', example: 'SCCP branch' },
  graphs: { eyebrow: '02 / control flow', title: 'Make flow visible.', body: 'Inspect real basic blocks, dominance and loop structure. Selecting a node focuses the matching IR block.', action: 'Open CFG', graph: 'cfg', example: 'Nested loops' },
  evidence: { eyebrow: '03 / proof layer', title: 'Ask why a pass changed code.', body: 'Pass events, verifier gates and optimization remarks stay tied to facts emitted by the compiler service.', action: 'Inspect optimization', stage: 'optimized-ssa', example: 'Why not LICM' }
};

export function FeatureTour({ onExample, onStage, onGraph }: Props) {
  const [lens, setLens] = useState<Lens>('pipeline');
  const selected = lenses[lens];
  const open = () => {
    onExample(selected.example);
    if (selected.stage) onStage(selected.stage);
    if (selected.graph) onGraph(selected.graph);
    // Guide is intentionally a separate endpoint; open the live workbench next.
    window.location.assign('/lab');
  };
  return <section className="feature-tour" id="feature-tour" aria-labelledby="feature-tour-title">
    <div className="feature-intro">
      <p className="eyebrow">NanoPass / interactive field guide</p>
      <h2 id="feature-tour-title">A compiler lab, not a black box.</h2>
      <p>Choose a lens, load a real example, then follow the same source through the live compiler pipeline.</p>
    </div>
    <div className="feature-console">
      <div className="feature-tabs" role="tablist" aria-label="Compiler lab features">
        {(Object.keys(lenses) as Lens[]).map((key) => <button key={key} role="tab" aria-selected={lens === key} className={lens === key ? 'selected' : ''} onClick={() => setLens(key)}>{lenses[key].eyebrow}</button>)}
      </div>
      <article className="feature-detail" aria-live="polite">
        <span className="feature-index">{selected.eyebrow}</span>
        <h3>{selected.title}</h3>
        <p>{selected.body}</p>
        <button className="feature-action" onClick={open}>{selected.action}<span aria-hidden="true">↗</span></button>
      </article>
      <div className="feature-trace" aria-label="Compiler pipeline overview">
        {['Source', 'Typed AST', 'CoreIR', 'ANF', 'CFG', 'SSA', 'Optimized SSA', 'LLVM'].map((stage, index) => <div key={stage} className={index === 5 || index === 6 ? 'trace-node active' : 'trace-node'}><span>{String(index + 1).padStart(2, '0')}</span><b>{stage}</b></div>)}
      </div>
    </div>
  </section>;
}
