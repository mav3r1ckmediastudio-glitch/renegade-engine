# Phase 5 Gate 2 — Scene Camera Authoring

## Outcome

Gate 2 exposes native Wicked scene cameras through Renegade Studio without replacing the editor navigation camera or inventing a second camera format.

A creator can:

- create a serialized scene camera from the current editor view;
- edit perspective/orthographic projection;
- edit field of view, near/far clip planes, focal distance, aperture size and orthographic vertical size;
- align a selected scene camera back to the current editor view;
- move the editor view to the selected scene camera for visual checking;
- Undo/Redo camera creation, camera property edits and camera alignment;
- save/reopen the resulting native `CameraComponent` and `TransformComponent` through normal WISCENE serialization.

## Architecture

`CameraService` owns the stable EngineBridge exposure for native Wicked camera state. Studio never serializes a competing camera model.

The editor navigation camera remains transient editor state. `CREATE CAMERA FROM VIEW` copies its current view transform and curated projection state into a real scene entity. `VIEW FROM CAMERA` does the reverse only for editor preview: it adopts the selected scene camera's projection and transform into the transient editor camera and does not modify the authored scene.

`ALIGN CAMERA TO VIEW` changes only the selected scene camera transform and routes through the existing central `SetTransformCommand`.

## Curated creator-facing state

Gate 2 exposes:

- Perspective / Orthographic
- Field of View
- Near Clip
- Far Clip
- Focal Distance
- Aperture Size
- Orthographic Vertical Size

Wicked camera viewport dimensions are preserved but are not exposed as scene authoring controls because Studio owns the active viewport size.

## Deferred camera work

This gate does not yet claim gameplay camera management. Persistent primary-camera arbitration, blends/cuts, camera gameplay scripting and runtime switching belong with later gameplay/runtime integration. Gate 2 establishes the native scene-camera authoring seam those systems will consume.

## Acceptance

After green CI, one Release owner pass is enough:

1. Use **ADD → CAMERA** and confirm a `Camera` entity appears under CAMERAS at the current editor viewpoint.
2. Select it and change FOV, near/far clip and aperture; confirm the values survive save/reopen.
3. Switch Perspective/Orthographic and confirm the orthographic size control is usable.
4. Move the editor view and press **ALIGN CAMERA TO VIEW**; Undo/Redo once.
5. Move away, press **VIEW FROM CAMERA**, and confirm the editor returns to the authored camera viewpoint.
6. Undo camera creation and Redo it; confirm the same camera returns.

No additional Phase 5 systems are part of this gate.
