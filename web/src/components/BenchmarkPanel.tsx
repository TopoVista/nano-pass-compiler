export type BenchmarkMeasurement = { compileUs: number; samplesUs: number[]; medianUs: number; p25Us: number; p75Us: number };

export function BenchmarkPanel({ result }: { result?: BenchmarkMeasurement }) {
  return <section className="comparison-panel" aria-label="Current-source benchmark"><header><strong>Current-source benchmark</strong><span>SSA interpreter · repeated measurements</span></header>{!result ? <p className="quiet">Measure the current source to collect one compiler telemetry value, one warmup, and five execution samples. Values are local measurements, not a performance claim.</p> : <table><thead><tr><th>metric</th><th>microseconds</th></tr></thead><tbody><tr><td>compiler telemetry</td><td>{result.compileUs}</td></tr><tr><td>execution median</td><td>{result.medianUs}</td></tr><tr><td>execution p25 / p75</td><td>{result.p25Us} / {result.p75Us}</td></tr><tr><td>samples</td><td>{result.samplesUs.join(', ')}</td></tr></tbody></table>}</section>;
}
