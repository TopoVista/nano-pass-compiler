import type { PolicyFeature } from '../lib/api';

export function PolicyFeatures({ features }: { features?: PolicyFeature[] }) {
  return <section className="policy-panel" aria-label="Optimization policy features"><header><strong>Policy features</strong><span>compiler extracted</span></header>{!features?.length ? <p className="quiet">Run or compile with telemetry to inspect static and dynamic optimization-policy features.</p> : features.map((feature) => <article key={feature.function}><b>@{feature.function} · {feature.hotness}</b><small>{feature.instructions} inst · {feature.blocks} blocks · {feature.loops} loops · {feature.callers} callers / {feature.callees} callees</small><small>{feature.dynamicCalls} calls · {feature.dynamicBlocks} dynamic blocks · {feature.dynamicLoopBackedges} loop backedges</small><small>decision evidence: {feature.hotnessReason}</small></article>)}</section>;
}
