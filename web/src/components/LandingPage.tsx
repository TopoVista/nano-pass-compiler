import { ArrowDown, ArrowUpRight, Check } from 'lucide-react';
import type { CSSProperties } from 'react';
import { ArchitectureDiagram } from './ArchitectureDiagram';
import { BuildAtlas } from './BuildAtlas';

const pipeline = [
  { name: "Source", action: "read", explanation: "The C-like program as a developer wrote it." },
  { name: "Typed AST", action: "resolve", explanation: "Names and types are attached to syntax." },
  { name: "CoreIR", action: "normalize", explanation: "Convenient syntax becomes smaller explicit forms." },
  { name: "ANF", action: "sequence", explanation: "Evaluation order and intermediate values become visible." },
  { name: "CFG", action: "branch", explanation: "Control flow is split into inspectable basic blocks." },
  { name: "SSA", action: "analyze", explanation: "Each value has one definition and traceable uses." },
  { name: "Optimizer", action: "prove", explanation: "Legal, verifier-gated rewrites simplify the SSA program." },
  { name: "LLVM", action: "emit", explanation: "The backend representation is prepared for machine code." },
];

const source = `int score(int x) {
  int bias = 4;
  if (x > 0) { return x * bias; }
  return bias;
}`;

const ssa = `bb0.entry:
  %0 = const 4
  %1 = cmp.gt %arg0, 0
  branch %1, bb1, bb2

bb1:
  %2 = mul %arg0, %0
  jump bb3

bb3:
  %3 = phi int [bb1: %2] [bb2: %0]
  return %3`;

function Arrow() {
  return <ArrowUpRight aria-hidden="true" className="landing-arrow" strokeWidth={1.8} />;
}

function Mark() {
  return <span className="landing-mark" aria-hidden="true"><i /><i /><i /></span>;
}

