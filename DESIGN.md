---
version: alpha
name: "NanoPass Compiler Lab"
description: "A dense, inspectable compiler workbench whose visual hierarchy follows program flow and compiler facts."
colors:
  primary: "#4F8DFF"
  canvas: "#070A0D"
  surface: "#0F151B"
  surfaceRaised: "#151D25"
  border: "#26323D"
  text: "#EDF2F4"
  muted: "#8E9DAA"
  wire: "#4F8DFF"
  signal: "#2EE093"
  warning: "#F2B866"
  danger: "#FF7784"
typography:
  sans:
    fontFamily: "Segoe UI Variable, Inter, ui-sans-serif, system-ui, sans-serif"
  mono:
    fontFamily: "Cascadia Mono, SFMono-Regular, Consolas, monospace"
rounded:
  DEFAULT: "0.625rem"
  sm: "0.375rem"
  md: "0.625rem"
spacing:
  workspace-gap: "0.5625rem"
  panel-padding: "0.8125rem"
components:
  panel: { backgroundColor: "{colors.surface}", rounded: "{rounded.md}", padding: "{spacing.panel-padding}" }
  action: { backgroundColor: "{colors.signal}", textColor: "#042B1D", rounded: "{rounded.sm}" }
---

# NanoPass Compiler Lab Design System

## Overview

### Creative North Star

The lab is a **compiler signal plane**: a dark workstation inspired by visual code/workflow builders, where real compiler stages read as connected technical instruments rather than generic analytics cards. The central graph and IR panels carry the visual energy; the rest of the interface remains deliberately quiet.

### Product context and register

- **Audience and primary job:** Compiler engineers and learners inspect a source program as it becomes typed IR, SSA, optimized SSA, graph data, and executable output.
- **Target market(s) and evidence:** Global English-language developer tooling; the Vite lab and local compiler API document the supported product boundary in `docs/web-lab.md` and `docs/compiler-api.md`.
- **Locale(s) and language policy:** English only at present. No Japan-market or Japanese-locale behavior is inferred.
- **Usage scene:** A desktop-first, sustained technical workbench. A narrower layout stacks panels without losing controls.
- **Register:** Product. Task clarity, signal density, keyboard access, and observable compiler truth outweigh decorative branding.
- **Memorable signature:** Electric-blue graph wires and block outlines act as a visual trace through the compiler; green is reserved for successful verifier/runtime facts.
- **Restraint:** No simulated metrics, ornamental hero, fake data, or strong shadows around every surface.
- **Anti-references:** Neon cyberpunk dashboards, generic SaaS metric-card grids, and terminal parody interfaces.
- **Token ownership/runtime mapping:** Existing runtime CSS is canonical. This file mirrors the exact variables in `web/src/styles.css`; panels, graph nodes, actions, and scrollbar surfaces consume those variables directly.

## Colors

`canvas` and `surface` establish a near-black instrument panel. `wire` is exclusively for IR selection, graph structure, and focus. `signal` only signifies verified/successful execution facts. Warning and danger retain textual labels and icons; color is never their sole meaning.

## Typography

The local system UI font carries labels, navigation, and readable technical summaries without a third-party font request. Cascadia Mono (with system fallbacks) is restricted to identifiers, IR, timings, revision metadata, and graph-adjacent facts. Uppercase micro-labels describe source/type/status; large marketing-style typography is intentionally absent from the workbench.

## Layout

Desktop layout uses a source column, a dominant representation/graph canvas, and an inspector column. The pass timeline bridges those regions as a real compiler sequence. At 1080px the two main columns remain intact; at 800px the panels stack in compiler-flow order. Every scrollable region keeps a visible, tokenized scrollbar.

## Elevation & Depth

Hierarchy comes from tonal layers and thin borders first. Panels use only a recessed inset edge and a low-opacity shadow. The graph canvas is darker than its inspector shell so blue wires and selected blocks retain contrast.

## Shapes

Cards and controls use a 6–10px technical radius; no pills except contextual status-like chips. Graph nodes use a smaller radius to feel like actual process blocks, not dashboard cards.

## Components

### Foundational visual states

Buttons have visible hover, pressed, disabled, and blue focus states. Compiler busy state keeps the Run SSA control's geometry stable. Empty and unavailable data uses explicit text rather than an invented visualization.

### Buttons and actions

The green Run SSA action is the only dominant action. Neutral controls remain graphite. Destructive intent is not currently exposed in the lab.

### Navigation and data display

The top bar establishes the workbench identity. The inspector owns SSA provenance and optimization explanations. Graph nodes render actual CFG/dominator/loop/call-graph JSON and use the same `wire` semantic token as selected IR.

### Motion

Short color/border transitions acknowledge selection and control states. Reduced-motion preferences remove transitions. The graph does not use decorative animation.

## Do's and Don'ts

- **Do:** Let actual compiler hierarchy decide emphasis: source → representation → graph/remarks → runtime.
- **Do:** Use blue for relationships and selection, green for verified facts, and monospaced text for machine-readable details.
- **Don't:** Represent unavailable compiler capability as a successful panel or statistic.
- **Don't:** hide scrollbars, rely on color alone, or replace semantic controls with clickable containers.
