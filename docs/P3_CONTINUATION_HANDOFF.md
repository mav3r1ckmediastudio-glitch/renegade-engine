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
