import { Background, Controls, MarkerType, ReactFlow, type Edge, type Node } from '@xyflow/react';
import '@xyflow/react/dist/style.css';
import type { Graph, RuntimeProfile } from '../lib/api';

type Props = { graph?: Graph; profile?: RuntimeProfile | null; selectedBlock?: number; onSelect: (id: string) => void };

const graphGuidance: Record<string, { title: string; description: string; relationship: string }> = {
  cfg: {
    title: 'Control-flow graph',
    description: 'Each node is a basic block. Directed edges are the paths execution may take after a block terminator.',
    relationship: 'Select a block to isolate its direct predecessors and successors.',
  },
  'dominator-tree': {
    title: 'Dominator tree',
    description: 'An edge shows that one block must be visited before another block can be reached.',
    relationship: 'Use this view to understand where an SSA definition is available.',
  },
  loops: {
    title: 'Loop structure',
    description: 'Nodes and edges describe the loop structure emitted by the compiler analysis for this function.',
    relationship: 'Select a node to focus the directly related emitted loop structure.',
  },
  'call-graph': {
    title: 'Call graph',
    description: 'Nodes represent functions and edges represent direct compiler-detected calls between them.',
    relationship: 'Select a function to see its direct caller and callee relationships.',
  },
};

export function GraphInspector({ graph, selectedBlock }: Pick<Props, 'graph' | 'selectedBlock'>) {
  if (!graph) return <aside className="list-panel graph-inspector" aria-label="Graph guide"><p className="quiet">Compile source to inspect a compiler-generated graph.</p></aside>;
  const guide = graphGuidance[graph.kind] ?? {
    title: 'Compiler graph',
    description: 'This view is a structured graph emitted by the compiler service.',
    relationship: 'Select a node to inspect its direct compiler-emitted relationships.',
  };
  const selectedNode = selectedBlock === undefined ? undefined : graph.nodes.find((node) => node.id.endsWith(`bb${selectedBlock}`));
  const selectedEdges = selectedNode ? graph.edges.filter((edge) => edge.from === selectedNode.id || edge.to === selectedNode.id) : [];
  return <aside className="list-panel graph-inspector" aria-label="Graph guide">
    <header><span>READ THIS GRAPH</span><strong>{guide.title}</strong></header>
    <p>{guide.description}</p>
    <dl className="graph-inspector-facts">
      <div><dt>FUNCTION</dt><dd>@{graph.function}</dd></div>
      <div><dt>EMITTED</dt><dd>{graph.nodes.length} {graph.nodes.length === 1 ? 'node' : 'nodes'} · {graph.edges.length} {graph.edges.length === 1 ? 'edge' : 'edges'}</dd></div>
    </dl>
    <section className="graph-inspector-selection" aria-live="polite">
      <span>{selectedNode ? 'CURRENT SELECTION' : 'NEXT STEP'}</span>
      <strong>{selectedNode?.label ?? 'Choose a graph node'}</strong>
      <p>{selectedNode ? `${selectedEdges.length} direct ${selectedEdges.length === 1 ? 'relationship is' : 'relationships are'} highlighted in the canvas.` : guide.relationship}</p>
    </section>
  </aside>;
}

export function arrangeGraph(graph: Graph): Map<string, { x: number; y: number }> {
  const orderedNodes = [...graph.nodes].sort((left, right) => left.id.localeCompare(right.id));
  const outgoing = new Map(orderedNodes.map((node) => [node.id, [] as string[]]));
  const incoming = new Map(orderedNodes.map((node) => [node.id, 0]));
  for (const edge of [...graph.edges].sort((left, right) => `${left.from}:${left.to}:${left.kind}`.localeCompare(`${right.from}:${right.to}:${right.kind}`))) {
    if (outgoing.has(edge.from) && incoming.has(edge.to)) {
      outgoing.get(edge.from)!.push(edge.to);
      incoming.set(edge.to, incoming.get(edge.to)! + 1);
    }
  }
  for (const targets of outgoing.values()) targets.sort();
  const roots = orderedNodes.filter((node) => incoming.get(node.id) === 0).map((node) => node.id);
  const queue = roots.length ? [...roots] : orderedNodes.slice(0, 1).map((node) => node.id);
  const layer = new Map(queue.map((id) => [id, 0]));
  for (let cursor = 0; cursor < queue.length; ++cursor) {
    const id = queue[cursor];
    const depth = layer.get(id) ?? 0;
    for (const target of outgoing.get(id) ?? []) {
      if (!layer.has(target)) { layer.set(target, depth + 1); queue.push(target); }
    }
  }
  for (const node of orderedNodes) if (!layer.has(node.id)) layer.set(node.id, (Math.max(...layer.values(), 0) + 1));
  const lanes = new Map<number, string[]>();
  for (const node of orderedNodes) { const depth = layer.get(node.id)!; lanes.set(depth, [...(lanes.get(depth) ?? []), node.id]); }
  const position = new Map<string, { x: number; y: number }>();
  for (const [depth, ids] of [...lanes.entries()].sort(([left], [right]) => left - right)) {
    ids.sort().forEach((id, index) => position.set(id, { x: 64 + depth * 228, y: 56 + index * 124 }));
  }
  return position;
}

