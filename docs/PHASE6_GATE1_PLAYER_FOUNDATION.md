# Phase 6 Gate 1 — Player Start, Possession and Movement

## Outcome

A creator can add one Player Start to a Level, position and rotate it with the
existing gizmo, immediately edit its player settings in a dedicated Inspector,
save/reopen the Level, and enter Test Level or a packaged game as a controllable
first-person capsule using the accepted Wicked/Jolt character controller.

## Locked architecture

- The authored Player Start is a WISCENE entity containing Transform, Name and
  native Metadata. It is an inert spawn marker and settings asset, not the live
  player body.
- Studio draws its flat forward arrow and selected-state capsule as editor-only
  geometry. Neither visualizer is a renderable Scene object or Runtime asset.
- A Renegade metadata key distinguishes Player Start from the broader Wicked
  `MetadataComponent::Preset::Player` classification.
- Runtime resolves exactly one Player Start after each complete Scene load.
- Runtime creates one non-serialized character entity through Wicked's existing
  rigid-body/character component and removes/recreates it across Scene changes.
- Gameplay input is captured into an action-shaped `PlayerInputFrame`; the
  player service never polls raw keyboard or mouse state.
- Runtime remains the sole gameplay and camera owner. Studio only authors the
  marker and launches the existing separate Runtime process.
- The initial camera mode is explicitly first-person. Third-person and player
  arms are later bounded work, not accidental Gate 1 scope.

## Initial controls

| Action | Default binding |
|---|---|
| Move | W/A/S/D and left gamepad stick |
| Look | Mouse delta and right gamepad stick |
| Jump | Space and the primary lower gamepad face button |
| Sprint | Left Shift and left-stick press |

Gate 1 defaults are represented as named actions so Gate 2 can persist/rebind
them without replacing the player service.

## Creator workflow

1. Open a Level.
2. Choose `ADD > PLAYER START`.
3. Renegade creates the marker from the current editor view and selects it. Its
   flat ground arrow points along the horizontal camera heading; the selected
   marker also shows the configured Wicked/Jolt capsule guide.
4. Selection opens the dedicated `PLAYER START // SPAWN + CONTROLLER`
   Inspector automatically. Position and Y heading use the existing gizmo;
   capsule radius/total height, eye height, walk/sprint/jump speed, look
   sensitivity, slope, gravity and pitch limits are command-backed controls.
5. A second Player Start is rejected with a clear status. Runtime never chooses
   between multiple starts by entity order. A legacy Level with no Player Start
   retains the existing spectator camera and does not silently spawn a player.
6. Save and Reopen retain the marker identity, transform and settings.
7. Test Level and Build Windows Game consume the same WISCENE authority.

## Acceptance

- command-backed Create/Undo/Redo preserves the same entity identity;
- the arrow remains flat and points along the exact Runtime spawn yaw;
- selecting the arrow opens the dedicated settings Inspector;
- settings edits support Undo/Redo and survive WISCENE save/reopen;
- Player Start detection ignores an unrelated generic `Player` metadata preset;
- duplicate starts are rejected deterministically;
- Runtime spawns at the authored position and heading;
- WASD and gamepad movement use the camera-relative horizontal heading;
- mouse/gamepad look has bounded pitch and stable yaw;
- jump is accepted only through the Wicked character controller;
- the camera follows the live character rather than the authored marker;
- existing Scene physics remains the one Wicked/Jolt world;
- save/reopen, unsaved Test Level and packaged standalone paths match; and
- the #122 75 FPS baseline and editor interaction remain regression checks.

## Gate boundary

No audio, general Lua lifecycle, objectives, AI/navigation, Runtime player mesh,
arms, weapons, animation graph, input-remapping UI or in-process Studio
simulation is part of Gate 1. The Studio-only arrow and capsule are authoring
visualizers and do not broaden that boundary.
