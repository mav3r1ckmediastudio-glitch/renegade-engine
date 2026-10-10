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


**Status: OWNER-ACCEPTED IMPLEMENTATION CHECKPOINT; PR #180 / CI / independent review pending.**

On 2026-10-06 the owner confirmed movement and Build Game both act as expected.
The P2 branch includes equipment/loadout persistence and Runtime routing,
staged/held/cancelled actions, independent sword/shield presentation, directional
charge/chaining, blending, presentation contact correction, hand assembly
editing/mesh swaps and explicit model import roles/folders/moves. This is bounded
acceptance of the tested setup, not every combat/Alpha requirement. Generic
editing, inventory, reserve ammunition and wider UX/transfer remain follow-ups.
See [P2 implementation](P2_EQUIPMENT_ACTION_IMPLEMENTATION.md) and
[P3 continuation handoff](P3_CONTINUATION_HANDOFF.md). Historical checkpoints
later in this document retain their original evidence and limits.

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


### Staged discrete Runtime equipment checkpoint (2026-10-05)

Primary fire/reload/equip/unequip now use EquipmentActionState preparation, windup,
native-completed Active and authored recovery. Pause/reset and hand reservations
are retained; native animation remains the skeleton and ammo authority.
Release Runtime build, focused CTest 4/4, staged DX12 paired Attack/ammo/completion
proof and standalone immediate-definition routing passed. Held actions/cancellation
input, off-hand presentation and packaged acceptance remain pending.
See P2_EQUIPMENT_ACTION_IMPLEMENTATION.md and HANDOFF.md for exact evidence.


### Held primary input and cancellation (2026-10-05)

Authored PrimaryUse/Attack can wait in Hold until release of its Fire binding.
Cancel (default C; remappable) respects pre-active cancellation and leaves native
Active/recovery untouched. Old version-1 input documents adopt Cancel in memory
without rewriting custom controls. Release bridge/Runtime builds, focused CTest
5/5, DX12 held-shotgun proof and standalone hold/release/cancel check passed.
Separate Charge/Release clips, charge-power mechanics and off-hand presentation
remain pending. See P2_EQUIPMENT_ACTION_IMPLEMENTATION.md for limits and evidence.


### Explicit Charge/Release presentation checkpoint (2026-10-05)

Assemblies optionally map Charge and Release as native arms/weapon pairs.
CREATE FROM ASSEMBLY derives those definitions; an unassigned Attack selects the
Charge/Release primary path. Runtime plays Charge to its held endpoint and
retargets ownership to Release on mouse-up, preserving reservations into recovery.
Aim reconciliation cannot steal Charge; pre-active cancellation clears its pose.
Release bridge/Runtime/Studio builds, six focused CTests, DX12 cold-load/pose/hand
proof and standalone release/cancel check passed. Charge strength/projectiles and
off-hand presentation remain pending. See P2_EQUIPMENT_ACTION_IMPLEMENTATION.md.


Player authoring checkpoint (2026-10-05): selected Player Camera Preview inset
implemented in Studio with frozen world and native primary presentation.
Windows Release build, five focused tests, DX12 image proof and native inset
inspection pass; owner/independent acceptance remains pending. Large-world clone
profiling and full render-settings parity remain follow-ups. Independent
sword/shield input/presentation is next; owner animation pack is expected.

### Independent hand assembly authoring

ASSEMBLY > HAND / MELEE SETUP exposes sword/shield mesh attachments, explicit hand roots,
directional Charge/Hold/Release groups, block clips, attack variants, scales,
charge/chaining timing and collision proxies. Existing arms bindings survive a
weapon-only swap. Draft Undo/Redo, automatic masked preview and governed save
are shared with paired firearm assemblies. See [hand assembly authoring](P2_ASSEMBLY_HAND_AUTHORING.md).
Local validation is recorded in HANDOFF; owner UI acceptance and independent
verification remain open. This does not complete NPC combat or the wider P2 gate.

### Import destinations and model organisation

Native model import now exposes a general Content destination, optional tags and
explicit player arms/weapon roles independent of folder. Registered model MOVE
preserves stable IDs and source provenance through journaled rollback/recovery.
See [import destinations](MODEL_IMPORT_DESTINATIONS.md).
Owner UX and independent exact-commit verification remain pending.


### P3 first foundation checkpoint - 2026-10-06

