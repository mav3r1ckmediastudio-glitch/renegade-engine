# Player authoring continuation

This document records the owner-requested player authoring programme, alongside
PLAYER_ARMS_COMBAT_FRAMEWORK.md. It does not close a release gate.

## Merged acceptance checkpoint - 2026-10-05

PR #178 merged into main on 2026-10-05 at `7105a95ddcc103d6a024a17fddefd62f705a86b3`.
All four pre-merge Windows checks passed: Studio Debug/Release and baseline
Debug/Release. The owner confirmed the expected player/shotgun behaviour in the
existing v2 game project and explicitly accepted the PR's functionality.
This records functional acceptance of the merged scope, not completion of every
P1 requirement or of the full Alpha Playability combat programme.

See [PLAYER_AUTHORING_UX_FOLLOWUP](PLAYER_AUTHORING_UX_FOLLOWUP.md) for the required
UI/UX revisit. Functionality is accepted; the setup experience is not final.

## Accepted baseline

The owner confirmed the supplied shotgun assembly works: movement, aim in/out,
aimed fire, two loaded shots, partial/full reload, holster/equip and native
jump start/loop/land. The imported humanoid ragdoll collision failure was fixed
and the corrected runtime was owner accepted on 2026-10-05.

## Asset responsibilities

- Player Start owns spawn transform and facing.
- A reusable Player prefab owns controller/camera defaults, arms
  selection and starting equipment; explicit level overrides remain visible.
- First-person assembly owns presentation parts, anchors and paired animations.
- Equipment definitions own gameplay rules independently of arm geometry.
- Existing Player, Wicked/Jolt physics, native animation and damage/event seams
  remain authoritative. There is no new parallel controller.

## Implementation sequence

1. Implemented: editable firearm settings through the existing assembly save path.
2. Implemented functional baseline: Player Start and native Player Inspector; UX revisit pending.
3. Implemented: reusable Player defaults, placement and explicit overrides. Multiple starts/duplication remain outside the single-player contract.
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

Player Start displays an always-visible editor-only wireframe collision capsule, cyan normally and orange when selected. Click its interior or edges to select the player; a camera body/lens marker at the authored eye height and an extruded ground-facing arrow show Runtime spawn yaw, including when unselected.
It reads the runtime controller radius and total height each frame, follows the
spawn feet position, and updates after inspector edits and Undo/Redo. It stays
upright and unscaled like the runtime character, and is absent during Test Level.

Capsule display repair: connected 3D capsule edges are projected and drawn in
Studio Compose after scene temporal postprocessing, beside the transform gizmo.
This avoids the motion trails from temporal accumulation of debug-world lines.
The overlay is clipped to the scene viewport and camera planes; it is not a
serialized asset, does not enter Runtime, and is hidden behind assembly/grip workspaces.


## Reusable player prefabs

The native Player Start Inspector exposes a project prefab selector, SAVE AS
PLAYER PREFAB and RESET TO PREFAB. Saving captures all current controller/camera
defaults and the first-person arms/assembly StableId into a registered version-1
Content/Player/*.rplayerprefab asset. Its display name comes from the Player
Start name; the asset filename uses its StableId. Each save creates a separate
immutable prefab. The arms bundle remains a separate governed asset.

Selecting a prefab applies an undoable copy of its defaults without moving or
rotating the level's Player Start. The scene retains the prefab StableId and
serialized assignment baseline. Editing controller values or the arms reference
is a level-local override, visibly reported in the Inspector. RESET TO PREFAB
restores the assigned baseline through Undo/Redo. Reusing it in another level:
drag the named prefab from Content/Player into the level, or select it in an
existing Player Start's Inspector.

Runtime consumes the resolved scene settings, preserving the existing controller,
physics and animation boundaries. Test Level snapshots copy the registered prefab;
Build Game dependency discovery includes scene -> prefab -> default arms as well
as locally assigned arms. Save/reopen retains the baseline and overrides.

This first reusable stage does not update all placed players when another prefab
is saved, duplicate the single governed Player Start in one level, or expose a
starting inventory. Global prefab updates, per-field inheritance indicators,
Asset Browser drag placement is implemented. Equipment definitions remain
subsequent work.


### Player placement from Content/Player
New levels have no automatic Player Start. Project browsing ensures a registered Basic
Player Start preset under Content/Player without creating a scene entity. Saved player
prefabs appear by authored name in the Asset Browser and support drag-and-drop surface
placement (ground-plane fallback) and the existing Place control. One command creates
the governed start and assigns resolved prefab defaults; Undo/Redo and WISCENE preserve
identity, transform and baseline. The always-visible selectable capsule represents the
placed player. Add no longer exposes Player Start. A second placement is refused: use
the existing Inspector to change prefab, or delete the old start before placing another.
Immutable saves refresh the browser. PR #178 functionality is owner-accepted; the UI/UX
follow-up remains open.

### Approved Player Camera Preview inset (owner reaffirmed 2026-10-05)

The accepted Player Start mockup remains the visual target: a small Player
Camera Preview at the bottom-right of the scene viewport, beside the selected
player capsule. It shows the authored world from the player camera with equipped
first-person arms/weapon, using the same eye height, facing and field of view.
This is an editor authoring preview with gameplay paused; Test Level continues
to run the separate real Runtime. The inset is implemented on the P2 branch and remains subject to owner and
independent verification. Do not treat Assembly or
Hand Grips isolated previews as completion of this scene-camera preview.


Implementation checkpoint: selected Player Start shows the frozen world plus
equipped native Idle arms/weapon in a responsive bottom-right inset. Position,
eye height and yaw come from the same spawn policy as Runtime; current FOV is
Runtime's default 60 degrees. Preview is editor-only and does not author WISCENE.
DX12 proof rejects empty images and verifies source settings/component counts
are unchanged; native placement inspection is recorded in HANDOFF.md.
Independent sword/shield presentation awaits the supplied animation pack.

Off-hand preparation (2026-10-05): hand-scoped action channels, held Block policy,
OffHandUse rebinding and pure Runtime adapter are tested. Live native off-hand
capability remains disabled until the actual sword/shield pack is integrated.

Player camera preview controls: click its header to collapse/expand (saved Studio
preference); drag the upper-left handle to resize its 16:9 image. Width stays
for the session. Native large/small/collapsed inspection passed on 2026-10-05.
