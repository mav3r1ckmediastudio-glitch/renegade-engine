# Phase 3 — Renegade-owned Studio chrome functional slice

## Outcome

The visual proof at `bf41c75` was accepted by the product owner on packaged
Windows DX12. This slice keeps that exact information architecture and begins
moving the already-accepted editor behaviour onto Renegade-rendered controls.

## Included

- The viewport is bounded by the 320 px Hierarchy and 360 px Inspector at
  1920-wide layouts.
- Hierarchy rows select the real scene entity and refresh the shared
  `SelectionService` state.
- The owned hierarchy search field filters the visible rows without changing
  scene data.
- Select, Translate, Rotate and Scale are clickable square command states.
- The right-hand Inspector is restored on the owned surface.
- Transform numeric fields, Environment presets, sky mode, fog, volumetric
  cloud values, height fog and cloud-shadow toggles retain their existing
  `CommandService` and Undo/Redo wiring.
- Asset Browser, Console, Output and Diagnostics tabs toggle one collapsed-by-
  default bottom drawer. Their content remains intentionally limited to the
  capabilities that actually exist.
- The stock FPS/error overlay is disabled because it collided with the
  wordmark; diagnostics have an owned drawer destination.

## Rendering boundary

The functional fields inherit Wicked input mechanics for focus, keyboard
entry, selection and callbacks. `RenegadeTextInputField`, `RenegadeButton`,
`RenegadeCheckBox` and `RenegadeComboBox` override every visible pixel. Stock
widget rendering is not used for the Studio workspace.

## Preserved behaviour

This slice does not replace scene, selection, command or weather services.
Viewport navigation, viewport selection, gizmos, grid, outline, duplicate,
delete, save, save-as, reopen, Undo/Redo, atmosphere, fog, volumetric clouds
and cloud shadows continue through the previously accepted implementation.

## Explicit limits

- Asset import is still not built; the Asset Browser says so.
- Console/build/diagnostic data models are not built; the drawer supplies the
  approved interaction and destination rather than invented output.
- Menus, play controls, scene-tab lifecycle and docking persistence remain
  later vertical slices.
- Light and material authoring remain the next engine-feature milestone after
  the owned workspace regression gate.

## Acceptance

Required packaged Windows Release checks:

1. Hierarchy click and text filter select/filter the expected real entities.
2. Select/Translate/Rotate/Scale respond to click and W/E/R remain unchanged.
3. Transform entry, gizmo, Undo/Redo, save and reopen pass.
4. Selecting Environment exposes the owned weather Inspector.
5. Clouds, cloud shadows, fog and height fog visibly change; Undo/Redo and
   save/reopen preserve them.
6. Every bottom tab opens and closes the drawer while viewport input remains
   constrained to the visible viewport.
7. No stock pill-shaped Studio workspace control or overlapping diagnostics
   text is visible.
8. Vulkan receives the same regression pass after DX12 acceptance.
