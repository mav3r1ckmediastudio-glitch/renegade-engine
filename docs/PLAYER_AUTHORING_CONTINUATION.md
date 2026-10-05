# Player authoring continuation

This document records the owner-requested player authoring programme, alongside
PLAYER_ARMS_COMBAT_FRAMEWORK.md. It does not close a release gate.

## Accepted baseline

The owner confirmed the supplied shotgun assembly works: movement, aim in/out,
aimed fire, two loaded shots, partial/full reload, holster/equip and native
jump start/loop/land. The imported humanoid ragdoll collision failure was fixed
and the corrected runtime was owner accepted on 2026-10-05.

## Asset responsibilities

- Player Start owns spawn transform and facing.
- A future reusable Player definition owns controller/camera defaults, arms
  selection and starting equipment; explicit level overrides remain visible.
- First-person assembly owns presentation parts, anchors and paired animations.
- Equipment definitions own gameplay rules independently of arm geometry.
- Existing Player, Wicked/Jolt physics, native animation and damage/event seams
  remain authoritative. There is no new parallel controller.

## Implementation sequence

1. Editable firearm settings through the existing assembly save path.
2. Complete Player Start and native Player Inspector workflow.
3. Reusable Player definitions, placement, duplication and explicit overrides.
4. Complete arms authoring validation, automatic preview and replacement workflow.
5. Extract reusable equipment definitions and starting loadout; add ammunition
   reserve, fire modes, switching and staged action rules.
6. Hits/projectiles, Character damage and surface impacts.
7. Effects/audio, player health/death/respawn and HUD bindings.
8. Verify save/reopen, Test Level and independently packaged game end to end.

The initial firearm settings are embedded in the assembly recipe and native
payload so all accepted snapshot/package paths carry them without a second
unregistered file. This is an incremental authoring seam, not the final inventory
or independently reusable equipment asset format. The typed settings can later
move into an equipment definition while retaining compatibility.

## Current firearm controls

ASSEMBLY > WEAPON SETTINGS exposes capacity (1..1000), minimum shot interval
(0..60 seconds) and whether a nonempty weapon may reload. Settings share assembly
Undo/Redo and SAVE CHANGES / SAVE AS NEW. Existing recipes without these fields
retain capacity 2, interval 0 and partial reload enabled.

Fire is discrete. The authored firing clip must finish as well as the configured
interval before another shot. Paused time cannot expire the interval. Reload
refills only when both tracks finish; nonempty reload uses the partial pair or
falls back to the explicit full pair. No reserve ammunition is modelled yet.
Capacity describes gameplay ammunition; changing it does not change the physical
number of shells in an authored animation.

Selected Player Start displays an editor-only orange wireframe collision capsule.
It reads the runtime controller radius and total height each frame, follows the
spawn feet position, and updates after inspector edits and Undo/Redo. It stays
upright and unscaled like the runtime character, and is absent during Test Level.

Capsule display repair: connected 3D capsule edges are projected and drawn in
Studio Compose after scene temporal postprocessing, beside the transform gizmo.
This avoids the motion trails from temporal accumulation of debug-world lines.
The overlay is clipped to the scene viewport and camera planes; it is not a
serialized asset, does not enter Runtime, and is hidden behind assembly/grip workspaces.
