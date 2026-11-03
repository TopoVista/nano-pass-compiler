import type { Compilation, RunResult } from '../lib/api';

export function TerminalPane({ compilation, run }: { compilation?: Compilation; run?: RunResult }) {
  const diagnostics = compilation?.diagnostics ?? [];
  const lines = run
    ? [
        `$ nano run --mode=${run.status}`,
        ...(run.stdout ? run.stdout.trimEnd().split('\n').map((line) => `› ${line}`) : ['› (program produced no output)']),
        run.returnValue ? `return ${run.returnValue}` : '',
      ].filter(Boolean)
    : diagnostics.length
      ? diagnostics.map((item) => `${item.severity}[${item.code}] ${item.message}`)
      : compilation?.success
        ? [`$ nano verify`, `✓ ${compilation.telemetry.functions} function${compilation.telemetry.functions === 1 ? '' : 's'} · ${compilation.telemetry.blocks} blocks`, 'Ready to run in the selected engine.']
        : ['Waiting for a source program.'];
  return <section className="terminal-pane" aria-label="Compiler terminal" aria-live="polite">
    <header><span className="terminal-lights" aria-hidden="true"><i /><i /><i /></span><strong>Compiler terminal</strong><small>{run ? run.status : compilation?.success ? 'verified' : 'idle'}</small></header>
    <pre>{lines.join('\n')}</pre>
  </section>;
}
