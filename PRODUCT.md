# Product

<!-- impeccable:product-schema 1 -->

## Platform

web

## Users

Students, compiler engineers, systems developers, and technical recruiters who need to inspect how a small C-like program is transformed, verified, optimized, and executed.

## Product Purpose

NanoPass Compiler Lab makes compiler construction observable. It lets a user move from source through typed intermediate representations, control-flow graphs, SSA, optimizer evidence, verification, LLVM output, and safe local execution.

## Positioning

NanoPass is not a code playground that happens to show compiler output. Its deterministic compiler emits the representations, analyses, diagnostics, pass telemetry, optimization remarks, and verifier results that the product presents as first-class evidence.

## Operating Context

The public site is used to understand the project and enter the lab. The lab is a desktop-first technical workbench for writing NanoPass C, selecting compiler views, inspecting actual service responses, and running only allowlisted compiler actions through a bounded local API.

## Capabilities and Constraints

- The core compiler is C++20/LLVM with explicit CoreIR, ANF, CFG, SSA, analysis, and optimizer infrastructure.
- The web client is a Vite/React/TypeScript application served separately from the C++ JSON service.
- The compiler API is the source of truth for execution, diagnostics, telemetry, graphs, and optimization evidence; the UI must not invent results.
- Public execution is intentionally bounded and should use the documented interpreter-only service policy unless a stronger isolation boundary is supplied.
- Hosted deployment configuration may be prepared in-repository but is not proof of a live deployment.

## Brand Commitments

The name is NanoPass Compiler Lab. The public surface should feel like an editorial research artifact: decisive, technical, legible, and evidence-led. The supplied monochrome editorial reference is a binding inspiration for pacing and restraint, not content to reproduce.

## Evidence on Hand

- Compiler source and tests in this repository.
- Real examples in `examples/` and compiler-generated API data.
- Existing lab application in `web/`.
- Compiler imagery in `docs/images/compiler-lab.png` and `web/design/compiler-lab-concept.png`.

## Product Principles

1. Show compiler facts rather than simulated outcomes.
2. Explain advanced ideas in precise language without hiding their mechanism.
3. Keep deterministic compiler infrastructure independent of AI assistance.
4. Make every major transformation inspectable, measurable, and verifier-gated.
5. Treat user programs as untrusted input at service boundaries.

## Accessibility & Inclusion

The web experience must be keyboard navigable, responsive, readable at high contrast, and understandable to a general software engineer without requiring prior compiler specialization.
