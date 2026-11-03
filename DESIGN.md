---
version: alpha
name: "NanoPass Compiler Lab"
description: "A dense, inspectable compiler workbench whose visual hierarchy follows program flow and compiler facts."
colors:
  primary: "#5AA7FF"
  secondary: "#47DFAD"
  canvas: "#F5F2F8"
  surface: "#FFFFFF"
  surfaceRaised: "#FCFAFF"
  border: "#E4DEEB"
  text: "#2A2338"
  muted: "#847B90"
  wire: "#7351AA"
  signal: "#208463"
  warning: "#A66316"
  danger: "#BF3F62"
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
  action: { backgroundColor: "{colors.signal}", textColor: "#FFFFFF", rounded: "{rounded.sm}" }
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
- **Register:** Hybrid. The public landing route is an editorial product introduction; the compiler routes are dense technical instruments. Task clarity, signal density, keyboard access, and observable compiler truth outweigh decorative branding inside the lab.
- **Memorable signature:** The public landing page uses a quiet lavender editorial frame and one large, rounded compiler-artifact hero. The workbench extends that same lavender system into a sharp left navigation rail and white dashboard panels, with deep plum surfaces reserved for source, IR, and terminal instruments. Mint is reserved for verified/successful facts. The Guide is a separate route that introduces a real interactive compiler journey.
- **Restraint:** No simulated metrics, ornamental hero, fake data, or strong shadows around every surface.
- **Anti-references:** Neon cyberpunk dashboards, generic SaaS metric-card grids, and terminal parody interfaces.
- **Token ownership/runtime mapping:** Existing runtime CSS is canonical. This file mirrors the exact variables in `web/src/styles.css`; panels, graph nodes, actions, and scrollbar surfaces consume those variables directly.

## Colors

`canvas` and `surface` establish a deep blue-black instrument panel. Primary blue is exclusively for IR selection, graph structure, and focus; mint signifies verified/successful execution facts. Warning and danger retain textual labels and icons; color is never their sole meaning.

## Typography

The Apple system font stack carries labels, navigation, and readable technical summaries without a third-party font request. SF Mono/Cascadia Mono (with system fallbacks) is restricted to identifiers, IR, timings, revision metadata, and graph-adjacent facts. Uppercase micro-labels describe source/type/status. A large headline is reserved for the Guide route only, where it introduces an actual interactive task rather than decorative marketing copy.

## Layout

The public landing route uses generous whitespace, a centered nav, a single statement of value, and a compiler-artifact preview rather than stock imagery. Desktop lab layout begins with a persistent 272px left rail, then a compact workspace header and a two-column instrument grid: source/terminal on the left, representation on the right, and graph/inspector evidence below. The pass timeline bridges those regions as a real compiler sequence. The source header exposes a Focus mode for sustained editing: it expands the source editor and its compiler terminal into the full workspace without changing source or compiler state. Below 900px the rail becomes a compact horizontal navigation; below 800px the panels stack in compiler-flow order. Every scrollable region keeps a visible, tokenized scrollbar.

## Elevation & Depth

Hierarchy comes from tonal layers and thin borders first. Light panels have a 12px technical radius and a restrained shadow; code, terminal, and IR viewers use deep plum surfaces with square interior lines. The landing hero gets its depth from a violet tonal field and soft nested code windows; the graph canvas uses the light dashboard surface so selected violet wires retain contrast.

## Shapes

Cards and controls use a 6–10px technical radius; no pills except contextual status-like chips. Graph nodes use a smaller radius to feel like actual process blocks, not dashboard cards.

## Components

### Foundational visual states

Buttons have visible hover, pressed, disabled, and blue focus states. Compiler busy state keeps the Run SSA control's geometry stable. Empty and unavailable data uses explicit text rather than an invented visualization.

### Buttons and actions

The green Run SSA action is the only dominant action. Neutral controls remain graphite. Destructive intent is not currently exposed in the lab.

### Navigation and data display

The top bar establishes route-level endpoints for Workbench, Guide, Graphs, and Optimizations. The terminal uses a VS Code-like command prompt, a command reference strip, and only renders command output assembled from compiler responses; it is deliberately not an operating-system shell. The inspector owns SSA provenance and a compact plain-language representation explanation. The pass rail reflects compiler order with proportional duration marks. Proof, benchmark, and policy panels use visible paths, samples, and gates derived from actual state; they never substitute decorative charts for missing data. Graph nodes render actual CFG/dominator/loop/call-graph JSON in deterministic flow layers and use the same `wire` semantic token as selected IR.

### Motion

Short color/border transitions acknowledge selection and control states. Reduced-motion preferences remove transitions. The graph does not use decorative animation.

## Do's and Don'ts

- **Do:** Let actual compiler hierarchy decide emphasis: source → representation → graph/remarks → runtime.
- **Do:** Use blue for relationships and selection, green for verified facts, and monospaced text for machine-readable details.
- **Don't:** Represent unavailable compiler capability as a successful panel or statistic.
- **Don't:** hide scrollbars, rely on color alone, or replace semantic controls with clickable containers.
