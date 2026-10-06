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