P3 began on feature/p3-projectile-impact, dependent on P2 PR #180 head cde4106.
Shared transient projectile records now retain owner/source/faction attribution,
gravity/lifetime policy and typed collision-to-impact output through an injected
world-query contract. Windows x64 Debug and Release builds and the simulation
test pass. Native collision/damage integration, effects, persisted definitions
and Test Level/packaged gameplay proof remain open.
See [P3 implementation](P3_PROJECTILE_IMPACT_IMPLEMENTATION.md).

P3 continuation: native scene-query/Character-damage adapter is implemented.
Release 3/3 projectile and existing combat tests pass; Debug 2/2 projectile tests
pass. CPU collider query proof covers owner hierarchy exclusion, nearest cover,
Character root resolution, attributed health/death/perception/events and origin
overlap. Live Runtime integration, physics-only body coverage and Test Level/
packaged gameplay acceptance remain open.

P3 authoring continuation: registered projectile assets and equipment semantic-action
bindings now have a native named picker, presets and safe local assignment. Shotgun
PrimaryUse/Bullet assignment and saved level reopen were inspected with held
presentation preserved. Release targeted tests pass 5/5, including registry
ordering, rollback, legacy schemas, assignment Undo/Redo and cold scene reload.
Live emission, visuals, impact effects and packaged firing remain open. Usable
Player/NPC health is not implemented and must not become a prerequisite.
See P3_PROJECTILE_AUTHORING_UX.md.


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

### Projectile preview mouse navigation - 2026-10-06

Projectile inspection now uses left-drag orbit, right-drag pan and wheel zoom.
FIT / RESET VIEW recentres and returns to the tip-right side view. Drag starts
inside the image and remains owned until release, including outside the image.
View state is transient; saved appearance and Runtime launch rules are unchanged.
Other importer controls and the Wicked pin are unchanged. See
[P3 preview evidence](P3_PROJECTILE_PREVIEW.md). Native verification is recorded
there; weapon/palm launch sockets remain the next gameplay increment.

P3 continuation (2026-10-06): named weapon/bone/palm socket authoring, assembly
recipe/native persistence and primary Runtime muzzle launch are implemented
candidates. See P3_LAUNCH_SOCKETS.md for proof and limits. Precise animation release
markers, barrel/pellet policies, animated socket-preview action selection,
direct importer editing, gizmos, effects, hitscan and surface-impact profiles
remain open. Player/NPC health remains outside this increment. No P3 gate closure;
independent exact-commit verification and package/Jolt-only proof remain required.

## P3 grouped authored projectile candidate - 2026-10-07

The owner's four checks are one bounded gate: native animation release timing,
first/alternate/both PSPs, saved layered flight/impact effects, and disappear/stick.
Implementation and targeted tests pass; standalone saved timing/barrel/impact
proof is recorded in [P3_AUTHORED_PROJECTILES_GATE.md](P3_AUTHORED_PROJECTILES_GATE.md).
Editor/package acceptance and independent exact-commit review remain open.
Health remains outside this increment; P2 PR #180 remains unmerged.

## Projectile authoring UX sequence — owner-directed 7 October 2026

After owner gameplay acceptance of the repaired disposable bow, continue the
previous conversation's sequence: UX cleanup, preset refinement, visual animation
event authoring. Basic/Advanced disclosure and conditional spawn-point/release
controls are implemented candidates; evidence is in
[P3_PROJECTILE_UX_CLEANUP](P3_PROJECTILE_UX_CLEANUP.md).
Existing physical Bullet/Arrow/Bolt/Thrown/Spell presets are reused. The draggable
animation fire marker is implemented and locally verified. This sequence does not close wider P3 or full P6
draw/hold gameplay. Human arms replacement and retargeting remain deferred.

## P3 first-class Hitscan checkpoint — 2026-10-08

Weapon firing now supports an explicit Physical projectile or Hitscan / instant
ray mode per semantic action. Hitscan is not represented by fake projectile
speed/gravity values. Equipment schema v3 persists fire_mode, range and damage,
while schema v1/v2 physical-projectile equipment remains readable and unchanged.
A Hitscan binding has no projectile asset dependency.

Studio's Weapon firing panel exposes Fire mode, Range and Damage for Hitscan and
retains the same named PSP, first/alternate/both policy and animation fire marker
used by travelling projectiles. Projectile search/New/Edit Copy are hidden in
Hitscan mode. Hitscan authoring does not depend on the projectile asset catalogue.

