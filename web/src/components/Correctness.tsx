import type { CorrectnessResult } from '../lib/api';

export function Correctness({ result, onRun, disabled = false }: { result?: CorrectnessResult; onRun?: () => void; disabled?: boolean }) {
  const label = (status: CorrectnessResult['checks'][number]['status']) => status === 'passed' ? 'passed' : status === 'failed' ? 'failed' : status;
  return <section className="correctness-panel" aria-label="Differential correctness">
    <header><strong>Differential correctness</strong><span>{result ? result.equivalent ? 'EQUIVALENT' : 'MISMATCH' : 'not checked'}</span></header>
    {!result ? <div className="proof-empty"><div className="proof-path" aria-label="Independent execution paths compared"><i><small>reference</small>CoreIR</i><b>≡</b><i><small>baseline</small>SSA O0</i><b>≡</b><i><small>optimized</small>SSA O3</i></div><p>Runs the available independent execution paths, then compares their observable result. Native checks are shown as unavailable or skipped when they were not executed.</p><button disabled={disabled} onClick={onRun}>Run correctness check</button></div> : <div className="proof-results">{result.checks.map((check) => <article key={check.stage} className={`check ${check.status}`}><b><i>{check.status === 'passed' ? '✓' : check.status === 'failed' ? '×' : '−'}</i>{check.stage}<em>{label(check.status)}</em></b><small>{check.detail}</small></article>)}</div>}
  </section>;
}

