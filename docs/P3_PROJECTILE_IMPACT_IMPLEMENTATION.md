# P3 projectile and impact implementation

## Native scene-query and Character damage checkpoint - 2026-10-06

RuntimeProjectileWorld now adapts projectile segments to the pinned native
Scene::IntersectsAll path. Explicit subject/root binding excludes all contacts
belonging to the shooter hierarchy; the remaining nearest valid contact wins.
Character child geometry resolves to the governed Character root. Material subset
identity is retained as a surface lookup seam; no material-name inference or
complete surface/effect profile authoring is implemented.

The adapter checks origin overlaps with a tiny native sphere query. A confirmed
overlap is an immediate impact. At a coincident sphere centre, native overlap
math produces an undefined normal; an incoming-facing normal preserves the
confirmed blocker. Native malformed ray contact values fail closed. A missing
or mismatched owner binding also fails closed.

ApplyProjectileCharacterImpact delegates to ApplyAttributedCombatDamage for
health, legitimate DamagedBy knowledge and the existing ai.damage event. Static
world contacts do not mutate Character health. Invalid source asset identity,
self damage, invalid faction and already-dead targets are rejected through the
adapter/existing damage authority. Zero damage remains a contact without damage.

Windows x64 VS18 Release and Debug native test builds pass using:
cmake --build BUILD/renegade --config <Release|Debug>
 --target RenegadeProjectileWorldTests --
 /m:2 /p:BuildProjectReferences=false /verbosity:minimal
The retained P2 engine/bridge libraries supply unchanged dependencies; this is
not a clean-checkout full Runtime build. Final Release build/test cycle exit 0,
9.20s; Debug cycle exit 0, 6.16s. Existing MSB8029 warnings remain.
Logs: BUILD/p3-world-final-build.log and BUILD/p3-world-debug-build.log.

CTest Release expression:
RenegadeProjectile(Simulation|World)Tests|RenegadeCharacterAiCombatTests
passed 3/3, 0.14s total. Debug projectile expression passed 2/2, 0.14s.
An intermediate Release test exposed the coincident-origin normal edge case;
the corrected final build passes that regression.

ProjectileWorldTests uses the real native Scene ray/sphere/BVH/primitive code
with fixture-populated CPU collider query caches. It proves owner exclusion
through ten child colliders, nearest wall blocking, segment bounds, origin inside
a collider, Character child resolution, existing health/death/perception/events,
invalid source/faction/self/dead-target rejection and material subset lookup.
It does not exercise Scene::Update cache construction, GPU/native mesh skinning,
terrain, physics-only Jolt bodies, or live weapon input.

Current boundary is implemented and tested but is not yet installed into the
live Runtime update/action lifecycle. Physics-only Jolt coverage, owner binding
from real Runtime actors, persisted definitions/effects and live Test Level/
standalone Build Game proof remain next. No P3 gate closure or full collision
coverage is claimed. Historical initial checkpoint below remains dated evidence.

## First implementation checkpoint - 2026-10-06

Dependent branch: feature/p3-projectile-impact, based on P2 PR #180 head
cde41068fcf38b8dd293a9defc85ebbf93435dcc. P2 remains under review; no merge
or gate closure is implied. Owner requested P3 continuation after P2 handover.

The first bounded slice is ProjectileSimulation, a bridge-owned, UI-independent
transient record simulation. It accepts explicit owner/source/faction identity,
legitimately known source position/velocity, launch position/velocity, gravity,
lifetime, damage and impact-profile identity. It emits typed impact records for
one contact per projectile. It does not own health, effects, input or physics.

The world-query adapter receives each travel segment and must return the nearest
eligible contact after source-hierarchy filtering. Travel uses steps no longer
than 1/120 second with analytic constant-acceleration integration per step.
Collision queries cover each complete segment rather than an endpoint-only
overlap. Lifetime clips travel before collision; contact terminates the record.
Malformed/self contacts and failed queries retire without damage/effects.
Simulation supports at most 1024 records. Zero time pauses; Reset clears records
without reusing IDs. Nonfinite, negative or over-one-second updates are rejected
before advancing; callers must subdivide longer intervals.

The initial model is a point projectile. A curved trajectory is approximated by
short straight query segments. This is not finite-radius continuous collision,
a native rigid body, throwable contact response, penetration or ricochet.

## Audit of existing authority

- RuntimeCombatDamage.h::ApplyAttributedCombatDamage already validates legitimate
  source information through ReportDamageStimulus, updates Character health
  through ApplyCombatDamage and emits the existing ai.damage gameplay event.
  Use this function for Character impact damage. Do not create another health
  registry or lookup hidden attacker positions.
- RuntimeCombatService.h owns Character and Player combat health and event
  dispatch. Player receiver integration requires explicit review of its existing
  mutation path; this checkpoint does not add one.
- RuntimeCharacterPerception.h already uses Scene::Intersects for filtered
  world visibility and EntityBelongsToRoot for hierarchical ownership.
- Pinned Wicked Scene exposes ray Intersects/IntersectsAll, sphere/capsule
  overlaps and ray result position, normal, velocity, subset and bone fields.
  wiPhysics.h exposes Jolt-backed ray Intersects, including ragdoll information.
  The audited public header does not expose a general finite-radius shape cast.
  Adapter selection still needs a coverage proof for physics-only bodies,
  terrain, dynamic/animated targets and Character hierarchies.
- GameplayEventService supplies bounded governed events; damage already uses
  that path. Impact presentation should consume typed contacts and invoke the
  common event/effect boundary, not create an unrelated event queue.
- Governed stable asset identity, dependency graph, Test Level snapshot and
  standalone Build Game paths remain the asset/lifecycle authority. The current
  transient simulation has no persisted projectile asset or external resource;
  later impact SFX/VFX/decal/light references must enter the existing dependency
  closure and cold-load/package tests.

## Validation

Windows x64 VS18 Release:
cmake --build BUILD/renegade --config Release
  --target RenegadeProjectileSimulationTests -- /m:2 /verbosity:minimal
passed, exit 0, 8.99 seconds (including CMake regeneration).
Existing MSB8029 temporary-output warnings remain.

ctest --test-dir BUILD/renegade -C Release
  -R '^RenegadeProjectileSimulationTests$' --output-on-failure
passed 1/1, 0.23 seconds total.

The test checks analytic ballistic travel, thin-wall segment contact across a
long frame, single-impact retirement, lifetime travel clipping, source/owner/
surface/profile retention, pause/reset, invalid time/launch/contact rejection,
self contact, failed query and bounded capacity. The wall is a deterministic
test query, not a native Wicked collision proof.

## Next bounded slice

1. Implement the native world-query adapter with explicit owner hierarchy
   exclusion, nearest world blocker, Character target identity and surface
   resolution. Prove query coverage before choosing a universal policy.
2. Route accepted contacts through existing attributed Character damage and
   governed impact events. Distinguish an impact from accepted damage.
3. Connect simulation to Runtime scene initialization, pause/reset and semantic
   action dispatch; retain first-person presentation as a separate consumer.
4. Prove the same collision/damage case in Test Level and independently packaged
   Runtime, including cover/owner exclusion and cold loading.
5. Add persisted definitions and governed SFX/VFX/decal/light dependency hooks,
   then extend hitscan/muzzle checks and projectile reference families.

No live weapon input, native collision, damage, visual effects, asset authoring,
Test Level or package gameplay proof is claimed by this first checkpoint.
P4 firearm and P5 swept directional melee remain separately scoped.

Debug cross-check: the same build command with --config Debug passed.
CTest with -C Debug and the same expression passed 1/1, 0.09 seconds total.

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