Runtime resolves Hitscan without loading a projectile asset, preserves accepted
weapon action/ammunition/animation timing, applies the existing muzzle-to-camera
aim and cover rules, then performs one bounded nearest-contact scene query.
Character contacts use the existing attributed combat-damage seam and display a
brief centre hit confirmation; static world contacts do not require a usable
health system. hitscan.fired and hitscan.impact use the governed gameplay event
boundary. Beam/continuous casts and pellet spread remain separate work.
The shared surface presentation checkpoint below supersedes the earlier
decal-profile limitation; governed per-surface audio asset binding remains open.

Validation: Release EngineBridge, Runtime and normal Studio compile passed; the
normal Studio link was blocked only because the owner's existing Studio process
held RenegadeStudio.exe. A separate BUILD/hitscan-studio/RenegadeStudio.exe
linked successfully without closing that session. EquipmentAsset, LaunchSocket
and RuntimeProjectileSession focused executables pass. Tests cover schema-v3
roundtrip/invalid values, Runtime resolution without a projectile asset, authored
ray range, blocked-query failure, and unchanged projectile simulation behavior.
git diff --check passes. Native owner visual/gameplay acceptance, package parity,
Jolt-only blockers and independent exact-head review remain open; no P3 gate
closure, commit, push or merge is claimed.

## P3 shared impact-surface presentation checkpoint — 2026-10-08

Shared surface response is now an implemented candidate for both Hitscan and
travelling projectiles. Material Inspector exposes an undoable explicit Impact
Surface classification: Default, Metal, Wood, Concrete, Stone, Dirt / ground,
Glass or Water. Governed Character contacts classify automatically; new Renegade
terrain tags slope as Stone and other terrain materials as Dirt. Runtime keeps the
stable material identity and adds the semantic surface type without filename,
colour or texture inference.

The shared presentation layer emits bounded surface-tuned native particle bursts
and transient Wicked impact marks on eligible world contacts. Marks follow the
contacted transform when available, cap at 64 and expire after 18 seconds.
Water, Glass and Character intentionally do not receive the generic bullet-hole
mark. Existing projectile-authored effects and stick/disappear behaviour remain
independent. Both Hitscan and travelling-projectile Character contacts show the
same short centre hit confirmation. Impact gameplay events include
`surface_type` for scripts and future sound-cue routing.

Per-surface audio asset binding remains open because the current governed audio
system has no one-shot asset-ID API; raw filename bypasses are not accepted.
Beam/continuous casts, pellet spread, Jolt-only blocker parity, package parity and
independent exact-head review also remain open. Release Bridge/Runtime/normal
Studio build and ProjectileWorld, RuntimeProjectileSession and LaunchSocket
focused tests pass. No P3 gate closure or merge is implied.

## P3 object Surface Type continuation - 2026-10-08

Supersedes the material-first creator workflow in the preceding checkpoint.
Creator steps: select an object; choose Surface Type directly in its Inspector.
The field stays visible independently of collapsed Transform/Rendering/Materials
sections. Choices: Default, Metal, Wood, Concrete, Stone, Dirt / Ground, Glass,
Water. No creator-facing impact profile or override chain is introduced.

EngineBridge ObjectImpactSurfaceService stores the classification on the
selected Object/Collider entity's native MetadataComponent. No mesh subset
binding or shared MaterialComponent is changed. Runtime checks governed Character
first, then explicit object metadata, then existing material classification for
untouched objects. Explicit Default means generic response. Older material
tags and terrain defaults remain compatible. Both firing modes already share
the same classification and impact presentation/event path. Audio binding
remains future work; surface_type is the event seam, not a claim of finished SFX.

The command restores both prior value and prior absence on Undo; Redo reapplies.
ProjectileWorldTests adds shared-mesh instance isolation, untouched material and
subset assertions, explicit Default, subset-free object response, automatic
Character precedence, invalid authoring rejection and native WISCENE archive
save/reload. This does not claim packaged gameplay or owner visual acceptance.

Visual material assignment is a separate visual property. Material Target still
selects an already-bound material to edit; it is not relabelled as assignment.
A future assignment command must create a private mesh derivative for the selected
instance before altering subset bindings, preserve skinning/LOD/resource identity,
and restore the original mesh on Undo. Surface Type needs none of those changes.

## P3 governed impact audio - 2026-10-08

ImpactAudioService adds a project-owned schema-v1 bank at
Content/Audio/Impacts/ImpactAudio.renegade-impact-audio. It maps explicit semantic
surface tokens to bounded lists of governed LP08 Audio .rasset stable IDs.
No filenames or renderer material names classify contacts. Ordinary creation stays
select object -> Surface Type -> done; there is no per-object sound setup.
Default and Character have no supplied cues and remain silent unless an explicit
Default/Character bank entry is provided.

