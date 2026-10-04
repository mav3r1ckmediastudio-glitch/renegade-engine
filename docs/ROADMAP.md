# Renegade Engine Roadmap

**Current programme:** Alpha Playability — Player Arms & Combat Framework
**Current integrated main:** PR #176 merged as 3bb768b148da590ea9dd7aa47fbb28006c0508d6
**Wicked pin:** 3a800b7134aafe58461093c8abb2e274d4e64033
**Canonical combat design:** [PLAYER_ARMS_COMBAT_FRAMEWORK](PLAYER_ARMS_COMBAT_FRAMEWORK.md)

## Current state

Renegade has moved beyond editor/world foundations into a playable-core engine.
Current main includes the custom Studio, project/scene lifecycle, Terrain and
rendering authoring, Wicked/Jolt player movement, governed input and Lua, audio,
Story Flow/Screens, standalone Build Game, the rebuilt native model/Character
importer, Character Prefabs, semantic animation assignment/crossfading,
navigation, perception, role behaviour and owner-tested hostile NPC combat.

The next programme is intentionally not another deep AI expansion. The priority
is to make the existing Player a complete gameplay actor so Renegade can build a
small representative game rather than only demonstrate engine subsystems.

The Alpha Playability target is:

> spawn -> equip -> explore -> encounter -> fight -> somebody dies -> HUD reflects
> state -> restart/respawn -> the same loop works in an independently packaged game.
## Accepted foundations reused by Alpha Playability

| Foundation | Reused ownership |
|---|---|
| Player/Core | Existing Player Start, Wicked/Jolt capsule, camera, movement, sprint, jump and Runtime lifecycle remain authoritative. |
| Character combat | Existing Character health/damage/combat seams are extended to Player-facing combat rather than replaced. |
| Animation | Wicked AnimationComponent and Renegade semantic action/crossfade work remain the animation authority. |
| Physics | Wicked/Jolt remains world collision and projectile/melee query authority. |
| Input | Existing governed action maps are extended; no separate weapon input stack. |
| Audio/VFX | Existing audio and particle/light/render systems provide presentation hooks. |
| Stable identity/events | Existing identity and governed gameplay-event boundaries are reused. |
| Test Level / Build Game | Combat must prove parity through the existing Runtime and packaging lifecycle. |

## Alpha Playability gates

### P1 — First-person Arms Rig

**Status: IN PROGRESS — foundation implemented; real-arms visual acceptance failed/pending.**
Implementation baseline: `51513b9` on `feature/p1-first-person-arms-rig`.
Camera synchronization repair `4dd9953` removes owner-reported proxy movement
stutter. Owner accepts proxy gameplay through standalone Runtime and Studio Test
Level, and confirms the actual Studio Build Game export launches and renders.
Authored skeletal socket binding now has serialized/packaged-load regression and
native DX12 animation proof. A native Studio Hand Grips editor now exposes bone
selection, grip offsets, Undo/Redo and governed save/reopen; real skinned-asset
acceptance remains open. See [P1_HAND_SOCKET_BINDINGS](P1_HAND_SOCKET_BINDINGS.md)
and [P1_HAND_GRIP_EDITOR](P1_HAND_GRIP_EDITOR.md).
See [P1_STATUS_AND_RECOVERY](P1_STATUS_AND_RECOVERY.md) for the commit inventory,
local build/test evidence, missing acceptance and controlled recovery sequence.

Extend the existing Player camera with a presentation-only first-person View Rig.
Deliver primary hand, off hand and two-hand/support sockets, movement presentation
and semantic animation ownership. Do not create a second Player controller.

Acceptance includes Test Level and packaged Runtime parity, correct camera
pitch/yaw behaviour and a foundation that does not assume the right hand is the
only gameplay hand.
### P2 — Equipment & Action Framework

Implement generic item/ability ownership, equip/unequip, primary/alternate use,
charge/release, reload, staged actions and hand reservation.

Items declare PrimaryOnly, OffHandOnly, EitherHand, TwoHanded or
PrimaryWithSupport semantics. Gameplay requests semantic actions rather than
asset-specific animation filenames.

### P3 — Projectile & Impact Framework

Implement the reusable projectile/hit boundary used by bullets, arrows, bolts,
throwables and spell projectiles. Include collision, damage routing, owner/source
identity, gravity/lifetime policy and surface-aware impact presentation.

Provide the common SFX/VFX/decal/light hooks needed for weapon feel.

### P4 — Firearm Reference

Use one intentionally simple pistol to prove the framework:
equip, hip/ADS aim, fire, ammo, reload, dry fire, hitscan/physical-projectile
seam, recoil, weapon impulse, muzzle flash/smoke/light, shot/mechanical audio,
surface impact and Character damage.

Camera recoil and weapon recoil remain independently tunable.
### P5 — Directional Melee

