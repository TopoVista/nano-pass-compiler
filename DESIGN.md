---
version: alpha
name: "NanoPass Compiler Lab"
description: "A dense, inspectable compiler workbench whose visual hierarchy follows program flow and compiler facts."
colors:
  primary: "#5AA7FF"
  secondary: "#47DFAD"
  canvas: "#07111E"
  surface: "#0C1826"
  surfaceRaised: "#101F31"
  border: "#1E3348"
  text: "#EDF2F4"
  muted: "#8E9DAA"
  wire: "#4F8DFF"
  signal: "#2EE093"
  warning: "#F2B866"
  danger: "#FF7784"
typography:
  sans:
    fontFamily: "-apple-system, BlinkMacSystemFont, SF Pro Text, Helvetica Neue, Arial, sans-serif"
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

The lab is a **compiler signal plane**: a calm, Apple-system UI around deliberately high-contrast technical instruments. Real compiler stages read as an observable sequence rather than generic dashboard cards. The IR and graph canvases carry the visual energy; the rest of the interface remains deliberately quiet.

### Product context and register

- **Audience and primary job:** Compiler engineers and learners inspect a source program as it becomes typed IR, SSA, optimized SSA, graph data, and executable output.
- **Target market(s) and evidence:** Global English-language developer tooling; the Vite lab and local compiler API document the supported product boundary in `docs/web-lab.md` and `docs/compiler-api.md`.
- **Locale(s) and language policy:** English only at present. No Japan-market or Japanese-locale behavior is inferred.
- **Usage scene:** A desktop-first, sustained technical workbench. A narrower layout stacks panels without losing controls.
- **Register:** Product. Task clarity, signal density, keyboard access, and observable compiler truth outweigh decorative branding.
- **Memorable signature:** Primary blue represents structural relationships and selection, a cooler blue secondary supports machine surfaces, and mint is reserved for verified/successful facts. The Guide is a separate route that introduces a real interactive compiler journey.
- **Restraint:** No simulated metrics, ornamental hero, fake data, or strong shadows around every surface.
- **Anti-references:** Neon cyberpunk dashboards, generic SaaS metric-card grids, and terminal parody interfaces.
- **Token ownership/runtime mapping:** Existing runtime CSS is canonical. This file mirrors the exact variables in `web/src/styles.css`; panels, graph nodes, actions, and scrollbar surfaces consume those variables directly.

## Colors

`canvas` and `surface` establish a deep blue-black instrument panel. Primary blue is exclusively for IR selection, graph structure, and focus; mint signifies verified/successful execution facts. Warning and danger retain textual labels and icons; color is never their sole meaning.

## Typography

The Apple system font stack carries labels, navigation, and readable technical summaries without a third-party font request. SF Mono/Cascadia Mono (with system fallbacks) is restricted to identifiers, IR, timings, revision metadata, and graph-adjacent facts. Uppercase micro-labels describe source/type/status. A large headline is reserved for the Guide route only, where it introduces an actual interactive task rather than decorative marketing copy.

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

The top bar establishes route-level endpoints for Workbench, Guide, Graphs, and Optimizations. The terminal uses a VS Code-like command-reference strip and only renders command output assembled from compiler responses. The inspector owns SSA provenance and a compact plain-language representation explanation. The pass rail reflects compiler order with proportional duration marks. Proof, benchmark, and policy panels use visible paths, samples, and gates derived from actual state; they never substitute decorative charts for missing data. Graph nodes render actual CFG/dominator/loop/call-graph JSON in deterministic flow layers and use the same `wire` semantic token as selected IR.

### Motion

Short color/border transitions acknowledge selection and control states. Reduced-motion preferences remove transitions. The graph does not use decorative animation.

## Do's and Don'ts

- **Do:** Let actual compiler hierarchy decide emphasis: source → representation → graph/remarks → runtime.
- **Do:** Use blue for relationships and selection, green for verified facts, and monospaced text for machine-readable details.
- **Don't:** Represent unavailable compiler capability as a successful panel or statistic.
- **Don't:** hide scrollbars, rely on color alone, or replace semantic controls with clickable containers.
