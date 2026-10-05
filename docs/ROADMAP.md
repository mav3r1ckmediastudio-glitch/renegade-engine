# Renegade Engine Roadmap

**Current programme:** Alpha Playability — Player Arms & Combat Framework
**Current integrated main:** PR #178 merged as 7105a95ddcc103d6a024a17fddefd62f705a86b3
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

**Status: MERGED FUNCTIONAL CHECKPOINT; OWNER ACCEPTED; broader P1/Alpha scope remains open.**

PR #178 merged into main on 2026-10-05 at `7105a95ddcc103d6a024a17fddefd62f705a86b3`.
All four pre-merge Windows checks passed: Studio Debug/Release and baseline
Debug/Release. The owner confirmed the expected player/shotgun behaviour in the
existing v2 game project and explicitly accepted the PR's functionality.
This records functional acceptance of the merged scope, not completion of every
P1 requirement or of the full Alpha Playability combat programme.

The merged slice includes the complete supplied shotgun's 14 paired actions,
fire/reload/dry-fire ammunition state, partial reload, aim/equip/jump routing,
editable firearm policy, native assembly lifecycle and reusable player prefabs.
Player starts are placed from `Content/Player`, remain selectable as always-visible
capsules, and retain Inspector defaults/overrides with Undo/Redo and save/reopen.
The v2 transfer passed native full-library cold load and isolated packaged-runtime
checks; owner gameplay acceptance followed. Cross-project adoption was manual.

**Required follow-up:** revisit player authoring UI/UX, particularly dedicated
arms/weapon picker folders or role-filtered collections, combined-folder packs,
readable names and complete dependency-aware transfer. See
[PLAYER_AUTHORING_UX_FOLLOWUP](PLAYER_AUTHORING_UX_FOLLOWUP.md).
The first P2 branch slice implements scoped filtering; broader UI/UX and transfer
work remain recorded requirements.

See [PLAYER_AUTHORING_CONTINUATION](PLAYER_AUTHORING_CONTINUATION.md),
[P1_ASSEMBLY_AUTHORING](P1_ASSEMBLY_AUTHORING.md) and
[P1_STATUS_AND_RECOVERY](P1_STATUS_AND_RECOVERY.md) for current and historical evidence.

Extend the existing Player camera with a presentation-only first-person View Rig.
Deliver primary hand, off hand and two-hand/support sockets, movement presentation
and semantic animation ownership. Do not create a second Player controller.

Acceptance includes Test Level and packaged Runtime parity, correct camera
pitch/yaw behaviour and a foundation that does not assume the right hand is the
only gameplay hand.
### P2 — Equipment & Action Framework


**Status: IN PROGRESS.** The owner authorized the small picker repair first,
then P2 in the authored order on 2026-10-05. The initial shared action-state
foundation covers semantic requests, staged phases and hand reservation;
project equipment assets and starting-loadout persistence/authoring are implemented
on the P2 branch; Runtime integration and end-to-end acceptance remain open.
See [P2_EQUIPMENT_ACTION_IMPLEMENTATION](P2_EQUIPMENT_ACTION_IMPLEMENTATION.md).

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


### Runtime fire and reload animation controls

Left mouse triggers the assigned Attack pair once; R triggers the assigned Reload
pair once. F8 resets the play session. Reload wins simultaneous presses; an active
action completes before another action can start, then idle/walk/run resumes.
Pause freezes the paired clock. Missing action assignments remain harmless.
These controls currently drive animation only; ammunition, damage, sound and
recoil gameplay remain later combat work. Legacy version-1 input maps gain fire
and reload defaults; the old default R reset moves to F8 while custom bindings
remain authored. Full-library Runtime must be rebuilt alongside the bridge.


### Two-shell shotgun Runtime prototype

The current paired shotgun starts with two loaded shells. Each accepted fire
press consumes one shell; empty fire presses do not play Attack. R after one
shot selects ReloadPartial, and R after two selects Reload. Reload at capacity
is ignored. The shell count returns to two only when the paired reload finishes,
including its weapon track. Busy actions and pause do not consume/refill shells.
An absent partial pair uses an explicitly assigned full reload if available.
Reset/reinitialization restores two shells. This is a bounded two-barrel prototype;
creator-configurable weapon definitions, reserve ammunition, HUD, damage and
reload interruption remain later work.


### Right-mouse aiming

