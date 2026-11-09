import { Braces, Cpu, Database, Monitor, Server, ShieldCheck, Workflow } from 'lucide-react';

type NodeProps = {
  x: number;
  y: number;
  label: string;
  detail: string;
  icon: typeof Monitor;
};

function ArchitectureNode({ x, y, label, detail, icon: Icon }: NodeProps) {
  return (
    <div className="architecture-node" style={{ left: x, top: y }}>
      <span className="architecture-tile"><Icon aria-hidden="true" size={24} strokeWidth={1.5} /></span>
      <strong>{label}</strong>
      <small>{detail}</small>
    </div>
  );
}

const boundaries = [
  ['Browser contract', 'source text + allowlisted options'],
  ['Compiler contract', 'typed IR + verifier results'],
  ['Execution contract', 'LLVM IR + bounded run result'],
] as const;

/** Stack-diagram geometry where labels stay outside every connector path. */
export function ArchitectureDiagram() {
  return (
    <figure className="architecture-diagram" aria-labelledby="architecture-diagram-title">
      <figcaption>
        <span>System topology</span>
        <strong id="architecture-diagram-title">A request becomes a verified representation before it can execute.</strong>
        <p>The lab never manufactures compiler facts: the deterministic compiler owns every IR snapshot, diagnostic, graph, and execution result.</p>
      </figcaption>

      <div className="architecture-scroll" aria-label="NanoPass system architecture diagram">
        <div className="architecture-canvas">
          <div className="architecture-lane architecture-lane-workspace"><b>Developer workspace</b><span>what the browser owns</span></div>
          <div className="architecture-lane architecture-lane-compiler"><b>Compiler service</b><span>what the C++ compiler proves</span></div>
          <div className="architecture-lane architecture-lane-execution"><b>Execution boundary</b><span>available only when the local backend permits it</span></div>

          <svg className="architecture-edges" viewBox="0 0 1200 550" aria-hidden="true">
            <defs>
              <marker id="architecture-arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse"><path d="M0 0 L10 5 L0 10 z" fill="context-stroke" /></marker>
            </defs>
            <path className="architecture-edge architecture-edge-primary" d="M132 194 H286" markerEnd="url(#architecture-arrow)" />
            <text x="209" y="182">open lab</text>
            <path className="architecture-edge architecture-edge-primary" d="M358 194 H470" markerEnd="url(#architecture-arrow)" />
            <text x="414" y="182">POST /compile</text>
            <path className="architecture-edge architecture-edge-primary" d="M542 194 H684" markerEnd="url(#architecture-arrow)" />
            <text x="613" y="182">bounded JSON</text>
            <path className="architecture-edge" d="M756 194 H854" markerEnd="url(#architecture-arrow)" />
            <text x="805" y="182">resolve + type</text>
            <path className="architecture-edge" d="M926 194 H1024" markerEnd="url(#architecture-arrow)" />
            <text x="975" y="182">lower + verify</text>
            <path className="architecture-edge" d="M1060 270 V352" markerEnd="url(#architecture-arrow)" />
            <text x="1071" y="316">valid IR only</text>
            <path className="architecture-edge" d="M926 396 H1024" markerEnd="url(#architecture-arrow)" />
            <text x="975" y="384">LLVM IR</text>
            <path className="architecture-edge architecture-edge-return" d="M720 270 V308 H506 V270" markerEnd="url(#architecture-arrow)" />
            <text x="613" y="301">diagnostics · graphs · telemetry</text>
          </svg>

          <ArchitectureNode x={30} y={164} icon={Monitor} label="Developer" detail="browser" />
          <ArchitectureNode x={256} y={164} icon={Braces} label="React + Vite" detail="workbench UI" />
          <ArchitectureNode x={440} y={164} icon={ShieldCheck} label="Request guard" detail="size · timeout · options" />
          <ArchitectureNode x={654} y={164} icon={Server} label="nano-server" detail="structured compiler API" />
          <ArchitectureNode x={824} y={164} icon={Database} label="Frontend + IR" detail="AST · Core · ANF · CFG" />
          <ArchitectureNode x={994} y={164} icon={Cpu} label="SSA optimizer" detail="verify · analyze · rewrite" />
          <ArchitectureNode x={824} y={352} icon={Workflow} label="LLVM lowering" detail="verified backend IR" />
          <ArchitectureNode x={994} y={352} icon={Cpu} label="AOT / ORC" detail="feature-gated execution" />
        </div>
      </div>

      <div className="architecture-boundaries">
        {boundaries.map(([name, detail], index) => <article key={name}><span>{String(index + 1).padStart(2, '0')}</span><b>{name}</b><p>{detail}</p></article>)}
      </div>
      <div className="architecture-legend" aria-label="Architecture diagram legend">
        <span><i className="architecture-primary" />primary request path</span>
        <span><i />compiler-internal transfer</span>
        <span><i className="architecture-return" />compiler facts returned to the lab</span>
      </div>
    </figure>
  );
}