export function GraphPane({ graph, profile, selectedBlock, onSelect }: Props) {
  if (!graph) return <section className="empty graph-empty">Compile an example to inspect its structured compiler graphs.</section>;
  if (graph.nodes.length === 0) return <section className="empty graph-empty"><strong>No nodes in this view yet.</strong><p>This program has no emitted {graph.kind.replace('-', ' ')} nodes for the selected function. Choose CFG, or compile an example with branching or a loop.</p></section>;
  const functionProfile = profile?.functions?.[graph.function];
  const hottestBlock = Math.max(0, ...Object.values(functionProfile?.block_counts ?? {}));
  const position = arrangeGraph(graph);
  const selectedNode = selectedBlock === undefined ? undefined : graph.nodes.find((node) => node.id.endsWith(`bb${selectedBlock}`));
  const relatedNodes = new Set<string>(selectedNode ? [selectedNode.id, ...graph.edges.filter((edge) => edge.from === selectedNode.id || edge.to === selectedNode.id).flatMap((edge) => [edge.from, edge.to])] : []);
  const nodes: Node[] = graph.nodes.map((node) => { const match = /:bb(\d+)$/.exec(node.id); const count = match ? functionProfile?.block_counts?.[match[1]] ?? 0 : 0; const hot = hottestBlock > 0 && count * 2 >= hottestBlock; const selected = selectedNode?.id === node.id; const related = relatedNodes.has(node.id); const suffix = count ? `${count} executions` : node.metadata?.instructions ? `${node.metadata.instructions} instructions` : node.metadata?.depth ? `depth ${node.metadata.depth}` : graph.kind; return { id: node.id, position: position.get(node.id)!, data: { label: <div className="flow-node-label"><strong>{node.label}</strong><small>{suffix}</small></div> }, className: `${selected ? 'selected ' : ''}${related ? 'related ' : ''}${selectedNode && !related ? 'muted ' : ''}${hot ? 'hot ' : ''}flow-node` }; });
  const edges: Edge[] = graph.edges.map((edge, index) => { const from = /:bb(\d+)$/.exec(edge.from)?.[1]; const to = /:bb(\d+)$/.exec(edge.to)?.[1]; const count = from && to ? functionProfile?.edge_counts.find((item) => item.from === Number(from) && item.to === Number(to))?.count : undefined; const related = selectedNode && (edge.from === selectedNode.id || edge.to === selectedNode.id); return { id: `${edge.from}-${edge.to}-${index}`, source: edge.from, target: edge.to, label: count ? `${count}` : edge.kind === 'cfg' ? undefined : edge.kind, type: 'smoothstep', markerEnd: { type: MarkerType.ArrowClosed }, animated: Boolean(related && count && count > 0), className: `${related ? 'graph-edge-related' : ''}${selectedNode && !related ? ' graph-edge-muted' : ''}` }; });
  const selectedMetadata = Object.entries(selectedNode?.metadata ?? {});
  return <section className={`graph-canvas${graph.nodes.length === 1 ? ' graph-canvas-single' : ''}`} aria-label={`${graph.kind} graph`}>
    <div className="graph-facts"><span><b>{graph.nodes.length}</b> blocks</span><span><b>{graph.edges.length}</b> edges</span><span className="graph-legend"><i /> selected <i className="hot" /> profiled hot path</span></div>
    <div className="graph-flow-viewport">
      <ReactFlow nodes={nodes} edges={edges} fitView fitViewOptions={{ padding: .22 }} onNodeClick={(_, node) => onSelect(node.id)} nodesDraggable={false} nodesConnectable={false} elementsSelectable><Background gap={20} size={1} /><Controls showInteractive={false} /></ReactFlow>
    </div>
    <aside className="graph-selection" aria-live="polite">
      {selectedNode ? <><strong>{selectedNode.label}</strong><span>{selectedMetadata.length ? selectedMetadata.map(([key, value]) => `${key}: ${value}`).join(' · ') : 'No additional metadata was emitted for this node.'}</span><small>{graph.edges.filter((edge) => edge.from === selectedNode.id).length} outgoing · {graph.edges.filter((edge) => edge.to === selectedNode.id).length} incoming compiler-emitted edges highlighted</small></> : <span>Select a node to inspect compiler-emitted metadata and its direct relationships.</span>}
    </aside>
  </section>;
}
