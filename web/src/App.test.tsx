import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import { beforeEach, vi } from 'vitest';
import App from './App';
import type { Compilation, RunResult } from './lib/api';

vi.mock('./components/SourceEditor', () => ({ SourceEditor: ({ value, onChange }: { value: string; onChange: (text: string) => void }) => <textarea className="resize-none" aria-label="Nano source editor" value={value} onChange={(event) => onChange(event.target.value)} /> }));
vi.mock('./components/GraphPane', () => ({ GraphPane: ({ graph, onSelect }: { graph?: { nodes: { id: string; label: string }[] }; onSelect: (id: string) => void }) => <div aria-label="CFG graph">{graph?.nodes.map((node) => <button key={node.id} onClick={() => onSelect(node.id)}>{node.label}</button>)}</div> }));

const response: Compilation = { schemaVersion: 1, success: true, compilerRevision: 'test', diagnostics: [], snapshots: [{ stageId: 'ssa', displayName: 'SSA', text: 'bb0:\n  %7 = const 1\n  return %7' }, { stageId: 'optimized-ssa', displayName: 'Optimized SSA', text: 'bb0:\n  return 1' }], transformations: [{ passId: 'sccp', before: 'bb0:\n  %7 = const 1', after: 'bb0:\n  return 1' }], passes: [{ id: 'sccp', displayName: 'SCCP', category: 'optimization', changed: true, durationUs: 12, instructionsBefore: 3, instructionsAfter: 1, blocksBefore: 1, blocksAfter: 1, valuesBefore: 1, valuesAfter: 0, verified: true }], remarks: [{ kind: 'missed', passId: 'licm', message: 'UNSAFE_TO_SPECULATE: division may trap' }], graphs: [{ kind: 'cfg', function: 'main', nodes: [{ id: 'main:bb0', label: 'bb0' }], edges: [] }, { kind: 'dominator-tree', function: 'main', nodes: [{ id: 'main:bb0', label: 'bb0' }], edges: [] }, { kind: 'loops', function: 'main', nodes: [], edges: [] }, { kind: 'call-graph', function: 'program', nodes: [], edges: [] }], values: [{ value: 7, function: 'main', kind: 'instruction', type: 'int', definingBlock: 0, line: 1, column: 1 }], policyFeatures: [{ function: 'main', instructions: 1, blocks: 1, loops: 0, callers: 0, callees: 0, dynamicCalls: 1, dynamicBlocks: 1, dynamicLoopBackedges: 0, hotness: 'cold', hotnessReason: 'COLD_FUNCTION' }], policyRecords: [{ record_kind: 'inline-imitation', caller: 'main', callee: 'helper', features: { instruction_count: 2, block_count: 1, dynamic_call_count: 0, callee_count: 0 }, estimated_inline_cost: 4, caller_instruction_count: 5, heuristic_action: 'inline', legality: 'pass', policy_type: 'learned', model_version: 'inline-imitation-v1', model_action: 'inline', confidence: 0.812 }], telemetry: { totalCompileUs: 55, functions: 1, blocks: 1, values: 1 } };
const runResponse: RunResult = { success: true, errorKind: 'none', status: 'success', stdout: '1\n', stderr: '', returnValue: '1', stepsExecuted: 3, executionDurationUs: 42, runtimeProfile: { functions: { main: { calls: 1, blocks_executed: 1, block_counts: { '0': 1 }, edge_counts: [], loops: {} } } }, compilation: response };
vi.mock('./lib/api', async (original) => { const actual = await original<typeof import('./lib/api')>(); return { ...actual, compile: vi.fn(async () => response), run: vi.fn(async (_options, mode) => ({ ...runResponse, adaptiveEvents: mode === 'adaptive-jit' ? [{ kind: 'tier1_installed', function: 'main', calls: 2, detail: 'installed' }] : [] })), checkCorrectness: vi.fn(async () => ({ schemaVersion: 1, success: true, equivalent: true, checks: [{ stage: 'CoreIR', passed: true, detail: 'reference execution completed' }], compilation: response })) }; });

