import { useState } from 'react';
import type { Compilation, RunResult } from '../lib/api';

type TerminalView = 'verify' | 'emit' | 'explain' | 'optimize' | 'learned';

const commands: Record<TerminalView, { label: string; command: string; description: string }> = {
  verify: { label: 'Verify', command: 'nano verify program.nano', description: 'Checks compiler invariants at every available stage.' },
  emit: { label: 'Emit SSA', command: 'nano emit program.nano --format=ssa', description: 'Prints the compiler-generated SSA representation.' },
  explain: { label: 'Explain', command: 'nano explain program.nano', description: 'Shows the transformation journey and factual pass telemetry.' },
  optimize: { label: 'Optimize', command: 'nano optimize program.nano --passes=sccp,dce', description: 'Runs only the named, allowlisted deterministic passes.' },
  learned: { label: 'Learned policy', command: 'nano optimize program.nano -O3 --policy=learned --remarks', description: 'Uses a reviewed inline-profitability policy; it cannot invent transformations or bypass verification.' },
};

export function TerminalPane({ compilation, run }: { compilation?: Compilation; run?: RunResult }) {
  const [view, setView] = useState<TerminalView>('verify');
  const diagnostics = compilation?.diagnostics ?? [];
  const selected = commands[view];
  const snapshot = compilation?.snapshots.find((item) => item.stageId === 'ssa');
  const lines = run
    ? [`$ nano run --mode=${run.status}`, ...(run.stdout ? run.stdout.trimEnd().split('\n').map((line) => `› ${line}`) : ['› (program produced no output)']), run.returnValue ? `return ${run.returnValue}` : ''].filter(Boolean)
    : diagnostics.length
      ? diagnostics.map((item) => `${item.severity}[${item.code}] ${item.message}`)
      : !compilation?.success
        ? ['Waiting for a source program.']
        : view === 'emit'
          ? [`$ ${selected.command}`, ...(snapshot?.text.split('\n').slice(0, 4) ?? ['SSA is not available for this program.'])]
          : view === 'explain'
            ? [`$ ${selected.command}`, ...compilation.passes.slice(0, 4).map((pass) => `✓ ${pass.displayName} · ${pass.durationUs} μs`)]
            : view === 'optimize'
              ? [`$ ${selected.command}`, ...compilation.remarks.slice(0, 3).map((remark) => `${remark.kind.toUpperCase()} ${remark.passId}: ${remark.message}`)]
              : view === 'learned'
                ? [`$ ${selected.command}`, 'policy scope: direct-call inlining profitability only', 'compiler gate: SSA verifier runs after an applied inline clone']
                : [`$ ${selected.command}`, `✓ ${compilation.telemetry.functions} function${compilation.telemetry.functions === 1 ? '' : 's'} · ${compilation.telemetry.blocks} blocks`, 'All available requested invariants passed.'];
  return <section className="terminal-pane" aria-label="Compiler terminal" aria-live="polite">
    <header><span className="terminal-lights" aria-hidden="true"><i /><i /><i /></span><strong>TERMINAL</strong><small>{run ? run.status : compilation?.success ? 'ready' : 'idle'}</small></header>
    <nav className="terminal-tabs" aria-label="Nano command reference">{(Object.keys(commands) as TerminalView[]).map((key) => <button key={key} className={view === key ? 'active' : ''} onClick={() => setView(key)}>{commands[key].label}</button>)}</nav>
    <div className="terminal-body"><div className="terminal-prompt"><span>nano</span><code>{selected.command.replace('nano ', '')}</code></div><pre>{lines.join('\n')}</pre><p>{selected.description}</p></div>
  </section>;
}
