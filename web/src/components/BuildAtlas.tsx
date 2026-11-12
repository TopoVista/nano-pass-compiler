import { useState } from 'react';
import type { SVGProps } from 'react';
import { ArrowRight, Braces, Check, ChevronRight, Code2, Cpu, FolderTree, ShieldCheck, Workflow } from 'lucide-react';

type AtlasMode = 'brief' | 'tour';

type BuildLayer = {
  id: string;
  number: string;
  title: string;
  short: string;
  responsibility: string;
  input: string;
  output: string;
  contract: string;
  files: string[];
  code: string;
  nodes: string[];
};

const layers: BuildLayer[] = [
  {
    id: 'frontend', number: '01', title: 'Frontend', short: 'make source meaningful',
    responsibility: 'The lexer, parser, resolver, and type checker turn NanoPass C into a typed program with source-aware diagnostics.',
    input: 'C-like source text', output: 'typed AST + diagnostics', contract: 'Names resolve before lowering; expressions have a type.',
    files: ['compiler/lexer/lexer.h', 'compiler/parser/parser.h', 'compiler/sema/resolve_scopes.h', 'compiler/sema/type_check.h'],
    code: `class Parser {
public:
  vector<unique_ptr<Stmt>> parseProgram() {
    vector<unique_ptr<Stmt>> program;
    while (!isAtEnd()) program.push_back(statement());
    return program;
  }
};`,
    nodes: ['tokens', 'AST', 'symbols', 'types'],
  },
  {
    id: 'ir', number: '02', title: 'Explicit IR', short: 'lower in small steps',
    responsibility: 'The middle end deliberately does not optimize the AST directly. CoreIR removes surface shortcuts, ANF fixes evaluation order, then CFG makes blocks and edges explicit.',
    input: 'typed AST', output: 'CoreIR → ANF → CFG', contract: 'Every representation has a printer and verifier.',
    files: ['compiler/ir/core/ast_to_core.h', 'compiler/ir/anf/core_to_anf.h', 'compiler/ir/cfg/anf_to_cfg.h', 'compiler/ir/pipeline.h'],
    code: `// IRPipeline::runAll
res.core = astToCore.lower(desugaredAST);
res.coreVerResult = verifyCoreIR(res.core);

res.anf = coreToANF.lower(res.core);
res.anfVerResult = verifyANF(res.anf);

res.cfg = anfToCFG.lower(res.anf);
res.cfgVerResult = verifyCFG(res.cfg);`,
    nodes: ['CoreIR', 'ANF', 'CFG', 'verifiers'],
  },
  {
    id: 'ssa', number: '03', title: 'SSA builder', short: 'give values one origin',
    responsibility: 'SSA construction discovers promotable locals, computes dominance frontiers, places phi nodes, and renames values along the dominator tree.',
    input: 'verified CFG', output: 'SSA program + statistics', contract: 'Each SSA value is defined once; phi inputs are explicit.',
    files: ['compiler/ir/ssa/ssa_builder.h', 'compiler/ir/ssa/ssa_ir.h', 'compiler/ir/ssa/ssa_printer.h', 'compiler/ir/ssa/ssa_def_use.h'],
    code: `const DominatorTree dom = DominatorAnalysis::run(cfg);
const DominanceFrontier frontier =
    DominanceFrontierAnalysis::run(cfg, dom);
const PhiPlan plan = placePhis(locals, frontier);

rename(cfg, out, dom, cfg.entryBlock,
       locals, stacks, valueMap, nextValue, stats);`,
    nodes: ['dominators', 'phi plan', 'rename', 'def-use'],
  },
  {
    id: 'proof', number: '04', title: 'Analysis + proof', short: 'check the structure',
    responsibility: 'Graph analyses answer reachability and dominance questions. The SSA verifier turns those graph facts into a gate after structural transformations.',
    input: 'CFG or SSA function', output: 'analysis result or verifier result', contract: 'A pass cannot silently continue with broken control flow or dominance.',
    files: ['compiler/ir/cfg/analysis/dominator_analysis.h', 'compiler/ir/cfg/analysis/dominance_frontier_analysis.h', 'compiler/ir/ssa/ssa_verifier.h', 'compiler/ir/ssa/analysis_manager.h'],
    code: `class DominatorAnalysis {
public:
  static DominatorTree run(const CFGFunction &function) {
    const auto rpo = ReversePostOrderAnalysis::run(function).order;
    // Cooper–Harvey–Kennedy iteration over reverse postorder.
    while (changed) { /* refine immediate dominators */ }
  }
};`,
    nodes: ['RPO', 'idom', 'frontier', 'verify'],
  },
  {
    id: 'optimizer', number: '05', title: 'Optimizer', short: 'rewrite with evidence',
    responsibility: 'Nano SSA passes implement constant folding, SCCP, DCE, CSE, CFG cleanup, LICM, strength reduction, and bounded inlining. Each is observable and verifier-gated.',
    input: 'verified SSA', output: 'optimized SSA + remarks', contract: 'Only legal transformations run; every applied or missed choice is recorded.',
    files: ['compiler/optimizer/ssa_optimizer.h', 'compiler/optimizer/advanced_optimizer.h', 'compiler/ir/ssa/call_graph.h', 'compiler/ir/ssa/ssa_verifier.h'],
    code: `class SSAFunctionPass {
public:
  virtual std::string_view id() const = 0;
  virtual SSAOptimizationResult run(
      SSAFunction &, SSAOptimizationContext &) = 0;
};

verifyAfter(pass, function, context);
// verifier failure becomes an internal compiler error`,
    nodes: ['SCCP', 'DCE', 'LICM', 'remarks'],
  },
  {
    id: 'backend', number: '06', title: 'Backend + service', short: 'lower or explain',
    responsibility: 'The verified compiler can lower SSA to LLVM, optionally use ORC, and expose bounded JSON endpoints for the web workbench.',
    input: 'verified SSA + options', output: 'LLVM IR, execution, JSON artifacts', contract: 'The browser receives structured facts; source text never becomes a shell command.',
    files: ['compiler/backend/llvm/ssa_llvm_lowering.h', 'compiler/backend/llvm/ssa_llvm_jit.h', 'compiler/service/compiler_service.h', 'compiler/server/nano_server.cpp'],
    code: `class SSALLVMLowering {
public:
  LoweringResult lower(const SSAProgram &program,
      std::string moduleName = "nano_ssa_module") const;
};

// nano_server accepts only allowlisted JSON fields.
// It never treats a client string as a file name or shell command.`,
    nodes: ['LLVM IR', 'AOT', 'ORC JIT', 'JSON API'],
  },
];

