# Renegade Engine — Development Handoff

**Handoff date:** 2026-07-30

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Canonical branch:** `main`

**Canonical main commit:** `a80e180909ed9e0a62618bc4dee5916e19621d67`

**Milestone branch under repair:** `phase3/editor-visual-polish`

**Reviewed branch tip:** `90531fb`

**Repair branch:** `fix/editor-grid-render-pass`

**Wicked Engine pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

**Active phase:** Phase 3 — Studio foundation

## Current status

Phase 2 is complete. The Phase 3 Project Hub, viewport interaction, editor
usability, and Viewport/Proving Ground Visual Foundation milestones have passed
their stated Windows DX12 functional gates. The visual-foundation work is merged
to `main`.

Claude's Editor Visual Polish work is retained on
`phase3/editor-visual-polish`. It adds:

- a Renegade-owned infinite shader grid;
- persisted grid visibility through `ProjectService`;
- a smaller and cleaner transform gizmo;
- a thinner selection outline;
- unique generated-entity names;
- a non-clipping workspace title; and
- a proper Content Browser empty state.

The branch compiled successfully in GitHub Actions before this repair, but green
CI was not sufficient to validate its live renderer integration.

## Render-pass defect and repair

The reviewed branch overrode `RenderPath3D::RenderTransparents`, called the
Wicked base implementation, and issued the grid draw immediately afterward.
Its comment claimed the main render target and depth buffer remained bound.

That claim is false at the pinned Wicked commit. Wicked ends its transparent
and distortion render passes before `RenderTransparents` returns, and also
updates a downsampled scene copy. A bare `Draw(3)` after the base call therefore
has no active render pass. This can silently lose the grid on DX12 and is
invalid for Vulkan's explicit render-pass model.

The repair keeps the same render hook and shader design, but after the base call
it now:

1. skips the pass when the grid is hidden, the Hub is visible, resources are
   unavailable, or no camera is bound;
2. opens an explicit pass loading `rtMain_render` and `depthBuffer_Main`;
3. adds an `rtMain` resolve attachment when MSAA is enabled;
4. restores Wicked's internal-resolution viewport;
5. restores the viewport crop scissor;
6. draws the grid; and
7. ends the pass before post-processing begins.

This preserves depth occlusion, HDR/bloom/tonemapping, MSAA, and the existing
shader implementation without modifying Wicked source.

## Source of truth and required reading

Before changing code, read:

1. `AGENTS.md`
2. `README.md`
3. `docs/PROJECT_CHARTER.md`
4. `docs/ARCHITECTURE.md`
5. `docs/ROADMAP.md`
6. `docs/AI_WORKFLOW.md`
7. `docs/PHASE3_PROJECT_HUB.md`
8. `docs/PHASE3_VIEWPORT_INTERACTION.md`
9. `docs/PHASE3_EDITOR_USABILITY.md`
10. `docs/PHASE3_VIEWPORT_VISUAL_FOUNDATION.md`
11. `docs/PHASE3_EDITOR_VISUAL_POLISH.md`
12. the relevant rows in `docs/FEATURE_MATRIX.csv`

The repository is canonical project memory. Do not substitute an earlier chat
or create a competing tool-specific plan.

## Non-negotiable architecture rules

- Wicked Engine is a pinned upstream submodule. Do not edit its source or move
  the submodule pointer without an explicitly approved upstream-sync or
  documented core-patch task.
- Prefer implementation in `Studio`, `EngineBridge`, `Runtime`, `Tools`, and
  `Tests`.
- Studio owns presentation and input routing.
- Persistent scene mutations belong in UI-independent `EngineBridge` commands
  and require Undo/Redo.
- Bridge code must not call renderer-dependent `Scene::Update()`. The
  render-capable Studio frame loop owns scene advancement.
- Studio-owned renderer extensions must open their own valid render pass unless
  they are called from inside a documented active Wicked pass.
- Keep `.renegade` project metadata above Wicked's WISCENE format.
- Windows x64/DX12 is the initial target. Vulkan on Windows is the required
  development cross-check.
- Green CI is compilation evidence, not visual acceptance.
- A visible or behavioural failure overrides nominal automated success.

## Accepted milestones

### Phase 2

Project-owner Windows report:

```text
DX12 PASS / VULKAN PASS / DPI PASS / INPUT PASS / HDR NOT AVAILABLE
```

### Project Hub

Confirmed on Windows:

- project creation;
- reopen and recent-project registry;
- generated WISCENE load;
- hierarchy population;
- Proving Ground render; and
- stable VSync-limited 75 FPS on an RTX 4070 Ti.

### Viewport interaction

Confirmed on Windows:

- viewport click selection and empty-space deselect;
- synchronised viewport, hierarchy, Inspector, gizmo, and outline selection;
- right-mouse freelook;
- WASD/Q/E movement;
- Shift acceleration; and
- mouse-wheel speed control.

### Editor usability

Implemented through `d04e346` and confirmed in packaged DX12 Release:

- creator-facing hierarchy filtering;
- unobstructed command bar and viewport-only FPS;
- position, rotation, and scale fields;
- Move/Rotate/Scale gizmos;
- W/E/R tool shortcuts;
- F focus;
- Ctrl+D duplicate;
- Delete;
- Ctrl+Z Undo and Ctrl+Y Redo;
- Ctrl+S Save and Ctrl+Shift+S Save As; and
- saved transform persistence.