describe('compiler lab', () => {
  beforeEach(() => window.history.pushState({}, '', '/lab'));

  it('keeps code, IR, and the compiler timeline in the focused workbench', async () => {
    render(<App />);
    await waitFor(() => expect(screen.getByLabelText('Compiler revision')).toHaveTextContent('test'));
    expect(screen.getByText('SCCP')).toBeInTheDocument();
    fireEvent.change(screen.getByLabelText('IR stage'), { target: { value: 'ssa' } });
    fireEvent.click(screen.getAllByText('%7')[0]);
    expect(screen.getByLabelText('Compiler terminal')).toBeInTheDocument();
  });
  it('offers an allowlisted custom pipeline control', async () => {
    render(<App />);
    await waitFor(() => expect(screen.getByLabelText('Optimization level')).toBeInTheDocument());
    fireEvent.click(screen.getByLabelText('sccp'));
    expect(screen.getByLabelText('sccp')).toBeChecked();
    fireEvent.click(screen.getByLabelText('dce'));
    fireEvent.click(screen.getByLabelText('Move dce earlier'));
    expect(screen.getByLabelText('Move dce earlier')).toBeDisabled();
  });
  it('runs only allowlisted compiler commands in the in-app terminal', async () => {
    render(<App />);
    await waitFor(() => expect(screen.getByLabelText('Terminal command')).toBeEnabled());
    const command = screen.getByLabelText('Terminal command');
    fireEvent.change(command, { target: { value: 'verify' } });
    fireEvent.submit(command.closest('form')!);
    await waitFor(() => expect(screen.getByText(/PASS · 1 function/)).toBeInTheDocument());
    fireEvent.change(command, { target: { value: 'rm -rf anything' } });
    fireEvent.submit(command.closest('form')!);
    await waitFor(() => expect(screen.getByText(/Unknown command: rm/)).toBeInTheDocument());
  });
  it('formats source and can generate a valid-looking NanoPass C program', async () => {
    render(<App />);
    const editor = screen.getByLabelText('Nano source editor');
    fireEvent.change(editor, { target: { value: 'int main(){return 0;}' } });
    fireEvent.click(screen.getByRole('button', { name: 'Format source' }));
    expect(editor).toHaveValue('int main() {\n  return 0;\n}');
    fireEvent.click(screen.getByRole('button', { name: 'Generate program' }));
    expect((editor as HTMLTextAreaElement).value).toContain('int main()');
  });
  it('offers a distraction-free source and terminal focus mode', async () => {
    render(<App />);
    await waitFor(() => expect(screen.getByRole('button', { name: 'Focus' })).toBeEnabled());
    fireEvent.click(screen.getByRole('button', { name: 'Focus' }));
    expect(document.getElementById('compiler-workspace')).toHaveClass('editor-focus');
    expect(screen.getByRole('button', { name: 'Restore' })).toBeInTheDocument();
    fireEvent.click(screen.getByRole('button', { name: 'Restore' }));
    expect(document.getElementById('compiler-workspace')).not.toHaveClass('editor-focus');
  });
  it('serves the field guide from its own route', async () => {
    window.history.pushState({}, '', '/guide');
    render(<App />);
    expect(screen.getByText(/A compiler lab, not a black box/i)).toBeInTheDocument();
    fireEvent.click(screen.getByRole('tab', { name: /control flow/i }));
    expect(screen.getByRole('button', { name: 'Open CFG' })).toBeInTheDocument();
    window.history.pushState({}, '', '/lab');
  });
  it('serves a dedicated public landing route', () => {
    window.history.pushState({}, '', '/');
    render(<App />);
    expect(screen.getByRole('heading', { name: /a compiler you can interrogate/i })).toBeInTheDocument();
    expect(screen.getByRole('link', { name: /open compiler lab/i })).toHaveAttribute('href', '/lab');
    expect(screen.getByRole('figure', { name: /a request becomes a verified representation before it can execute/i })).toBeInTheDocument();
    expect(screen.getByText('nano-server')).toBeInTheDocument();
  });
  it('keeps optimization evidence and verification controls on dedicated routes', async () => {
    window.history.pushState({}, '', '/optimizations');
    render(<App />);
    await waitFor(() => expect(screen.getByText(/UNSAFE_TO_SPECULATE/)).toBeInTheDocument());
    expect(screen.getByText(/inline-imitation-v1/)).toBeInTheDocument();
    window.history.pushState({}, '', '/verification');
  });
  it('runs structured adaptive events, correctness, and comparison from verification', async () => {
    window.history.pushState({}, '', '/verification');
    render(<App />);
    await waitFor(() => expect(screen.getByRole('button', { name: 'adaptive-jit' })).toBeEnabled());
    fireEvent.click(screen.getByRole('button', { name: 'adaptive-jit' }));
    await waitFor(() => expect(screen.getByText(/tier1_installed/)).toBeInTheDocument());
    fireEvent.click(screen.getByRole('button', { name: 'check correctness' }));
    await waitFor(() => expect(screen.getByText('CoreIR')).toBeInTheDocument());
    fireEvent.click(screen.getByRole('button', { name: 'Compare O-levels' }));
    await waitFor(() => expect(screen.getByText('compiler measurements')).toBeInTheDocument());
    fireEvent.click(screen.getByRole('button', { name: 'Benchmark' }));
    await waitFor(() => expect(screen.getByText(/μs median/)).toBeInTheDocument());
  });
});
