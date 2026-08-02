# Renegade Engine — Current Handoff

**Date:** 2026-08-02

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Active branch:** `phase3/light-material-authoring`

**Branch base:** `c5a2fb8` (`Add native terrain authoring foundation (#11)`)

**Pull request:** #12 into `main` (draft)

**Gate 1 remote commit:** `38c9f24`

**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

The GitHub integration can read PR #12 but returned HTTP 403 when asked to
update its metadata. Its current title remains
`Plan and prove light material authoring bridge`; the intended milestone title
is `Light and Material Authoring`. This metadata cleanup does not block CI.

## Current truth

PR #11 is merged. Terrain Authoring V1 and the protected WISCENE document
workflow are on `main` at `c5a2fb8`. The project owner confirmed the terrain
sculpt and Save/Open path behaves as expected in packaged DX12 and Vulkan.

PR #12 is the active Light and Material Authoring branch. Gate 1 contains
UI-independent `LightService` and `MaterialService` contracts, command-based
Undo/Redo, no-op filtering, native-field preservation tests, material target
resolution, and service-level rejection of every terrain-owned material. The
project owner reported all four required PR checks green at `38c9f24`.

## Gate 2 — Light Inspector implementation

The next patch adds the first visible authoring slice without starting the
Material Inspector. It:

- extends `LightState` with Wicked's native source `radius`, `length`, and
  `height` fields;
- updates `RenegadeLightTests` so these shape fields are proven through native
  apply, Undo, Redo, sanitization, and no-op detection;
- adds a Renegade-owned Light section beneath Transform for selected light
  entities;
- exposes Directional, Point, Spot, and Rectangle types;
- exposes RGB colour, intensity, range, cast-shadow, real volumetrics, and
  volumetric boost;
- exposes type-aware source shapes matching Wicked's reference editor:
  Directional radius; Point radius and capsule length; Spot cone angles and
  radius; Rectangle width and height;
- disables range for Directional lights;
- previews slider edits directly, restores the captured native state at drag
  completion, and executes exactly one `SetLightCommand`; and
- preserves the existing Transform section and every unexposed Wicked light
  field.

No stock Wicked Editor window is embedded. No Wicked file or submodule pointer
is changed. No material UI or terrain material path is added.

## Local validation of the Gate 2 patch

- `git diff --check` passes.
- Changed `LightService`, `LightTests`, and `StudioApplication` translation
  units pass local C++17 syntax validation using the pinned Wicked headers.
- CSV shape and documentation consistency checks remain part of the final
  packaging pass.
- Windows compilation and runtime behaviour are not claimed locally; PR #12
  must run fresh Windows checks after the patch is pushed.

## Required Gate 2 acceptance

1. Apply and push the Gate 2 patch to
   `phase3/light-material-authoring` without merging PR #12.
2. Require all four PR checks to pass again.
3. Package Release and launch DX12 Studio.
4. Select `Gateway Beam`; confirm the Inspector shows Spot controls and no
   Rectangle/Point-only shape controls.
5. Tune RGB, intensity, range, inner/outer cone, radius, cast-shadow,
   volumetrics, and boost; confirm the viewport responds live.
6. Confirm one completed slider drag creates exactly one Undo step, then verify
   Undo and Redo.
7. Change selected native lights through Directional, Point, Spot, and
   Rectangle; verify the correct type-specific shape controls and visible
   renderer behaviour.
8. Save, close, reopen, and confirm the authored values and appearance remain.
9. Launch Runtime with the saved scene and compare the light appearance.
10. Repeat the editor check with the `vulkan` argument.

A visible or behavioural failure stops the gate even if CI is green. Do not
begin the Material Inspector until the Light Inspector passes the applicable
checks above.

## Following slice

After Gate 2 passes, wire the already-tested `MaterialService` into a
Renegade-owned Material Inspector for ordinary mesh materials only. Keep
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
