import { Braces, Cpu, Monitor, Server, Workflow } from 'lucide-react';

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
      <span className="architecture-tile"><Icon aria-hidden="true" size={25} strokeWidth={1.5} /></span>
      <strong>{label}</strong>
      <small>{detail}</small>
    </div>
  );
}

/** Stack-diagram geometry translated into the app's existing React surface. */
export function ArchitectureDiagram() {
  return (
    <figure className="architecture-diagram" aria-labelledby="architecture-diagram-title">
      <figcaption>
        <span>System topology</span>
        <strong id="architecture-diagram-title">One compiler core. Inspectable at every boundary.</strong>
        <p>Requests carry source and bounded options into the compiler service; results return as representations, diagnostics, telemetry, and graphs.</p>
      </figcaption>

      <div className="architecture-scroll">
        <div className="architecture-canvas">
          <div className="architecture-group architecture-group-client"><b>Browser workspace</b></div>
          <div className="architecture-group architecture-group-compiler"><b>Compiler service</b></div>
          <div className="architecture-group architecture-group-backend"><b>Backend execution</b></div>

          <svg className="architecture-edges" viewBox="0 0 1000 500" aria-hidden="true">
            <defs>
              <marker id="architecture-arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
                <path d="M0 0 L10 5 L0 10 z" fill="context-stroke" />
              </marker>
            </defs>
            <path className="architecture-edge architecture-edge-primary" d="M156 180 H264" markerEnd="url(#architecture-arrow)" />
            <text x="210" y="169">HTTPS</text>
            <path className="architecture-edge architecture-edge-primary" d="M396 180 H464" markerEnd="url(#architecture-arrow)" />
            <text x="430" y="169">source + options</text>
            <path className="architecture-edge" d="M596 180 H664" markerEnd="url(#architecture-arrow)" />
            <text x="630" y="169">lower</text>
            <path className="architecture-edge" d="M730 256 V334" markerEnd="url(#architecture-arrow)" />
            <text x="741" y="302">verified IR</text>
            <path className="architecture-edge" d="M796 364 H864" markerEnd="url(#architecture-arrow)" />
            <text x="830" y="353">AOT / JIT</text>
          </svg>

          <ArchitectureNode x={46} y={150} icon={Monitor} label="Developer" detail="browser" />
          <ArchitectureNode x={264} y={150} icon={Braces} label="React + Vite" detail="compiler lab" />
          <ArchitectureNode x={464} y={150} icon={Server} label="nano-server" detail="bounded JSON API" />
          <ArchitectureNode x={664} y={150} icon={Cpu} label="NanoPass core" detail="C++20 · typed IR" />
          <ArchitectureNode x={664} y={334} icon={Workflow} label="LLVM backend" detail="verified lowering" />
          <ArchitectureNode x={864} y={334} icon={Cpu} label="Native / ORC" detail="optional execution" />
        </div>
      </div>

      <div className="architecture-legend" aria-label="Architecture diagram legend">
        <span><i className="architecture-primary" />request path</span>
        <span><i />compiler-internal boundary</span>
        <span>every stage emits a machine-readable inspection artifact</span>
      </div>
    </figure>
  );
}