Both Hitscan and travelling-projectile contacts dispatch the same transient native
3D audio player. It resolves and validates all bank assets once per scene revision
before activation, uses the SoundEffect bus, avoids consecutive variant repeats,
caps voices at 32, updates listener spatialization, removes ended voices with a
ten-second safety lifetime, pauses/resumes existing voices and stops them on reset,
screen transitions, scene replacement and shutdown. It never serializes playback
voices into the scene. Legacy projects without a bank remain compatible/silent.

Audio products live in Content/Audio/Impacts/<Surface>/Impact_1.rasset and
Impact_2.rasset. Byte-identical supplied WAVs are retained in SourceAssets/Audio/Impacts
for governed reimport. Test Level snapshots include bank, registry and required
products; Build Game discovery adds the bank and all required audio products to
the normal dependency graph. Packaged Runtime resolves audio stable IDs through
content-manifest.json and never needs retained SourceAssets.

Tests/ImpactAudioTests.cpp covers bank identity/save-reload, duplicate rejection,
bounded PCM WAV validation, governed import/resolution, snapshot closure, package
lookup without sources/registry, native voice playback/variant isolation, cap,
pause lifetime and reset. Its --install helper imports the explicitly mapped
supplied packs; --verify and --tag-metal run graphics-enabled real-scene Build Game
discovery plus supplied WAV decode checks. --tag-metal is for the disposable Bow
Playground only. New-project automatic starter-bank installation and bank editing
UI are not added by this checkpoint.

P3 impact VFX first pass: Metal sparks/scuff, Wood splinters/dust/split and Concrete chips/dust/chip masks implemented with shared procedural defaults; orange diagnostic contact visuals removed. Focused Release tests pass. Final owner appearance acceptance remains pending; remaining-surface art and realistic flipbook quality are not complete. See HANDOFF for failed/retried visual capture evidence. No gate closure.

P3 2026-10-08: owner rejected first-pass surface VFX as cartoonish/too similar. Revised Metal/Wood/Concrete candidate uses sparse brief sparks, brown splinters, and embedded detailed Concrete powder with angular chips. Release builds plus four focused checks pass; native save/reload, package sprite inclusion, and live three-surface impacts verified. Owner motion/appearance acceptance remains open; no gate closure. Comparison targets are in the disposable Bow playground. Material assignment safety and working-tree provenance review remain open.

P3 2026-10-09: Glass shard/crack and Water splash/ripple behaviour prototypes added and five-target playground save/reload verified. Four focused tests pass, including cap/expiry/reset and Water arrow retirement. Owner says behaviour test is very successful but ALL impact visuals are too generic/stylised and rejects their quality. Next: authored realistic art pass with a single approved Concrete reference, then remaining surfaces. No visual acceptance or P3 gate closure; no commit/push/merge until working-tree provenance reviewed.

P3 2026-10-09 blood prioritised by owner ahead of Concrete art reference. Projectile/Hitscan Character blood candidate adds detailed embedded spray/splat art, native lit rendering with no emission, bounded falling mesh droplets and collision-derived world stains. Release build/four focused tests/native floor collision and package inclusion pass. Standalone dummy ready; motion and owner quality acceptance pending after foreground guard aborted input proof. Melee contacts, skinned wounds and fluid animation remain open. All earlier impact art remains quality-rejected; no gate closure or commits.

P3 blood size/retention follow-up: owner requests exaggerated readable spray and visible floor residue. Candidate now enlarges spray ~2.5x, doubles droplets, increases floor splash radius to 0.15-0.20m, retains stains ten gameplay minutes and dries/darkens them over 90s. Bounded 96 drops/128 stains; oldest eviction and reset remain. Visual acceptance still pending; no Ghost of Tsushima parity claim.

P3 owner rejects static blood and missing floor stains. Current revision replaces spray with embedded 4x4/16-frame lifetime-driven atlas and fixes wrong local-space attachment flag on blood and generic impact decals. Actual-scene offscreen proof and atlas-age captures added; new live blood_drops/blood_stains counters aid diagnosis. Visual owner acceptance remains pending.

P3 2026-10-09 owner rejects generated blood atlas and floor art. Current bounded experiment adapts KNIFE authored liquid sheets with matching normal maps to 64-frame native PBR cards, slower separate lifetimes and corrected incoming-side normal handling. Native droplets/stain persistence retained; compact procedural floor masks replace furry art but remain below realistic artwork target. No SPH or gate closure. See P3_KNIFE_BLOOD_CANDIDATE.md; actual gameplay owner check and proper floor art remain open.