Directional melee is core scope, not a generic Melee button.

Prove left/right slash, overhead and thrust; directional guards; sword trajectory
and swept collision; wind-up/hold/active/recovery phases; block/parry/feint
seams; shield interaction; hit deflection/rebound; weapon reach; stamina hooks;
and surface-specific SFX/VFX.

The reference proof is sword + shield because it exercises independent hands,
directional attack and defence, collision, animation and impact feedback.

### P6 — Bow Reference

Prove a two-handed nock/draw/hold/release loop with draw amount as a real 0..1
gameplay value affecting velocity/trajectory/damage/presentation as appropriate.
Use the shared projectile framework for arrows and preserve extension seams for
embedding, bounce, break, recovery and elemental/utility arrows.

### P7 — Crossbow Reference

Prove staged Fired/Unloaded -> Cock -> Load Bolt -> Ready -> Aim -> Fire
behaviour. Multi-stage reload/actions must be generic rather than hard-coded to
crossbows so firearms and other equipment can reuse them.
### P8 — Magic Reference

Prove that spells are first-class abilities rather than fake weapons. Implement
at least one charged projectile spell and one continuous/channel spell through
the same hand/action/effect architecture.

The design must remain capable of projectile, beam, area, targeted and self-cast
abilities plus sword + spell, shield + spell, dual spells and staff/off-hand use.

### P9 — Dual-hand Composition Proof

Deliberately combine independently useful hand systems without Player special
cases. Required reference combinations include sword + shield and at least one
weapon/spell or item/off-hand combination.

This gate proves that hand reservation, animation and action ownership were
designed generically rather than around the pistol reference.

### P10 — Combat Feel & First Playable Loop

This is an owner-led acceptance gate, not merely an automated-test gate.

Tune responsiveness, animation timing, camera motion, procedural recoil, sway,
muzzle flash, particles, SFX, mechanical audio, impacts, melee weight, guard and
parry timing, bow draw/release, crossbow staging and magic presentation until
combat feels good in direct play.
Complete the loop with Player health/death, NPC damage/death, HUD state,
restart/respawn and independently packaged Runtime parity.

A weapon or combat mode is not accepted merely because a test proves the expected
damage number. Direct owner play is mandatory for feel.

## Immediately following Alpha Playability

### Gameplay HUD / UI authoring

The playable loop requires health, ammo/resource, crosshair/aim state,
interaction prompts and equipment state. This should build on the existing
Screen/Journey infrastructure and the planned creator UI designer rather than
introducing a disconnected HUD runtime.

The UI path should remain capable of static images, sprites/animated imagery and
video/overlay content where the renderer/runtime supports them.

### Performance and scale

Open issue #169 remains the explicit rigged-character performance investigation.
Before city-scale populations, profile first-rigged-character cost, multi-NPC
animation/cognition/navigation load, projectile pressure and practical actor
budgets. Preserve native Wicked streaming/scene ownership rather than solving
scale with a parallel world.
## Character & AI continuation backlog

The earlier Character & AI programme established Character identity, profiles,
factions, perception/memory, patrol/decision foundations and the later integrated
Runtime now demonstrates Wander, hostile perception, Chase/Attack, pursuit
leashing/disengagement, semantic animation playback and crossfading in owner
gameplay.

The following remain later expansion work unless Alpha Playability exposes a
blocking need:

- squad communication and imperfect information sharing;
- authored/generated cover;
- smart objects and reservations;
- cognition/animation LOD and large-population scheduling;
- richer animation layers/blend spaces/contact markers;
- broader behaviour/prefab authoring;
- advanced navigation scaling.

Do not let those features displace the Player/combat work required for a small
complete game.

## Architecture rules

- Extend accepted systems; do not create parallel Player, physics, animation,
  navigation, event or damage stacks.
- World simulation is authoritative; first-person arms are presentation.
- Both hands are first-class.
- Combat families share action/projectile/effect foundations.
- Directional melee uses swept weapon motion, not a centre-camera raycast.
- Bows/crossbows/magic are reference requirements, not post-alpha add-ons.
- Creator-facing definitions should be data-driven and semantically named.
- Visual/behavioural owner failure overrides nominal automated success.
- Save/reopen, Test Level and packaged Runtime parity are acceptance requirements.
## Verification policy

CI must build and test from a clean checkout. The repaired Studio CI explicitly
builds the registered test dependencies and treats hosted graphics visibility
separately from owner-hardware rendered-pixel acceptance.

The importer and Character stack remains protected by automated tests, while
real rendered visibility and subjective gameplay feel remain owner-hardware
acceptance responsibilities.

Historical programme evidence remains in subsystem handoffs/specifications.
This roadmap describes current priority and should not be used as a release
claim for unfinished gates.
