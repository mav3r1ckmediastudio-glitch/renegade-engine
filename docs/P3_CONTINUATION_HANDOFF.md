# P3 continuation handoff - 2026-10-06

## Start here

P2 integration PR: https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/180
Base main: 7105a95ddcc103d6a024a17fddefd62f705a86b3.
P2 implementation through 6135b2d5395b4718b3adf198a95a08805307a919;
82be254 records the solid marker verification. Acceptance/documentation checkpoint
follows these commits on feature/p2-equipment-actions. HANDOFF.md records its hash.

Read AGENTS.md and canonical context, then PLAYER_ARMS_COMBAT_FRAMEWORK.md,
ROADMAP.md and P2_EQUIPMENT_ACTION_IMPLEMENTATION.md. P2 PR/Windows CI and independent
exact-head review remain pending. Do not merge or claim gate closure automatically.
If PR #180 is merged, branch P3 from updated main. If it is still open, create a
separate P3 branch from its current head and document the dependency. Do not add
P3 commits to the P2 review branch or discard local/untracked owner files.

## Owner acceptance

On 2026-10-06 the owner reported: "ive checked movement and build game, and both
act as expected". This is owner evidence for movement and exported-game behavior
of the tested setup; no independently recorded package hash/revision was supplied.
The owner also accepted the solid Player Start camera and arrow appearance.

P2 includes equipment/loadout assets, staged semantic Runtime actions, held
charge/release/cancel, independent sword/shield input and animation layers,
directional gesture strikes and chaining, native transition blends, bounded
blade/shield pose correction, hand assembly authoring and a tested replacement
fantasy sword. Native import supports explicit Content destinations, player roles,
optional tags and journaled stable-ID-preserving moves. Tests/build/native evidence
and exact limitations are in HANDOFF.md. Broader UI/UX, inventory, generic action
editing, reserve ammunition and automatic cross-project transfer remain open.

## P3 authorized programme

Implement the shared projectile/hit boundary for bullets, arrows, bolts,
throwables and spell projectiles: collision, damage routing, owner/source identity,
gravity/lifetime policy and surface-aware impact presentation. Provide reusable
SFX/VFX/decal/light hooks. Follow the authored roadmap; do not start another AI
expansion or invent a second Player/physics/damage stack.

Begin by auditing existing Wicked/Jolt queries, Character health/damage and
governed gameplay events/dependency paths. Choose a bounded first implementation
with source/owner attribution, collision-to-impact data and existing damage routing;
verify it in Test Level and packaged Runtime before expanding reference weapons.
World simulation is authoritative; first-person meshes are presentation.
Blade/shield pose avoidance is not hit detection. Directional melee world sweeps,
guards/parry/stamina remain P5; the complete firearm reference remains P4.
Preserve independent-hand semantics and do not substitute a centre-camera ray
for the later swept directional melee design.

## Projectile authoring checkpoint - 2026-10-06

See P3_PROJECTILE_AUTHORING_UX.md for the native weapon assignment workflow.
ProjectileAssetService adds registered, journaled .rprojectile flight definitions.
Equipment schema v2 binds a projectile to an authored launch action and retains
schema-v1 compatibility. Dependency discovery follows those references.
Starting Equipment now opens Weapon Projectiles with named project-only choices,
search, preset creation and Edit Copy. Apply creates an immutable equipment copy
and assigns it through the existing Player settings command; Undo and Save Level
use the existing loadout lifecycle. Shared definitions are not silently mutated.

The owner clarified that usable Player/NPC health is not implemented. Existing
internal damage fields and seam tests do not prove gameplay health. Projectile
implementation continues independently, with damage as an integration hook.

This slice does not yet connect the binding to live Runtime emission, provide
projectile visuals or prove independently packaged firing. Native rendered UI,
flight/contact proof and package parity remain required for P3 acceptance.

Latest authoring checkpoint:
The shotgun is the owner-selected reference weapon. Native authoring assignment
and saved level reopen were verified on BUILD/p3-shotgun-ui-project; the saved
Shotgun retains its presentation, hand policy and Equip/Unequip/Attack/Aim/Reload
metadata, with Bullet bound to PrimaryUse. This is not live emission proof.
Tests include presentation-plus-projectile sorted dependencies, actual assignment
command Undo/Redo and cold native scene serialization (Release 5/5, 0.73s).
The creator picker excludes AimIn/AimOut animation actions from launch choices.
Weapon projectiles UI uses native activation when opening create/edit windows.
User-friendly reusable weapon asset editing, muzzle socket, visual/effect pickers,
live emission, Jolt collision coverage and packaged firing remain open.
No usable Player/NPC health implementation is claimed or required for this slice.
See docs/P3_PROJECTILE_AUTHORING_UX.md for exact commands and native evidence.

Projectile authoring implementation commit: 6f5401db2a7fa8d487aea09df781a4fd3a08ea13. Final native polish also verified Edit Copy priority, Cancel, concise flight values and shotgun PrimaryUse-only launch choices.


## P3 live shotgun checkpoint - 2026-10-06

Accepted shotgun shots now launch cached projectile definitions in Runtime, with
native scene contacts and bounded flight/contact feedback. Test Level snapshots
include projectile dependencies. Release targeted tests and native Test Level /
standalone source firing pass. Initial camera-eye origin is explicit; authored
muzzles, pellet spread, effects, Jolt-only coverage, packaged firing and independent
verification remain open. Health is not required. See
[P3 live shotgun evidence](P3_LIVE_SHOTGUN_PROJECTILES.md) for commands and limits.
This checkpoint supersedes earlier statements that live Runtime firing is pending.

## P3 projectile model/editor checkpoint — 2026-10-06

Add → Projectile now creates model-backed projectile definitions without a Player
selection. Imported model identity, scale and rotation persist in projectile schema
v2 with v1 compatibility and required model/texture dependencies. Runtime render
instances follow transient flight and retire on contact, expiry and reset; visual
hierarchies are excluded from projectile collision queries. The supplied arrow was
imported and assigned to the shotgun in the disposable native fixture, saved and
cold-loaded. Model snapshot closure and source standalone firing were exercised.
Evidence and limits: [Projectile model checkpoint](P3_PROJECTILE_MESH_CHECKPOINT.md).
Target workflow: [Projectile editor design](P3_PROJECTILE_EDITOR_DESIGN.md).

Next: dedicated model preview, explicit weapon/palm launch sockets, then first-class
Hitscan/Beam authoring and surface impact profiles. Camera-eye launch remains the
current policy. Packaged firing and Jolt-only coverage are open. Health is not a
prerequisite; no P3 gate closure or automatic P2 merge is authorized.

## P3 projectile preview checkpoint — 2026-10-06

The projectile editor now includes an automatically refreshed isolated model
preview with camera orbit, elevation, side/rear views, zoom and fit. Authored
scale/rotation use the Runtime convention and are validated before rendering;
camera inspection does not change those values. Physical model size is shown.
Private preview resources are released when hidden/project changes. Level
markers, gizmo, selection outline and player-camera inset do not cover this
editor. Saved copies display Custom / saved projectile rather than a misleading
preset. See [Projectile preview evidence](P3_PROJECTILE_PREVIEW.md).

Next: explicit weapon/palm sockets, placement and runtime launch/aim timing.
Camera-eye launch remains current; Hitscan/Beam, effects, Jolt-only coverage,
packaged firing and independent P3 verification remain open.
