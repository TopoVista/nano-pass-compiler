import { useState } from 'react';
import type { Compilation, RunResult } from '../lib/api';

export type TerminalReply = { lines: string[]; tone?: 'success' | 'error' | 'info'; clear?: boolean };
type Props = { compilation?: Compilation; run?: RunResult; onCommand: (command: string) => Promise<TerminalReply> };

const suggestions = ['help', 'verify', 'run ssa', 'emit ssa', 'explain', 'optimize', 'correctness', 'benchmark', 'policy learned'];

export function TerminalPane({ compilation, run, onCommand }: Props) {
  const [input, setInput] = useState('');
  const [history, setHistory] = useState<TerminalReply[]>([]);
  const [running, setRunning] = useState(false);
  const submit = async (command = input) => {
    const trimmed = command.trim();
    if (!trimmed || running) return;
    setInput('');
    setRunning(true);
    const reply = await onCommand(trimmed);
    setRunning(false);
    setHistory((current) => reply.clear ? [] : [...current, { lines: [`$ ${trimmed}`, ...reply.lines], tone: reply.tone }]);
  };
  const startup = run
    ? [`last run: ${run.status}`, ...(run.stdout ? run.stdout.trimEnd().split('\n').map((line) => `› ${line}`) : ['› (program produced no output)']), run.returnValue ? `return ${run.returnValue}` : ''].filter(Boolean)
    : compilation?.success
      ? [`compiler ready · ${compilation.telemetry.functions} function${compilation.telemetry.functions === 1 ? '' : 's'} · ${compilation.telemetry.blocks} blocks`, 'Type help to see the safe in-app CLI commands.']
      : ['Compiling source…'];
  return <section className="terminal-pane" aria-label="Compiler terminal" aria-live="polite">
    <header><span className="terminal-lights" aria-hidden="true"><i /><i /><i /></span><strong>TERMINAL</strong><small>{running ? 'running' : compilation?.success ? 'ready' : 'waiting'}</small></header>
    <div className="terminal-output"><pre className="terminal-startup">{startup.join('\n')}</pre>{history.map((entry, index) => <pre key={index} className={`terminal-entry ${entry.tone ?? 'info'}`}>{entry.lines.join('\n')}</pre>)}</div>
    <form className="terminal-input" noValidate onSubmit={(event) => { event.preventDefault(); void submit(); }}><span>nano ❯</span><input aria-label="Terminal command" autoComplete="off" spellCheck={false} value={input} onChange={(event) => setInput(event.target.value)} placeholder="help, verify, run ssa, emit llvm…" /><button type="submit" disabled={running}>Run</button></form>
    <div className="terminal-suggestions" aria-label="Terminal command suggestions">{suggestions.map((command) => <button key={command} onClick={() => void submit(command)}>{command}</button>)}</div>
  </section>;
}
