import { fireEvent, render, screen } from '@testing-library/react';
import type { ReactNode } from 'react';
import { vi } from 'vitest';
import { arrangeGraph, GraphPane } from './GraphPane';
import type { Graph } from '../lib/api';

vi.mock('@xyflow/react', () => ({
  Background: () => null,
  Controls: () => null,
  MarkerType: { ArrowClosed: 'arrow' },
  ReactFlow: ({ nodes, onNodeClick }: { nodes: Array<{ id: string; data: { label: ReactNode } }>; onNodeClick: (event: unknown, node: { id: string }) => void }) => <div>{nodes.map((node) => <button key={node.id} onClick={() => onNodeClick({}, node)}>{node.data.label}</button>)}</div>,
}));

const graph: Graph = {
  kind: 'cfg',
  function: 'main',
  nodes: [
    { id: 'main:bb1', label: 'bb1.then', metadata: { instructions: '2', role: 'branch' } },
    { id: 'main:bb0', label: 'bb0.entry', metadata: { instructions: '1' } },
  ],
  edges: [{ from: 'main:bb0', to: 'main:bb1', kind: 'cfg' }],
};

describe('GraphPane', () => {
  it('lays out identical graph data deterministically without coordinate collisions', () => {
    const first = arrangeGraph(graph);
    const second = arrangeGraph({ ...graph, nodes: [...graph.nodes].reverse() });
    expect([...first.entries()]).toEqual([...second.entries()]);
    expect(new Set([...first.values()].map(({ x, y }) => `${x}:${y}`)).size).toBe(graph.nodes.length);
  });

  it('separates compiler-emitted node labels from their metadata and selects a node', () => {
    const onSelect = vi.fn();
    render(<GraphPane graph={graph} onSelect={onSelect} selectedBlock={1} />);
    expect(screen.getAllByText('bb1.then')).toHaveLength(2);
    expect(screen.getByText('2 instructions')).toBeInTheDocument();
    expect(screen.getByText(/instructions: 2/)).toBeInTheDocument();
    fireEvent.click(screen.getByText('bb0.entry'));
    expect(onSelect).toHaveBeenCalledWith('main:bb0');
  });
});
