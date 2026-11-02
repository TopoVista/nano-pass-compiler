import { Background, Controls, MarkerType, MiniMap, ReactFlow, type Edge, type Node } from '@xyflow/react';
import '@xyflow/react/dist/style.css';
import type { Graph } from '../lib/api';

type Props = { graph?: Graph; selectedBlock?: number; onSelect: (id: string) => void };
export function GraphPane({ graph, selectedBlock, onSelect }: Props) {
  if (!graph) return <section className="empty graph-empty">Compile an example to inspect its structured compiler graphs.</section>;
  const nodes: Node[] = graph.nodes.map((node, index) => ({ id: node.id, position: { x: 80 + (index % 3) * 220, y: 64 + Math.floor(index / 3) * 130 }, data: { label: <div><strong>{node.label}</strong><small>{node.metadata?.instructions ? `${node.metadata.instructions} instructions` : node.metadata?.depth ? `depth ${node.metadata.depth}` : graph.kind}</small></div> }, className: selectedBlock !== undefined && node.id.endsWith(`bb${selectedBlock}`) ? 'flow-node selected' : 'flow-node' }));
  const edges: Edge[] = graph.edges.map((edge, index) => ({ id: `${edge.from}-${edge.to}-${index}`, source: edge.from, target: edge.to, label: edge.kind === 'cfg' ? undefined : edge.kind, markerEnd: { type: MarkerType.ArrowClosed }, animated: false }));
  return <section className="graph-canvas" aria-label={`${graph.kind} graph`}><ReactFlow nodes={nodes} edges={edges} fitView onNodeClick={(_, node) => onSelect(node.id)} nodesDraggable={false} nodesConnectable={false} elementsSelectable><Background gap={18} size={1} /><MiniMap /><Controls showInteractive={false} /></ReactFlow></section>;
}
