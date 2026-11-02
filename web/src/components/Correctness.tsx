import type { CorrectnessResult } from '../lib/api';

export function Correctness({ result }: { result?: CorrectnessResult }) {
  return <section className="correctness-panel" aria-label="Differential correctness">
    <header><strong>Differential correctness</strong><span>{result ? result.equivalent ? 'EQUIVALENT' : 'MISMATCH' : 'not checked'}</span></header>
    {!result ? <p className="quiet">Compare the CoreIR reference result with SSA O0–O3 and native modes.</p> : result.checks.map((check) => <article key={check.stage} className={check.passed ? 'check passed' : 'check failed'}><b>{check.passed ? 'PASS' : 'FAIL'} · {check.stage}</b><small>{check.detail}</small></article>)}
  </section>;
}
