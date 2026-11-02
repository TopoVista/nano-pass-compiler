import { Background, Controls, MarkerType, MiniMap, ReactFlow, type Edge, type Node } from '@xyflow/react';
import '@xyflow/react/dist/style.css';
import type { Graph, RuntimeProfile } from '../lib/api';

type Props = { graph?: Graph; profile?: RuntimeProfile | null; selectedBlock?: number; onSelect: (id: string) => void };
export function GraphPane({ graph, profile, selectedBlock, onSelect }: Props) {
  if (!graph) return <section className="empty graph-empty">Compile an example to inspect its structured compiler graphs.</section>;
  const functionProfile = profile?.functions?.[graph.function];
  const hottestBlock = Math.max(0, ...Object.values(functionProfile?.block_counts ?? {}));
  const nodes: Node[] = graph.nodes.map((node, index) => { const match = /:bb(\d+)$/.exec(node.id); const count = match ? functionProfile?.block_counts?.[match[1]] ?? 0 : 0; const hot = hottestBlock > 0 && count * 2 >= hottestBlock; const suffix = count ? `${count} executions` : node.metadata?.instructions ? `${node.metadata.instructions} instructions` : node.metadata?.depth ? `depth ${node.metadata.depth}` : graph.kind; return { id: node.id, position: { x: 80 + (index % 3) * 220, y: 64 + Math.floor(index / 3) * 130 }, data: { label: <div><strong>{node.label}</strong><small>{suffix}</small></div> }, className: `${selectedBlock !== undefined && node.id.endsWith(`bb${selectedBlock}`) ? 'selected ' : ''}${hot ? 'hot ' : ''}flow-node` }; });
  const edges: Edge[] = graph.edges.map((edge, index) => { const from = /:bb(\d+)$/.exec(edge.from)?.[1]; const to = /:bb(\d+)$/.exec(edge.to)?.[1]; const count = from && to ? functionProfile?.edge_counts.find((item) => item.from === Number(from) && item.to === Number(to))?.count : undefined; return { id: `${edge.from}-${edge.to}-${index}`, source: edge.from, target: edge.to, label: count ? `${count}` : edge.kind === 'cfg' ? undefined : edge.kind, markerEnd: { type: MarkerType.ArrowClosed }, animated: false }; });
  return <section className="graph-canvas" aria-label={`${graph.kind} graph`}><ReactFlow nodes={nodes} edges={edges} fitView onNodeClick={(_, node) => onSelect(node.id)} nodesDraggable={false} nodesConnectable={false} elementsSelectable><Background gap={18} size={1} /><MiniMap /><Controls showInteractive={false} /></ReactFlow></section>;
}