Hold right mouse to play the assigned AimIn pair once and hold its final sight
pose; release plays AimOut then returns to movement. Left mouse while aimed uses
AimAttack and consumes the same two-shell ammunition. Reload lowers the sights,
uses partial/full reload as appropriate, and resumes aim-in if right mouse is
still held. Transitions finish before queued hold/release changes are reconciled;
pause freezes them. This uses authored native animation only, with no zoom/FOV
change or new camera/controller.


### Equipment and jump animation routing

Q toggles holster/equip using the assigned Unequip/Equip pairs. Holstered arms
hold the final out-of-view pose; fire, aim and reload are blocked, with shells
preserved. Existing Space jump physics drives grounded-to-airborne JumpStart,
airborne JumpLoop and grounded-contact JumpLand. Busy actions finish before
pending jump transitions. Jump animation ownership blocks fire/aim/reload until
landing; movement physics remains authoritative. Pause freezes action clocks.
Initial airborne spawn does not pretend a jump occurred. A collidable floor is
required to exercise takeoff and landing; the empty preview fixture is not a
complete gameplay level. No new controller or root-motion locomotion is added.

### Editable firearm settings

ASSEMBLY > WEAPON SETTINGS exposes loaded capacity, minimum seconds between shots
and permission for partial reload. Assembly draft Undo/Redo and governed SAVE
CHANGES / SAVE AS NEW persist these fields in the version-1 recipe and native
payload. Player Start's existing assembly reference carries settings through
Test Level snapshots and package loading. Old recipes retain the accepted two-shot
defaults. Runtime reload fills configured capacity only at animation completion;
shot cooldown advances only during gameplay. These are discrete shots, with no
reserve ammunition or automatic firing yet. Gameplay capacity does not change the
number of shells visible in authored clips. Independent equipment definitions and
Player prefabs remain next stages; see PLAYER_AUTHORING_CONTINUATION.md.

Player authoring continuation: reusable immutable player prefab capture and
Inspector assignment/reset are implemented on the active P1 branch. They combine
controller/camera defaults and arms assembly identity while keeping spawn
transform level-specific. Local override status and Undo/Redo are included.
Exact-build owner acceptance remains pending. Global updates, browser placement,
equipment/loadout definitions and combat/HUD remain subsequent milestones.


### Player placement from Content/Player
New levels have no automatic Player Start. Project browsing ensures a registered Basic Player Start preset under Content/Player without creating a scene entity. Saved player prefabs appear by authored name in the Asset Browser and support drag-and-drop surface placement (ground-plane fallback) and the existing Place control. One command creates the governed start and assigns resolved prefab defaults; Undo/Redo and WISCENE preserve identity, transform and baseline. The always-visible selectable capsule represents the placed player. Add no longer exposes Player Start. A second placement is refused: use the existing Inspector to change prefab, or delete the old start before placing another. Immutable saves refresh the browser. Owner/exact-commit verification remains required.


## Runtime equipment ownership and immediate action checkpoint — 2026-10-05

Runtime resolves primary/off-hand equipment atomically on player scene synchronization.
Authored primary equipment owns the effective presentation; empty or invalid authored
loadouts do not inherit a legacy gun. Original WISCENE authoring settings remain intact.
Legacy players with no equipment assignment retain their accepted animation path.

The adapter admits matching immediate PrimaryUse/Attack, Reload/Reload,
AlternateUse/AimIn and Equip/Unequip definitions. Missing, mismatched, staged or held
definitions block their input. Active duration and ammo remain native paired-animation
authority. This does not integrate the staged EquipmentActionState clock, charge/release,
independent off-hand presentation, inventory, or packaged gameplay acceptance.

Windows Release Runtime build passed. Release CTest snapshot, prefab, EquipmentActionState
and EquipmentAsset passed 4/4. The DX12 equipment snapshot proof cold-loads the real
paired shotgun, routes PrimaryUse to Attack and verifies one shell consumed.
Standalone Runtime PID 40508 loaded the generated snapshot: equipment authored/ready,
no equipment error, presentation loaded, paired animation initialized, two active tracks.
Screenshot BUILD/p2-route-runtime.png retains the fixture's existing washed-out lighting.
Diagnostics: BUILD/p2-route-runtime-diagnostics.json. No owner or independent acceptance.


Next: staged action routing, independent off-hand presentation and package verification.
