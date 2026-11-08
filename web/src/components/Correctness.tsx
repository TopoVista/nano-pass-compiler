import type { CorrectnessResult } from '../lib/api';

export function Correctness({ result, onRun }: { result?: CorrectnessResult; onRun?: () => void }) {
  return <section className="correctness-panel" aria-label="Differential correctness">
    <header><strong>Differential correctness</strong><span>{result ? result.equivalent ? 'EQUIVALENT' : 'MISMATCH' : 'not checked'}</span></header>
    {!result ? <div className="proof-empty"><div className="proof-path" aria-label="Independent execution paths compared"><i><small>reference</small>CoreIR</i><b>≡</b><i><small>baseline</small>SSA O0</i><b>≡</b><i><small>optimized</small>SSA O3</i><b>≡</b><i><small>backend</small>Native</i></div><p>Runs the same program through independent execution paths, then compares the observable result. This is an empirical check—not a formal proof.</p><button onClick={onRun}>Run correctness check</button></div> : <div className="proof-results">{result.checks.map((check) => <article key={check.stage} className={check.passed ? 'check passed' : 'check failed'}><b><i>{check.passed ? '✓' : '×'}</i>{check.stage}</b><small>{check.detail}</small></article>)}</div>}
  </section>;
}
