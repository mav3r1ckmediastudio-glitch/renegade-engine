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
The action state now gates discrete primary Runtime actions; equipment
asset persistence and starting-loadout authoring are implemented. An authored
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


## Staged discrete Runtime equipment actions — 2026-10-05

Runtime now routes authored primary fire, reload and equip/unequip through
EquipmentActionState. Preparation and windup reserve the authored hands before
one semantic dispatch. Active waits for native paired-animation completion;
authored recovery then retains the reservation before the next press can start.
Native ammo, partial reload, aim variants and jump presentation remain authoritative.
Pause does not advance phases. Busy native jump/aim presentation defers dispatch;
aim transitions cannot steal a staged reservation. Scene reload/shutdown clears state.
No-equipment legacy players retain the existing input path.

This slice covers canonical Attack/Reload/Equip/Unequip bindings. Immediate aim
remains on the previous path. Held/charge/release and cancellation input, independent
off-hand presentation, inventory and generic action editing remain pending.
ActiveSeconds remains the standalone gameplay-state duration; paired Runtime uses
native clip completion instead. Phase boundaries dispatch on gameplay frame updates.

Windows Release Runtime build passed after correcting the new ground-check pointer
and diagnostic integer types. Rebuilt EquipmentActionState and EquipmentAsset targets;
focused snapshot/prefab/action/asset CTest passed 4/4. DX12 real paired shotgun proof
verifies delayed dispatch, one shell consumed, native completion and recovery release.
Standalone PID 34884 observed Attack and hand reservation returning to Ready; first
focus click produced no action and the subsequent click passed. Its assigned definition
uses immediate timing; nonzero phase delays are exercised by regression and DX12 proof.
Evidence: BUILD/p2-staged-native-events.json and BUILD/p2-route-runtime-diagnostics.json.
Commands: powershell -NoProfile -ExecutionPolicy Bypass -File
BUILD/build_p2_assets_runtime.ps1; BUILD/p2_runtime_route_verify.ps1; and
BUILD/p2_equipment_proof.ps1 (same PowerShell invocation).
Standalone: BUILD/p2_route_launch.ps1 and BUILD/p2_route_capture.ps1 with that invocation;
python BUILD/p2_staged_native.py. Test window closed; owner Studio left open.
git diff --check passed. No owner/independent verification or packaged acceptance claimed.
Next: held/charge/release input and cancellation, independent off-hand presentation,
then package gameplay verification. The editor camera inset remains pending.


## Held primary action input and cancellation — 2026-10-05

GameplayInputFrame now carries Fire's held state from the existing authored binding.
Version-1 maps append cancel_equipment (default C); old maps receive it in memory
without rewriting their bytes or replacing custom controls. Cancel can be rebound.

Authored PrimaryUse/Attack with holdUntilRelease reserves the hands through
prepare/windup and Hold, then dispatches exactly once when Fire is released.
A short tap latches release during windup. Pause advances neither release nor
cancellation; held state is reconciled on the next gameplay frame.
Cancel uses the authored cancellableBeforeActive policy in prepare/windup/hold.
It never interrupts active native playback or recovery. Holding after a cancel
cannot restart without a fresh press; releasing the cancelled hold does not fire.
Existing immediate shotgun behavior retains discrete press semantics.

This is held PrimaryUse with canonical Attack presentation, not a complete bow,
charge-power mechanic or separate Charge/Release animation adapter. Hold currently
retains ordinary movement/aim presentation. Generic action editing and independent
off-hand presentation remain pending; full package acceptance remains pending.