const pipelineArtifacts = [
  { id: 'frontend', label: 'AST', kind: 'tree' },
  { id: 'ir', label: 'CoreIR', kind: 'blocks' },
  { id: 'ir', label: 'ANF', kind: 'anf' },
  { id: 'proof', label: 'CFG', kind: 'cfg' },
  { id: 'ssa', label: 'SSA', kind: 'ssa' },
  { id: 'backend', label: 'LLVM', kind: 'llvm' },
] as const;

function LayerIcon({ id }: { id: string }) {
  if (id === 'frontend') return <Braces aria-hidden="true" />;
  if (id === 'ir') return <Workflow aria-hidden="true" />;
  if (id === 'ssa') return <GitShape aria-hidden="true" />;
  if (id === 'proof') return <ShieldCheck aria-hidden="true" />;
  if (id === 'optimizer') return <Cpu aria-hidden="true" />;
  return <Code2 aria-hidden="true" />;
}

function GitShape(props: SVGProps<SVGSVGElement>) {
  return <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round" {...props}><circle cx="6" cy="5" r="2" /><circle cx="18" cy="5" r="2" /><circle cx="12" cy="19" r="2" /><path d="M6 7v3a6 6 0 0 0 6 6M18 7v3a6 6 0 0 1-6 6" /></svg>;
}

