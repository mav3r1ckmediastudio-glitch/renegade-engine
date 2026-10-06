# P3 projectile and impact implementation

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