Verification: updated Release bridge, input/action/equipment/prefab/snapshot targets;
focused CTest 5/5 and real DX12 held-shotgun snapshot proof passed.
Runtime Release build passed. Standalone saved held-definition check passed:
no Attack while held, Attack after release, hand reservation returned to Ready,
C cancelled another hold, and its subsequent release did not play Attack.
Evidence: BUILD/p2-held-native-events.json and BUILD/p2-route-runtime-diagnostics.json.
Commands: powershell -NoProfile -ExecutionPolicy Bypass -File BUILD/p2_held_verify.ps1;
same invocation for BUILD/p2_held_runtime_build.ps1, BUILD/p2_route_launch.ps1 and
BUILD/p2_route_capture.ps1; python BUILD/p2_held_native.py.
The native harness was corrected for the diagnostics PID location and for a fresh
F8 reset with a focus click before testing. Binary metadata still embeds ef399ac;
compiled implementation includes this working slice, with no exact-head independent
verification claimed. Owner Studio was left open; only the launched test Runtime was closed.
git diff --check passed. No owner/independent verification or release gate closure.

Next: separate semantic Charge/Release presentation and independent off-hand support,
generic action editing and packaged gameplay verification. The approved small editor
Player Camera Preview inset remains pending.


## Explicit native Charge/Release presentation — 2026-10-05

The assembly action whitelist now has 16 optional bindings: Charge and Release
join the accepted 14. Version-1 recipes remain readable. Studio's More actions
page exposes the new arms/weapon selectors and preview action choices.
CREATE FROM ASSEMBLY derives Charge (hold-until-release) and Release definitions
when those pairs exist. Leave Attack unassigned for the primary Charge/Release
path; an explicit PrimaryUse retains precedence when both kinds are authored.

Runtime requires matching Charge/Release definitions and both native pairs.
Charge owns preparation/windup/Hold presentation, playing to the pair's endpoint
and holding it. Fire release atomically retargets the active equipment channel to
Release without freeing its hands. Native Release completion then enters the
Release definition's recovery. Cancel clears charge presentation and pre-active
ownership; pause keeps the charge pose. Aim transitions cannot overwrite Charge.
Missing pairs or bindings never substitute Idle/Attack as Release.

This adapter requires Charge holdUntilRelease and zero Release prepare/windup
with no Release hold. Charge prepare/windup/cancel policy and Release recovery are
used. Native release duration owns Active. Generic charge strength, damage and
projectiles, independent off-hand presentation and complete packaged acceptance
remain pending. Generic Release has no firearm ammunition effect. The proof maps
existing AimIn/Attack clips to the new semantics; it does not claim a bow asset.

Windows Release bridge, Runtime and Studio builds passed. Studio was built to
BUILD/p2-charge-studio with OutDir override, preserving the open owner executable.
Six focused CTests passed. DX12 held-primary regression and charge snapshot proof
passed; the latter saves/cold-loads a new assembly and equipment definition,
checks exact native pair counts, held endpoint, continuous hand ownership,
Release playback/recovery and no firearm ammo mutation. Final Runtime/proof
rebuild repeated both proofs after protecting Charge from aim reconciliation.
The repeated charge fixture initially collided with its saved name; a unique short
fixture name fixed repeatability and BUILD/p2_charge_proof.ps1 then passed.
Standalone Charge/Release/cancel check passed; diagnostics/evidence live under
BUILD/p2-charge-native-events.json. Native held-pose screenshot
BUILD/p2-charge-held-pose.png was inspected; the fixture retains its washed-out
lighting. Capture command: python BUILD/p2_charge_pose.py. No native Studio dropdown visual acceptance
or independent verifier/owner acceptance is claimed for this slice.

Commands from repo root: powershell -NoProfile -ExecutionPolicy Bypass -File
BUILD/p2_charge_verify.ps1; same invocation for BUILD/p2_charge_final_verify.ps1,
BUILD/p2_charge_proof.ps1,
BUILD/p2_charge_launch.ps1 and BUILD/p2_route_capture.ps1;
python BUILD/p2_charge_native.py. git diff --check passed.
Only the launched test Runtime was closed; owner Studio remains open.
Embedded build metadata may predate these compiled edits; no exact-head release
verification is claimed. P2 and Alpha release gates remain open.

Next: independent off-hand presentation, generic action editing, native authoring
review and packaged gameplay verification. The small editor Player Camera Preview
inset remains pending.
