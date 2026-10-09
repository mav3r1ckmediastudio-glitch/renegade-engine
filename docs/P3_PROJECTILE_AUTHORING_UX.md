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

## 7 October UX cleanup supersedes the initial editor description

The current editor includes model preview, effects and disappear/stick authoring.
New/Edit Copy now opens in Basic mode; Advanced contains retained lifetime,
rotation, damage and particle tuning. Weapon setup conditionally shows multi-PSP
policy and a second point, and presents release as animation start or delay.
See [current cleanup evidence](P3_PROJECTILE_UX_CLEANUP.md). Existing presets,
immutable save/assignment and schemas are retained; no health workflow claim.

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

## Shared impact-surface authoring — 2026-10-08

Surface response is authored on the material rather than repeated on every weapon
or projectile. Select an ordinary editable material and use **Impact Surface** in
the Material Inspector: Default / generic, Metal, Wood, Concrete, Stone,
Dirt / ground, Glass or Water. The change participates in Studio Undo/Redo.
Character is automatic at Runtime and is intentionally not offered as a material
choice. New Renegade terrain already carries Dirt / Stone defaults.

The same classification drives Hitscan and travelling projectiles. A creator does
not need a second Hitscan effects panel or duplicate projectile definitions just
to get material-aware contact feedback. Eligible world hits receive a transient
impact mark and surface-tuned burst; Water, Glass and Character skip the generic
bullet-hole mark. Existing authored projectile impact effects remain available as
an additional projectile-specific layer.

Impact events publish `surface_type` so scripts and future sound cues can select
the same semantic category. Per-surface audio asset selection is intentionally not
shown yet because the current governed audio path has no one-shot asset-ID API.

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

Imported asset roots are supported: a selected transform parent with Object/Collider descendants can carry Surface Type. Runtime uses the nearest authored object/ancestor, then legacy material metadata. This changes only the selected asset instance hierarchy; automatic Character classification remains first. Shared-mesh sibling and imported-root precedence/Undo are regression checked.

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


### P3 impact VFX first three surfaces (local candidate, 2026-10-08)
Metal, Wood and Concrete now have separate generated 64x64 masks and bounded presets:
Metal narrow short sparks and a dent/scuff mask; Wood pointed splinters, a small tan dust puff and a split mask; Concrete irregular chips, a broader grey dust puff and a chipped mask. GetImpactTexture shares seven built-in GPU textures; no external texture paths, imports, profiles or new inspector controls. Other categories retain their earlier presets. Hit normals direct bursts outward; random velocity spread uses native normal_factor with random_factor=1 to avoid negative starting sizes. Dust expands and fades, fragments shrink/fall. Embedding projectile definitions use reduced particle counts and decal size; this is an internal stick-on-impact heuristic, not a dedicated weapon-class system.
Native effect cleanup remains capped at 128 emitters, each polished burst capped at 64 particles; decals remain capped at 64 and expire after 18 seconds or reset. Both Hitscan and physical projectile presentation use this common function. Contact diagnostic records remain, but orange debug contact spheres are removed from normal gameplay rendering.
No change to select-object -> Surface Type -> done workflow. No authored custom flipbook UI, mesh debris physics, or final remaining-surface art added.
Changed files: Runtime/src/RuntimeImpactTextures.h (new), Runtime/src/RuntimeProjectileVisuals.h, Runtime/src/RuntimeProjectileSession.h, Runtime/CMakeLists.txt; Tests/ImpactAudioTests.cpp adds disposable --tag-wood/--tag-concrete and governed --quiet-projectile <descriptor> <asset-id> validation helpers.
Bow Playground original flaming-arrow definition retained at BUILD/p3-audio-before/FlamingArrow-before-vfx.rprojectile. Quiet-arrow test clears only its flight layers and explicit projectile impact overlay through SaveProjectileAsset, to make surface response visible. Test wall surface is set using the normal scene-document authoring service.
First visual captures failed acceptance: flaming-arrow overlay obscured surface bursts; debug spheres persisted; native spread was disabled and fragments were too small. Corrected emitter spread and size before accepting later visual evidence. Runtime link initially failed because the test game locked the binary; test game closed and rebuilt. Quiet-projectile helper initially lacked ProjectileAssetService include; fixed before use.
Verification results and any remaining visual limits follow in HANDOFF. No commit/push/merge; existing broad working-tree provenance remains under review.

2026-10-09 owner acceptance split: Object → Surface Type remains the complete creator workflow. Glass and Water behaviour prototypes are successful tests; ALL current impact visuals are explicitly rejected for realism/quality. Built-in effects need an authored art pass while keeping profile/material internals hidden. Glass does not destroy its pane; Water produces splash/ripples and consumes projectile appearance at contact. These are prototype presentations, not production artwork.
