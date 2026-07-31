# Phase 3 Brand Identity Application

> **Correction, folded into `docs/PHASE3_STUDIO_SHELL_REBUILD.md`:** once the
> brand guidelines PDF's own "UI design tokens" page (page 22) was found,
> two role assignments below turned out to be wrong: panel background is
> Carbon `#171716`, not Obsidian; the default border/ambient shadow is
> neutral Gunmetal `#474442`, not Forge (Forge is focus/active only - "glow
> is reserved for focus, launch and live energy," not an always-on effect).
> Both are fixed in the current code. The table below is left as originally
> written for history.

## Outcome

Studio's theme stops using the placeholder "holographic workstation"
palette invented before the brand existed and starts using the accepted
Renegade brand: `Renegade_Studio_UI_Design_Tokens_v1.0.json` (v1.0, 2026-07-30),
`Renegade_Brand_Guidelines_v1.0.pdf`, and the brand quick-reference slide,
all supplied by the project owner. This was blocked work: `HANDOFF.md` had
recorded "The approved concept imagery is still not stored in this
repository. Ask the project owner for the reference images before making
subjective redesign decisions." That reference material now exists and this
milestone applies it.

## What changed

All theme state lives in `Studio/src/StudioApplication.cpp`; no other file
defines a colour, so the change is contained there.

**Palette (anonymous namespace at the top of the file).** The `Hologram*`
names are kept to hold the diff down, but every value is now a pinned brand
hex rather than an invented approximation:

| Constant | Old (placeholder) | New | Brand token |
|---|---|---|---|
| `HologramIdle` | `(8,30,42,224)` | `(46,43,42,224)` | `graphite` `#2E2B2A` |
| `HologramFocus` | `(0,126,164,238)` | `(210,91,29,238)` | `forge` `#D25B1D` |
| `HologramActive` | `(92,232,255,255)` | `(237,123,45,255)` | `ignition` `#ED7B2D` |
| `HologramText` | `(196,244,255,255)` | `(210,200,188,255)` | `bone` `#D2C8BC` |
| `HologramMuted` | `(102,166,181,255)` | `(133,126,120,255)` | `ash` `#857E78` |
| `HologramBorder` | `(0,183,224,150)` | `(210,91,29,150)` | `forge` `#D25B1D` |
| `HologramPanel` | `(3,12,20,236)` | `(11,11,11,236)` | `obsidian` `#0B0B0B` |
| `HologramSelected` | `(0,102,138,245)` | `(210,91,29,245)` | `forge` `#D25B1D` |
| `WarningAmber` | `(255,150,40,255)` | `(240,166,64,255)` | `warning` `#F0A640` |
| `ViewportCyan` (new) | — | `(56,183,215,238)` | `tech_cyan` `#38B7D7` |

The brand quick-reference assigns Forge as the single general-purpose UI
accent and reserves Tech Cyan for the viewport specifically. That is the rule
applied here: every command-bar and panel interaction state (focus, active,
selected, tool-mode buttons, recent-project cards, text field focus/active,
scrollbar hover/grab) now runs on Forge/Ignition instead of the old cyan-blue
placeholder. Tech Cyan is used in exactly two places, both viewport-owned:
the Renegade grid shader and the grid-visibility toggle button, which now
lights up cyan while the grid is on and returns to neutral graphite when off.

**Grid shader constants** (`DrawEditorGrid`): minor line, major line, and the
Z axis now carry exact `tech_cyan` `#38B7D7`; the X-axis orientation accent
carries exact `ignition` `#ED7B2D` in place of the previous hand-picked amber
approximation. Behaviour (adaptive spacing, occlusion, depth write) is
unchanged; only colour values moved.

**Theme shape.** `theme.image.corners_rounding` radius moved from an
arbitrary `7.0f` to `8.0f`, matching `radius_px.panel` in the design tokens.
`theme.shadow_highlight_color` moved from a cyan glow to a Forge-tinted glow,
consistent with Forge now being the one accent used across panel chrome.

**Copy.** The Project Hub brand line changed from `"RENEGADE // IDENTITY
ACCEPTED"` to `"RENEGADE // BUILD WITHOUT PERMISSION."`, the accepted brand
tagline, in the one place the hub already surfaces a brand-identity string.

## Explicitly out of scope for this milestone

- **Typography.** The tokens specify Roboto Condensed (display/micro) and
  Roboto (body/UI). No font files exist anywhere in this repository or its
  submodule, and Studio currently renders through Wicked's built-in font
  with no custom `wi::font` style registered. Swapping type families needs
  licensed font assets added under `assets/` and a follow-up task; it was not
  bundled into this pass so the colour milestone stays reviewable on its own.
- **Layout geometry.** `spacing_px` and `control_height_px` (28/32/40) are
  close to, but not identical with, the current ad hoc button and field
  sizes (28/30/32 mixed) and toolbar/panel positions computed from them.
  Reflowing every `SetPos`/`SetSize` call to the token scale is a larger,
  layout-risk change that needs a Windows visual pass of its own; it is a
  reasonable next follow-up, not part of this milestone.
- `docs/FEATURE_MATRIX.csv` is unchanged: this milestone is a re-theme, not a
  change in which Wicked capability is exposed through which surface.

## Required verification (not yet performed)

This change was authored and reviewed in a non-Windows environment with no
DirectX12/Vulkan toolchain, so it has **not** been built, run, or visually
inspected. Per `AGENTS.md`, automated compilation is not visual acceptance
and a human Windows test session is required before this can be accepted:

1. `git submodule update --init --recursive`, then build Studio for DX12 and
   Vulkan per `docs/BUILD_WINDOWS.md` / `Tools/Build-Studio-Windows.ps1`.
2. Launch both `Run-RenegadeStudio-DX12.cmd` and
   `Run-RenegadeStudio-Vulkan.cmd`.
3. Confirm command-bar buttons, panels, text fields, scrollbars, and the
   Project Hub read as smoked obsidian/graphite chrome with Forge/Ignition
   interaction states, not the previous blue-cyan holographic look.
4. Confirm the grid, its Z axis, and the grid-toggle button are Tech Cyan;
   confirm the grid's X axis is Ignition orange.
5. Confirm panel corner rounding and the panel glow read as intended at the
   new radius and Forge tint.
6. Run the full Phase 3 Editor Visual Polish and Environment Authoring
   regression list (selection, gizmo, undo/redo, save/reopen) to confirm the
   colour-only change did not disturb behaviour.

Record the result in `docs/VERIFICATION_CHECKLIST.md` and update `HANDOFF.md`
per the usual process before this is treated as accepted.
