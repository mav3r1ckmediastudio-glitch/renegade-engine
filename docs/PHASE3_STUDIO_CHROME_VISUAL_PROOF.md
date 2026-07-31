# Phase 3 - Renegade-owned Studio Chrome Visual Proof

## Outcome

Prove that Renegade can own the Studio shell's pixels while retaining Wicked
Engine as the renderer and low-level UI host.

This is a visual architecture gate, not a complete workspace milestone.

**Accepted:** The product owner approved the packaged Windows DX12 result from
PR #7 at `bf41c75` on 2026-07-31. Functional migration now continues in
`PHASE3_STUDIO_CHROME_FUNCTIONAL_SLICE.md`.

## Starting point

- Branch base: `main`
- Base commit: `944f8991a0fbc25885f27f4ae9f278708e95e334`
- Wicked pin: `3a800b7134aafe58461093c8abb2e274d4e64033`

## Design authority

In descending order:

1. `Renegade_Studio_UI_Design_Tokens_v1.0.json`
2. `Renegade_Brand_Guidelines_v1.0.pdf`, pages 18-23
3. `Renegade_Studio_Workspace_Specification_v1.0.pdf`
4. `Renegade_Studio_Workspace_Prototype_v1.0_Standalone.html`

The live editor on `main` is functional scaffolding and is not a visual
baseline.

## Architecture

Pinned Wicked makes `wi::gui::Widget::Render` virtual. The proof introduces
`RenegadeStudioChrome`, which inherits `Widget` only to participate in
Wicked's canvas/update/render scheduling. Its `Render` implementation draws
all visible shell primitives directly with `wi::image` and `wi::font`.

No Wicked source or submodule pointer changes. No stock `Window`, `Button`,
`Label`, or `TreeList` is visible in the authoring workspace during this
proof.

## Included

- 64 px Renegade-owned top application bar
- restrained wordmark and industrial geometry
- quiet menu labels with no permanent accent fill
- squared transform tools with a two-pixel active-state line
- scene metadata and play-state area
- 34 px active scene tab
- 320 px Hierarchy edge at the 1920 px reference width
- Hierarchy header, search field, live entity names, selection state
- viewport-first central canvas and viewport mode chips
- selection tag
- collapsed Asset Browser/Console/Output/Diagnostics tab strip
- 28 px low-noise status bar
- exact role separation for Forge, Tech Cyan, success, text, borders, and
  surfaces

## Explicit exclusions

- clickable menus and toolbar controls
- Hierarchy search, row selection, rename, reparent, and context menus
- Inspector presentation
- bottom-drawer expansion
- panel resizing, docking, floating, closing, restoring, or persistence
- Project Hub restyling
- scene dirty state and unsaved-change modal

Those features must not be built until this visual proof is accepted.

## Behaviour retained for the proof

- live DX12/Vulkan scene viewport
- viewport navigation and selection
- W/E/R transform shortcuts
- transform gizmo
- selection outline
- G grid toggle
- Undo/Redo and save shortcuts
- Project Hub startup and project launch

## Required verification

1. Build Windows x64 Debug and Release in GitHub Actions.
2. Launch the packaged Release in DX12.
3. Open the Proving Ground.
4. Capture a full 1920x1080 screenshot.
5. Compare it directly with the standalone prototype and the page-18 Studio
   anchor.
6. Confirm the viewport remains live and selectable.
7. Record either `VISUAL PROOF PASS` or `VISUAL PROOF FAIL`.

Do not mark this task complete from compilation alone. A visible failure
overrides green CI.
