# P3 projectile authoring and weapon assignment

## Owner instructions - 2026-10-06

P3 remains focused on projectile simulation, collision and impact presentation.
The owner clarified that usable Player/NPC health is not implemented yet.
Internal health fields and isolated damage-seam tests are not evidence of a
complete gameplay health system. Health implementation must not distract from
or become a prerequisite for projectiles.

Projectile assignment must be easy to discover and use. Creators should choose
named projectile definitions on their weapon rather than IDs, JSON, animation
filenames or arbitrary project files.

## Current native authoring flow

Select a Player Start, open Starting Equipment, then WEAPON PROJECTILES.
Choose primary or off-hand weapon and a readable launch action.
The action list contains only launch-capable semantic actions that this weapon
actually defines. Charge itself is not a launch event; a charged weapon uses
Release. The default chooses Release when available, otherwise the first
supported authored action.

Projectile selection lists only registered project projectile definitions,
by name and flight speed. Search filters projectile names; Enter applies search.
None explicitly removes the chosen action's projectile binding.
A hidden existing selection cannot be mistaken for None: Apply is disabled
until the creator picks a visible result or clears search.

NEW opens a small preset editor:
- Bullet: 300 m/s, no gravity, 5 seconds.
- Arrow: 45 m/s, normal gravity, 5 seconds.
- Bolt: 65 m/s, normal gravity, 5 seconds.
- Thrown projectile: 15 m/s, normal gravity, 5 seconds.
- Spell projectile: 20 m/s, no gravity, 8 seconds.

These are starting values for physical flight records, not proof of hitscan,
bounce, rolling, embedding, explosions or reference weapon gameplay.
The editor exposes Name, Speed (m/s), Gravity multiplier and Lifetime (seconds).
Gravity 0 is straight flight; 1 is normal gravity. The initial collision policy
stops on contact or expiry. Imported visual models and impact effects are not
currently exposed by this editor.

SAVE AS NEW registers a reusable projectile in the project. EDIT COPY copies
selected settings into the editor with a new suggested name; it does not mutate
a shared definition. Damage payload is preserved internally when copying but
is not exposed as a completed health workflow.

APPLY TO THIS PLAYER saves a new immutable equipment definition with the
selected action binding, retaining hand policy, presentation and action timings.
It then assigns that definition through the existing Player settings command.
Undo restores the prior loadout; Save Level persists the new reference.
Other player prefabs and loadouts remain unchanged. Saving the projectile alone
does not apply it to a weapon. The panel makes scope and incomplete live emission
explicit rather than implying a firing feature is already ready.

Current binding assignment is scoped to a placed Player Start's equipment.
A general reusable weapon asset editor and intentional global definition update
workflow remain follow-ups. Avoid silently changing every weapon that happens to
share the same immutable asset identity.

## Data and packaging

ProjectileAssetService persists schema-v1 .rprojectile definitions and the asset
registry together through the existing journaled project transaction. Project
and asset identities, flight limits and names are validated. List/load operations
admit registered project projectile assets only; no raw path guessing is used.

Equipment without projectile bindings retains schema v1. Bound equipment writes
schema v2 with explicit semantic-action/projectile-asset pairs. The loader
continues to accept v1 and rejects missing, foreign, duplicate or unavailable
bindings. Missing projectiles fail loading rather than silently firing defaults.

Equipment dependency records and the existing reusable dependency provider
include bound projectiles, so Test Level/Build Game can discover the same project
definition closure. Actual end-to-end snapshot and package proof is still required.

## Completion sequence

1. Build and validate asset save/reopen, rollback, v1 compatibility, binding
   validation and dependency discovery.
2. Build the native editor and inspect authoring/picking with an owner-project
   copy. Verify search, correct hand/action, creation, assignment Undo/Redo and
   Save Level/reopen without changing unrelated equipment setup.
3. Resolve projectile definitions when Runtime loads equipment. Emit exactly
   once from the accepted semantic action's Active/Release boundary.
4. Complete scene/Jolt world collision coverage, muzzle-to-aim rules and real
   actor source exclusion; prove flight/contact/expiry/pause/reset.
5. Add visual projectile and surface-impact bindings using scoped asset pickers
   and the existing dependency/preview paths.
6. Verify the same setup in Test Level and independently packaged Runtime.

Future presentation controls should be grouped under Appearance and Impact:
visible model or trail, shot/spawn sound, impact sound/effect/decal/light and
surface overrides. Offer sensible defaults and a trajectory/impact preview.
Keep advanced collision policies collapsed. Do not add controls that merely
persist ignored values or promise unsupported behavior.

Native rendered inspection and live Runtime/packaged firing remain required
before calling this a completed P3 creator workflow.

