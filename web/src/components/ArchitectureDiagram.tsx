import { Braces, Cpu, Database, Monitor, Server, ShieldCheck, Workflow } from 'lucide-react';
import { useState } from 'react';

type NodeProps = {
  id: string;
  x: number;
  y: number;
  label: string;
  detail: string;
  icon: typeof Monitor;
  active: boolean;
  onSelect: (id: string) => void;
};

function ArchitectureNode({ id, x, y, label, detail, icon: Icon, active, onSelect }: NodeProps) {
  return (
    <button type="button" className={`architecture-node${active ? ' active' : ''}`} style={{ left: x, top: y }} onClick={() => onSelect(id)} onFocus={() => onSelect(id)} aria-pressed={active}>
      <span className="architecture-tile"><Icon aria-hidden="true" size={24} strokeWidth={1.5} /></span>
      <strong>{label}</strong>
      <small>{detail}</small>
    </button>
  );
}

const boundaries = [
  ['Browser contract', 'source text + allowlisted options'],
  ['Compiler contract', 'typed IR + verifier results'],
  ['Execution contract', 'LLVM IR + bounded run result'],
] as const;

/** Stack-diagram geometry where labels stay outside every connector path. */
export function ArchitectureDiagram() {
  const [selected, setSelected] = useState('service');
  const detail = {
    developer: ['Developer', 'Chooses a source program and an inspection view.', 'Input: source text', 'Output: an intentional lab request'],
    browser: ['React + Vite', 'Keeps editor state and renders structured compiler artifacts.', 'Input: bounded JSON response', 'Output: readable source, IR, graphs, and diagnostics'],
    guard: ['Request guard', 'Applies the service boundary before compilation begins.', 'Input: source plus allowlisted options', 'Output: bounded compiler request'],
    service: ['Compiler service', 'The nano-server route handler sends a versioned request into the compiler service without creating a second compiler.', 'Input: validated API request', 'Output: compiler response or structured diagnostic'],
    frontend: ['Frontend + IR', 'Lexing, parsing, scope resolution, and typing establish source meaning before lower IR stages.', 'Input: tokens and AST', 'Output: typed IR and diagnostics'],
    optimizer: ['SSA optimizer', 'Applies verifier-gated rewrites and records applied, missed, and analysis remarks.', 'Input: verified SSA', 'Output: optimized SSA and evidence'],
    llvm: ['LLVM lowering', 'Converts verified NanoPass SSA into LLVM IR when the build supports it.', 'Input: verified SSA', 'Output: LLVM module'],
    execution: ['AOT / ORC', 'Executes only if the current build and execution policy permit it.', 'Input: supported LLVM capability', 'Output: bounded execution result'],
  } as const;
  const selectedDetail = detail[selected as keyof typeof detail] ?? detail.service;
  return (
    <figure className="architecture-diagram" aria-labelledby="architecture-diagram-title">
      <figcaption>
        <span>System topology</span>
        <strong id="architecture-diagram-title">A request becomes a verified representation before it can execute.</strong>
        <p>The lab never manufactures compiler facts: the deterministic compiler owns every IR snapshot, diagnostic, graph, and execution result.</p>
      </figcaption>

      <div className="architecture-scroll" aria-label="NanoPass system architecture diagram">
        <div className={`architecture-canvas architecture-focus-${selected}`}>
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

          <ArchitectureNode id="developer" x={30} y={164} icon={Monitor} label="Developer" detail="browser" active={selected === 'developer'} onSelect={setSelected} />
          <ArchitectureNode id="browser" x={256} y={164} icon={Braces} label="React + Vite" detail="workbench UI" active={selected === 'browser'} onSelect={setSelected} />
          <ArchitectureNode id="guard" x={440} y={164} icon={ShieldCheck} label="Request guard" detail="size · timeout · options" active={selected === 'guard'} onSelect={setSelected} />
          <ArchitectureNode id="service" x={654} y={164} icon={Server} label="nano-server" detail="structured compiler API" active={selected === 'service'} onSelect={setSelected} />
          <ArchitectureNode id="frontend" x={824} y={164} icon={Database} label="Frontend + IR" detail="AST · Core · ANF · CFG" active={selected === 'frontend'} onSelect={setSelected} />
          <ArchitectureNode id="optimizer" x={994} y={164} icon={Cpu} label="SSA optimizer" detail="verify · analyze · rewrite" active={selected === 'optimizer'} onSelect={setSelected} />
          <ArchitectureNode id="llvm" x={824} y={352} icon={Workflow} label="LLVM lowering" detail="verified backend IR" active={selected === 'llvm'} onSelect={setSelected} />
          <ArchitectureNode id="execution" x={994} y={352} icon={Cpu} label="AOT / ORC" detail="feature-gated execution" active={selected === 'execution'} onSelect={setSelected} />
        </div>
      </div>

      <aside className="architecture-inspector" aria-live="polite">
        <span>SELECTED SYSTEM LAYER</span>
        <strong>{selectedDetail[0]}</strong>
        <p>{selectedDetail[1]}</p>
        <div><small>{selectedDetail[2]}</small><i aria-hidden="true">→</i><small>{selectedDetail[3]}</small></div>
      </aside>

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
