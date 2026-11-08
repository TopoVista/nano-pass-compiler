---
name: "NanoPass: Monochrome Editorial Instrument"
description: "A monochrome editorial public site paired with a deep-navy compiler workbench."
colors:
  primary: "#67e8f9"
  neutral-canvas: "#060a10"
  neutral-surface: "#0b1521"
  neutral-raised: "#0f1d2b"
  neutral-ink: "#edf5ff"
  neutral-muted: "#8ba0b5"
  neutral-line: "#1c3448"
  warning: "#ffc76d"
  danger: "#ff7f91"
typography:
  display:
    fontFamily: "Iowan Old Style, Baskerville, Georgia, serif"
    fontSize: "108px"
    fontWeight: 600
    lineHeight: 0.92
    letterSpacing: "-0.07em"
  headline:
    fontFamily: "-apple-system, BlinkMacSystemFont, Segoe UI, sans-serif"
    fontSize: "76px"
    fontWeight: 600
    lineHeight: 0.98
    letterSpacing: "-0.065em"
  body:
    fontFamily: "-apple-system, BlinkMacSystemFont, Segoe UI, sans-serif"
    fontSize: "15px"
    fontWeight: 400
    lineHeight: 1.65
  label:
    fontFamily: "SFMono-Regular, Menlo, Consolas, monospace"
    fontSize: "10px"
    fontWeight: 500
    lineHeight: 1.4
    letterSpacing: "0.08em"
rounded:
  instrument: "3px"
spacing:
  compact: "12px"
  panel: "20px"
  section: "96px"
components:
  button-primary:
    backgroundColor: "{colors.primary}"
    textColor: "#031019"
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

NanoPass is a calm black research artifact in public and a focused deep-navy technical instrument in the lab. The public route uses intentional negative space, a limited serif voice, and one compiler representation artifact; the workbench is dense only where the compiler creates facts.

Key characteristics:

- Public canvas is near-black and monochrome with off-white content and thin structural rules.
- The lab uses a restrained cyan action signal, mint for verified state, and amber for caution.
- Editorial serif only for a few public-heading words; UI and prose remain system-sans.
- No rounded SaaS cards, decorative gradients, stock visuals, or invented performance claims.

## Colors

The palette is a restrained neutral field with one high-contrast signal color.

### Primary

- **Inspection cyan** (#67e8f9): active navigation, inspectable values, and explicit user action in the lab.
- **Verification mint** (#61e6b0): compiler-reported success only.

### Neutral

- **Public black** (#050505): editorial landing ground.
- **Lab canvas** (#060a10): application ground.
- **Instrument surface** (#0b1521): panels and terminals.
- **Raised surface** (#0f1d2b): headers and selected controls.
- **Lab ink** (#edf5ff): high-priority text.
- **Muted blue-grey** (#8ba0b5): secondary explanation and metadata.
- **Structure line** (#1c3448): all lab panel and grid boundaries.

**The Signal Rarity Rule.** Cyan is never decorative. Mint appears only when a compiler-reported check has succeeded.

## Typography

**Display Font:** Iowan Old Style with Baskerville/Georgia fallback.

**Body Font:** Native Apple/system sans with Segoe UI fallback.
**Label/Mono Font:** SFMono/Cascadia Code with platform monospace fallback.

The editorial serif earns attention on the public site; the workbench prioritizes fast technical scanning with sans and monospaced facts.

### Hierarchy

- **Display:** 56–108px, tight leading. Used only for the public narrative headline.
- **Headline:** 42–76px, tight leading. Used for landing-section statements and focused route titles.
- **Body:** 15px/1.65. Used for explanations intended for a general software engineer.
- **Label:** 9–10px mono with tracked uppercase. Used for compiler state, timing, stages, and controls.

## Layout

The public route uses a 1440px maximum editorial grid, high vertical breathing room, and asymmetric source-to-SSA artifacts. The workbench begins with a fixed 216px navigation rail, then a 64px context bar and 76px command bar. Its primary layout is a two-column source/terminal and representation system; the Focus control deliberately expands the source and terminal into the full workspace.

Below 900px, the rail becomes a compact horizontal navigation bar and panels stack in workflow order. At 620px and below, controls remain operable without horizontal content clipping.

## Elevation & Depth

The interface is intentionally flat. Borders and tonal layers establish hierarchy; modest shadows are reserved for major workspace separation and the public landing artifact. The lab does not use floating cards or translucent dashboard glass.

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
