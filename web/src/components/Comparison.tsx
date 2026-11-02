import type { Compilation } from '../lib/api';

export function OptimizationComparison({ results }: { results?: Compilation[] }) {
  return <section className="comparison-panel" aria-label="Optimization level comparison"><header><strong>O-level comparison</strong><span>{results ? 'compiler measurements' : 'not compared'}</span></header>{!results ? <p className="quiet">Compile O0–O3 to compare actual SSA size, block count, and compiler time.</p> : <table><thead><tr><th>Level</th><th>Instructions</th><th>Blocks</th><th>Values</th><th>Compile μs</th></tr></thead><tbody>{results.map((result, index) => <tr key={index}><td>O{index}</td><td>{result.passes.reduce((sum, pass) => sum + pass.instructionsAfter, 0) || '—'}</td><td>{result.telemetry.blocks}</td><td>{result.telemetry.values}</td><td>{result.telemetry.totalCompileUs}</td></tr>)}</tbody></table>}</section>;
}
