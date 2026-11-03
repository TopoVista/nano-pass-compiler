const stages = ["Source", "Typed AST", "CoreIR", "ANF", "CFG / SSA", "Verified LLVM"];

export function LandingPage() {
  return (
    <main className="landing-shell">
      <header className="landing-nav">
        <a className="landing-brand" href="/" aria-label="NanoPass home">
          <span aria-hidden="true">N</span>
          NanoPass
        </a>
        <nav aria-label="NanoPass navigation">
          <a href="/lab">Workbench</a>
          <a href="/guide">Guide</a>
          <a href="/graphs">Graphs</a>
          <a href="/optimizations">Optimizations</a>
        </nav>
        <a className="landing-nav-action" href="/lab">
          Open the lab <span aria-hidden="true">↗</span>
        </a>
      </header>

      <section className="landing-hero" aria-labelledby="landing-title">
        <div className="landing-copy">
          <p className="landing-eyebrow">Transparent compilation, applied</p>
          <h1 id="landing-title">Watch a program become machine code.</h1>
          <p className="landing-lede">
            NanoPass makes each compiler decision inspectable: from parsed source
            through typed IR, SSA, optimization, verification, and LLVM output.
          </p>
          <div className="landing-actions">
            <a className="landing-primary" href="/lab">Open Compiler Lab <span aria-hidden="true">→</span></a>
            <a className="landing-secondary" href="/guide">Take the guided tour</a>
          </div>
        </div>
        <div className="landing-visual" aria-label="A compiler pipeline preview">
          <div className="landing-orbit landing-orbit-one" aria-hidden="true" />
          <div className="landing-orbit landing-orbit-two" aria-hidden="true" />
          <div className="landing-code-card landing-input-card">
            <header><span /> Source</header>
            <pre><code><b>int</b> main() {'{'}{`\n`}  <i>int</i> x = 4;{`\n`}  <i>return</i> x + 3;{`\n`}{'}'}</code></pre>
          </div>
          <div className="landing-transform" aria-hidden="true">
            <span>lower</span><i>↓</i><span>verify</span>
          </div>
          <div className="landing-code-card landing-output-card">
            <header><span /> Optimized SSA</header>
            <pre><code><i>%1</i> = const 7{`\n`}call @print(<i>%1</i>){`\n`}return <i>%1</i></code></pre>
          </div>
          <div className="landing-verifier"><i>✓</i> invariants verified</div>
        </div>
      </section>

      <section className="landing-proof" aria-label="NanoPass capabilities">
        <p>Built for understanding, not just output.</p>
        <div className="landing-proof-items">
          <span>Structured diagnostics</span>
          <span>SSA visualizations</span>
          <span>Verifier-gated passes</span>
          <span>Local-first execution</span>
        </div>
      </section>

      <section className="landing-pipeline" aria-labelledby="pipeline-title">
        <div>
          <p className="landing-eyebrow">One program · many honest views</p>
          <h2 id="pipeline-title">Every representation earns its place.</h2>
        </div>
        <ol>
          {stages.map((stage, index) => <li key={stage}><span>{String(index + 1).padStart(2, "0")}</span>{stage}</li>)}
        </ol>
      </section>
    </main>
  );
}
