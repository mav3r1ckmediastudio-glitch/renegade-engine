# Renegade Engine — Current Handoff

**Date:** 2026-08-02

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Active branch:** `phase3/light-material-authoring`

**Branch base:** `c5a2fb8` (`Add native terrain authoring foundation (#11)`)

**Pull request:** #12 into `main` (draft)

**Gate 1 remote commit:** `38c9f24`

**Gate 2 remote commit:** `32351d9`

**Add Light remote commit:** `b81c4bb`

**Discoverability correction commit:** `4ebc7a8` (patch-ready; not yet
Windows-verified)

**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Current truth

PR #11 is merged. Terrain Authoring V1 and the protected WISCENE document
workflow are on `main` at `c5a2fb8`. The project owner confirmed the terrain
sculpt and Save/Open path behaves as expected in packaged DX12 and Vulkan.

PR #12 is the active Light and Material Authoring branch. Gate 1 contains
UI-independent `LightService` and `MaterialService` contracts, command-based
Undo/Redo, no-op filtering, native-field preservation tests, material target
resolution, and service-level rejection of every terrain-owned material. The
project owner reported all four required PR checks green at `38c9f24`.

Gate 2 at `32351d9` adds the Renegade-owned selection-driven Light Inspector.
All four Windows checks passed and the project owner visually confirmed that
selecting `Gateway Beam` reveals the expected native Spot controls. The full
edit/Undo/Redo/Save/Open/Runtime/Vulkan behaviour matrix has not yet been
reported complete.

The project owner then tested Add Light at `b81c4bb`. All four native light
types illuminate correctly, but the custom hierarchy clipped entities below
its first visible page and only Directional had a viewport marker. This is a
behavioural gate failure despite green CI. Commit `4ebc7a8` corrects both
discoverability issues and remains pending fresh Windows CI and packaged visual
acceptance.

## Active slice — Add Light workflow

The next patch completes light authoring before Material UI starts. It:

- adds a permanent `ADD` menu to Renegade's owned top bar;
- offers Point, Spot, Directional, and Rectangle Light;
- calls Wicked's native `Scene::Entity_CreateLight` behind a UI-independent
  `CreateLightCommand`;
- enters click-to-place mode for Point, Spot, and Rectangle, raycasting the
  viewport against terrain and scene geometry;
- previews the surface hit and normal, consumes placement input, and supports
  `Esc` or right-click cancellation;
- aligns Spot and Rectangle emission to the clicked surface normal;
- creates Directional immediately five metres in front of the editor camera;
- draws distinct constant-size, selectable Point, Spot, Directional, and
  Rectangle markers entirely in Studio, with no serialized helper state;
- classifies visible entities from their native Wicked components and presents
  collapsible Lights, Models, Characters, Cameras, Terrain, Effects, Audio,
  and Scene Objects headers only when those categories are present;
- reveals the selected entity by expanding its category and scrolling its row
  into view, while retaining mouse-wheel scrolling for large categories;
- assigns unique creator-facing names such as `Point Light 2`;
- selects the new entity automatically so the existing Inspector opens;
- snapshots the complete native entity for Undo/Redo;
- reuses the existing Delete command and restoration path; and
- adds native component, all-type, naming, rectangle-shape, history, Delete,
  and WISCENE round-trip tests.

No stock Wicked Editor window is embedded. No Wicked file or submodule pointer
is changed. No material UI or terrain material path is added.

## Local validation of the Add Light patch

- `git diff --check` passes.
- Changed `LightService`, `LightTests`, `BridgeCommandTests`,
  `RenegadeStudioChrome`, and `StudioApplication` translation units pass local
  C++17 syntax validation using the pinned Wicked headers and a temporary SDL
  declaration shim required only because this container lacks SDL development
  headers.
- The container has no CMake installation, so Windows compilation and runtime
  behaviour are not claimed locally. PR #12 must run fresh Windows checks.

The `4ebc7a8` correction additionally passes `git diff --check`, preserves the
16-column feature ledger, and passes C++17 syntax validation for
`SceneService.cpp`, `RenegadeStudioChrome.cpp`, `StudioApplication.cpp`, and
`BridgeCommandTests.cpp` against pinned Wicked headers. The bridge test now
asserts native Light, Model, and Humanoid entities resolve to the intended
hierarchy categories. Full Windows build and visual interaction remain the
authoritative gate.

## Required Add Light acceptance

1. Apply and push the Add Light patch to
   `phase3/light-material-authoring` without merging PR #12.
2. Require all four PR checks to pass again.
3. Package Release and launch DX12 Studio.
4. Open `ADD` and create each of Point, Spot, Directional, and Rectangle Light.
5. Confirm every new light appears beneath the collapsible **Lights** header,
   is revealed and selected automatically, and opens the correct type-aware
   Inspector controls. Collapse/expand the header and wheel-scroll a long
   category without selecting or moving a scene entity.
6. Confirm Point, Spot, and Rectangle show a placement preview and appear at
   the clicked surface; Spot and Rectangle face along its normal. Confirm the
   placement click does not also select, sculpt, move a gizmo, or start camera
   navigation, and cancellation creates no light.
7. Confirm every type shows its distinct selectable viewport marker—Point
   burst, Spot cone, Directional sun/direction, and Rectangle panel/direction—
   and that selecting each marker opens its Inspector and reveals its hierarchy
   row. Confirm no editor marker or light visualizer appears in Runtime.
8. Verify Undo removes the new light, Redo restores it, Delete removes it, and
   Undo restores the deletion.
9. Edit representative values, then Save, close, reopen, and confirm all four
   created lights and their appearance remain.
10. Launch Runtime with the saved scene and compare light appearance.
11. Repeat the complete editor visual check with the `vulkan` argument.

A visible or behavioural failure stops the gate even if CI is green. Do not
begin the Material Inspector until the Light Inspector passes the applicable
checks above.

## Following slice

After the complete Add/Edit/Delete light workflow passes DX12, Save/Open,
Runtime, and Vulkan acceptance, wire the already-tested `MaterialService` into
a Renegade-owned Material Inspector for ordinary mesh materials only. Keep
terrain materials behind `TerrainService`, explain ambiguous multi-material
targets, and require a sculpt-preserving packaged regression check.

## Repository rules

- Do not edit Wicked or move its pin without an explicit core-patch/upstream
  task.
- Do not claim behavioural success from compilation alone.
- A visible failure overrides green CI.
- Persistent scene mutations belong in EngineBridge commands and require
  Undo/Redo plus Save/Open evidence.
- PR #12 remains a draft until every Light and Material gate is accepted.