export function LandingPage() {
  return (
    <main className="public-shell">
      <header className="public-nav">
        <a className="public-brand" href="/" aria-label="NanoPass home"><Mark /><span>NanoPass</span></a>
        <nav aria-label="Primary navigation">
          <a href="#method">Method</a>
          <a href="#optimizer">Optimizer</a>
          <a href="#architecture">Architecture</a>
        </nav>
        <a className="public-nav-link" href="/lab">Open the lab <Arrow /></a>
      </header>

      <section className="public-hero" aria-labelledby="public-title">
        <div className="public-hero-copy">
          <h1 id="public-title">A compiler you can <em>interrogate.</em></h1>
          <p className="public-intro">Trace a small C-like language through typed representations, SSA construction, verification, optimization, LLVM, and execution, without treating the middle as a black box.</p>
          <p className="public-status-line">C++20 <span>·</span> LLVM 18 <span>·</span> SSA <span>·</span> ORC JIT</p>
          <div className="public-actions">
            <a className="public-action primary" href="/lab">Open Compiler Lab <Arrow /></a>
            <a className="public-action text" href="#method">See the method <ArrowDown aria-hidden="true" className="landing-arrow-down" strokeWidth={1.8} /></a>
          </div>
        </div>
        <div className="hero-artifact" aria-label="Compiler transformation from source to verified SSA">
          <div className="artifact-topline"><span>REPRESENTATION EXAMPLE</span><b>compiler pipeline</b></div>
          <div className="artifact-columns">
            <article className="artifact-code source"><header><span>01</span> Source / NanoPass C</header><pre><code className="hero-code-typing hero-source-typing">{source}</code></pre></article>
            <div className="artifact-transfer" aria-label="Parse, lower, and verify the program">
              {['Parse', 'Lower', 'Verify'].map((stage, index) => <div className="artifact-step" style={{ '--step-delay': `${index * 760}ms` } as CSSProperties} key={stage}><i>{stage}</i><span aria-hidden="true">↓</span></div>)}
            </div>
            <article className="artifact-code output"><header><span>06</span> SSA / @score</header><pre><code className="hero-code-typing hero-ssa-typing">{ssa}</code></pre></article>
          </div>
          <footer><span><Check aria-hidden="true" strokeWidth={2} /> SSA checks unique value definitions</span><span><Check aria-hidden="true" strokeWidth={2} /> CFG makes control flow explicit</span></footer>
        </div>
      </section>

      <section className="public-section public-architecture" id="architecture">
        <div className="section-title"><p>Architecture</p><h2>Compiler first.<br />Interface <em>second.</em></h2></div>
        <div className="architecture-copy"><p>The browser consumes a versioned, bounded JSON compiler API. It does not scrape terminal output or invent a compiler. The deterministic C++ compiler remains the source of truth; optional policies can only choose from known legal decisions and are always verifier-gated.</p><a href="https://github.com/TopoVista/nano-pass-compiler">Read the source on GitHub <Arrow /></a></div>
        <ArchitectureDiagram />
      </section>

      <section className="public-proof" aria-label="Compiler proof points">
        <div><strong>Source</strong><span>human intent</span></div>
        <i aria-hidden="true">→</i>
        <div><strong>IR</strong><span>explicit structure</span></div>
        <i aria-hidden="true">→</i>
        <div><strong>Verification</strong><span>machine-checkable invariants</span></div>
        <i aria-hidden="true">→</i>
        <div><strong>Execution</strong><span>bounded, observable results</span></div>
      </section>

      <section className="public-section public-method" id="method">
        <div className="section-title"><p>Every layer has a job.</p><h2>Not one tree.<br />A chain of <em>contracts.</em></h2></div>
        <div className="method-copy">
          <p>Surface syntax is convenient for people. SSA supports data-flow optimization. LLVM is the backend representation. NanoPass gives each state its own representation, printer, verifier where applicable, and public view.</p>
          <a href="/guide">Take the interactive guided tour <Arrow /></a>
        </div>
        <ol className="pipeline-list" aria-label="NanoPass pipeline">
          {pipeline.map((stage, index) => <li key={stage.name} tabIndex={0} aria-label={`${stage.name}: ${stage.explanation}`}><span>{String(index + 1).padStart(2, "0")}</span><b>{stage.name}</b><i>{stage.action}</i><small className="pipeline-explanation"><span>{stage.explanation}</span></small></li>)}
        </ol>
      </section>

      <BuildAtlas mode="brief" />

      <section className="public-feature public-feature-optimizer" id="optimizer">
        <div className="feature-number">A</div>
        <div className="feature-statement"><p>Optimization is a claim with evidence.</p><h2>See what changed.<br />See why it <em>didn’t.</em></h2><p>Pass telemetry, structured optimization remarks, and SSA verification keep every rewrite accountable. A skipped loop-invariant move has a concrete recorded reason, not a generated excuse.</p><a href="/optimizations">Inspect optimization evidence <Arrow /></a></div>
        <div className="optimization-artifact" aria-label="Optimization remark example" tabIndex={0}>
          <header><span>OPTIMIZATION REMARK</span><b>LICM</b></header>
          <div><i className="remark-status missed">MISSED</i><strong>%23 = load ptr %a</strong><p>Memory dependence could not be proven loop-invariant.</p><p className="optimization-hover-explain"><span>Hover to unpack it: the compiler saw a value that might change while the loop runs, so moving it would be unsafe.</span></p></div>
          <footer>reason code · MEMORY_DEPENDENCE_UNKNOWN</footer>
        </div>
      </section>

      <section className="public-feature public-feature-correctness">
        <div className="feature-number">B</div>
        <div className="correctness-diagram" aria-label="Differential correctness flow"><span>CoreIR interpreter</span><i>→</i><span>SSA O0</span><i>→</i><span>SSA O3</span><i>→</i><span>compare observable result</span></div>
        <div className="feature-statement"><p>Correctness is exercised, not presumed.</p><h2>Different paths.<br /><em>Same program.</em></h2><p>NanoPass can compare reference CoreIR execution with independently executed optimized SSA programs. It is differential testing, clearly labeled as empirical, not a substitute for formal proof.</p><a href="/verification">Run a bounded correctness check <Arrow /></a></div>
      </section>

      <section className="public-section public-runtime">
        <div className="section-title"><p>Execution is optional. Evidence is not.</p><h2>Build ahead of time.<br />Or observe <em>hot paths.</em></h2></div>
        <div className="runtime-rows">
          <article><span>LLVM AOT</span><b>Generate native artifacts through the configured LLVM toolchain.</b><small>Build-gated capability</small></article>
          <article><span>ORC JIT</span><b>Execute verified SSA through LLVM’s ORC APIs where the local build supports it.</b><small>Optional capability</small></article>
          <article><span>Profile-guided choices</span><b>Use measured function and block counts to inform bounded compiler decisions.</b><small>Profile schema is versioned</small></article>
        </div>
      </section>

      <section className="public-lab-callout">
        <div><p>THE WORKBENCH</p><h2>Bring a program.<br />Leave with a <em>trace.</em></h2></div>
        <div className="mini-lab" aria-label="Compiler Lab preview"><header><span>examples/optimizer.nano</span><b>O2</b></header><pre>{`int total = 0;\nwhile (i < 4) {\n  total = total + 2 * 3;\n}`}</pre><aside><strong>03</strong><span>pass details</span><strong>CFG</strong><span>graph state</span><strong>✓</strong><span>SSA valid</span></aside></div>
        <a className="public-action primary" href="/lab">Start in the lab <Arrow /></a>
      </section>

      <footer className="public-footer"><a className="public-brand" href="/"><Mark /><span>NanoPass</span></a><p>Transparent compiler engineering.</p><div><a href="/lab">Compiler Lab</a><a href="/guide">Guide</a><a href="https://github.com/TopoVista/nano-pass-compiler">GitHub</a></div></footer>
    </main>
  );
}

