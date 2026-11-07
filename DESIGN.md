---
name: "NanoPass: Black Editorial Instrument"
description: "A monochrome editorial public site paired with a precise dark compiler workbench."
colors:
  primary: "#dfff4f"
  neutral-canvas: "#060606"
  neutral-surface: "#0d0d0d"
  neutral-raised: "#121212"
  neutral-ink: "#f4f2ec"
  neutral-muted: "#92918b"
  neutral-line: "#2c2c29"
  warning: "#ffd166"
  danger: "#ff8078"
typography:
  display:
    fontFamily: "Fraunces, Georgia, serif"
    fontSize: "108px"
    fontWeight: 600
    lineHeight: 0.92
    letterSpacing: "-0.07em"
  headline:
    fontFamily: "Inter, ui-sans-serif, system-ui, sans-serif"
    fontSize: "76px"
    fontWeight: 600
    lineHeight: 0.98
    letterSpacing: "-0.065em"
  body:
    fontFamily: "Inter, ui-sans-serif, system-ui, sans-serif"
    fontSize: "15px"
    fontWeight: 400
    lineHeight: 1.65
  label:
    fontFamily: "DM Mono, SFMono-Regular, Consolas, monospace"
    fontSize: "10px"
    fontWeight: 500
    lineHeight: 1.4
    letterSpacing: "0.08em"
rounded:
  instrument: "0px"
spacing:
  compact: "12px"
  panel: "20px"
  section: "96px"
components:
  button-primary:
    backgroundColor: "{colors.primary}"
    textColor: "#101207"
    rounded: "{rounded.instrument}"
    padding: "0 17px"
    height: "47px"
  panel:
    backgroundColor: "{colors.neutral-surface}"
    rounded: "{rounded.instrument}"
    padding: "{spacing.panel}"
---

# Design System: NanoPass

## Overview

**Creative North Star: “The Black Editorial Instrument.”**

NanoPass is a calm black research artifact in public and a focused technical instrument in the lab. The public route uses intentional negative space, a limited serif voice, and one compiler representation artifact; the workbench is dense only where the compiler creates facts.

Key characteristics:

- Black canvas, off-white content, thin structural rules.
- A single acid-lime signal for verification, primary action, and active state.
- Editorial serif only for a few public-heading words; UI and prose remain system-sans.
- No rounded SaaS cards, decorative gradients, stock visuals, or invented performance claims.

## Colors

The palette is a restrained neutral field with one high-contrast signal color.

### Primary

- **Verification lime** (#dfff4f): primary action, active navigation, verified state, and meaningful compiler trace only.

### Neutral

- **Black canvas** (#060606): application and public-site ground.
- **Instrument surface** (#0d0d0d): panels and terminals.
- **Raised surface** (#121212): headers and selected controls.
- **Off-white ink** (#f4f2ec): high-priority text.
- **Muted graphite** (#92918b): secondary explanation and metadata.
- **Structure line** (#2c2c29): all panel and grid boundaries.

**The Signal Rarity Rule.** Lime is never decorative. It appears only when a user may act, inspect an active view, or trust a compiler-reported success.

## Typography

**Display Font:** Fraunces with Georgia fallback.

**Body Font:** Inter with system fallback.
**Label/Mono Font:** DM Mono with platform monospace fallback.

The editorial serif earns attention on the public site; the workbench prioritizes fast technical scanning with sans and monospaced facts.

### Hierarchy

- **Display:** 56–108px, tight leading. Used only for the public narrative headline.
- **Headline:** 42–76px, tight leading. Used for landing-section statements and focused route titles.
- **Body:** 15px/1.65. Used for explanations intended for a general software engineer.
- **Label:** 9–10px mono with tracked uppercase. Used for compiler state, timing, stages, and controls.

## Layout

The public route uses a 1440px maximum editorial grid, high vertical breathing room, and asymmetric source-to-SSA artifacts. The workbench begins with a fixed 240px navigation rail, then a 68px context bar and 76px command bar. Its primary layout is a two-column source/terminal and representation system; secondary evidence follows in a compiler-flow sequence.

Below 900px, the rail becomes a compact horizontal navigation bar and panels stack in workflow order. At 620px and below, controls remain operable without horizontal content clipping.

## Elevation & Depth

The interface is intentionally flat. Borders and tonal layers establish hierarchy; shadows are reserved for a single public landing artifact. The lab does not use floating cards or translucent dashboard glass.

## Shapes

The instrument uses square corners, thin one-pixel borders, and compact controls. Circular forms are reserved for status lights, pass state, and execution steps.

## Components

- **Primary button:** verification lime with near-black text; reserved for “Open Compiler Lab” and “Run SSA.”
- **Secondary button:** dark surface with a structural border; used for non-destructive workbench actions.
- **Terminal:** visible state lights, compiler prompt, bounded command input, suggestions, output history, and shell-free command handling.
- **Panel:** thin border, compact header, no drop shadow, explicit artifact label.

## Do's and Don'ts

- Do explain compiler terminology adjacent to live output.
- Do keep optimization statements attached to compiler-recorded remarks.
- Do show an unavailable state rather than fabricate JIT, PGO, benchmark, or proof results.
- Do not use lime as general decoration.
- Do not make the terminal an arbitrary operating-system shell.
- Do not replace compiler artifacts with generic dashboard charts.
