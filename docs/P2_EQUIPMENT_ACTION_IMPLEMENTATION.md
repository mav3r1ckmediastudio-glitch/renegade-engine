# P2 equipment and action framework

Design authority: [PLAYER_ARMS_COMBAT_FRAMEWORK](PLAYER_ARMS_COMBAT_FRAMEWORK.md)
and [ROADMAP](ROADMAP.md). P2 is in progress; the accepted shotgun remains the
compatibility reference. This document does not close a release gate.

## Implementation sequence

1. Small assembly picker UX repair: dedicated arms/weapon folder eligibility and
   saved recipe roles, preserving existing assemblies and command history.
2. Shared equipment/action definition and staged gameplay state with independent
   hand reservations, semantic presentation requests, cancellation and pause/reset.
3. Governed project equipment assets, save/reopen and starting-loadout assignment
   in the existing Player Inspector/prefab workflow.
4. Runtime equipment ownership, equip/unequip, primary/alternate use, charge/release
   and reload integration; retain native Wicked animation as presentation authority.
5. Test Level snapshots, transitive packaging and owner gameplay verification.

Do not replace Player movement, physics, camera ownership, native skeleton
animation or existing damage/event boundaries. P3 still owns projectiles/impacts;
P4 owns the complete firearm reference. Starting loadout and later ammo policies
must use these common definitions rather than a second shotgun-only controller.

## Current implementation

`CollectFirstPersonPartChoices` is bridge-owned. Only available imported `.rasset`
parts in `Content/Player/Arms/` or `Content/Player/Weapons/`, or parts explicitly
referenced by saved assembly recipes, enter the relevant picker. Nested folders
work; prefix lookalikes, missing records and completed assemblies are excluded.
A combined-folder pack works when its saved recipe identifies each part's role.
Brand-new combined packs still need a future explicit role/import authoring route.
No filenames are guessed and no existing product is moved. Full project-relative
labels distinguish duplicate filenames. Combo user-data preserves stable identity
when the two filtered lists have different row numbers.

`EquipmentActionState` is a pure gameplay foundation with typed hand use and
semantic actions. Each actor has independent channels. Prepare, windup, hold,
active and recovery phases emit events; conflicting hands cannot reserve together.
Zero/nonfinite/negative update time cannot advance phases. Hold requires release;
cancellation is allowed only before active when authored. Reset clears reservations
and queued events. Definitions reject invalid timing and duplicate action kinds.
The action state is not yet connected to the Runtime shotgun; equipment
asset persistence and starting-loadout authoring are now implemented. An authored
animation string identifies a semantic action rather than a clip filename.

## Validation

- Assembly settings regression includes 100 unrelated models, nested dedicated
  folders, shared-pack recipe roles, missing assets, misleading folder prefixes
  and malformed provenance.
- Equipment action regression checks independent sword/shield hands, two-hand
  exclusion, active event counts, hold/release, cancellation restrictions, pause,
  invalid definitions, instant actions and reset.
- Native picker visual/save/reopen and Windows build evidence is recorded in
  HANDOFF.md at the implementation checkpoint.

Remaining: Runtime ownership/integration, semantic animation binding and
end-to-end snapshot/package gameplay acceptance. No P2 gate
completion or new firearm/projectile capability is claimed by this foundation.

## Equipment assets and starting loadout

EquipmentAssetService persists schema-v1 .requipment definitions under
Content/Player/Equipment with project/stable identity, hand-use policy, semantic
actions, staged durations and an optional governed presentation .rasset. Immutable
saves journal the definition and registry together, record presentation dependency
edges and validate both files. Missing/foreign assets and unsupported schema are
rejected. Names appear in the Asset Browser and scoped equipment selectors.

PlayerControllerSettings now carries primaryEquipmentAssetId and
offHandEquipmentAssetId in native WISCENE metadata. The existing settings command
provides Undo/Redo. Player prefab schema v2 includes both slots; schema v1 remains
readable with empty slots. Prefab apply/reset/placement retains the loadout and
level-local overrides. Save validates hand compatibility and available equipment.

Player Inspector > STARTING EQUIPMENT opens primary/off-hand selectors and an
explicit APPLY LOADOUT command. SAVE LEVEL persists the assignment. CREATE FROM
ASSEMBLY creates a new equipment definition from the player's assigned saved
assembly: supported Equip, Unequip, Attack, AimIn and Reload bindings become
semantic equipment actions, with active duration copied from the longer native
paired track. It does not invent missing actions or alter the source assembly.
Generic action/timing editing and richer inventory/ability authoring remain open.

The existing dependency provider discovers scene/prefab equipment references and
equipment presentation references. These edges feed the common snapshot/build
graph; their end-to-end Runtime consumption still requires the next P2 slice.
Assigning a loadout does not yet change gameplay. The accepted legacy shotgun
path remains the current Runtime reference until semantic routing is integrated.

## Second checkpoint verification

Windows Release equipment asset, player prefab and equipment action tests pass
(3/3). Native Studio created a definition from the full shotgun assembly, filtered
the two-handed item out of the off-hand selector, applied the loadout, saved a
schema-v2 player prefab and retained both after SAVE LEVEL / REOPEN SCENE.
Standalone Runtime displayed the existing shotgun arms from this saved level.
The naming field was subsequently initialized empty and Studio rebuilt.

The initial Test Level ProjectRejected (22) was traced to a missing gameplay
input map and a Windows path-length failure while Runtime tried to journal
defaults. Snapshots now contain a validated input document before launch.
Release snapshot regression and native Test Level startup on the same deeply
nested validation project pass. The paired shotgun rig loads with both tracks.
Equipment snapshot closure and semantic Runtime routing remain unverified;
this checkpoint remains draft and does not establish packaged gameplay readiness.

## Equipment snapshot closure checkpoint

Test Level now preserves primary/off-hand equipment definitions from resolved
level settings and from player prefab defaults, including equipment displaced
by a local override. Equipment presentation products and governed texture bindings
use the existing first-person presentation closure. Each copied equipment
asset is reloaded against the snapshot registry; missing/foreign/incompatible
loadouts abort snapshot creation. No Runtime equipment action routing is claimed.

Release snapshot, prefab, action-state and equipment asset tests pass (4/4).
DX12 equipment snapshot proof cold-loads the real shotgun definition, its paired
presentation and the unchanged saved loadout in a cloned validation project.
This establishes asset transport, not gameplay action or full Build Game acceptance.


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
