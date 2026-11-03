import { Background, Controls, MarkerType, ReactFlow, type Edge, type Node } from '@xyflow/react';
import '@xyflow/react/dist/style.css';
import type { Graph, RuntimeProfile } from '../lib/api';

type Props = { graph?: Graph; profile?: RuntimeProfile | null; selectedBlock?: number; onSelect: (id: string) => void };

function arrange(graph: Graph): Map<string, { x: number; y: number }> {
  const outgoing = new Map(graph.nodes.map((node) => [node.id, [] as string[]]));
  const incoming = new Map(graph.nodes.map((node) => [node.id, 0]));
  for (const edge of graph.edges) {
    if (outgoing.has(edge.from) && incoming.has(edge.to)) {
      outgoing.get(edge.from)!.push(edge.to);
      incoming.set(edge.to, incoming.get(edge.to)! + 1);
    }
  }
  const roots = graph.nodes.filter((node) => incoming.get(node.id) === 0).map((node) => node.id);
  const queue = roots.length ? [...roots] : graph.nodes.slice(0, 1).map((node) => node.id);
  const layer = new Map(queue.map((id) => [id, 0]));
  for (let cursor = 0; cursor < queue.length; ++cursor) {
    const id = queue[cursor];
    const depth = layer.get(id) ?? 0;
    for (const target of outgoing.get(id) ?? []) {
      if (!layer.has(target)) { layer.set(target, depth + 1); queue.push(target); }
    }
  }
  for (const node of graph.nodes) if (!layer.has(node.id)) layer.set(node.id, (Math.max(...layer.values(), 0) + 1));
  const lanes = new Map<number, string[]>();
  for (const node of graph.nodes) { const depth = layer.get(node.id)!; lanes.set(depth, [...(lanes.get(depth) ?? []), node.id]); }
  const position = new Map<string, { x: number; y: number }>();
  for (const [depth, ids] of [...lanes.entries()].sort(([left], [right]) => left - right)) {
    ids.forEach((id, index) => position.set(id, { x: 64 + depth * 228, y: 56 + index * 124 }));
  }
  return position;
}

export function GraphPane({ graph, profile, selectedBlock, onSelect }: Props) {
  if (!graph) return <section className="empty graph-empty">Compile an example to inspect its structured compiler graphs.</section>;
  const functionProfile = profile?.functions?.[graph.function];
  const hottestBlock = Math.max(0, ...Object.values(functionProfile?.block_counts ?? {}));
  const position = arrange(graph);
  const nodes: Node[] = graph.nodes.map((node) => { const match = /:bb(\d+)$/.exec(node.id); const count = match ? functionProfile?.block_counts?.[match[1]] ?? 0 : 0; const hot = hottestBlock > 0 && count * 2 >= hottestBlock; const suffix = count ? `${count} executions` : node.metadata?.instructions ? `${node.metadata.instructions} instructions` : node.metadata?.depth ? `depth ${node.metadata.depth}` : graph.kind; return { id: node.id, position: position.get(node.id)!, data: { label: <div><strong>{node.label}</strong><small>{suffix}</small></div> }, className: `${selectedBlock !== undefined && node.id.endsWith(`bb${selectedBlock}`) ? 'selected ' : ''}${hot ? 'hot ' : ''}flow-node` }; });
  const edges: Edge[] = graph.edges.map((edge, index) => { const from = /:bb(\d+)$/.exec(edge.from)?.[1]; const to = /:bb(\d+)$/.exec(edge.to)?.[1]; const count = from && to ? functionProfile?.edge_counts.find((item) => item.from === Number(from) && item.to === Number(to))?.count : undefined; return { id: `${edge.from}-${edge.to}-${index}`, source: edge.from, target: edge.to, label: count ? `${count}` : edge.kind === 'cfg' ? undefined : edge.kind, type: 'smoothstep', markerEnd: { type: MarkerType.ArrowClosed }, animated: false }; });
  return <section className="graph-canvas" aria-label={`${graph.kind} graph`}><ReactFlow nodes={nodes} edges={edges} fitView fitViewOptions={{ padding: .22 }} onNodeClick={(_, node) => onSelect(node.id)} nodesDraggable={false} nodesConnectable={false} elementsSelectable><Background gap={20} size={1} /><Controls showInteractive={false} /></ReactFlow></section>;
}