Validation for projectile authoring (Windows x64 VS18 Release):
- Bridge build passed (BUILD/p3-authoring-bridge-build.log).
- CL=/MP4; cmake --build BUILD/renegade --config Release --target
  RenegadeProjectileAssetTests RenegadeEquipmentAssetTests
  RenegadeEquipmentActionStateTests RenegadeProjectileWorldTests
  RenegadeProjectileSimulationTests -- /m:2 /p:BuildProjectReferences=false
  /verbosity:minimal: passed.
- ctest --test-dir BUILD/renegade -C Release -R
  'Projectile|EquipmentAsset|EquipmentActionState' --output-on-failure:
  5/5 passed, 0.95 seconds. Includes asset save/reopen, rollback, strict schemas,
  unchanged action/hand metadata, missing/duplicate binding rejection and
  required projectile dependency discovery.
- CL=/MP4; cmake --build BUILD/renegade --config Release --target
  RenegadeStudio RenegadeRuntime -- /m:2 /p:BuildProjectReferences=false
  /verbosity:minimal: passed (128.43 seconds).
- git diff --check passed. Existing MSB8029 and unrelated C4834 warnings remain.
Native UI verification is in progress; no live firing or gate closure claimed.

Native regression and owner steering:
- Sword/shield was used only to check preservation of existing equipment.
  The owner requires the shotgun as the projectile reference weapon. Use the
  disposable BUILD/p3-shotgun-ui-project copied from arms-full-library-ready,
  with ArmsPlayground.renegade, for the creator and eventual live firing proof.
- Native assignment exposed sorted registry dependency order versus unsorted
  equipment expected IDs. EquipmentDependencies now sorts its unique IDs.
  The regression includes a presentation reference sorting after its projectile.
- Final Release build of asset tests, Studio and Runtime passed. Final targeted
  CTest passed 5/5, 0.77 seconds. Log: BUILD/p3-authoring-final-build.log.
- Current native screenshot inspection verified clean Starting Equipment,
  Weapon Projectiles and preset editor layouts; Arrow preset populated 45 m/s,
  gravity 1 and lifetime 5. Sword firing is not a P3 acceptance proof.

Shotgun native creator proof:
- Opened disposable BUILD/p3-shotgun-ui-project/ArmsPlayground.renegade;
  opened its ArmsPlayground level and selected the placed Player Start.
- Starting Equipment: created Shotgun from the existing full shotgun assembly,
  retained TwoHanded policy, chose primary hand and applied the loadout.
- Weapon Projectiles: Primary fire / use; NEW Bullet; SAVE AS NEW; APPLY TO THIS PLAYER.
- Saved ArmsPlayground.wiscene and REOPEN SCENE; reselected Player Start.
  Weapon Projectiles reopened with Bullet / 300 m/s on Primary fire / use.
  Held shotgun preview remained valid. Screenshot: BUILD/p3-shotgun-binding-reopen.png.
- Inspected stored schema-v2 equipment: original Equip, Unequip, PrimaryUse,
  AlternateUse and Reload timings retained, original shotgun presentation ID
  retained, PrimaryUse references registered Bullet. Off-hand remained empty.
- Search zzzz plus Enter hid Bullet and disabled Apply with an explicit message.
  Screenshot: BUILD/p3-shotgun-search-guard.png.
- Edit Copy exposed native window priority: creator window could appear behind
  the previously focused picker. Opening it now calls native Widget::Activate.
  Flight numbers use four significant digits instead of six trailing zeros.
- Final assignment CTest also proves actual settings command Undo/Redo and
  native WISCENE serialization/cold reopen with the bound projectile: 5/5 pass,
  0.73 seconds. BUILD/p3-assignment-tests-build.log.
This proves authoring, not shotgun projectile emission or pellet spread.
The launch picker excludes authored AimIn/AimOut actions, so shotgun aim cannot be mistaken for secondary fire.

Final Studio-only polish build passed (13.11s). Reopened the saved shotgun project in that build: Edit Copy now appears in front with Bullet copy, speed 300, gravity 0 and lifetime 5; Cancel leaves the saved assignment intact. Screenshot: BUILD/p3-shotgun-edit-copy-front.png.


## P3 live shotgun checkpoint - 2026-10-06

Accepted shotgun shots now launch cached projectile definitions in Runtime, with
native scene contacts and bounded flight/contact feedback. Test Level snapshots
include projectile dependencies. Release targeted tests and native Test Level /
standalone source firing pass. Initial camera-eye origin is explicit; authored
muzzles, pellet spread, effects, Jolt-only coverage, packaged firing and independent
verification remain open. Health is not required. See
[P3 live shotgun evidence](P3_LIVE_SHOTGUN_PROJECTILES.md) for commands and limits.
This checkpoint supersedes earlier statements that live Runtime firing is pending.
