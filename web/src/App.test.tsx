import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import { vi } from 'vitest';
import App from './App';
import type { Compilation, RunResult } from './lib/api';

vi.mock('./components/SourceEditor', () => ({ SourceEditor: ({ value, onChange }: { value: string; onChange: (text: string) => void }) => <textarea aria-label="Nano source editor" value={value} onChange={(event) => onChange(event.target.value)} /> }));
vi.mock('./components/GraphPane', () => ({ GraphPane: ({ graph, onSelect }: { graph?: { nodes: { id: string; label: string }[] }; onSelect: (id: string) => void }) => <div aria-label="CFG graph">{graph?.nodes.map((node) => <button key={node.id} onClick={() => onSelect(node.id)}>{node.label}</button>)}</div> }));

const response: Compilation = { schemaVersion: 1, success: true, compilerRevision: 'test', diagnostics: [], snapshots: [{ stageId: 'ssa', displayName: 'SSA', text: 'bb0:\n  %7 = const 1\n  return %7' }, { stageId: 'optimized-ssa', displayName: 'Optimized SSA', text: 'bb0:\n  return 1' }], passes: [{ id: 'sccp', displayName: 'SCCP', category: 'optimization', changed: true, durationUs: 12, instructionsBefore: 3, instructionsAfter: 1, blocksBefore: 1, blocksAfter: 1, valuesBefore: 1, valuesAfter: 0, verified: true }], remarks: [{ kind: 'missed', passId: 'licm', message: 'UNSAFE_TO_SPECULATE: division may trap' }], graphs: [{ kind: 'cfg', function: 'main', nodes: [{ id: 'main:bb0', label: 'bb0' }], edges: [] }, { kind: 'dominator-tree', function: 'main', nodes: [{ id: 'main:bb0', label: 'bb0' }], edges: [] }, { kind: 'loops', function: 'main', nodes: [], edges: [] }, { kind: 'call-graph', function: 'program', nodes: [], edges: [] }], values: [{ value: 7, function: 'main', kind: 'instruction', type: 'int', definingBlock: 0, line: 1, column: 1 }], telemetry: { totalCompileUs: 55, functions: 1, blocks: 1, values: 1 } };
const runResponse: RunResult = { success: true, errorKind: 'none', status: 'success', stdout: '1\n', stderr: '', returnValue: '1', stepsExecuted: 3, runtimeProfile: { functions: { main: { calls: 1, blocks_executed: 1, block_counts: { '0': 1 }, edge_counts: [], loops: {} } } }, compilation: response };
vi.mock('./lib/api', async (original) => { const actual = await original<typeof import('./lib/api')>(); return { ...actual, compile: vi.fn(async () => response), run: vi.fn(async () => runResponse) }; });

describe('compiler lab', () => {
  it('renders compiler facts, graph selection, value inspection, and execution', async () => {
    render(<App />);
    await waitFor(() => expect(screen.getAllByText('Optimized SSA').length).toBeGreaterThan(0));
    expect(screen.getByText('SCCP')).toBeInTheDocument();
    expect(screen.getByText(/UNSAFE_TO_SPECULATE/)).toBeInTheDocument();
    fireEvent.change(screen.getByLabelText('IR stage'), { target: { value: 'ssa' } });
    fireEvent.click(screen.getAllByText('%7')[0]);
    expect(screen.getByText(/%7 · int/)).toBeInTheDocument();
    fireEvent.click(screen.getByText('bb0'));
    fireEvent.click(screen.getByRole('button', { name: 'ssa' }));
    await waitFor(() => expect(screen.getByText('1 calls')).toBeInTheDocument());
  });
  it('offers an allowlisted custom pipeline control', async () => {
    render(<App />);
    await waitFor(() => expect(screen.getByLabelText('Optimization level')).toBeInTheDocument());
    fireEvent.click(screen.getByLabelText('sccp'));
    expect(screen.getByLabelText('sccp')).toBeChecked();
  });
});
