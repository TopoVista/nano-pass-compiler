---
name: "NanoPass: Monochrome Editorial Instrument"
description: "A single monochrome editorial instrument across the public site and compiler workbench."
colors:
  primary: "#f5f5f1"
  neutral-canvas: "#050505"
  neutral-surface: "#0a0a0a"
  neutral-raised: "#111111"
  neutral-ink: "#f5f5f1"
  neutral-muted: "#9c9b94"
  neutral-line: "#282825"
  warning: "#ddd28f"
  danger: "#e5a8b2"
typography:
  display:
    fontFamily: "DM Serif Display, Georgia, serif"
    fontSize: "108px"
    fontWeight: 600
    lineHeight: 0.92
    letterSpacing: "-0.07em"
  headline:
    fontFamily: "Manrope Variable, Manrope, -apple-system, BlinkMacSystemFont, Segoe UI, sans-serif"
    fontSize: "76px"
    fontWeight: 600
    lineHeight: 0.98
    letterSpacing: "-0.065em"
  body:
    fontFamily: "Manrope Variable, Manrope, -apple-system, BlinkMacSystemFont, Segoe UI, sans-serif"
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

NanoPass is one calm black research instrument. The public route uses intentional negative space, a limited serif voice, and one compiler representation artifact; the workbench carries the same graphite-and-off-white material system while becoming denser only where the compiler creates facts.

Key characteristics:

- Public canvas and lab canvas are near-black and monochrome with off-white content and thin structural rules.
- The lab reserves muted mint for compiler-reported success, amber for caution, and rose for errors; ordinary navigation and code inspection stay neutral.
- Editorial serif only for a few public-heading words; UI and prose remain system-sans.
- No rounded SaaS cards, decorative gradients, stock visuals, or invented performance claims.

## Colors

The palette is a restrained neutral field with one high-contrast signal color.

### Primary

- **Editorial white** (#f5f5f1): navigation, inspection, and explicit user action.
- **Verification mint** (#b6e3c8): compiler-reported success only.

### Neutral

- **Public black** (#050505): editorial landing ground.
- **Lab canvas** (#050505): application ground.
- **Instrument surface** (#0a0a0a): panels and terminals.
- **Raised surface** (#111111): headers and selected controls.
- **Lab ink** (#f5f5f1): high-priority text.
- **Muted graphite** (#9c9b94): secondary explanation and metadata.
- **Structure line** (#282825): all lab panel and grid boundaries.

**The Signal Rarity Rule.** White carries interaction and inspection. Mint appears only when a compiler-reported check has succeeded.

## Typography

**Display Font:** locally bundled DM Serif Display with a Georgia fallback.

**Body Font:** locally bundled Manrope Variable with an Apple/system fallback. This prevents the public hierarchy from changing with the visitor's installed font set and keeps snapshots stable across supported browsers.
**Label/Mono Font:** SFMono/Cascadia Code with platform monospace fallback.

The editorial serif earns attention on the public site; the workbench prioritizes fast technical scanning with sans and monospaced facts.

### Hierarchy

- **Display:** 56–108px, tight leading. Used only for the public narrative headline.
- **Headline:** 42–76px, tight leading. Used for landing-section statements and focused route titles.
- **Body:** 15px/1.65. Used for explanations intended for a general software engineer.
- **Label:** 9–10px mono with tracked uppercase. Used for compiler state, timing, stages, and controls.

## Layout

The public route uses a 1440px maximum editorial grid, high vertical breathing room, and asymmetric source-to-SSA artifacts. The workbench begins with a fixed 248px navigation rail, then a 72px context bar and a compact command bar. Its primary layout is a two-column source/terminal and representation system; the Focus control deliberately expands the source and terminal into the full workspace. Graphs, optimizer evidence, runtime verification, and the field guide have their own persistent routes so the working editor does not become a card wall.

Below 1180px, the rail becomes a compact icon rail and the output moves beneath the editor. At 760px and below, controls remain operable without horizontal content clipping.

## Elevation & Depth

The interface is intentionally flat. Borders and tonal layers establish hierarchy; modest shadows are reserved for major workspace separation and the public landing artifact. The lab does not use floating cards or translucent dashboard glass.

## Shapes

The instrument uses square corners, thin one-pixel borders, and compact controls. Circular forms are reserved for status lights, pass state, and execution steps.

## Components

- **Primary button:** editorial white with near-black text; reserved for “Open Compiler Lab” and “Run SSA.”
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