export function BuildAtlas({ mode }: { mode: AtlasMode }) {
  const [selectedId, setSelectedId] = useState('ssa');
  const selected = layers.find((layer) => layer.id === selectedId) ?? layers[0];
  const isTour = mode === 'tour';

  if (!isTour) {
    return <section className="build-blueprint" aria-labelledby="build-blueprint-title">
      <header className="blueprint-navline"><span>CODEBASE / EXPLICIT REPRESENTATIONS / VERIFIER GATES</span><a href="/guide">Explore the build tour <ChevronRight aria-hidden="true" /></a></header>
      <div className="blueprint-hero">
        <div><h2 id="build-blueprint-title">How NanoPass<br />is <em>built.</em></h2><p>A map of the implementation.</p></div>
        <div className="blueprint-stack" aria-label="Layered compiler architecture">
          <div className="blueprint-slab syntax"><i /><b>Syntax / typed AST</b></div>
          <div className="blueprint-slab ir"><i /><b>Explicit representations</b></div>
          <div className="blueprint-slab flow"><i /><b>Control + data flow</b></div>
          <div className="blueprint-slab llvm"><code>%0 = add i64 %1, %2</code><b>Lowering / LLVM</b></div>
        </div>
      </div>
      <div className="blueprint-board">
        <aside className="blueprint-tree"><header><FolderTree aria-hidden="true" /><b>Codebase</b></header><div className="tree-root">nano-pass-compiler/</div><ul>{layers.map((layer) => <li key={layer.id}><button type="button" className={selected.id === layer.id ? 'selected' : ''} onClick={() => setSelectedId(layer.id)}><span>{layer.number}</span><b>{layer.title}</b><small>{layer.files[0].replace('compiler/', '')}</small></button></li>)}</ul><p>Click a subsystem to inspect its implementation surface.</p></aside>
        <section className="blueprint-pipeline" aria-label="NanoPass compilation pipeline"><header><h3>Compiler pipeline</h3><span>small passes / explicit contracts</span></header><div className="blueprint-artifacts">{pipelineArtifacts.map((artifact, index) => <div className="blueprint-artifact" key={`${artifact.label}-${index}`}><button type="button" className={selected.id === artifact.id ? 'selected' : ''} onClick={() => setSelectedId(artifact.id)}><b>{artifact.label}</b><i className={`artifact-glyph ${artifact.kind}`} aria-hidden="true"><span /><span /><span /></i><small>{artifact.id === 'frontend' ? 'read + type' : artifact.id === 'ir' ? 'normalize' : artifact.id === 'proof' ? 'analyze' : artifact.id === 'ssa' ? 'construct' : 'emit'}</small></button>{index < pipelineArtifacts.length - 1 ? <ArrowRight aria-hidden="true" /> : null}</div>)}</div><footer><div><b>Front-end</b><span>parse + elaborate</span></div><div><b>Middle-end</b><span>represent + transform</span></div><div><b>Back-end</b><span>lower + emit</span></div></footer></section>
        <aside className="blueprint-proof"><header><ShieldCheck aria-hidden="true" /><b>Verifier gates</b></header><ol><li>Typed-IR checks</li><li>CFG consistency</li><li>SSA dominance</li><li>Pass preservation</li><li>LLVM module checks</li></ol><strong><Check aria-hidden="true" /> Checks run at applicable stages</strong></aside>
      </div>
      <section className="blueprint-inspector" aria-live="polite"><header><span>SELECTED IMPLEMENTATION</span><b>{selected.title}</b><code>{selected.files[0]}</code></header><pre><code>{selected.code}</code></pre><aside><span>ACCEPTS</span><b>{selected.input}</b><ArrowRight aria-hidden="true" /><span>EMITS</span><b>{selected.output}</b><p>{selected.contract}</p></aside></section>
    </section>;
  }

  return <section className={`build-atlas build-atlas-${mode}`} aria-labelledby={`build-atlas-${mode}`}>
    <header className="build-atlas-header">
      <div>
        <p>{isTour ? '02 / codebase anatomy' : 'How it is built'}</p>
        <h2 id={`build-atlas-${mode}`}>{isTour ? <>Trace the compiler through its <em>actual modules.</em></> : <>A pipeline is not a picture.<br />It is a chain of <em>implemented contracts.</em></>}</h2>
      </div>
      <p>{isTour ? 'Choose a layer to see the corresponding C++ surface, the artifact it accepts, the artifact it produces, and the invariant that keeps the next stage honest.' : 'NanoPass is built as a C++20 compiler first: separate frontend, explicit IRs, graph algorithms, SSA construction, verifier-gated optimization, LLVM lowering, and a bounded service boundary.'}</p>
    </header>

    <div className="build-atlas-flow" aria-label="NanoPass implementation layers">
      {layers.map((layer, index) => <div className="build-flow-item" key={layer.id}>
        <button type="button" className={selected.id === layer.id ? 'selected' : ''} onClick={() => setSelectedId(layer.id)} aria-pressed={selected.id === layer.id}>
          <span>{layer.number}</span><LayerIcon id={layer.id} /><b>{layer.title}</b><i>{layer.short}</i>
        </button>
        {index < layers.length - 1 ? <ArrowRight aria-hidden="true" /> : null}
      </div>)}
    </div>

    <div className="build-atlas-workspace" key={selected.id}>
      <aside className="build-atlas-files">
        <header><FolderTree aria-hidden="true" /><span>Repository surface</span></header>
        <p>Selected layer · <b>{selected.title}</b></p>
        <ul>{selected.files.map((file, index) => <li key={file} className={index === 0 ? 'active' : ''}><span>{index === 0 ? '↳' : '·'}</span>{file}</li>)}</ul>
      </aside>
      <article className="build-atlas-code">
        <header><span>C++20 / source anatomy</span><b>{selected.files[0].split('/').pop()}</b></header>
        <pre><code>{selected.code}</code></pre>
        <footer><span>implementation surface</span><span>repository-backed</span></footer>
      </article>
      <aside className="build-atlas-contract">
        <header><span>Transformation contract</span><strong>{selected.number}</strong></header>
        <div className="build-io"><div><small>accepts</small><b>{selected.input}</b></div><ArrowRight aria-hidden="true" /><div><small>emits</small><b>{selected.output}</b></div></div>
        <p>{selected.responsibility}</p>
        <div className="build-contract-proof"><Check aria-hidden="true" /><span>{selected.contract}</span></div>
      </aside>
    </div>

    <div className="build-atlas-legend">
      <div><span>IMPLEMENTED AS</span><b>{selected.files.length} connected modules</b></div>
      <div><span>LOCAL VIEW</span><b>{selected.nodes.join(' · ')}</b></div>
      <div><span>WHY IT EXISTS</span><b>{selected.short}</b></div>
      {isTour ? <a href="/lab">Open this layer in the workbench <ChevronRight aria-hidden="true" /></a> : <a href="/guide">Open the detailed build tour <ChevronRight aria-hidden="true" /></a>}
    </div>
  </section>;
}