### Viewport and Proving Ground Visual Foundation

Merged to `main` through PR #1 and accepted functionally on Windows:

- renderer-owned interim grid removed serialized grid geometry;
- renderer-independent Proving Ground blueprint;
- PBR composition and deterministic ground relief;
- serialized weather entity;
- realistic sky and aerial perspective;
- distance fog and height fog;
- per-light volumetric scattering;
- headless blueprint and scene round-trip coverage; and
- no renderer-dependent `Scene::Update()` in `EngineBridge`.

The project owner observed roughly 800 FPS with VSync disabled on an RTX
4070 Ti, so the viewport currently has substantial performance headroom.

## Editor Visual Polish implementation

The reviewed branch history is:

| Commit | Subject |
|---|---|
| `0b7d590` | Replace the stock grid helper with a Renegade shader grid |
| `5669f38` | Persist editor preferences beside the recent-project registry |
| `2f97604` | Make generated entity names unique and cover it with a test |
| `d5afe55` | Record the Phase 3 editor visual polish |
| `90531fb` | Lift the editor grid off the deck plane so it survives the depth test |

Important implementation details:

- `RenegadeGridVS.hlsl` emits a full-screen triangle from `SV_VertexID`.
- `RenegadeGridPS.hlsl` intersects the camera ray with the ground plane.
- `ddx`/`ddy` produce approximately screen-space-stable line widths.
- grid spacing cross-fades between powers of ten.
- the pixel shader writes projected `SV_Depth`, inheriting Wicked's reverse-Z
  convention and using normal depth testing.
- grid visibility is an editor preference, never WISCENE or `.renegade` state.
- the X axis is deliberately amber; minor/major lines and the Z axis are cyan.
- the grid is lifted 0.02 units above the deck to avoid a coplanar reverse-Z
  depth-test loss.

## Validation state

Completed before the render-pass repair:

- GitHub Actions Windows baseline: green;
- GitHub Actions Studio Debug/Release: green;
- bridge tests: green; and
- Wicked source and submodule pointer: unchanged.

Required after the repair:

- `git diff --check`;
- CMake configure/integrity checks available in the current environment;
- Windows Studio Debug and Release builds;
- `RenegadeBridgeTests` in both configurations;
- packaged DX12 visual test; and
- packaged Vulkan visual test.

The current environment cannot replace the project owner's packaged Windows GPU
test. Do not report the grid as accepted until the exact repaired commit has
been observed.

## Packaged Windows acceptance

Test the exact Release artifact through:

```text
Run-RenegadeStudio-DX12.cmd
Run-RenegadeStudio-Vulkan.cmd
```

Confirm:

1. the grid renders to the horizon;
2. lines remain crisp at low angles without severe crawling or moiré;
3. spacing changes smoothly with camera height and distance;
4. scene geometry correctly occludes the grid;
5. minor/major lines and Z axis are cyan, while X is the deliberate amber
   orientation accent;
6. the command-bar toggle and `G` both work;
7. grid visibility persists across restart;
8. the grid never appears over the Project Hub;
9. no grid entity appears in the hierarchy or a saved WISCENE;
10. the smaller gizmo works in all three modes;
11. the selection outline remains thin and save-isolated;
12. generated hierarchy names are unique;
13. long workspace titles do not clip;
14. selection, navigation, Inspector editing, Undo/Redo, Save, and reopen are
    unaffected; and
15. VSync-off performance shows no meaningful regression.

Required report:

```text
DX12 GRID PASS / GRID PERSISTENCE PASS / GIZMO PASS / OUTLINE PASS /
HIERARCHY PASS / REGRESSION PASS / PERFORMANCE PASS / VULKAN PASS
```

## Known limitations and deferred work

- The visual-polish grid has not yet passed packaged DX12/Vulkan observation.
- Full generated-scene reload remains a packaged GPU test because Wicked's
  primitive render-data creation needs a graphics device.
- Projects generated before the visual-foundation milestone may retain old
  hidden serialized grid helpers; no legacy migration exists yet.
- Environment values are still hard-coded. The next bounded milestone is an
  Environment Authoring panel with undoable, persistent controls.
- Arbitrary local fog volumes are not implemented.
- Camera speed and editor layout are still session-only.
- Scene tabs, docking, dirty-state tracking, unsaved-change prompts, crash
  recovery, asset import, terrain authoring, and the Identity Handshake remain
  out of scope until scheduled.

## Next milestone after acceptance

Build the bounded Environment Authoring vertical slice. It should allow a
creator to correct the known visual issues without rebuilding:

- sun direction, colour, intensity, and shadow toggle;
- realistic-sky mode and curated atmosphere controls;
- distance fog and height-fog controls;
- light volumetric scattering;
- material base colour, roughness, metalness, and emissive strength;
- viewport exposure, bloom, and AO controls; and
- full persistence plus Undo/Redo through `EngineBridge`.

Do not expose Wicked's entire weather structure raw. Use curated controls and
presets, with specialist parameters behind an Advanced disclosure.

## Delivery model

Each push/build/test cycle is expensive. Group roughly 4–8 related improvements
into one coherent milestone, retain reviewable commits for major concerns,
push one development branch, run CI once for the finished batch, and perform
one meaningful Windows test session.

Do not merge the visual-polish branch until both CI and the packaged DX12/Vulkan
acceptance report pass.
