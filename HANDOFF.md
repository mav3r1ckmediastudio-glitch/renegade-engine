## P3 grouped authored projectile candidate - 2026-10-07

Implementation commits: 8de29c1c82f3d546a9c83c82cdbce38ab8b7467f (authored projectile gate) and 74c0eb0 (governed Runtime asset-registry staging).
Branch: feature/p3-projectile-impact. P2 PR #180 remains unmerged.

Four owner checks are grouped: animation release, multiple PSPs, layered flight/
impact effects, disappear/stick. Implemented and targeted/native proofs recorded
in docs/P3_AUTHORED_PROJECTILES_GATE.md. Player/NPC health is not implemented or
a prerequisite. Wicked pin/source unchanged. No gate closure or independent
exact-commit review claim.

Native disposable Projectile Playground: saved 350ms release pending at 12ms and
launch at 359ms; Left then Right; Both launches two per shell; dry fire none;
disappear/stick contacts; moving-native-object follow and pause/lifetime tests.
Impact burst crash resolved by burst_on_create deferred native initialization.
Shared radial-alpha emitter texture fixes square fallback; native editor preview
soft mask pass. Editor cold reopen, SAVE AS NEW, assignment/reopen and packaged
runtime gameplay acceptance now pass.

Build Game exposed catalog issues: loaders reject generic lp07.rasset discovery
provider; source catalog refresh tombstones unrelated still-present assets and
recovery loses edges. Both fixed with regression tests. Package state still
includes only the reachable closure. The disposable fixture's embedded arrow
textures were extracted into exact generated paths; general importer/dependency
embedded-resource behavior remains a separate limitation.

Release latest bridge/Runtime/Studio/build-project-test build PASS 28.08s.
Build-project CTest 2/2 PASS 0.59s; provider compatibility build PASS 45.23s;
ProjectileAsset/RuntimeProjectileSession/EquipmentAsset/LaunchSocket 4/4 PASS 1.79s.
Runtime recopied to Studio embedded Runtime. Diff check PASS. Build Game initially
exposed a missing staged Runtime asset registry; 74c0eb0 adds canonical governed
GameData/AssetRegistry.renegade-assets staging plus tamper validation. Rebuilt
packaged gameplay then loaded authored equipment and fired two accepted shots:
PSP_Left then PSP_Right, 2 impacts, layered emitters observed (max 3), and 2 stuck
arrows retained. Evidence: BUILD/p3-package-native-proof.json,
BUILD/p3-package-acceptance.json and BUILD/p3-package-impact.png. Owner visual
acceptance and independent exact-commit CI review remain before final gate closure.
Unrelated Tools/__pycache__/ and log.txt remain untouched.

## P3 mesh visibility repair - 2026-10-06

Implementation commit: ca0a5a26710840d74cf2d6c0f87aa6673f0b0b9b.
Branch: feature/p3-projectile-impact. P2 PR #180 remains unmerged.

Owner reported seeing only sphere/trail despite arrow assignment. Runtime did
instantiate the arrow, but unconditional debug flight feedback obscured its
appearance. Model-backed launches now suppress flight sphere/trail; meshless
shots keep basic feedback, impacts keep existing markers. Transient ID tracking
retires/reset-clears; retained traces remember their appearance policy. No asset
schema/settings change. Changed RuntimeProjectileSession.h, its tests,
architecture and feature matrix. Wicked source/pin untouched.

Release build: CL=/MP4; cmake --build BUILD/renegade --config Release --target
RenegadeRuntime RenegadeRuntimeProjectileSessionTests -- /m:2
/p:BuildProjectReferences=false /verbosity:minimal: PASS 26.13s.
ctest --test-dir BUILD/renegade -C Release -R
'^RenegadeRuntimeProjectileSessionTests$' --output-on-failure --timeout 30:
1/1 PASS 0.11s including new mesh feedback suppression assertion. Diff check PASS.
Runtime copied to Studio/Release/Runtime for Test Level.

Native standalone slow-copy proof (BUILD/p3-arrow-visibility-proof) changed ONLY
its copied Arrow to speed 2, zero gravity, lifetime 10 for inspection. Actual
fixture Arrow remains speed 45, gravity 1, lifetime 5, scale 1. Captures
BUILD/p3-arrow-slow.png and p3-arrow-profile.png show arrow appearance without
sphere/trail. Owner confirms 'yeah i can see it now'. PrintWindow colour/exposure
is not renderer parity. No package proof. Source fixture equipment had meanwhile
changed to 6c1a5d17-09b0-4a6a-b2ed-1e5bd36726c3; standalone resolves this saved
loadout with Muzzle and Arrow.
Original fixture standalone native regression PASS 16.31s: named Muzzle, ground
impact, pause, dry fire, reload, reset. Evidence BUILD/p3-socket-native-events.json,
p3-socket-shot-impact.png and p3-socket-paused-impact.png. No new serialized state
so original fixture cold load covers unchanged saved assignment.

No P3 gate closure; independent exact-commit review and package/Jolt-only proof
remain required. Next: precise animation release timing and authored effects.
User-facing naming preference from manual: Projectile Spawn Point (PSP), not
FIRESPOT; compatibility bone lookup may retain that imported bone name.
Unrelated Tools/__pycache__/ and log.txt untouched.

## P3 launch socket checkpoint - 2026-10-06

Implementation commit: ec1a4166c5187cbac8091b15fee527cd4a68bff3.
Branch: feature/p3-projectile-impact. P2 PR #180 remains unmerged.

Named launch sockets now have a native editor reached through Player Start >
Starting Equipment > Weapon Projectiles > Edit Launch Sockets. It edits an
existing assembly with model/bone/palm selection, one-click surface placement,
FIRESPOT reuse, orange direction arrow, six committed position/direction controls,
mouse orbit/pan/wheel and Fit. Unique short bone labels retain full-path tooltips.
Apply changes the undoable assembly draft; SAVE CHANGES persists recipe/native
transform attachments and registry through the existing journal. Weapon
Projectiles selects a saved socket independently of the projectile. Empty names
explicitly retain legacy camera-origin launch.

Runtime reads named world pose after native animation/hierarchy evaluation.
Camera aim converges from muzzle; eye-to-muzzle cover clips before obstruction.
Missing/backwards/blocked poses refuse launch with diagnostics; accepted ammo has
already been consumed in this failure case. No Player/NPC health prerequisite.
One selected primary socket/record per accepted shot; off-hand generic dispatch,
pellets/barrel policies, precise animation release markers and effects remain open.

Changed: bridge LaunchSocketService plus assembly/settings/equipment integration;
Runtime muzzle aim/session/loadout/diagnostics; Studio socket editor, model preview,
Weapon Projectiles and modal lifecycle; CMake and LaunchSocketTests; architecture,
roadmap, feature matrix and docs/P3_LAUNCH_SOCKETS.md. No Wicked source/pin change.
Independent exact-commit verification remains required; no P3 gate closure.

Release commands: CL=/MP4; cmake --build BUILD/renegade --config Release --target
RenegadeEngineBridge -- /m:2 /p:BuildProjectReferences=false /verbosity:minimal.
Bridge initial PASS 188.96s. Runtime/Studio + selected test build PASS 72.51s.
Studio private-preview rebuild PASS 13.40s; modal rebuilds PASS 15.64s/15.57s;
binding layout PASS 15.56s; final one-time modal restore rebuild PASS 78.50s.
Full selected targets and commands are in docs/P3_LAUNCH_SOCKETS.md.
ctest --test-dir BUILD/renegade -C Release -R
'LaunchSocket|FirstPersonAssemblySettings|EquipmentAsset|RuntimeProjectileSession'
--output-on-failure --timeout 30: Studio-closed final 4/4 PASS 0.61s.
CPU fixture uses native transform/hierarchy systems; full Scene.Update without
graphics initially crashed. Studio-open test rerun timed out in Runtime Session
and LaunchSocket; cold retry passed. Intermittent job-test stall remains open.

Native shotgun socket placement, direction, Apply/Save and cold Studio reopen
PASS. Saved Muzzle primary root offset [0.013476461,-0.246668816,-0.062851310],
pitch 90. Assembly 07858b1d-4b5a-4be9-90ab-c9dd5629761c. Final modal Cancel restores
assembly once; closing it stays closed. Fire from Muzzle / Arrow assignment saved
to equipment b8e8ca55-55f3-49f9-bde7-2ad27a758562 and File > Save persisted scene.
Cold standalone --project BUILD/p3-shotgun-ui-project/ProjectileTest.renegade dx12
PASS 20.25s launch/check process: named Muzzle, real ground impact, pause, dry fire,
reload and reset. Impact-state muzzle [0.067675,3.238707,0.635946].
Evidence: BUILD/p3-socket-placement.png, p3-socket-final-reopen.png,
p3-socket-binding.png, p3-socket-native.log, p3-socket-native-events.json and
p3-socket-shot-impact.png / p3-socket-paused-impact.png. Colour quality of native
PrintWindow captures is not renderer parity. Standalone source proof is not
Build Game/package proof. Runtime copy also updated in Studio/Release/Runtime.

Next: precise animation release timing, then barrel/pellet policies and projectile
effects. Direct model-importer sockets, animated socket preview, gizmos, compact
layout, hitscan/surface decal profiles and Jolt-only/package proof remain open.
Do not merge P2 automatically. Unrelated Tools/__pycache__/ and log.txt untouched.

## P3 mouse preview checkpoint - 2026-10-06

Implementation commit: 1303ce75f5fc7bd2f89968afede416746beb6fdb.
Branch: feature/p3-projectile-impact. P2 PR #180 remains unmerged.

Projectile preview replaces nine buttons with left-drag orbit, right-drag pan,
wheel zoom and FIT / RESET VIEW. Fit clears camera pan/zoom and restores side
view. Drag begins in the image, continues outside and ends on button release;
hidden/model-changed/invalid previews clear drag. Saved appearance is unchanged.
Changed: Studio ModelImportPreview h/cpp, PlayerProjectileEditor,
StudioApplication.h; architecture, roadmap, feature matrix and preview evidence.

Release build PASS 77.69s; include rebuild PASS 16.82s; final hidden-drag cleanup
build PASS 16.02s. Commands: CL=/MP4; cmake --build BUILD/renegade --config
Release --target RenegadeStudio -- /m:2 /p:BuildProjectReferences=false
/verbosity:minimal. ctest --test-dir BUILD/renegade -C Release -R
'Projectile|EquipmentAsset|EquipmentActionState|PlayerViewRig|TestLevelSnapshot'
--output-on-failure: 10/10 PASS 2.21s before final cleanup. Exact-head full
rerun and Studio-closed retry stalled in unchanged RuntimeProjectileSessionTests;
owned tests were terminated, cause unresolved. Same command with
-E 'RenegadeRuntimeProjectileSessionTests': remaining 9/9 PASS 1.98s.
Investigate this intermittent test stall before P3 gate verification.
Diff check PASS. No Wicked source/pin or Runtime code change.

Native arrow orbit/pan/zoom/Fit/outside-edge drag/release visually passed.
Scale 1 and rotation [0,0,0] remain unchanged. Owner reports "Works perfect".
Evidence/commands/limits: docs/P3_PROJECTILE_PREVIEW.md, BUILD/p3-mouse-*.png.
No new serialized state; previous save/reopen and standalone firing proof remain.
No P3 gate closure or broader Sketchfab parity claim. Responsive compact layout
remains open. Next: weapon/palm sockets and runtime muzzle launch, as below.
Do not merge P2 automatically. Unrelated Tools/__pycache__/ and log.txt untouched.

## P3 projectile preview checkpoint - 2026-10-06

Implementation commit: 833faa473ab9ca9ab187040998564430d98191c0.
Branch: feature/p3-projectile-impact. Dependent P2 PR #180 remains unmerged.

Projectile editor now has an automatically refreshed isolated model preview.
Camera orbit/elevation, side/rear, zoom and fit are inspection-only. Scale and
XYZ rotation update a private appearance root using Runtime conventions;
physical size is shown in metres. Mesh preparation only runs on model/project
change. Hidden/project-changed previews release resources. Invalid appearance
values clear the preview and disable Save; correction recovers it. Meshless
projectiles remain valid. Saved copies display Custom / saved projectile.

Fixed control-theme tint/background blur, capsule/gizmo/outline overlays and
player-camera inset covering the modal editor. Number help is plain language.
Changed: Studio ModelImportPreview h/cpp, PlayerProjectileEditor,
StudioApplication h/cpp; architecture, roadmap, feature matrix, P3 continuation
and new docs/P3_PROJECTILE_PREVIEW.md.

Release Studio final build PASS 15.64s. Exact command: CL=/MP4; cmake --build
BUILD/renegade --config Release --target RenegadeStudio -- /m:2
/p:BuildProjectReferences=false /verbosity:minimal. Targeted ctest:
ctest --test-dir BUILD/renegade -C Release -R
'Projectile|EquipmentAsset|EquipmentActionState|PlayerViewRig|TestLevelSnapshot'
--output-on-failure: 10/10 PASS. Diff check PASS. No Wicked source/pin change.

Native supplied arrow proof: side view tip right; orbit, scale 1->2, X rotation
0->30, fit and zoom visibly update. Native Save As New and cold Studio Edit Copy
restore model ID, scale 2 and rotation [30,0,0]. Final cold reopen has no level
overlays. Invalid scale 500 plus Save click created no file; correcting to 2
restored image. Proof asset 63750c6e-86b7-43c1-9a56-3ac67c355f15 is disposable
and not assigned to the shotgun; original Arrow assignment remains. Standalone
source firing/ground impact/pause/dry fire/reload/reset PASS 11.52s. Earlier reset
missed while Studio startup stole focus; stable-focus rerun passed. Exact native
screenshots, commands, IDs and limits: docs/P3_PROJECTILE_PREVIEW.md.

Next: explicit weapon/palm sockets, imported FIRESPOT resolution, click placement
and orientation controls, action socket assignment and runtime launch/aim timing.
Camera-eye launch remains current. Static inspection only; flight/effect preview,
Hitscan/Beam, sticking, particles/trails, surface impacts, pellets, Jolt-only
coverage and packaged firing remain open. Compact responsive layout and mouse
orbit drag remain open. No health prerequisite or P3 gate closure is claimed;
independent exact-commit verification is required. Do not merge P2 automatically.
Unrelated Tools/__pycache__/ and log.txt are untouched.

## P3 projectile model/editor checkpoint - 2026-10-06

Implementation commit: d4d216b0ca9ea5aaadff0429d919f1fed7709747.
Branch: feature/p3-projectile-impact. Dependent P2 PR #180 remains unmerged.

Add -> Projectile creates project-level definitions under Content/Projectiles.
Imported model selection, uniform scale and XYZ rotation persist in schema v2;
v1 defaults are preserved. Import Mesh defaults to Content/Projectiles/Models
and returns to the retained draft with the committed model selected. Cancel
restores the draft. Weapon Projectiles still assigns immutable equipment copies.

Runtime caches prepared appearances per session, instances them on accepted
shots, follows simulated position/velocity and removes complete hierarchies on
impact, expiry and reset. Visuals are excluded from projectile world queries.
Registry dependency discovery and Test Level closure include model and textures.
Changed: ProjectileAssetService h/cpp; ReusableAssetDependencyService;
TestLevelSnapshotService; RuntimeApplication h/cpp, RuntimeLiveDiagnostics,
RuntimeProjectileWorld and new RuntimeProjectileVisuals; Studio projectile
editor/chrome/application; asset, query and session tests; architecture, roadmap,
feature matrix and P3 design/evidence/continuation docs.

Release Runtime/Studio/session build PASS 102.56s. Current Runtime copied to
Studio embedded Runtime. Exact build uses CL=/MP4 and cmake --build
BUILD/renegade --config Release --target RenegadeRuntime RenegadeStudio
RenegadeRuntimeProjectileSessionTests -- /m:2 /p:BuildProjectReferences=false
/verbosity:minimal. ctest --test-dir BUILD/renegade -C Release -R
'Projectile|EquipmentAsset|EquipmentActionState|PlayerViewRig|TestLevelSnapshot'
--output-on-failure: 10/10 PASS 2.50s. Diff check PASS.

Owner arrow ZIP converted via Blender 4.3 with supplied textures; original
retained untouched. Disposable native importer preview and projectile save/
shotgun assignment/scene save/cold standalone load verified. Rebuilt native
Import Mesh Cancel and successful ArrowReturn import restored the draft;
new model selected automatically. Standalone final ground-contact proof PASS
11.38s: accepted shots, pause, dry fire, reload and reset. Visual instance,
pause/freeze, expiry and reset proof also recorded. Paused arrow is small/distant;
a dedicated inspection preview remains required. Exact evidence and fixture IDs:
docs/P3_PROJECTILE_MESH_CHECKPOINT.md. Target design:
docs/P3_PROJECTILE_EDITOR_DESIGN.md.

Next: rotatable projectile preview, explicit weapon/palm launch sockets and
shot direction/animation timing; then Hitscan/Beam and material impact profiles.
Camera-eye launch is still current. Pellets, effects, sticking, damage authoring,
Jolt-only collision coverage and actual packaged firing remain open. No Player/
NPC health dependency, P3 gate closure, independent verification or P2 merge
is claimed. Only unrelated Tools/__pycache__/ and log.txt remain untracked.

## P3 live shotgun firing checkpoint - 2026-10-06

Implementation commit: 41308d3d68eeb8d2421bf57e21271c21309db180.
Branch: feature/p3-projectile-impact; dependent P2 PR #180 remains unmerged.

Accepted shotgun shots now launch cached Bullet definitions through the Runtime
session. Ammo/cooldown/equipped animation acceptance gates each launch; dry fire
and pause do not launch. Native scene queries produce attributed impacts and
bounded flight/contact feedback without requiring Player/NPC health. Test Level
snapshots include projectile dependencies. Initial camera-eye launch policy is
explicit; authorable muzzle sockets and pellet spread remain pending.

Changed files: RuntimeApplication h/cpp, RuntimeEquipmentLoadout,
RuntimeLiveDiagnostics, new RuntimeProjectileSession; bridge PlayerViewAnimation
and TestLevelSnapshotService; Studio PlayerProjectileEditor; PlayerViewRigTests,
ProjectileAssetTests, ProjectileSimulation.cmake and new Runtime session tests.
Architecture, roadmap, feature matrix and P3 authoring/continuation docs updated.
Exact commands, native evidence and limits: docs/P3_LIVE_SHOTGUN_PROJECTILES.md.

Release Runtime/Studio/bridge and targeted test builds PASS. Final Runtime build
PASS 15.49s; current exe copied to Studio embedded Runtime. Final targeted ctest
regex Projectile|EquipmentAsset|EquipmentActionState|PlayerViewRig|TestLevelSnapshot:
10/10 PASS 2.12s. Native Test Level firing PASS; standalone source proof PASS
11.15s including impacts, pause, dry fire, reload and reset. Orange ground contact
marker visually inspected. Save/reopen assignment verified in prior checkpoint.
Final overlay cleanup clears contacts on Screen/no-player paths; diff check PASS.

Next: authorable muzzle/aim convergence and shotgun pellets/effects, then packaged
firing proof. CPU scene mesh/collider coverage excludes Jolt-only bodies. Generic
Cast/AlternateUse/offhand launch acceptance is not claimed. P3 is open; independent
exact-commit verification required before gate closure. Do not add health as a
prerequisite or merge P2 automatically. Sky-shot distance was expected lifetime
behaviour; owner clarified it was not a defect. Unrelated untracked cache/log files
were left untouched.

## P3 projectile authoring and shotgun assignment - 2026-10-06

Implementation commit: 6f5401db2a7fa8d487aea09df781a4fd3a08ea13.

On feature/p3-projectile-impact, dependent on P2 PR #180. ProjectileAssetService
adds registered journaled .rprojectile assets; equipment schema v2 binds named
projectiles to authored semantic actions and retains v1 compatibility.
Native Starting Equipment -> Weapon Projectiles offers presets, project-only
name picking, search and Edit Copy. Assignment saves an immutable equipment copy
through the existing Player settings command; shared assets are unaffected.
The owner requires shotgun reference proof. Its PrimaryUse/Bullet assignment,
held presentation and Save Level/Reopen were verified on the disposable
BUILD/p3-shotgun-ui-project. Sword use was preservation testing, not firing proof.

Release targeted tests pass 5/5 (0.73s), including presentation-plus-projectile
sorted registry edges, save/reopen, rollback, legacy files, actual command
Undo/Redo and native scene cold reload. Studio and Runtime builds pass.
Native inspection found and fixed dependency ordering and create-window priority.
Exact commands/evidence/limitations: docs/P3_PROJECTILE_AUTHORING_UX.md.

Usable Player/NPC health is not implemented. Continue projectiles independently
with damage as an integration seam. Live shotgun launches, muzzle/aim rules,
visual/effect assets, Jolt coverage and packaged firing remain open. No P3 gate
closure or independent exact-head review is claimed. Do not merge P2 automatically.

## Explicit import folders, player roles and safe model moves - 2026-10-06

Implementation commit: b7471e2b0f1dedcc6efa4b4386fde358f04af2ed.

Model import now accepts any valid project-relative Content folder, including
new folders, with the selected browser folder as its default. Search tags are
optional labels and never route files. Explicit General model / Player arms /
Weapon roles let Assembly discover player parts outside the legacy folders.
Role metadata joins the existing import transaction. MOVE uses one journaled
transaction for the model, managed projection, optional thumbnail and registry;
asset IDs, dependency edges, tags and retained source bundles are preserved.
Transaction deletion supports rollback and interrupted recovery. Scope remains
reusable registered model .rasset products, not arbitrary assets or folders.
See docs/MODEL_IMPORT_DESTINATIONS.md.

Validation (Release x64, local owner device):
- BUILD/import_folder_verify.ps1 builds bridge, Runtime, assembly settings,
  assembly workflow proof, transaction tests and Studio using MSBuild /m:2
  /p:BuildProjectReferences=false, CL=/MP4. Final complete build PASS.
- ctest --test-dir BUILD/renegade -C Release
  -R 'ProjectDocumentTransaction|FirstPersonAssemblySettings|PlayerViewRig|Equipment|AssetCatalogue'
  --output-on-failure --timeout 30: 7/7 PASS, 1.40s.
- RENEGADE_ASSEMBLY_SWAP=BUILD/replacement-sword.glb;
  RENEGADE_HAND_COLLISION=1; RENEGADE_IMPORT_FOLDERS=1;
  RENEGADE_IMPORT_THUMBNAIL=BUILD/sword-assembly-authoring-proof5/replacement-sword-held-shield.png.
  BUILD/renegade/Tests/Release/RenegadeFirstPersonAssemblyWorkflowProof.exe
  BUILD/sword-ue-proof4 BUILD/sword-import-folder-proof3 --sword-playable:
  PASS, 8.86s. Custom folder + role + tags, invalid path/arms role rejection,
  forced move rollback, retained-source collision rejection, thumbnail move,
  stable-ID cold placement, assembly update/reopen, all masked preview slots,
  965 corrected frames / zero unresolved, saved-level and dependency closure.
- Native disposable BUILD/assembly-hand-ui-project: static GLB preview/import
  into Content/My Gear/Blades with Weapon role and iron,test tags; reopen editor
  and find card/tags; MOVE into new Content/Reorganised/Native; all three files
  verified in destination; selected card and tags retained. Assembly lists the
  moved weapon, loads it with the original shield/bindings, and SAVE CHANGES
  preserves the Player Start assignment. Standalone opens this saved project.
- BUILD/chain_focus.ps1 against that standalone: PASS, 9.77s. Released
  directional queue, held charged chain, single dispatch, shield independence;
  unresolved counters 0 -> 0. BUILD/chain-native-events.json and native capture.
- Static and character importer layouts visually inspected at 1920x1080.
  Fixed static animation-control visibility, sibling render scissor clipping,
  MOVE/collapse hit-target overlap, move-dialog click-through and taller
  character panel positioning. Final Studio-only refresh PASS.
- git diff --check PASS.

Failures retained as evidence: initial Studio link failed while the test editor
held the executable open; closed only that test editor and rebuilt. An edit made
during an earlier bridge build left the transaction object stale; touching and
rebuilding corrected it. First model-move proof found a Windows projection
stream lock; explicitly closing that stream before transaction fixed it.
Proof2 passed all new import/move checks but hit one existing bounded collision
fallback at direction0/time0.533333/residual0.00798244; proof3 and native chain
passed with zero unresolved. This does not establish universal no-clipping.
No animation/avoidance algorithm changes in this work.

Owner's original project/editor were not modified by these native checks.
Local commits only; no push. Independent exact-commit review and owner usability
approval remain pending; no release gate or Wicked parity claims are closed.
Fixed-height importer still needs a compact/scrolling treatment for smaller
displays; this check covers the owner's 1080p layout.


## Independent hand transition blending - 2026-10-06
Implementation commit: 8810af7a675f7a8a86d5f3d6b9dd085cdaca0020.
PlayerViewHandBlend.h captures the last evaluated local pose per hand and
restores it as a fixed transition origin before Wicked evaluates the destination
masked clip. Native AnimationComponent.amount supplies translation/scale lerp
and quaternion slerp; no custom clip sampler or Wicked source change.
Smoothstep fades: strike entry 60ms; charge/direction/hold 100ms;
shield start/loop/end 120ms; primary locomotion/recovery/cancel 140ms.
Interrupted fades snapshot the displayed pose. Loop wraps do not restart fades.
Zero or nonfinite dt leaves initialized hand state and blend clocks unchanged.
Shared base and gameplay charge/release/completion ownership remain unchanged.
The paired shotgun already uses its existing native crossfade path.
Scope is the currently bound sword/shield locomotion, attacks, directional
Charge/Hold/Release and BlockStart/Loop/End; unbound pack actions are retained
but not newly routed by this task. No new animation authoring UI or schema.

Changed files: EngineBridge/include/renegade/bridge/PlayerViewHandBlend.h,
PlayerViewHandAnimation.h; Tests/PlayerViewHandBlendTests.h,
PlayerViewRigTests.cpp and SwordShieldPlayableProof.h.
Validation:
- BUILD/blend_build.ps1: MSBuild Release x64 bridge, Runtime, PlayerViewRig
  and assembly proof using /m:2 /p:BuildProjectReferences=false, CL=/MP4 PASS.
- ctest --test-dir BUILD/renegade -C Release
  -R 'FirstPersonAssemblySettings|PlayerViewRig|Equipment'
  --output-on-failure: 5/5 PASS, 0.78s.
- Native T/R/S midpoint, fixed-origin drift, interrupted restart, paused elapsed
  and disjoint hand tests PASS in PlayerViewRig.
- BUILD/blend_proof_build.ps1 builds the expanded manual fixture proof.
  Run BUILD/renegade/Tests/Release/RenegadeFirstPersonAssemblyWorkflowProof.exe
  BUILD/sword-ue-proof4 BUILD/sword-playable-blend-proof2 --sword-playable:
  PASS, 7.46s. Four charge and hold fades, release midpoint, direction
  interruption, cancel, save/reopen and TestLevel dependency closure checked.
  Second output directory used because repeat save correctly rejects an existing
  assembly destination; original proof output preserved.
- Captures stab-blend-charge.png, stab-blend-release.png and
  blend-cancel-idle.png in BUILD/sword-playable-blend-proof2 inspected.
- BUILD/directional_focus.ps1 native mouse test PASS, 27.07s:
  four selected charged releases, independent held shield, low-charge quick tap.
  Evidence BUILD/directional-native-events.json.
- BUILD/sword_studio_build.ps1 clean alternate Studio Release PASS, 108.43s.
- Owner playing updated standalone reports: 'they do look better'.
- git diff --check PASS. Existing desktop SwordShieldTest uses rebuilt Runtime;
  no asset migration needed. Owner editor stays open.
A separate screenshot automation attempt lacked PIL; no dependency installed;
native input and offscreen evaluated-pose captures supply the evidence instead.
No Wicked Editor parity or packaged acceptance claim for this new slice.
No release gate closure.

Next priority explicitly requested by owner: blade/shield collision-aware arm
pose correction (shoulder/elbow/wrist while preserving grip), separate from NPC
damage. Blending does not prevent interpenetration. Collision correction, generic
authoring, unused action routing, damage, stamina and parries remain open.

## Directional charge playback - 2026-10-06
Replaces automatic basic-attack cycling in the owner fixture with explicit
four-direction Charge/Hold/Release bindings. Hold LMB, move left/right/down/up
to select Left/Right/Down/Stab; release to strike. Quick tap uses last direction.
Gesture threshold 0.025 radians ignores small motion; selection consumes camera
look while LMB held. Charge strength saturates at 1 second and is recorded on
release; no damage calculation or hit detection is claimed. RMB block independent.
C cancels pending charge; zero dt freezes clocks and selection.
Runtime shows selected direction and charge percent near screen centre.
Legacy paired shotgun and existing basic-attack independent assemblies keep
their existing path unless all 12 directional clips are explicitly bound.
Native fixture proof checks all four charge-to-hold/release groups, four unique
release clips, held shield, zero-dt freeze, low-strength quick release and cancel.
Release bridge/Runtime builds and five focused CTests pass; fixture cold load
and TestLevel equipment/presentation closure pass.
Evidence BUILD/sword-playable-directional/sword-charge-0..3.png and
sword-release-0..3.png. Owner project Desktop/renegade tests/SwordShieldTest;
previous cycle fixture retained as SwordShieldTest-attack-cycle.
Files: PlayerViewHandAnimation.h, PlayerViewAnimation.h,
FirstPersonAssemblyService.cpp, RuntimeApplication.cpp/.h,
RuntimeLiveDiagnostics.cpp, SwordShieldPlayableProof.h.
Reproduce BUILD/sword_variants_build.ps1 (directional output).
Remaining: animation fades, sword/shield clipping, authored charge settings,
stamina, directional block/damage, collision and generic v2 UI. No P2 gate closure.

## Four sword attacks - 2026-10-06
Independent schema-v2 assemblies now admit multiple explicit Attack bindings
to distinct source indices. Legacy paired assemblies still reject duplicate
actions. Shield/movement/attack bindings cannot alias a source clip.
Recipe order persists as native attack_order metadata and sorts generated
primary clips, so LMB cycles Left, Right, Down, Stab in the owner fixture.
Each swing completes before another is accepted; no click buffering added.
Shared arms orientation and lowered grip retained. Independent held block retained.
Five focused CTests pass; Release bridge/Runtime builds pass; saved/reopened
fixture TestLevel closure passes; four unique generated attack clips exercised
with shield phase2 maintained and rendered captures inspected.
Evidence: BUILD/sword-playable-variants/sword-variant-0..3.png.
Updated owner project stays Desktop/renegade tests/SwordShieldTest;
prior single attack project preserved in SwordShieldTest-single-attack.
Files: FirstPersonAssemblyService.cpp, FirstPersonHandAssemblyPreparation.h,
PlayerViewHandAnimation.h, FirstPersonAssemblySettingsTests.cpp,
SwordShieldPlayableProof.h. Reproduce BUILD/sword_variants_build.ps1.
Still no directional input, charge mapping, damage, collision or parry.
Sword/shield clipping remains visible; no P2 gate closure.
Next: compatible attack/block poses and blending, then directional selection.

# Sword/shield playable checkpoint - 2026-10-05
Implemented schema-v2 shared arms with static primary/off-hand attachments,
explicit clavicle partitions, transient native per-hand clips and independent clocks.
Runtime routes LMB Attack and RMB held Block only for a matching shared presentation.
Fixed missing MOUSE_RIGHT press support in GameplayInputService.
Owner requested camera-local 90-degree right yaw and sword 2cm lower in grip.
Saved project: C:/Users/paulw/OneDrive/Desktop/renegade tests/SwordShieldTest.
Launcher: Play Sword Shield Test.cmd. All 35 source clips retained; only Idle,
Walk, Sprint, AttackLeft and BlockStart/Loop/End wired for tonight.
Release bridge and Runtime builds pass. Alternate Studio build passes at
BUILD/sword-studio (owner Studio untouched). Five focused CTests pass:
PlayerViewRig, source contract, FirstPersonAssemblySettings (v1/v2/undo),
EquipmentActionState, EquipmentAsset.
Native input evidence: BUILD/sword-native-events.json: equipment ready,
independent hands enabled; held block phase2 concurrent primary attack;
release phase3 then phase0. Fixture save/reopen and TestLevel closure checked
by --sword-playable in Tests/SwordShieldPlayableProof.h (final output proof6).
No gate closure. Remaining: sword/shield clipping is visible and unsolved;
no collision, hit damage, directional selection, parry, charge or equip wiring.
Hard animation transitions currently; blends and compatible combined poses need work.
Generic v2 assembly editor controls not yet exposed; fixture authored through service.
Native TestLevel snapshot smoke is required before claiming editor-button parity.
Files: FirstPersonAssemblyService.h/.cpp, FirstPersonHandAssemblyPreparation.h,
PlayerViewHandAnimation.h, PlayerViewAnimation.h, GameplayInputService.cpp,
RuntimeApplication.cpp, RuntimeLiveDiagnostics.cpp, RuntimeEquipmentLoadout.h,
FirstPersonAssemblyGraphicsProof.cpp, FirstPersonAssemblySettingsTests.cpp,
SwordShieldPlayableProof.h.
Reproduce: BUILD/sword_bridge_runtime_build.ps1, sword_playable_build.ps1,
sword_tests.ps1; manual native input BUILD/sword_native.py.
Next: correct clipping with compatible block/attack poses, meaningful per-hand
controller edge-case tests and v2 authoring controls; do not declare P2 complete.

## 2026-10-05 - Native independent hand animation masks

Implementation commit: 7c7f7b3692a3f0c647a922ac9e3b310c1afe6052.
Changed: EngineBridge/include/renegade/bridge/PlayerViewAnimationMask.h;
Tests/PlayerViewAnimationMaskTests.h; Tests/SwordShieldLayerGraphicsProof.h;
Tests/PlayerViewRigTests.cpp; Tests/FirstPersonAssemblyGraphicsProof.cpp.
Documentation follow-up: docs/P2_HAND_ANIMATION_MASKS.md, architecture,
equipment implementation notes and feature matrix.

Bridge candidate partitions explicit roots on one armature into disjoint hand
and base bones; transient transform-only native clips share retained keyframes.
No source clip mutation, custom evaluator or Wicked source/pin change.
Synthetic native regression checks distinct hand times, fingers, lifetime and
atomic overlap/foreign/cyclic/retarget/external-data rejection.

Commands/results:
- powershell -NoProfile -ExecutionPolicy Bypass -File BUILD/sword_layer_verify.ps1
  builds Release RenegadePlayerViewRigTests and assembly graphics proof with
  MSBuild /m:2 /verbosity:quiet /nologo /p:Configuration=Release
  /p:Platform=x64 /p:BuildProjectReferences=false, CL=/MP4.
  Final build/test/graphics run passed (29.54 seconds).
- BUILD/renegade/Tests/Release/RenegadeFirstPersonAssemblyWorkflowProof.exe
  BUILD/sword-normalized-source BUILD/sword-layer-proof --sword-layers
  PASS: 21 primary + 21 off-hand + 26 base bones on one 68-bone armature;
  four attacks preserve held left world matrices within 4.17233e-07;
  block start/end preserve attacking right matrices within 8.9407e-07;
  0.0001 tolerance, right motion 1.43963. Native loop progression, pause
  and right-clip restart without resetting left clock pass.
- ctest --test-dir BUILD/renegade -C Release --output-on-failure
  -R 'PlayerViewRig|EquipmentActionState|EquipmentAsset|GameplayInput'
  passed 5/5, 1.38 seconds.
- Visual inspection: held-block.png, AttackLeft-0.350000.png,
  BlockEnd-0.950000.png and released-idle.png in BUILD/sword-layer-proof
  show distinct masked poses with neutral matte diagnostic material.
  Twenty captures produced. git diff --check passed.

Initial mask proof failed a base-bone invariant on procedural head movement
(delta 0.0798943); the arms stayed independent. Clearing the imported humanoid
modifier on the private proof scene isolates native clip evaluation and fixes
that diagnostic. No production sanitation change is made by this checkpoint.

Limits/next: no final textures, weapon attachments, first-person framing,
authored layer schema or live input/controller integration. Generated mask
clips are ephemeral and require caller-owned cleanup before view-model unload.
Live off-hand capability remains false. Next is governed two-hand mesh/layer
authoring and socket alignment, then Runtime start/held/end, recovery/blending,
Test Level and independent package verification. No gate closure claimed.
No editor foreground takeover; owner projects untouched.

## 2026-10-05 - Sword/shield source verification (parent da550c8)

Changed: Tests/FirstPersonAssemblyGraphicsProof.cpp adds manual --sword-inspect owner-asset proof. No gameplay capability enabled, no gate closure.

Inputs: user Idle.FBX with arms mesh (SHA256 6025ad752595c3a0210e496d8c538208737d2cfaf2565d29bb8711dafb528633) and sword/shield pack (SHA256 64d46273d6fff7ca321200eff551cfd1a2f5a8b82606c71e977284b73404d451). Original animation-only source FBXs fail strict inverse-bind matching; humanoid fallback produces unacceptable grip poses. Strict matching was not weakened.

Preparation: isolated BUILD/sword-unreal-project copies supplied Unreal Content; UE 5.7 commandlet exports animation-only FBXs with export_preview_mesh=false. Maps BlockIdle to BlockLoop. Both X/Y sample exports match the supplied 68-bone arms directly. Preview-mesh export with null RHI failed; animation-only export succeeded. BUILD/sword-normalized-source holds the supplied Idle plus normalized clips and original weapon FBXs. Blender headless converts original static weapon meshes to GLB with export_animations=false because their FBXs contain empty animation takes. Original sources unchanged. Export helper scripts remain ignored BUILD diagnostics; owner assets are not committed.

Validation: Release RenegadeFirstPersonAssemblyWorkflowProof build, then executable BUILD/sword-normalized-source BUILD/sword-ue-proof4 --sword-inspect. Asserts 35 native clips, 34 retained matching-rig external sources, unchanged skin indices/inverse binds, saved asset reopen and retained-source recipe rebuild. Sword and shield static mesh assets save/reopen/render. Diagnostic action labels are reapplied for rebuilt pose selection; source recipe reconstruction does not preserve these temporary labels. Earlier rebuilt screenshots blended all tracks due absent labels; corrected diagnostic selection before final inspection.

Limits: neutral/static weapon previews and dark arms material; no final texture proof, socket alignment, simultaneous left-block/right-attack masking or standalone gameplay proof. Live off-hand capability remains disabled. Next task: one native arms rig with independent left/right channel masks and correct weapon attachments; verify idle, attack and held block before enabling gameplay. No editor foreground takeover in this source check.

## Off-hand action/input foundation - 2026-10-05

Prepared independent hand ownership while waiting for real sword/shield clips.
Channels/events identify the initiating hand; release/cancel/retarget/completion
can be scoped even when both hands reference the same asset. Held Block uses
active_while_held (optional v1 field): positive time enters Active, release enters
Recovery, and native primary completion cannot finish the shield channel.
OffHandUse defaults to right mouse with older input documents migrated only in
memory. Authored rebinding survives round trip. Pure Runtime adapter can route
concurrent primary attack and off-hand Block when explicitly capability-enabled.
The live Runtime keeps this capability false until real native off-hand clips
are integrated and checked; the shotgun remains a primary/two-hand regression.

Release bridge, Runtime and focused test builds passed using
BUILD/offhand_bridge_build.ps1 and BUILD/offhand_verify.ps1. Five CTests passed:
GameplayInput, PlayerViewRig/source contract, EquipmentActionState, EquipmentAsset.
Tests cover same-asset hand isolation, held shield release/recovery, paused
primary with advancing shield, pre-active cancel, schema/rebinding migration and
capability gating. No sword/shield native animation or independent gate proof
is claimed. Original owner projects were not changed.

## Player preview controls - 2026-10-05

Owner accepted the selected-player camera inset and requested collapse and resize.
Click the header to collapse/expand; collapse persists in Studio preferences and
pauses preview preparation/rendering. Drag the upper-left handle to resize the
16:9 image, anchored bottom-right, bounded by the scene viewport. Width is kept
for the current Studio session. Header clicks and resizing consume viewport input
so they cannot pick objects or move the editor camera behind the overlay.
Preview remains available only while a visible Player Start is selected.

Validation: Release bridge + clean Studio rebuild passed (BUILD/preview_clean_build.ps1,
100.18 seconds). Initial incremental build produced a startup exception; rebuilding
all Studio translation units repaired it. Native inspection passed enlarged
596-pixel width, minimum 240-pixel width, collapse/expand and selected-marker
retention. Collapse preference was read back from RenegadeStudio.ini and restored
after selecting another object and returning to Player Start. Evidence:
BUILD/player-preview-resized-large.png, player-preview-resized-small.png and
player-preview-collapsed.png. Source diff check passed. Render image is the
existing 432x243 cached texture; resizing changes its display bounds, not FOV.
Off-hand foundation is separate commit cd94a28 and its five focused tests passed;
native off-hand presentation is still disabled pending real sword/shield clips.

## Selected Player Camera Preview inset � 2026-10-05

Implementation commit: 6a2bf9753d7c7cc1aa2d6dd59832c11ad5fc13a5.
Selected Player Start now shows a small bottom-right scene-camera inset with the
world and equipped primary arms/weapon. It uses Runtime spawn yaw, eye height
and default 60-degree FOV. No separately authored player FOV exists yet.

Changed: bridge PlayerCameraPreviewService plus shared PlayerViewRig/Asset/
Animation headers; Runtime compatibility headers; StudioApplication and live
diagnostics; DX12 graphics proof and source contract; README, architecture,
feature matrix and player continuation. No Wicked source or pin changed.

Preview uses a private world copy, removes gameplay actors/physics/scripts/audio,
poses native Idle, suppresses during specialist workspaces/Test Level and caches
eight prepared frames. Edits settle 0.2 seconds; re-selection refreshes asset
changes. Its rectangle blocks viewport picking/navigation. Helpers do not enter
authoring WISCENE. Explicit equipment retains Runtime ownership rules.

Windows Release Studio and Runtime builds passed. Five focused CTests passed
(PlayerViewRig/source contract, prefab, equipment action/asset). DX12 proof checks
cold world/arms loading, no gameplay components, changed eye height/yaw, unchanged
source settings/component counts and contrasting rendered pixels. Images
BUILD/player-camera-preview-evidence/camera-preview.png and
camera-preview-turned.png were inspected. Initial dt==0 preview rendered sky only:
Wicked skips GPU geometry/instance allocation at zero. Minimal positive render
preparation followed by zero GPU effect time repaired it; native clips are paused.
The proof now rejects empty images.

Native Studio selected-capsule inspection shows the inset at bottom-right with
world and shotgun; approximately 75 FPS in the tiny validation scene. Clicking
inside the inset retains selection. BUILD/player-preview-window.png is evidence.
Standalone Charge/Release/cancel regression passed after moving shared headers.
Original owner Studio stayed open. Only the launched Runtime was closed; the
alternate preview Studio remains open for review. Startup capture initially
preceded the foreground window, and dialog SendKeys lost its first character;
native pointer opening corrected test setup. These are not accepted product
changes.

Commands from repository root:
powershell -NoProfile -ExecutionPolicy Bypass -File BUILD/player_preview_build.ps1
(same invocation for player_preview_verify.ps1 and player_preview_proof.ps1);
the final build repeats after refresh/caching fixes.
Graphics mode: BUILD/renegade/Tests/Release/RenegadeFirstPersonAssemblyWorkflowProof.exe
<validation-project> BUILD/player-camera-preview-evidence --camera-preview.
Standalone: same PowerShell invocation for BUILD/p2_charge_launch.ps1 and
BUILD/p2_route_capture.ps1; python BUILD/p2_charge_native.py.
Native inspection: same PowerShell invocation for BUILD/player_preview_launch.ps1
and BUILD/player_preview_capture.ps1, with pointer-only fixture navigation.
git diff --check passed. Studio output is BUILD/player-preview-studio.
Embedded build metadata still predates compiled edits; no exact-head independent
verification or owner acceptance is claimed.

Limits: private full-world cloning needs large-level memory/time profiling.
Complete render-settings/postprocess parity and configurable FOV remain follow-ups.
Preview currently presents the primary assembly; independent off-hand presentation
remains pending. P2 and release gates remain open.
Next: prepare separate hand input/routing and instance-scoped reservations, then
validate actual independent sword/shield clips when the owner's pack arrives.
A two-handed shotgun is only a reservation regression, never off-hand proof.

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

Verification commands from repository root:
- powershell -NoProfile -ExecutionPolicy Bypass -File BUILD/build_p2_assets_runtime.ps1
- powershell -NoProfile -ExecutionPolicy Bypass -File BUILD/p2_runtime_route_verify.ps1
  (four tests passed; proof compilation initially failed on a missing namespace import)
- powershell -NoProfile -ExecutionPolicy Bypass -File BUILD/p2_equipment_proof.ps1
  (passed after fixing the proof namespace)
- powershell -NoProfile -ExecutionPolicy Bypass -File BUILD/p2_route_launch.ps1
- python BUILD/p2_route_diagnostics.py
- powershell -NoProfile -ExecutionPolicy Bypass -File BUILD/p2_route_capture.ps1

Next: staged action clock and native presentation transition integration, then independent
off-hand support and package verification. The approved small Player Camera Preview inset
inside the editor remains pending. P2 and release gates remain open.

## Latest checkpoint: PR #178 merged; owner acceptance and UX follow-up - 5 October 2026

## 2026-10-05: first P2 checkpoint (in progress)

Implementation commit: `f685c5144668859d7e8ad9f4315666f16cfb3819`.
Branch: `feature/p2-equipment-actions`. Authored P2 remains the governing scope;
this checkpoint does not close P2, P1 or the Alpha release gate.

- Assembly Arms/Weapon pickers now use dedicated folder eligibility and saved
  assembly recipe roles. Existing/shared-pack parts remain usable without moving
  files. Completed assemblies and unrelated imported models are excluded.
  Stable-ID user-data preserves selection across different filtered row orders.
- Pure bridge equipment action foundation covers all five hand-use policies,
  phased events, hold/release, cancellation before active, pause and reset.
  It is not yet persisted or wired into Runtime; existing shotgun behavior is
  unchanged by this gameplay foundation.
- Windows Release Studio build passed before the checkpoint commit. Native
  filtered Arms/Weapon dropdowns, saved assembly opening and close/reopen retained
  the paired textured preview and action mappings in an isolated validation copy.
  The visual build embeds the earlier documentation revision `59e705e9`;
  its picker implementation matches this checkpoint. No exact-head independent
  verifier or owner acceptance of this new slice is claimed.
- Release CTest `FirstPersonAssemblySettings` and `EquipmentActionState`:
  2/2 passed after the final hand-policy test additions. Regression covers 100
  unrelated models, dedicated/shared folders, malformed provenance, all five
  hand policies, phase/event behavior, hold/release, pause, cancellation and reset.
- `git diff --check` passed. Existing untracked local files were left untouched.

Next authored P2 work: durable equipment assets/identity, starting-loadout
Inspector and prefab persistence, Runtime ownership and semantic action routing,
then snapshot/transitive packaging and owner gameplay acceptance.
See [P2 implementation](docs/P2_EQUIPMENT_ACTION_IMPLEMENTATION.md) and
[remaining UI/UX work](docs/PLAYER_AUTHORING_UX_FOLLOWUP.md).

Implementation merge: `7105a95ddcc103d6a024a17fddefd62f705a86b3` on main.
PR #178 head before merge: `207ba864e79211992b6eb11465c5a23850c36d3c`.
Documentation branch: `docs/player-pr178-acceptance-ux-followup`.
The documentation commit immediately following this checkpoint records these edits.

PR #178 merged into main on 2026-10-05 at `7105a95ddcc103d6a024a17fddefd62f705a86b3`.
All four pre-merge Windows checks passed: Studio Debug/Release and baseline
Debug/Release. The owner confirmed the expected player/shotgun behaviour in the
existing v2 game project and explicitly accepted the PR's functionality.
This records functional acceptance of the merged scope, not completion of every
P1 requirement or of the full Alpha Playability combat programme.

Changed documentation: README.md, CHANGELOG.md, HANDOFF.md, docs/ROADMAP.md,
ARCHITECTURE.md, FEATURE_MATRIX.csv, PLAYER_AUTHORING_CONTINUATION.md,
P1_ASSEMBLY_AUTHORING.md, P1_STATUS_AND_RECOVERY.md,
PLAYER_ARMS_COMBAT_FRAMEWORK.md and new PLAYER_AUTHORING_UX_FOLLOWUP.md.
No engine code, assets, schema or Wicked pin changes.

Evidence:
- `gh pr view 178 --json state,mergeCommit,mergedAt`: MERGED at
  2026-10-05T15:30:48Z, merge commit above.
- Exact pre-merge head had four successful checks: Studio Windows x64
  Debug/Release and Windows baseline Debug/Release.
- `RenegadeFirstPersonAssemblyWorkflowProof.exe <isolated-transfer-project>
  <evidence> --full-library-reopen`: exit 0; 14 paired actions, 28 native tracks,
  two armatures and ten retained textures.
- `RenegadeFirstPersonAssemblyWorkflowProof.exe <isolated-package>
  <evidence> --runtime-package`: exit 0; paired load/pose/pause/cleanup,
  capacity 2, partial reload enabled.
- Native v2 Runtime visual inspection showed textured arms/shotgun reloading.
  Owner then confirmed everything works as expected and accepted PR scope.
- Transfer required manual project identity, dependency/provenance registration
  and exact canonical serialization. It is not a shipped one-click importer.
- Documentation validation: git diff --check, CSV width/identity and Markdown
  local-link checks; exact results recorded in the documentation PR.

Risks/next task: functionality accepted; authoring UX explicitly needs a revisit.
Scope dedicated arms/weapon picker collections or folders, combined-folder pack
roles, readable prefab/assembly names and dependency-aware project adoption.
Preserve existing automatic preview refresh and check loading/error feedback.
Independent equipment, reserve ammo, hits/damage, recoil and wider combat gates
remain open. Do not mark the whole Alpha programme or every P1 requirement closed.
Keep earlier checkpoints below as historical evidence.

## Latest checkpoint: Wander normal role with 2x2 terrain-chunk extent - 2 October 2026

Branch: feature/character-animation-crossfades.

Added Wander as a first-class Character Role/Behaviour dropdown option without
changing existing persisted role values. CharacterAuthoringSettings now owns a
wanderExtentChunks value with default 2; the value persists through native
Character metadata, WISCENE entity serialization, duplication and Character
Prefab serialization. Existing prefabs without the field default to 2.

Runtime Wander uses the existing Wicked CharacterComponent/PathQuery movement
pipeline. At Runtime start it captures a fixed Wander origin from the Character
spawn position and derives world chunk span from the active Terrain chunkScale.
It chooses deterministic meandering destinations inside the fixed configured
extent, rejects/retries unreachable goals through the existing navigation query,
idles for a deterministic 1-4 seconds on arrival, then chooses another point.
Stuck recovery also abandons the current random destination rather than retrying
it forever. Wander uses Locomotion while travelling and Idle while paused.

Combat/threat states still interrupt the normal role. When the threat is dead or
pursuit disengages, NormalRoleIntent resolves back to Wander and requests a fresh
Wander destination. The existing pursuit/vision separation and animation
crossfade implementation were not replaced.

Diagnostics expose first_wander_origin, first_wander_extent_chunks and
first_wander_visit_count alongside existing intent/goal diagnostics.

Validation:
- EngineBridge Release rebuild: exit 0.
- AI Profiles, AI Decision, AI Combat and CW05 Character Prefab executables: pass.
- Focused CTest Profiles/Decision/Combat + source contracts + CW05 Prefab: 8/8 pass.
- Runtime RenegadeRuntime_Wander Release build: exit 0.
- Studio RenegadeStudio_Wander Release build: exit 0.
- git diff --check passes.
Known build warnings are the pre-existing MSB8029 build-directory warnings plus
existing C4834 nodiscard warnings in unrelated RenderSettings/Studio code.
No push or merge.

## Previous checkpoint: patrol/guard return diagnostics and NPC-centred pursuit - 2 October 2026

Branch: feature/character-animation-crossfades.

Pursuit origin is now the Character's own position when hostile engagement begins
(last Runtime position, then authored guard/start post as fallback), not the
player's first-seen position. Crossing Pursuit Radius suppresses Chase/Attack and
returns utility to the configured normal role. Patrol resolves its existing
authored route target; Guard resolves its saved start post. Transition diagnostics
now report pursuit origin, subject, exhausted flag, decision goal and the explicit
reason "Pursuit radius exceeded -> return to <role>".

Focused Character AI Profiles/Decision/Combat Tests + SourceContract: 6/6 pass.
Decision and Studio Release targets compile; Runtime ReturnRole build exists.
git diff --check passes. No push or merge.

## Previous checkpoint: authored pursuit leash and disengagement - 2 October 2026

Branch: feature/character-animation-crossfades.

Added a profile-driven Pursuit Radius (default 45 m) to Advanced AI. Runtime
latches the hostile engagement origin, suppresses Chase/Attack once the target
moves beyond the authored radius, and allows re-engagement if the target comes
back inside. The existing normal-role utility then resumes Guard/Patrol/Idle
behaviour; existing search/death handling remains intact. AI-05 combat scoring
respects the same exhausted pursuit subject so Attack cannot override disengage.

Changed: CharacterProfileService.h, RuntimeCharacterDecision.h,
RuntimeCombatDecision.h, AICharacterInspector.cpp, CharacterAiDecisionTests.cpp,
CharacterAiCombatTests.cpp. Studio and Runtime v145 Release builds pass. Focused
CTest: CharacterAi Profiles/Decision/Combat Tests + SourceContract = 6/6 pass.
git diff --check passes. No push or merge.

## Previous checkpoint: continuous action tails and post-combat roles - 2 October 2026

Branch: feature/character-animation-crossfades.
Implementation: e98deef50b7368c6593d191a4949df1ac45cb3ff (following 59420a5).
The following documentation commit records this exact implementation. No push
or merge. Independent exact-commit verification remains pending.

Owner reported: "that is significantly better" during native gameplay of the
continuous-tail build. Final self-sound exclusion was then tested locally;
that incremental behaviour fix has not received separate owner acceptance.

Changed files: Runtime/src/RuntimeCharacterAnimation.h, RuntimeCharacterDecision.h,
RuntimeCharacterPerception.h, RuntimeCombatDecision.h, RuntimeCombatService.h,
RuntimeLiveDiagnostics.cpp; Tests/CharacterAiAnimationTests.cpp,
CharacterAiCombatTests.cpp, CharacterAiDecisionTests.cpp,
CharacterAiPerceptionTests.cpp, CharacterAnimationBlendProof.h,
ModelImporterRebuildGraphicsProof.cpp; docs/ARCHITECTURE.md and FEATURE_MATRIX.csv.

Melee playback now overlaps the outgoing action's moving tail, including a sole
attack replay with two independent native timers. Attack transition: 0.18s.
Quietest rotational motion among assigned Idle clips becomes the base loop;
other Idle clips are occasional 12-second variations, play once, then fade back.
Dead player knowledge is excluded, perception stops observing dead players, and
self-generated attack sounds cannot create search memories. Patrol requires an
assigned route; Guard remembers and returns to its initial post. No route was
invented or added to the owner Level.

Windows x64 Release commands (installed VS18 MSBuild and CTest):
- MSBuild BUILD/no-import/Runtime/RenegadeRuntime.vcxproj /m:2 /verbosity:quiet /nologo /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /p:TargetName=RenegadeRuntime_NaturalPlayback
- MSBuild BUILD/no-import/RenegadeCharacterAiPerceptionTests.vcxproj with the same Release flags (no TargetName); likewise root DecisionTests and CombatTests targets.
- MSBuild BUILD/no-import/Tests/RenegadeCharacterAiAnimationTests.vcxproj with the same Release flags (no TargetName).
- MSBuild BUILD/no-import/RenegadeModelImporterRebuildGraphicsProof.vcxproj with the same Release flags (no TargetName).
- ctest --test-dir BUILD/no-import -C Release -R "CharacterAi(Animation|Decision|Combat|Perception)(Tests|SourceContract)|ModelImporterRebuild|HumanoidRetarget" --output-on-failure
- BUILD/no-import/Release/RenegadeModelImporterRebuildGraphicsProof.exe --inspect-scene BUILD/no-import/blending-owner-captured/Content/Scenes/TestLevel.wiscene BUILD/no-import/natural-playback-owner-proof

Final Runtime, perception, decision and combat builds exit 0. Animation and
native graphics proof builds pass. CTest 15/15, exit 0, 15.70s; includes existing
cold-reopen importer checks. Synthetic native TRS blending, action-tail overlap,
sole-attack replay, idle variation, dead-target role return and self-sound checks
pass. Captured owner scene native render proof passes, with nonblank textured
poses inspected. Actual idle scores: breathing 1.09575, flex 19.485, stretch 9.59737.
No serialized authoring changes; owner Level was not saved or overwritten.

Native Test Level final run: two characters, 13 assigned clips; five hits;
Attack -> Idle at about 7.95s when player health reaches zero; breathing at 10.42s;
flex once at 22.34s; breathing again at 26.94s. No Search after death. Existing
older character has only the stretch idle assigned and therefore retains it.
Screens from continuous gameplay were inspected; owner confirms substantially
better motion. Owner confirmation does not replace independent exact-head review.
Logs and private captures remain ignored under BUILD/no-import/natural-*.

Staged bundle: BUILD/no-import/BlendingTest/Runtime/RenegadeRuntime.exe.
SHA256 CDD417C6862A724ED049DDDCF0FF47076DAF5CE7A6BB6EB39169F49296A6729A.
Paired Studio remains the accepted importer bytes. Original Studio and Runtime
SHA256 remain unchanged (34AB9968...F5561 and 0F097DB8...B4C). Earlier bundle
runtime is preserved as RenegadeRuntime_PreNaturalPlayback.exe. Editor remains
open with owner setup; no original importer executable was replaced.

Limits: idle selection is a motion heuristic, no authored base-idle UI yet;
fixed fade durations; no gait phase synchronization, layers, additive blending
or blend spaces. Melee damage still uses the existing fire event, not authored
contact markers. Events/non-transform coverage keep guarded immediate fallback.
Next: owner verify this final bundle, especially route Patrol/Guard return, and
independent exact-commit review before merge or broader acceptance claims.
# Renegade Engine — Current Handoff

## Current importer recovery: static FBX - 1 October 2026

Branch: feature/model-importer-rebuild. Base: 8749a20, the recorded independent
native GLB verification. This checkpoint adds static FBX with embedded and
source-folder-relative textures while preserving the accepted GLB preview and
rotation workflow. Implementation commit: 07c21c9da696d8181626c92fda67b0e8fdbf98ef.
The following documentation-only commit records that exact checkpoint. No push, merge, owner-project mutation or global release claim.

### Implementation and files

- EngineBridge ModelImportCandidateService header/source: PrepareStaticModel,
  FBX dependency snapshots through ufbx, isolated conversion, exact-byte decoding
  with unique preview resource keys, and source/dependency change refusal.
- EngineBridge ModelImportCommitService header/source: CommitStaticModel,
  retained FBX/texture bundle, source registry records, cloned material relocation,
  embedded payload serialization with resource-mode restoration and paths based
  at the retained source bundle. Empty-directory cleanup preserves files and
  nonempty recovery folders. Existing strict GLB entry points remain.
- StudioApplication.cpp and RenegadeStudioChrome.cpp: static-model picker and
  summary accept GLB/FBX using the same working native preview/name/rotation/
  commit/cancel controls.
- Tests/ModelImporterRebuildGraphicsProof.cpp and ModelImporterRebuild.cmake:
  external/embedded FBX proof and fresh-process reopen cases. Generated fixture
  FBXs, checker PNG, Blender generator and provenance are under
  Tests/fixtures/Importer. Exporter-written machine paths are removed.
- docs/MODEL_IMPORTER_REBUILD.md, ARCHITECTURE.md and FEATURE_MATRIX.csv record
  the contract, evidence and outstanding acceptance.

### Build and automated evidence

VS18 Windows x64 Release, DX12, RTX 4070 Ti. Wicked remains pinned and clean at
3a800b7134aafe58461093c8abb2e274d4e64033.

Using the installed VS18 MSBuild executable, these sequential final commands
all returned exit 0 (BuildProjectReferences=false requires explicitly building
the bridge first so dependent executables link the current library):

    MSBuild BUILD/no-import/EngineBridge/RenegadeEngineBridge.vcxproj /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /m:1 /verbosity:quiet /nologo
    MSBuild BUILD/no-import/RenegadeModelImporterRebuildGraphicsProof.vcxproj /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /m:1 /verbosity:quiet /nologo
    MSBuild BUILD/no-import/Studio/RenegadeStudio.vcxproj /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /p:TargetName=RenegadeStudio_FbxImport /m:1 /verbosity:quiet /nologo
    ctest --test-dir BUILD/no-import -C Release -R RenegadeModelImporterRebuild --output-on-failure

CTest passed 5/5, exit 0, 9.29 seconds: existing GLB; external-texture FBX;
external FBX cold reopen; embedded-texture FBX; embedded FBX cold reopen.
The final sequential build-and-test process took 28.14 seconds. Existing
MSB8029 intermediate-directory warnings remain; earlier full Studio compilation
also reported existing ignored-nodiscard warnings.

The external proof rejects missing or changed texture bytes before producing a
product. Both FBX proofs verify preview pixels and rotation, retained texture
bytes, candidate isolation, thumbnail decode, placement Undo/Redo and WISCENE
save/reopen. They delete their disposable original source copies; separate
processes then verify textured placement and reconversion from retained source.
The embedded fixture's generated thumbnail was visually inspected.

Earlier failing runs are retained as diagnostic evidence, not acceptance:
an unsupported retained_dependencies recipe key was removed; a dependent build
initially linked the stale bridge library. Subsequent tests exposed texture loss
because the payload used the Intermediate directory as its relative path base.
The final implementation uses the source bundle base and embeds resource bytes;
all five cases now pass. Logs are ignored BUILD/no-import/texture-fix-*.log and
the CTest output, not committed machine-path transcripts.

### Native Studio evidence

Executable: BUILD/no-import/Studio/Release/RenegadeStudio_FbxImport.exe.
SHA-256: 492c645c608624203612ec09707f8bfd93c957526746ef7034d2f3360d79028d.
Built from the implementation changes over base 8749a20; the diagnostic revision
in the executable remains that base, so use this hash to identify the tested file.

In the disposable BUILD/no-import/model-preview-native-proof project:

- ADD > IMPORT STATIC MODEL selected static_textured_cube.fbx.
- Native preview visibly showed its red/blue checker texture; Rotate Right
  changed the view. Scene model count stayed 2 before import and after commit.
- Native name field created FBX Native Proof. Its textured thumbnail card was
  visible in Content/Models and drag/drop created a third logical model.
- Focus showed the textured cube in the viewport. Undo returned count 2;
  Redo returned count 3. Ctrl+S cleared the scene's dirty marker.
- Closed Studio normally and started a fresh process. Opened the same disposable
  project and Level: count 3 and the checker-textured cube survived. Reopened
  Content/Models and visually verified the textured card labelled CURRENT / FBX.
- Closed the test Studio normally after inspection; desktop input is released.

Stable asset ID: fc6efa99-06ff-4e1e-9b65-9e4f7e9eb281.
Thumbnail SHA-256 before/after reopen:
23ebc910d9892986dd4f78719accaac4baa23db65c255841089ffca7b68705cd.
Screenshots are reproducible ignored BUILD/no-import/import-proof-fbx-*.png.
The owner's V2 project was not opened.

### Remaining scope and next task

Static FBX only. External textures must be within the source folder tree; export
relative references. Embedded images need filenames. Ambiguous, missing,
unreadable or changed dependencies fail explicitly. glTF sidecars, arbitrary
outside-folder texture relocation, rig/animation/Character authoring, destination
selection and reimport UX are not part of this result. No Runtime/gameplay proof
or original-Wicked parity claim was made. Existing texture-free GLB tests remain
green; broader importer release acceptance is separate.

Next: owner inspects this exact FBX build with a static model, then independent
exact-commit verification before accepting any release gate. Character/animation
support requires its own bounded authoring and persistence slice. Consult this
entry and MODEL_IMPORTER_REBUILD.md instead of relying on the stalled chat.



## Character Importer action assignment - 22 September 2026

On isolated `feature/complete-a6-recovery-20260921`, the first explicit Character action assignment slice is implemented locally. Animations-page choices persist on governed native clip metadata and A6 reads them instead of guessing filenames; the base reference-pose clip is not a fallback Idle. Repeated attack requests no longer interrupt an active one-shot. Runtime and focused tests built Release; 3/3 selected tests passed, plus an actual owner Mutant four-clip governed import/reopen/AI graphics proof passed. Studio Release ClCompile passed; a separate `RenegadeStudio_ActionAssignments.exe` also linked successfully without replacing the running Studio. NOT a finished importer UI or owner acceptance. Read `docs/CHARACTER_ACTION_IMPORTER_HANDOFF.md` for scope, validation, unimplemented slots/frame-number/custom-action UI and next safe steps. No main edits, owner-project mutation, push or merge.

## Editor logical hierarchy and selector repair — 21 September 2026

**Branch:** `feature/editor-logical-hierarchy-selection`
**Worktree:** `C:\\Users\\paulw\\source\\repos\\renegade-editor-hierarchy-selection`
**Base:** `origin/main` at `57086b61dce0bfef22606a6707d6cafd74e82cfe`

### Completed implementation

- `SceneService::ListEntities()` now labels only hierarchy roots as logical editor assets while retaining every descendant for intentional hierarchy expansion.
- Category headers count logical assets only, fixing internal terrain chunks and imported character payload nodes inflating category counts.
- Generic scene-reference selectors exclude non-logical descendants, preventing internal terrain chunks and imported child entities from being offered as authoring targets.
- Hierarchy expansion/collapse responds only on its chevron hit area; clicking the row performs selection without unexpectedly changing disclosure.
- The native ComboBox filter input is rendered directly, restoring caret, keyboard focus, typed text and native filtering visibility.

### Validation

- `cmake --build BUILD\\hierarchy --config Release --target RenegadeBridgeTests --parallel 4` passed (full first build; 807.97 s).
- `cmake --build BUILD\\hierarchy --config Release --target RenegadeStudio --parallel 2` passed (270.12 s).
- `ctest --test-dir BUILD\\hierarchy -C Release -R '^RenegadeBridgeTests$' --output-on-failure` passed 1/1.
- `git diff --check` passed.

### Character-root and multi-clip placement repair

- Character inspectors now resolve a selected presentation mesh through its authoritative native `MeshComponent::armatureID`, so selecting the logical character root retains humanoid/IK/look-at controls even when the mesh and rig are separate hierarchy branches.
- Multi-action imports now remain paused on placement. A single-action import still auto-plays; a character action library no longer evaluates and blends every clip simultaneously, which was forcing poses and causing the observed FPS collapse.
- The owner's existing `Mutant001.rasset.json` was inspected without modification: it contains 21 actions and 1,971 channels, including ten repeated action names. The placement fix protects that existing asset immediately; duplicate source-action consolidation remains a follow-up importer data-quality repair.

### Validation

- `cmake --build BUILD\\hierarchy --config Release --target RenegadeBridgeTests --parallel 4` passed after this repair.
- `ctest --test-dir BUILD\\hierarchy -C Release -R '^(RenegadeBridgeTests|RenegadePhase7Gate7BHumanoidRetargetTests|RenegadePhase7Gate7BSourceContract)$' --output-on-failure` passed 3/3.
- `cmake --build BUILD\\hierarchy --config Release --target RenegadeStudio --parallel 2` passed after this repair.
- `git diff --check` passed.
- Untracked local-only `log.txt` contains four test-generated `Scene::Serialize` timing lines. It is intentionally not staged or committed.

### Required owner confirmation

Open the freshly built `BUILD\\hierarchy\\Studio\\Release\\RenegadeStudio.exe` and validate an existing Terrain and imported `Mutant.fbx` scene: one logical root/count per object; chevron-only expansion; viewport-to-root reveal; double-click framing; character/weapon selector eligibility; filter keyboard/caret interaction; character-root Inspector controls; and FPS/pose with the multi-clip character placed. Existing already-imported duplicate clips remain until reimport/data consolidation. No merge is authorised.


## Studio custom marker icons — 21 September 2026

Branch `codex/studio-custom-icons` starts at importer acceptance commit `bc11c845d81f402ff35bae2d27f7932ddbe64cd0`. The owner-provided `editor.zip` was extracted into `Studio/Content` with its `editor/markericons/*.png` paths preserved. `Studio/MarkerIcons.cmake` now copies those 17 loose PNG files into the compiled Studio `Content/editor/markericons` folder, and the shared local/CI Studio packaging script verifies the same complete set in both compiled and packaged Content. The marker source contract validates the exact file set and PNG signatures. No ZIP is copied into a build or package.

Verification: the archive-to-source SHA-256 comparison passed for all 17 files; `RenegadeMarkerIconAssets` built successfully in Release; `ctest --test-dir BUILD/renegade -C Release -R '^RenegadeMarkerIconsSourceContract$' --output-on-failure` passed 1/1; and a source-to-compiled-to-package proof passed all 17 SHA-256 comparisons while the packaging PowerShell parsed successfully. A fresh full `RenegadeStudio` Release build reached Wicked's `WickedEngine_emb_shaders` target and then CL.exe exited with `-1073740791`; the focused asset target and contract remain green, and the failure occurred outside changed code before Studio compilation. Changed files are `Studio/Content/editor/markericons/*.png`, `Studio/MarkerIcons.cmake`, `Tests/MarkerIconsSourceContract.cmake`, `Tools/Build-Studio-Windows.ps1`, and this handoff.

The owner-supplied PR58 Gate 2C Release package also provided the accepted startup media. The three loose files now live under `Studio/assets/startup`: the 8,338,556-byte logo reveal MP4, 2,441,958-byte identity-handshake MP4, and 2,764,854-byte final-frame BMP. CMake's existing Gate 2A/2C rules copy them into compiled `Content/startup`; the shared local/CI packaging script now requires all three in both compiled and packaged Studio Content. They add 13,545,368 source bytes and do not embed in or enlarge `RenegadeStudio.exe`.

## Importer v3 native UI fidelity — active draft programme

- Branch: `feature/importer-v3-native-ui-fidelity` from `main` merge `2c9c92195f72d868066177c5136d891860fa4873`; remote `https://github.com/mav3r1ckmediastudio-glitch/renegade-engine.git`; latest pushed implementation checkpoint `622c34787578f847ce2ee98349526e4da7e2badc`.
- First checkpoint suppresses authored-level marker overlays during the transient importer preview and changes the existing importer grid to a neutral low-contrast studio floor. It does not alter import/retarget/persistence behaviour or the pinned Wicked dependency.
- The pending playback-footer styling checkpoint turns the real `PLAY SELECTED CLIP` control into a full-width native strip between Previous/Next. It delegates to the existing native animation preview, is disabled unless a Character has a selectable clip, and introduces no simulated transport or invented playhead.
- Read `docs/importer-v3/IMPORTER_V3_HANDOFF.md` for complete current importer evidence and the exact next action. A focused Windows build and visual inspection of the pushed exact head remain required.

**Date:** 12 September 2026  
**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`  
**Merged baseline:** PR #156 — Phase 7F native mesh blending parity  
**Merged commit:** `3d305be84fedf73f5b3cfbb0522be2a732c1adca`  
**Active repair branch:** `repair/phase7-integrated-audit`  
**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Programme state

**Phase 6 — Playable Core is accepted and closed.** PR #148 remains the accepted Phase 6 exit baseline, including native navigation, repaired terrain/rigid-body contact and the owner's successful packaged mini-game acceptance.

**Phase 7A–7F are merged, but Phase 7 is temporarily reopened for integrated acceptance repair.** The individual gates intentionally deferred owner testing until the end of the sequence. The resulting integrated audit found three real contract defects in 7B, 7D and 7E. They are repaired together on `repair/phase7-integrated-audit` so one exact branch can receive the final Windows CI and owner acceptance.

Do not begin Phase 8 from this work until that repair PR passes both CI and owner acceptance.

## Merged Phase 7 sequence

- **PR #151 — Phase 7A:** native `AnimationComponent` playback, pause/stop, scrub, range, speed/blend, loop/ping-pong/play-once and guarded root-motion controls.
- **PR #152 — Phase 7B:** humanoid auto-map/manual correction, ResetPose and native baked retarget from WISCENE/FBX/GLTF/GLB/VRM/VRMA.
- **PR #153 — Phase 7C:** native IK, humanoid look-at and expression authoring.
- **PR #154 — Phase 7D:** native timeline/channel/sampler/keyframe authoring.
- **PR #155 — Phase 7E:** HairParticle, ForceField, Video, Spline, Gaussian Splat and remaining Terrain specialist exposure.
- **PR #156 — Phase 7F:** native mesh-blend material/global render-path exposure.

All of these remain part of the final owner smoke test; the repair does not replace accepted Wicked-native ownership with parallel Renegade runtimes.

## Integrated repair now implemented

### 7B — retarget Undo/Redo no longer depends on the source file

The initial operation still imports the creator source and calls Wicked's native baked retarget. Once that succeeds, `RetargetHumanoidAnimationsCommand` captures the created native `AnimationComponent` state and every referenced baked `AnimationDataComponent` at their entity IDs.

Undo removes the command-owned clips/data. Redo restores those snapshots directly and fails closed if an entity ID has been reused. Redo therefore does not reopen or reinterpret the source FBX/GLTF/GLB/VRM/VRMA/WISCENE.

Changing the humanoid bone map invalidates the old native ragdoll body/joint cache so the current mapping can be rebuilt rather than continuing with bodies tied to the previous map.

### 7D — timeline keys/events now follow native-safe ordering

Recording uses chronological insert-or-replace. Payload chunks move with their timestamps. Recording the same zero-payload event at the same time is a no-op. Event mutations reset Wicked's `next_event` traversal cursor.

`CLOSE LOOP` skips Event channels and only closes value continuity. It can no longer manufacture an extra SOUND PLAY/STOP event at the seam.

SCRIPT PLAY/STOP has been removed from the creator picker and is rejected by the bridge because Renegade's accepted creator scripting authority is `.rscripts`, not Wicked `ScriptComponent`.

A blank scene can create an undoable native clip through **NEW CLIP**.

### 7E — Video now uses governed project ownership

`ADOPT MP4` retains the selected creator source under `SourceAssets/Video`, imports an authoritative LP08 product under `Content/Video/*.rasset`, and binds the native `VideoComponent` through serializable StableId metadata instead of an absolute source path.

Studio restores that binding after Scene open/project adoption/reload. Test Level restores it from the active project. Build dependency extraction adds required governed Video products to the closure. Packaged Runtime resolves the video through the content manifest and `.rasset` payload, not the creator machine's original MP4.

Loop/transport remain native VideoComponent controls. Regression coverage explicitly protects governed StableId metadata through Loop Undo/Redo.

## Current verification state

The branch has been deliberately kept free of PR-triggered Windows CI while implementation and static audit are completed. Source inspection has confirmed:

- the new Video service is present in EngineBridge ownership;
- `CreateVideoInstance` usage matches the pinned Wicked bool-returning API;
- resource dependency extraction handles video-only as well as texture+video scenes;
- authoring and packaged Runtime each have explicit governed video restoration;
- Studio uses the project/Scene lifecycle rather than per-frame video repair;
- governed Video loop edits do not enter the filename/resource replacement path;
- 7D timeline implementation and executable tests agree on chronological/event semantics; and
- 7B source contracts require snapshot-based deterministic Redo rather than reopening source input.

This is still **not an acceptance claim**. The branch must compile and run the Windows test suite, and the creator-facing behaviours need owner proof with real character/video content.

## Exact next action

Finish the documentation/evidence ledger on the repair branch, then open one PR targeting `main`. That PR is the intended expensive CI boundary.

If the exact PR head is green, owner-test the artifact using [`docs/PHASE7_INTEGRATED_REPAIR_AUDIT.md`](docs/PHASE7_INTEGRATED_REPAIR_AUDIT.md):

1. 7B real humanoid retarget -> Undo -> make original source unavailable -> Redo -> play -> save/reopen.
2. 7D NEW CLIP -> record out-of-order keys -> SOUND PLAY event -> Close Loop -> prove no seam duplicate -> Undo/Redo -> save/reopen.
3. 7E ADOPT MP4 -> transport/seek/Loop -> Loop Undo/Redo -> save/reopen after original MP4 is unavailable -> Test Level -> Build Game -> standalone playback.
4. Quick regression smoke of 7A–7F Inspector surfaces, including 7C character controls, 7E Hair/Force/Spline/Gaussian/Terrain and 7F mesh blending.

Do **not** merge on green CI alone. Any owner-visible failure remains a Phase 7 repair blocker.

## Deliberate boundaries still in force

- One future shared ZoneService for reusable trigger volumes; do not recreate audio-only/objective-only zones.
- Ground navigation from Phase 6 remains accepted; flying/swimming NPC navigation requires a separate 3D movement/navigation design.
- Player arms, weapons, combat and production enemy AI are outside this repair.
- Creator-facing VSync control remains deferred.
- Wicked Video audio-track playback is not claimed.
- Timeline SCRIPT PLAY/STOP remains deferred until `.rscripts` has an explicit timeline adapter.
- Commercial redistribution/release packaging clearance remains separate from engineering Build Game acceptance.

## Canonical references

- [`README.md`](README.md) — product/build entry point.
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — current programme state and acceptance boundary.
- [`docs/MASTER_PLAN.md`](docs/MASTER_PLAN.md) — long-range programme.
- [`docs/PHASE7_INTEGRATED_REPAIR_AUDIT.md`](docs/PHASE7_INTEGRATED_REPAIR_AUDIT.md) — authoritative Phase 7 repair architecture and owner-test contract.
- [`docs/PHASE6_CAPABILITY_AUDIT.md`](docs/PHASE6_CAPABILITY_AUDIT.md) — accepted Phase 6 exit contract.
- [`docs/PHASE6_NATIVE_NAVIGATION_STAGING.md`](docs/PHASE6_NATIVE_NAVIGATION_STAGING.md) — accepted Phase 6 navigation architecture.
- [`docs/FEATURE_MATRIX.csv`](docs/FEATURE_MATRIX.csv) — capability evidence ledger.
- [`docs/AI_WORKFLOW.md`](docs/AI_WORKFLOW.md) — implementation/handover rules.

## Importer v3 — local Codex/ChatGPT continuation

A separate `feature/importer-v3-local-handoff` worktree based on `main` is prepared for the approved native importer redesign. Read [`CODEX_IMPORTER_V3_START.md`](CODEX_IMPORTER_V3_START.md), [`docs/importer-v3/CODEX_IMPLEMENTATION_BRIEF.md`](docs/importer-v3/CODEX_IMPLEMENTATION_BRIEF.md) and the live [`docs/importer-v3/IMPORTER_V3_HANDOFF.md`](docs/importer-v3/IMPORTER_V3_HANDOFF.md) before any work. The approved HTML and older contextual handoff are available **only in the Git-ignored local `docs/importer-v3/reference/` directory**; do not add them to version control or upload private assets. This entry records preparation only: no importer code, build or owner acceptance has been completed on this branch. Preserve all other worktrees and PRs. Codex should work locally in small commits with explicit testing and an up-to-date handoff so ChatGPT can continue the exact branch when usage runs out. No remote push/CI/PR without owner approval.

### Importer v3 local continuation — 18 September 2026

The dedicated branch now has a native Model verification WIP checkpoint: `a3eaf4a2af24c0cd837c05eee52e06bbcbed1382`. Changed files are `EngineBridge/include/renegade/bridge/{CreatorAssetWorkflowService,ReusableAssetService}.h`, `EngineBridge/src/{CreatorAssetWorkflowService,ReusableAssetService}.cpp`, `Studio/src/StudioApplication.cpp`, and `Tests/CreatorAssetWorkflowGraphicsProof.cpp`. It reopens a committed `.rasset`, checks identity/hash and catalogue presence, retains source after a post-commit failure, reports detailed native failure context, and starts preview at original source scale. A subsequent scene-reopen verification edit remains to be compiled and committed. `git diff --check` passed before the first commit. VS18 Release Studio build command was `cmake --build BUILD/renegade --config Release --target RenegadeStudio --parallel 4`; the sandboxed attempt failed with MSB4184 on Windows SDK access, and the authorized retry was still compiling when this note was written. No Model fixture test, visual audit, owner import, save/reopen or Runtime test has passed yet. The underlying owner-observed missing `.rasset` cause is unknown. See `docs/importer-v3/IMPORTER_V3_HANDOFF.md` for the live commands, results, risks and exact next action. No other worktree, pinned submodule, active binary or remote PR was changed.

**Final importer v3 local result at 13:48 UTC:** later implementation commits `64697c155c3fcc2d94aff007d41deb3a4f3bc2c6` and `ea043688ba646a2f17c636b5ef68382777109c43` added serialized scene/material reopen through normal stable-ID placement and a resizable native right inspector. Exact VS18 Release commands `cmake --build BUILD/renegade --config Release --target RenegadeStudio RenegadeCreatorAssetWorkflowGraphicsProof --parallel 4` and, after the inspector edit, `cmake --build BUILD/renegade --config Release --target RenegadeStudio --parallel 4` both passed (exit 0). `ctest --test-dir BUILD/renegade -C Release -R '^RenegadeCreatorAssetWorkflowGraphicsProof$' --output-on-failure` passed 1/1 with filesystem/graphics access (0.71 seconds); its sandboxed run failed containment before test execution. This proves a non-private FBX fixture `.rasset` commit/catalogue/scene reopen path, not the owner's Mutant import or v3 visual/Character parity. Modified implementation files are listed in the live importer handoff. Risks: the missing-asset symptom's root cause remains unknown, the right inspector has not had visual inspection, and the approved v3 stage navigation, isolated preview and external-animation playback are still outstanding. Exact next task is in `docs/importer-v3/IMPORTER_V3_HANDOFF.md`; preserve this branch and build output. Do not mark the importer accepted.

**Owner correction after this checkpoint:** asset import has been confirmed fixed by the owner. The earlier missing-`.rasset` concern is historical, not a current blocker. This message did not specify the tested build or asset set, so the confirmation is recorded as owner evidence without attributing it to a particular local commit. Native v3 layout, isolated preview, Character external-animation playback/retargeting and visual acceptance remain open. The next assistant should follow the updated importer handoff rather than repeat the old missing-asset diagnosis.

**Importer v3 continuation at 14:11 UTC:** local implementation commit `690cf96877e3b99375826b0d7cb5e466780786a3` changes `Studio/src/StudioApplication.cpp`, `Studio/src/StudioApplication.h` and `Studio/src/CreatorImportPreviewWindow.h`. The old section dropdown is replaced by six clickable right-inspector headings around the active page; Model/Character choice selects the governed destination, Model skips Rig/Animations, Transform includes preview lighting, and Rig shows measured source counts with an explicit unverified-mapping warning. VS18 Release command `cmake --build BUILD/renegade --config Release --target RenegadeStudio --parallel 4` passed (exit 0); `git diff --check` passed. No native visual inspection or Character external-animation proof was run. Risk is heading/body clipping or spacing until native inspection; preview still uses the active scene. The exact next action and remaining work are in `docs/importer-v3/IMPORTER_V3_HANDOFF.md`. No other worktree, active binary, pinned submodule, or remote PR was touched.

**Importer v3 continuation at 14:20 UTC:** implementation commit `3d08f4e71c207c89dac3e38713cabeef18942ac5` adds a read-only first-armature humanoid auto-map diagnostic in the Rig stage, reporting mapped slots and missing required bones through the existing bridge service. VS18 `MSBuild.exe BUILD\\renegade\\Studio\\RenegadeStudio.vcxproj /t:ClCompile /p:Configuration=Release /p:Platform=x64 /m:4 /v:minimal` passed (exit 0, existing C4834 warning), without linking over the running branch executable. `git diff --check` passed. Native visual inspection of the previous linked build confirmed Asset Setup, Model/Character choice, Transform stage interaction and an internally scrolling inspector. It also revealed a material failure: the authored level grid and an AUDIO marker remain visible behind the import preview; isolation is unfinished. The new diagnostic has not yet been seen in the running app. No save/reopen, Character external animation or Runtime check occurred. See the importer-specific handoff for exact next actions and limitations; no remote or pinned submodule changed.

**Importer v3 ChatGPT takeover (18 September 2026):** Local WIP implementation `c9467f414597ff93ad3738b859fe4dd5b4104f86` introduces a separate native preview scene, prevents importing preview/light into the authored level and editor Undo, redirects preview material/lighting/transform edits, suppresses editor overlays/input, and retains preview camera navigation. VS18 Release Studio ClCompile passed twice (exit 0); focused `RenegadeCreatorAssetWorkflowGraphicsProof` passed 1/1 (0.66s); `git diff --check` passed. This commit is NOT linked or visually tested: the v3 worktree Studio executable is running, and must not be overwritten or terminated without owner approval. Owner-confirmed merged asset-import repair has not been merged/rebased into this older-base worktree. Character external-animation queue, retarget, playback and save/reopen remain outstanding. Full commands, boundaries and next action: `docs/importer-v3/IMPORTER_V3_HANDOFF.md`. No push, PR, CI or other worktree changes.

**Importer v3 Character animation checkpoint (18 September 2026):** `314e0276b71c04e0efb3038a80e6dd6f7106f95e` adds native embedded clip preview transport; `71975ae106c04d453a2f0763030d149045273d32` adds isolated external animation queue, opt-in native humanoid source mapping, baked Wicked retarget and fingerprint-guarded commit. VS18 Release Studio ClCompile PASS and focused synthetic native retarget test PASS 1/1; Model RAsset proof separately PASS 1/1 (an earlier combined run had an intermittent journal-file I/O failure). **No new Studio link/visual proof, external FBX Character RAsset save/reopen, owner test or Runtime proof:** the v3 Studio binary remains in use and must not be overwritten. Original asset-import repair accepted by owner and merged separately; exact integration in this older v3 worktree unverified. See `docs/importer-v3/IMPORTER_V3_HANDOFF.md` for commands, modified files, constraints and next action. No remote changes.

### Importer v3 PR preparation - 19 September 2026

- `feature/importer-v3-local-handoff` now incorporates `origin/main` `8522e850` (#170) through merge commit `c7acb9c` and restores documented Wicked pin `3a800b7` through `c499b2a`. The previous `f540315` diagnostic submodule revision is excluded from the final PR diff.
- Owner confirmed successful native Character import and drag/drop; the latest fresh import was 3.477 seconds confirm-to-editor, and warmed drag preparation 95-268 ms. Eight focused tests and real Mutant/external-walk disposable proof passed before branch reconciliation. VS18 CMake configure against the reconciled source passed; matching Release Runtime rebuild is in progress. See `docs/importer-v3/IMPORTER_V3_HANDOFF.md` for exact evidence and next steps.
- Full Windows PR CI, matching standalone Runtime playback/save-reopen and native functional UI inspection remain acceptance gates. Cosmetic polish may follow separately, but do not claim full native visual or external animation parity or merge on CI alone. No private assets/screenshots or untracked logs staged; Studio executable has not been overwritten.


### Importer v3 UI-fidelity local build correction — 20 September 2026

The active remote UI branch is `feature/importer-v3-native-ui-fidelity` (draft PR #172). A GitHub Studio Debug failure was traced to a private visibility error for the existing `DiagnosticImportActive()` query used by the importer overlay suppression. The declaration is now public in `Studio/src/StudioApplication.h`. Local Visual Studio 18 Release configuration and `cmake --build BUILD/renegade --config Release --target RenegadeStudio --parallel 2` passed and linked the dedicated UI-fidelity worktree executable. The protected historical Studio process was left untouched. Routine UI work is local-build first; do not trigger GitHub Actions for normal visual iteration. The importer-specific handoff records the exact next action and PR status.

### Importer v3 3D reference checkpoint — 20 September 2026

Owner-supplied `Male_Reference.fbx` is committed publicly to the UI-fidelity branch as `Studio/assets/importer/male_reference.fbx` (`c2966f78654c730c5fba5957d6c498af165f5724`). The currently uncommitted native integration loads it only into the isolated preview scene, replaces the old white 2D reference load, wires the existing visibility controls, and copies the FBX beside the Studio executable. Local source-only Release compile passed (`MSBuild ... /t:ClCompile ... /m:2 /v:minimal`, exit 0; existing C4834 warning only); no Studio executable was overwritten because the dedicated UI-fidelity instance remains running. This is not yet visually accepted: the next action is commit/push the integrated code, then inspect the exact fresh executable after the owner closes it. No CI was triggered.

### PR #172 navigation timeout repair - 21 September 2026

Implementation commit: `6a9a34893f8c79ea18e22ce564c36b87286c9056`, based on owner-accepted importer head `5c1dd52aa761dd0c0082ed6c7a4496da405d0011`.

Changed only `Tests/Phase6NativeNavigationTests.cpp` and `Tests/Phase6NativeNavigation.cmake`. A scope guard finishes Wicked jobs and flushes the backlog after scene/command teardown but before CRT static destruction, including early failure returns. The navigation checks and real multithreaded voxelization remain intact. The test now has a 60-second timeout instead of inheriting CTest's 1,500-second default. No Studio, Runtime, bridge, asset, upstream source or submodule-pointer change.

Diagnosis: GitHub Studio Debug run `35541788311`, job `106160660605`, timed out test 80 after 1,500.01 seconds; the other 169 tests had no failures (one package test skipped). Locally, the unmodified executable timed out at 60 seconds in both Release and Debug. CDB stacks confirmed the main thread held the CRT on-exit lock while joining `wi::backlog::AsyncWriter::Stop`; the writer was blocked in `atexit` from `AsyncWriter::WriterLoop` while registering its function-static queue destructor. This is a shutdown race, not a slow path query or importer UI regression.

Local Windows VS18 evidence:

- Baseline Debug build: `cmake --build BUILD/renegade --config Debug --target RenegadePhase6NativeNavigationTests --parallel 2` passed.
- Baseline tests: `ctest --test-dir BUILD/renegade -C Debug -R '^RenegadePhase6NativeNavigationTests$' --timeout 60 --repeat until-fail:10 --output-on-failure` timed out on run 1; Release also timed out with `--timeout 60`.
- Fixed test builds: `MSBuild.exe BUILD/renegade/RenegadePhase6NativeNavigationTests.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /m:2 /v:minimal` and the same command with `Configuration=Release` passed against the already-built unchanged dependencies. An unnecessary full dependency rebuild following CMake regeneration was stopped before these focused builds.
- Fixed repetition: `ctest --test-dir BUILD/renegade -C Debug -R '^RenegadePhase6NativeNavigation' --repeat until-fail:50 --output-on-failure` passed 50 native tests and 50 source-contract runs (4.68 seconds total). The identical Release command passed 50 plus 50 runs (3.40 seconds total).
- `RenegadeReusableAssetReimportRecipeTests` passed in Release (0.10 seconds).
- The existing `RenegadeCreatorAssetWorkflowGraphicsProof` failed twice under its default long output path with a staged journal file-creation error. Running the same binary and fixtures with a shorter output directory passed: `BUILD/renegade/Release/RenegadeCreatorAssetWorkflowGraphicsProof.exe Tests/Fixtures/LP07/maya_cube_6100_ascii.fbx Tests/Fixtures/LP07/maya_transformed_skin_7700_ascii.fbx BUILD/nav172-proof`. No importer code was changed to accommodate this local path limitation.
- `git diff --check` passed. Diagnostic/build logs remain in ignored `BUILD/navigation-*` files; no private assets or logs were staged.

Next gate: push the implementation and this evidence to PR #172, then inspect all four exact-head Windows checks. CI is pending at this handoff; do not describe it as passed or merge before required checks pass. The accepted importer UI, existing Studio executables and other worktrees were preserved. This focused test lifecycle workaround does not claim to repair Wicked's general asynchronous-logger implementation.

### Creator model importer removal checkpoint — 30 September 2026

Branch: `feature/remove-importer-completely`, isolated Windows worktree `renegade-no-import`, based on accepted V4 commit `5152874`. This is a temporary no-model-import reset, not the replacement importer. Main and the previous experimental worktrees were not edited.

The old guided model/character importer was physically removed from Studio: its state, preview window/dashboard, callbacks, stage controls, commit path, ADD menu action, Asset Browser model import/reimport controls, reference FBX and package copy rule. The bridge model import transaction, prepared conversion entry points, external animation/material preparation and model reimport methods were deleted. Existing `.rasset` read and placement utilities remain so old projects can still open and place their assets. Independent texture, video, audio and specialist resource workflows are outside this model importer reset.

Windows Release `RenegadeStudio` compilation and link passed before the bridge deletion; the first post-build step failed only because the CMake copy command still referenced the deleted reference FBX. That rule is now removed. The final incremental Windows Release `RenegadeStudio` build after bridge deletion passed with exit code 0 and produced `BUILD/no-import/Studio/Release/RenegadeStudio.exe`. The executable launched to Project Hub on DX12. No new importer or UI has been started. DX12 editor inspection passed: an existing project opened with scene 9.WISCENE and a populated Project Assets pane; ADD showed no IMPORT MODEL entry, and the Asset Browser showed no model IMPORT or REIMPORT controls. This verifies the visible paths in the local executable, not owner acceptance or all historical import tests.

### New model importer rebuild checkpoint — 30 September 2026

Implementation commit `9543ace` on branch `feature/model-importer-rebuild` starts at no-import baseline `4d2bcda`; no main, other worktrees, or pinned Wicked submodule edits. `ModelImportCandidateService` now validates a selected GLB, converts it into an isolated heap-backed Wicked Scene with an initialized graphics device, records source byte/fingerprint evidence before and after conversion, reports converter failures, and rejects a candidate without mesh/object content. No Studio action, preview, package commit or new asset creation is exposed at this checkpoint.

Changed files: `EngineBridge/include/renegade/bridge/ModelImportCandidateService.h`, `EngineBridge/src/ModelImportCandidateService.cpp`, `EngineBridge/CMakeLists.txt`, `docs/MODEL_IMPORTER_REBUILD.md`, `docs/ARCHITECTURE.md`, and this handoff. The acceptance contract demands a governed `.rasset` transaction, current catalogue card, exact reopened placement, scene Save/Reopen, and a real native UI check before any READY or completion claim. The GLB source dependency closure must be checked at commit; a GLB extension alone does not guarantee embedded images. Character/animation controls remain out of this first gate.

Local Windows command: `cmake --build BUILD/no-import --config Release --target RenegadeEngineBridge --parallel 2` (VS18 CMake) passed with exit code 0; the immediate incremental repeat also passed with exit code 0 after final source transfer. `git diff --check` passed. No new GPU conversion proof or Studio interaction has run. Next: implement and test the commit/reopen transaction using the retained asset contract. Do not claim the importer works in Studio yet.

### Static GLB transaction build checkpoint - 30 September 2026

Implementation commits 46b8432 and aa9da5c on feature/model-importer-rebuild. ModelImportCommitService stages a
self-contained static GLB source, WISCENE-backed .rasset, managed projection,
registry and metadata in one project transaction. It refuses external URI
references, source fingerprint changes, rig/animation payload, existing paths
and recovery tombstones. After commit it checks the exact asset document,
current catalogue entry and stable-ID placement loader. A post-commit failure
reports committed=true and does not invite a blind retry.

Tests/ModelImporterRebuildGraphicsProof.cpp and two tiny GLB fixtures cover
actual conversion, external-reference rejection, commit/reopen and duplicate
refusal. Windows VS18 Release RenegadeEngineBridge target build passed
(exit 0, 619.46 s). RenegadeModelImporterRebuildGraphicsProof target build
passed (exit 0, 8.61 s). The first graphics run exposed a ReadBytes bug:
istreambuf_iterator did not set eofbit, causing a false source-change failure.
The reader now uses a sized binary read. VS18 Release rebuilt the proof
(exit 0, 650.92 s), and the focused graphics proof passed (exit 0, 0.66 s)
in BUILD/no-import/model-import-rebuild-proof. It covered static GLB conversion,
external-URI refusal, governed commit, exact asset reopen, current catalogue,
stable-ID placement preparation and duplicate refusal. Studio UI, scene
Save/Reopen, project reopen and creator acceptance remain unproven. Next wire
native Studio controls, then test the built UI and the full acceptance sequence.
Main and other worktrees remain untouched.

### Native static GLB Studio entry checkpoint - 30 September 2026

The isolated rebuild branch now has a real ADD > IMPORT STATIC GLB action, a
Windows GLB file picker, isolated conversion evidence, native asset-name input,
and IMPORT ASSET / CANCEL widgets. Commit calls the governed bridge transaction
at a Wicked thread-safe point; success refreshes and reveals the exact stable-ID
asset card. A committed-but-reveal-failed result is reported without suggesting
a retry. This is a static-model slice only, not the finished importer.

Windows VS18 Release RenegadeStudio built and linked (exit 0, 354.98 s); the
incremental reveal change rebuilt and linked (exit 0, 15.90 s). The executable
launched on DX12; ADD menu and its IMPORT STATIC GLB item were visible and a
click opened the native GLB picker. The previously open 9.WISCENE project was
automatically restored. No import was committed and no scene was saved. The
owner resumed desktop activity during the picker check, so interactive work
stopped and the test Studio process was closed. Candidate panel, cancel,
commit, asset reveal, placement and save/reopen have NOT been verified in the
built Studio. The panel currently reports structural counts; an actual model
render preview remains to build. Do not claim this UI READY or merge it.

Next, on a clear desktop, create a disposable Studio project and click through
file selection, evidence, name editing, cancel, commit, asset card, placement,
Undo/Redo and scene/project reopen. Then implement and verify an isolated 3D
model preview before owner acceptance. Main and other worktrees remain untouched.

### Static GLB drag repair - 1 October 2026

Implementation commit `5e0444c` on `feature/model-importer-rebuild` restores
Studio's per-frame `UpdateCreatorAssetDragPreview` call after GUI callbacks
and before chrome's consumed-pointer guard. The no-import reset accidentally
removed this existing placement block along with vegetation ticking, dirty
workspace layout handling and viewport bounds refresh; all four are restored.
The owner imported Bow 05 successfully but could not drag its CURRENT asset
card into the scene. This missing update explains the queued drag/drop failure.

Changed implementation: `Studio/src/StudioApplication.cpp` and
`Tests/ModelImporterRebuildGraphicsProof.cpp`. The graphics proof now executes
actual reusable placement, Undo, Redo, WISCENE save/reopen, stable instance
identity, mesh/object counts and wrapper position checks.

Windows VS18 command: `MSBuild BUILD/no-import/Studio/RenegadeStudio.vcxproj
/p:Configuration=Release /p:Platform=x64 /p:TargetName=RenegadeStudio_DragRepair
/p:BuildProjectReferences=false /m:2 /verbosity:minimal` passed (exit 0,
15.52 s). The side-by-side executable is in `BUILD/no-import/Studio/Release`.
The original Studio process remains open and predates this fix; no scene was
closed, saved or edited. Build warnings MSB8029 and existing C4834 remain.

VS18 CMake command: `cmake --build BUILD/no-import --config Release --target
RenegadeModelImporterRebuildGraphicsProof --parallel 2` passed (exit 0,
12.66 s). From `BUILD/no-import/Release`, ran
`RenegadeModelImporterRebuildGraphicsProof <static_triangle.glb>
<external_uri_triangle.glb> <BUILD/no-import/model-import-rebuild-proof>`
and then the same command with the owner's retained `Bow 05.glb` source and
`BUILD/no-import/model-import-bow-placement-proof`. Both printed placement/
Undo/Redo/save/reopen PASS; combined process exit 0 (1.47 s). Only disposable
BUILD projects were written. The owner's project was read for its GLB source.
`git diff --check` passed.

Remaining: save/close the old Studio safely, launch the DragRepair executable,
and visually test the exact asset card drag, drop, Undo/Redo and Save/Reopen.
Do not force-close a process with potentially unsaved work. Warn the owner
before mouse/keyboard interaction. Thumbnail generation and a real importer
model preview remain unfinished. Native acceptance and independent exact-head
verification remain pending; this is not a READY importer claim.

### Owner drag acceptance and preview implementation WIP - 1 October 2026

The owner saved/closed the old Studio. Launched the side-by-side
`RenegadeStudio_DragRepair.exe`; PID 62800 restored 9.WISCENE in DX12.
Warned before desktop control, dragged the CURRENT Bow 05 card into viewport,
and the owner immediately confirmed: "that now works". No scene save was
performed by the agent. User was released to resume desktop use.

Next authorized work is real importer model preview and asset thumbnails.
Uncommitted WIP adds `Studio/src/ModelImportPreview.h/.cpp`: private cloned
scene, auto-framed camera, fixed neutral lighting, offscreen 512x320 render,
rotation and PNG capture. Native panel has image plus real rotation buttons.
The commit service accepts an optional rendered PNG and includes thumbnail
and managed projection path in the same project transaction. Graphics proof
now renders/captures the preview and checks transaction thumbnail decode.
At this checkpoint the extended proof Release build is running; no preview
render, native preview click or thumbnail transaction success claimed yet.
Do not replace/close the running drag-repaired Studio without warning and
protecting the owner's potentially unsaved scene.

### Rendered model preview and thumbnail checkpoint - 1 October 2026

Supersedes the preview WIP checkpoint above. Implementation commit is recorded
in the following exact-commit checkpoint after the implementation is committed.

Changed: Studio ModelImportPreview.h/.cpp, StudioApplication.h/.cpp and CMake;
EngineBridge ModelImportCommitService header/source; Tests rebuild CMake and
graphics proof; MODEL_IMPORTER_REBUILD, ARCHITECTURE, FEATURE_MATRIX and HANDOFF.

The preview renders a private scene clone at 512x320 with auto-framed camera
and neutral illumination; native rotation changes only its camera. Studio
waits for usable frames then freezes the texture until rotation. The global
GUI theme had darkened the image; the image widget now bypasses tint and
background blur. Asset name positioning also keeps its label inside the panel.
No Wicked source or submodule pointer change. No forced GPU waits were added
to Studio. Captured PNG validation and persistence share the governed model
transaction; the existing Asset Browser reads the sibling thumbnail.

Commands/results (Windows VS18, Release x64):
- MSBuild BUILD/no-import/Studio/RenegadeStudio.vcxproj
  /p:Configuration=Release /p:Platform=x64
  /p:TargetName=RenegadeStudio_ModelPreview /p:BuildProjectReferences=false
  /m:1 /verbosity:quiet: exit 0, 18.61 seconds. Existing MSB8029/C4834 remain.
- MSBuild BUILD/no-import/RenegadeModelImporterRebuildGraphicsProof.vcxproj
  /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false
  /m:1 /verbosity:quiet: exit 0.
- From BUILD/no-import/Release, RenegadeModelImporterRebuildGraphicsProof
  <static_triangle.glb> <external_uri_triangle.glb>
  <BUILD/no-import/model-import-rebuild-proof>, then the same command with
  the owner's retained Bow 05.glb source and model-import-bow-placement-proof:
  combined exit 0. Triangle contrasting pixel counts 13293/4230 before/after
  rotation; Bow 2892/2574. Conversion, candidate isolation, changed rotated
  image, malformed PNG/external URI rejection, transaction thumbnail decode,
  stable-ID placement, Undo/Redo and WISCENE Save/Reopen all pass.
- ctest --test-dir BUILD/no-import -C Release --output-on-failure
  -R '^RenegadeModelImporterRebuildGraphicsProof$': 1/1 passed, 2.11 seconds.

The graphics harness now fires EVENT_THREAD_SAFE_POINT like Application,
installing asynchronously compiled pipelines before counting rendered frames.
It initializes components before conversion and drains pending pipeline work
before Application destruction, fixing the proof's earlier teardown crash.
Both generated PNGs and the corrected native Bow preview were visually
inspected. Only disposable BUILD projects were written by the proof.
A disposable native proof descriptor was prepared but native commit/card
verification was not completed: the owner resumed desktop use and 9.WISCENE
had unsaved edits. Desktop input stopped; no authored scene was saved/closed.

Next: owner or different reviewer verifies this exact implementation commit
using VERIFICATION_CHECKLIST, clicks rotation/cancel, imports into a disposable
project, confirms the thumbnail card and project reopen. Warn before desktop
input and protect unsaved work. Old thumbnails are not auto-regenerated.
Static self-contained GLB to Content/Models only; no FBX, rigs or animation.
This is an implemented/tested candidate, not a completed release gate.\n
### Exact implementation commit checkpoint - 1 October 2026

Implementation: fd8b24718085669a41f8fad1fe01f41eb52dcc7a
(Add isolated rendered GLB preview and transactional thumbnails).
The running RenegadeStudio_ModelPreview Release executable was built from
this implementation's source before its commit. All build/proof/visual
evidence and pending native checks are listed in the preceding checkpoint.
git diff --check passed before commit. This follow-up changes documentation
only. Independent verification must target fd8b247; no release gate accepted.\n
### Recovery-session native verification - 1 October 2026

Reviewed implementation: fd8b24718085669a41f8fad1fe01f41eb52dcc7a.
Repository HEAD before this documentation update:
462bf3f (Record exact preview implementation and acceptance handoff), on
feature/model-importer-rebuild. Working tree and Wicked submodule were clean.
Verifier: a different Codex conversation, recovering the interrupted session;
no implementation code was changed in this verification.

Owner evidence: the owner explicitly confirmed the corrected preview and BOTH
rotation buttons worked; that message had been lost by the frozen conversation.
The recovered desktop history contained the implementation and handoff commits.
There were no active Desktop Commander sessions or Studio/build processes when
recovery began; the old chat spinner did not indicate a running local build.

Independent native checks, Windows 11 Pro 10.0.26200, RTX 4070 Ti driver
32.0.15.9636, DX12, existing VS18 Release preview executable:
- Opened a disposable BUILD/no-import/model-preview-native-proof project.
  Its earlier incomplete descriptor was refused because it lacked startup
  content. Fixed only the disposable fixture by adding Content/Scenes/Preview.wiscene
  copied from the existing Bow placement proof; no product code change needed.
- ADD > IMPORT STATIC GLB selected the retained Bow fixture, rendered the model,
  and left the authored scene untouched. Cancel closed the panel without creating
  a model asset; the scene SHA256 stayed
  B009919E1A692EF69C9D8E7D2FBFEC48DC48E85DF572CE00B5BC6D47304E79E4.
- Reopened the picker, clicked Rotate Right (visible view change), typed Native
  Bow Thumbnail, and clicked Import Asset. Native success status identified
  Content/Models/Native Bow Thumbnail.rasset. The transaction created the source,
  RAsset, managed projection, registry record, and 512x320 PNG.
- Opened Content/Models in Project Assets and visually inspected the Bow thumbnail
  on the new card. Dragged that card into the viewport; model count rose 1 -> 2.
- Clicked native Undo: model count 2 -> 1, Redo available. Clicked native Redo
  in its updated Inspector position: count 1 -> 2. Ctrl+S cleared the dirty marker.
- Closed the saved disposable project through Alt+F4, launched a fresh process,
  opened Preview Proof from Recent Projects, and opened its Main Level. The saved
  scene reopened with model count 2 and clean Undo/Redo history.
- Asset ID a9d94fce-548d-4053-9977-ec9084e0ca46 remained in the reopened registry.
  RAsset, thumbnail and scene files retained these SHA256 hashes after reopen:
  RAsset: 4F91C22C5638B1CCF3CA3EAD54438E5E4D0E8D2E1DF7DD3121875EFD31F8C606
  PNG: C15D8D8701D904761B412F3C0CDCF1994DFB4CC9C16EAC85A74BA9F0E45C0561
  WISCENE: 51E12C4A261CE3363FAD0A21959850F589CBAE2D63800E3DCA2E5B56A98F6450

Evidence under BUILD/no-import (ignored, local only): import-proof-preview.png,
import-proof-cancel.png, import-proof-named.png, import-proof-committed.png,
import-proof-card.png, import-proof-placed.png, import-proof-undo.png,
import-proof-redo-saved.png, import-proof-project-reopened.png,
import-proof-reopened-scene.png. Screenshots were read and visually inspected.
The post-reopen PNG was verified byte-identical; the card screenshot documents
its appearance before reopening, not an additional post-reopen visual assertion.

Automated command (use the CMAKE_CTEST_COMMAND executable from CMakeCache.txt;
ctest is not on the ordinary PowerShell PATH):
ctest --test-dir BUILD/no-import -C Release --output-on-failure
  -R '^RenegadeModelImporterRebuildGraphicsProof$'
Result: 1/1 passed, 2.12 seconds test / 2.22 seconds total.
The reviewed preview executable SHA256 was
20CF05225B30D86FF942B2FFF5FBE5A976F4BDDBA67EE8995CF8B206871A4BAA;
it is the prior implementation build, not a fresh rebuild in this session.
Wicked remains 3a800b7134aafe58461093c8abb2e274d4e64033 with no tracked changes.

Result: PASS WITH LIMITATIONS for the bounded static embedded GLB native
preview/thumbnail/import/placement/save/project-reopen workflow. This completes
the outstanding native checks described in the preceding checkpoint. No global
release gate, full regression suite, new clean clone, FBX/sidecar/rig/animation,
Runtime/package parity, destination selection or old-thumbnail regeneration is
accepted by this result. Existing DX12 startup warnings in the local log remain;
no crash or visual failure was observed in the tested workflow.

Desktop input stopped; the saved disposable Studio session was closed normally.
The owner's authored project was neither opened nor modified. Next bounded work
is importer source-format expansion (FBX / retained dependency support), before
character and animation authoring. Keep the accepted static GLB slice intact.


### Rigged importer and collision recovery checkpoint - 1 October 2026

Implementation commit: `c14884ebdab5c5607891a1bd8f5953f68bc0ead5` on `feature/model-importer-rebuild`.
Base checkpoint: `9b5c5b4`. Main and other worktrees were not edited or merged;
no remote push or CI was triggered. Pinned Wicked source and pointer are unchanged.
The earlier conversation stalled, then the remote device connection stalled;
this recovery finished after reconnection. All implementation WIP is now committed.

Bounded result: native rigged GLB/FBX Character import with preserved skin/bind
payload and embedded clips; isolated clip preview; paused placement; corrected
native Character collision startup in standalone Runtime. The owner-reported
checker cube failure was reproduced: the floor ray hit y=5 but an unseeded
previous object matrix imparted platform inertia, sending the Character to
(1202,1365.47,1803) after 300 frames. Preparing rigid-body surface-query geometry
and seeding previous matrices from current matrices fixes it without a navigation
grid or upstream patch. A zero-time startup update is not a second gameplay loop.

Changed files:
EngineBridge/include/renegade/bridge/ModelAnimationPreviewService.h
EngineBridge/include/renegade/bridge/ModelImportCandidateService.h
EngineBridge/include/renegade/bridge/ModelImportCommitService.h
EngineBridge/src/ModelImportCandidateService.cpp
EngineBridge/src/ModelImportCommitService.cpp
EngineBridge/src/ReusableAssetInstanceService.cpp
Runtime/src/RuntimeCharacterCollision.h
Runtime/src/RuntimeLiveDiagnostics.cpp
Studio/src/CreatorAssetDragPreview.cpp
Studio/src/ModelImportPreview.cpp
Studio/src/ModelImportPreview.h
Studio/src/RenegadeStudioChrome.cpp
Studio/src/StudioApplication.cpp
Studio/src/StudioApplication.h
Tests/ModelImporterRebuild.cmake
Tests/ModelImporterRebuildGraphicsProof.cpp
Tests/fixtures/Importer/FBX_FIXTURES.md
Tests/fixtures/Importer/animated_character.fbx
Tests/fixtures/Importer/generate_character_fixture.py
Tests/fixtures/Importer/rig_checker.png
docs/ARCHITECTURE.md
docs/FEATURE_MATRIX.csv
docs/MODEL_IMPORTER_REBUILD.md

Windows x64/DX12 Release commands, run from repository root unless noted:
- MSBuild.exe BUILD/no-import/RenegadeModelImporterRebuildGraphicsProof.vcxproj
  /m:1 /verbosity:quiet /nologo /p:Configuration=Release /p:Platform=x64
  /p:BuildProjectReferences=false: PASS, exit 0.
- MSBuild.exe BUILD/no-import/Runtime/RenegadeRuntime.vcxproj
  with the same arguments: PASS, exit 0 (11.74 seconds).
- Use the CMAKE_CTEST_COMMAND executable from BUILD/no-import/CMakeCache.txt:
  ctest --test-dir BUILD/no-import -C Release
  -R '^RenegadeModelImporterRebuild' --output-on-failure:
  PASS, 7/7, 17.26 seconds. CTest is not on the ordinary PowerShell PATH.
- From BUILD/no-import/Release:
  RenegadeModelImporterRebuildGraphicsProof.exe --reopen
  ../character-mutant-proof 'SourceAssets/Models/Proof Triangle/Mutant.fbx':
  PASS, exit 0. Both fixture and Mutant have ground_intersect=true and position
  (4,5,6) after 300 updates at 1/60 second. The proof calls the production Runtime
  preparation helper, with no test-only navigation filter or BVH workaround.
- git diff --check and git -C WickedEngine status --short: clean before commit.

Standalone player inspection:
- BUILD/no-import/Runtime/Release/RenegadeRuntime.exe --project
  BUILD/no-import/model-import-animated_character-proof/RuntimeProof.renegade:
  checker cube visibly stable on the floor; Runtime startup SUCCESS and one
  synchronized Character. Saved disposable project and scene loaded in player.
- Same executable with --project
  BUILD/no-import/character-mutant-proof/RuntimeProof.renegade:
  Mutant visible with textures in reference pose on the floor at approximately
  20.69 seconds; startup SUCCESS, one synchronized Character, Player spawned,
  character_scene_sync_failed=false. Screenshot was read and visually inspected.
- Both player windows were closed normally after inspection. No input was sent
  to an authored project. Mutant remains private in ignored BUILD proof files.
- Runtime executable SHA256:
  F378221A069852D03C99F1D1F9B0062B3165C7FCEC51D771849FA5A0136D1807.
  The executable was built before the implementation commit; its embedded build
  revision is historical, so use this binary hash for the inspected build.

Local ignored evidence: collision-recovery-build.log, collision-runtime-build.log,
collision-runtime-screen.png, collision-runtime-diagnostics.json,
collision-mutant-runtime.png, collision-mutant-runtime-diagnostics.json and
Testing/Temporary/LastTest.log beneath BUILD/no-import.

Earlier-session Studio controls/build evidence is carried by the existing
character-studio-final.log and import-proof-character-* images. The recovery did
not repeat every native Studio control interaction. Existing MSB8029 and C4834
warnings remain. No full-suite Debug/Release CI, packaged export, universal FBX
compatibility, or independent exact-commit owner acceptance is claimed.

Next bounded task: external animation import/retarget and explicit per-Character
semantic action slots, starting with a real Idle/Walk example. The current slice
previews embedded clips but does not assign gameplay actions; Mutant's reference
pose in Runtime is expected. Preserve this committed static/rigged import and
collision checkpoint while adding that next slice. Do not mark a release gate
complete without independent verification of the exact implementation commit.

## External Character animation continuation checkpoint (2026-10-01)

Implementation commit: 5a53c404af545a9027b2739ac4224cbf30acce26 on
feature/model-importer-rebuild. Local only; no push, merge or release gate claim.

New Character import exposes ADD ANIMATION FBX and per-clip Unassigned, Idle,
Walk, Run, Attack, Reload, Hit and Death actions in native Wicked controls.
Append retargets an isolated clone, requires one usable humanoid and every source
take, rejects changed sources, and preserves the previous candidate on failure.
Commit retains external bytes under SourceAssets/Animations/Snapshots and writes
action metadata and durable take recipes through the governed transaction.
Legacy external recipes preserve defaults; new recipes opt into source mapping.
Existing valid destination humanoids are reused. New clips remain paused.
The asset name field now preserves edits when Import is clicked without Enter.

Changed files (relative to repository root):
- EngineBridge/include/renegade/bridge/{CreatorModelImportRecipe,HumanoidRetargetService,
  ModelImportCandidateService,ModelImportCommitService}.h
- EngineBridge/src/{CreatorModelImportRecipe,HumanoidRetargetService,
  ModelImportCandidateService,ModelImportCommitService}.cpp
- Studio/src/StudioApplication.{h,cpp}
- Tests/ModelImporterRebuildGraphicsProof.cpp
- docs/{ARCHITECTURE.md,FEATURE_MATRIX.csv,MODEL_IMPORTER_REBUILD.md}

Windows x64/DX12 Release commands follow. MSBuild.exe refers to the installed
Visual Studio BuildTools MSBuild; ctest is CMAKE_CTEST_COMMAND from CMakeCache.txt.

From repository root, each target was built with MSBuild.exe and:
  /m:2 /verbosity:quiet /nologo /p:Configuration=Release /p:Platform=x64
  /p:BuildProjectReferences=false
Targets (all exit 0):
  BUILD/no-import/EngineBridge/RenegadeEngineBridge.vcxproj
  BUILD/no-import/RenegadeModelImporterRebuildGraphicsProof.vcxproj
  BUILD/no-import/RenegadePhase7Gate7BTests.vcxproj
  BUILD/no-import/Runtime/RenegadeRuntime.vcxproj
  BUILD/no-import/Studio/RenegadeStudio.vcxproj
Studio additionally used /p:TargetName=RenegadeStudio_AnimationImport.
Final name-field-only Studio rebuild also passed, exit 0.
  ctest --test-dir BUILD/no-import -C Release
    -R 'RenegadeModelImporterRebuild|RenegadePhase7Gate7BHumanoidRetargetTests'
    --output-on-failure
PASS: 8/8, 14.12 seconds. Final name-field change does not affect bridge tests.

From BUILD/no-import/Release:
  RenegadeModelImporterRebuildGraphicsProof.exe
    '../character-mutant-proof/SourceAssets/Models/Proof Triangle/Mutant.fbx'
    '../../../Tests/fixtures/Importer/external_uri_triangle.glb'
    '../external-mutant-proof' <walk-fbx> <run-fbx> <swipe-fbx> <idle-fbx>
PASS, exit 0. Private owner source paths intentionally omitted. The proof copies
external files into disposable sources before testing. Retained Mutant had
37 bones, five clips, two textures and four external animation dependencies.
The log's "6 textures" label counts all six dependencies, not just textures.

Direct proof verified changed rendered scrub pixels, preview isolation,
Play/Pause/speed, invalid source and unmappable rig refusal, changed-source commit
refusal with no product, retained byte preservation, placement Undo/Redo and
WISCENE reopen, and 300-frame grounding at (4,5,6).
After disposable originals were deleted, retained-source reimport reproduced
clips/actions. Runtime authored Idle/Locomotion/Run/Attack each resolved one
playing variant. Full external retarget reimport was in the same proof process;
do not describe that part as a fresh-process proof.

Native Studio inspection used the side-by-side executable and disposable project.
Walk played and scrubbed visibly, its action survived adding Idle, and native
Idle and Walk assignments persisted into the three-clip Mutant.rasset recipe.
The final executable separately saved Native Name Verified.rasset after typing
the new name and clicking Import without Enter. Screenshots were read and
visually inspected. Studio was closed normally; owner projects were not edited.

Standalone Runtime loaded external-mutant-proof/RuntimeProof.renegade via an
absolute --project argument (relative paths resolve under the executable folder).
Its migrated Story Flow entered Main Level successfully. Mutant was textured
and visibly on the floor. Diagnostics: startup SUCCESS, scene_loaded=true,
character_scene_sync_failed=false, player_spawned=true, one synchronized
Character; first_animation_clip="mutant idle", semantic Idle, playing=true.
Runtime was closed normally after inspection.

Inspected binary SHA256:
- Studio: 34AB99682403DB624C5182FAE2E82F440A8B2095EC2674406B6DA84BBA7F5561
- Runtime: 0F097DB8EDB26468830100074C9BD00F839778DDAD10355E5E496F27EB889B4C
Binaries were built before the implementation commit. Embedded diagnostic
build_commit is historical 9b5c5b4; use these hashes for inspected builds.

Ignored local evidence under BUILD/no-import:
external-final-*.log, external-animation-studio-build-final.log,
external-animation-studio-name-build.log, external-animation-ctest-final.log,
external-mutant-proof.log, import-proof-external-native-*.png,
import-proof-external-name-*.png, import-proof-external-runtime-final.png,
external-runtime-final-diagnostics.json, and the disposable proof project.
git diff --check passed; FEATURE_MATRIX has 16 columns per row; Wicked unchanged.

Limitations: bounded new imports only. No custom slots, clip frame authoring,
existing-Character editing, universal multi-rig compatibility or packaged export
claim. Legacy ReusableAssetReimportRecipeTests and CreatorExternalAnimationImportTests
refer to removed preparation services in this baseline and were not counted;
new optional-recipe checks run in the active graphics proof. Existing MSB8029
and unrelated C4834 warnings remain. No full-suite CI or independent acceptance.

Next task: independent exact-commit inspection and owner acceptance of this
native importer slice, then separately scope existing-Character action editing.
Preserve the implementation commit and this evidence before further changes.

## Native Character crossfade checkpoint (2026-10-02)

Implementation commit: bea6313e2ebfa270fc955b4016f9e28215d5140e.
Branch: feature/character-animation-crossfades, based on importer evidence
checkpoint 6a08fb0. The importer branch and its owner test executables remain
unchanged. No push, merge, or release gate completion.

Crossfade slice:
- AI still chooses actions; native action metadata still selects clips.
- Runtime owns transient logical weights and elapsed simulation time; native
  Wicked evaluates the poses. Sequential amounts use cumulative normalization.
- Matching channel coverage and event-free clips blend; other clips immediately
  switch. Only contributing clips are scanned/sorted per character.
- Loops fade for 0.20s; Attack/Reload/Hit for 0.08s; Death for 0.05s.
- Repeated requests do not restart loops/one-shots. A returning contributing
  loop preserves its timer. Interrupted transitions preserve current weights.
- Hit finishes before a new Attack/Reload; Death interrupts and stays terminal.
  Missing Death stops owned playback once. Missing Idle also stops owned clips.
- Native root motion is disabled for AI-owned clips: Character movement remains
  controller-owned. No speed blend space, gait synchronization, upper-body layer
  or additive animation is claimed. Durations are fixed in this slice.

Changed files: Runtime/src/RuntimeCharacterAnimation.h, RuntimeLiveDiagnostics.cpp;
Tests/CharacterAiAnimationTests.cpp, CharacterAnimationBlendProof.h,
ModelImporterRebuildGraphicsProof.cpp; docs/ARCHITECTURE.md, FEATURE_MATRIX.csv,
MODEL_IMPORTER_REBUILD.md. This handoff is a separate documentation commit.

Exact Windows Release commands, from repository root unless noted:
- Use CMAKE_COMMAND from BUILD/no-import/CMakeCache.txt:
  cmake -S . -B BUILD/no-import: PASS.
- MSBuild.exe BUILD/no-import/Tests/RenegadeCharacterAiAnimationTests.vcxproj
  /m:2 /verbosity:quiet /nologo /p:Configuration=Release /p:Platform=x64
  /p:BuildProjectReferences=false: PASS, exit 0.
- Same MSBuild arguments with
  BUILD/no-import/RenegadeModelImporterRebuildGraphicsProof.vcxproj:
  PASS, exit 0.
- Same arguments with BUILD/no-import/Runtime/RenegadeRuntime.vcxproj
  plus /p:TargetName=RenegadeRuntime_Blending: PASS, exit 0.
- Use CMAKE_CTEST_COMMAND from the cache:
  ctest --test-dir BUILD/no-import -C Release
    -R 'RenegadeModelImporterRebuild|RenegadeCharacterAiAnimationTests|RenegadePhase7Gate7BHumanoidRetargetTests'
    --output-on-failure: PASS, 9/9, 16.14 seconds.
- From BUILD/no-import/Release:
  RenegadeModelImporterRebuildGraphicsProof.exe
    '../character-mutant-proof/SourceAssets/Models/Proof Triangle/Mutant.fbx'
    '../../../Tests/fixtures/Importer/external_uri_triangle.glb'
    '../blending-mutant-proof' <walk-fbx> <run-fbx> <swipe-fbx> <idle-fbx>
  Private source paths deliberately omitted; sources copied to disposable files.
  Native translation, rotation and scale tests verify midpoint/order, no repeated
  frame accumulation, interruption continuity, loop phase, completed Death pose
  and partial-track immediate fallback. Semantic tests cover Hit priority,
  missing Death termination, optional Run fallback and state reset.

Real Mutant rendering proof writes blend-*.png under blending-mutant-proof:
Idle, Idle/Walk halfway, Walk, Walk/Run halfway, Run, Run/Idle halfway and Idle.
These snapshots were opened and visually inspected. Native channels matched,
and intermediate poses showed the expected mixtures. The governed import,
retained-source reimport, placement Undo/Redo/reopen and 300-frame grounding
proofs also run in the same executable.

Standalone inspection:
  BUILD/no-import/Runtime/Release/RenegadeRuntime_Blending.exe --project
  <absolute repository path>/BUILD/no-import/blending-mutant-proof/RuntimeProof.renegade
PASS: startup SUCCESS; scene_loaded=true; character_scene_sync_failed=false;
one synchronized Character; Player spawned; mutant idle playing as Idle.
Mutant was visibly textured and on the floor; screenshot read and inspected.
This standalone inspection exercises saved-scene loading and Idle playback;
transition midpoints are validated by the native graphics proof, not manually
triggered through gameplay in that standalone run. Runtime closed normally.

Blending executable SHA256:
BAC9ED6BC29CC44CE1E6F891BFC1E65218B881D6620675B8D386F1480C664C4E.
Executable built before commit; embedded revision is historical.
Original Runtime hash remains
0F097DB8EDB26468830100074C9BD00F839778DDAD10355E5E496F27EB889B4C;
original Studio_AnimationImport hash remains
34AB99682403DB624C5182FAE2E82F440A8B2095EC2674406B6DA84BBA7F5561.

Ignored evidence under BUILD/no-import: blending-configure.log,
blending-animation-tests-build.log, blending-graphics-build.log,
blending-runtime-build.log, blending-ctest-final.log, blending-mutant-proof*.log,
blending-mutant-proof/blend-*.png, import-proof-blending-runtime-final.png,
blending-runtime-final-diagnostics.json. Private assets remain ignored.
git diff --check passes; all FEATURE_MATRIX nonempty rows have 16 columns;
Wicked submodule source and pointer unchanged. Existing MSB8029 warnings remain.

Limitations and next task:
Fixed durations and matching coverage only. No UI duration authoring, Debug/full
CI, packaged export, crowd-performance claim or independent acceptance.
Owner tests the preserved importer executable first. Independently inspect this
exact blending implementation before integration. Then test AI movement/action
interruptions in an owner-approved disposable Level; separately scope editing
existing Character assignments. No release gate is marked complete.

Final real Mutant proof rerun after the last source/test changes: PASS, exit 0,
6.81 seconds; native crossfade and real-render snapshot checks pass; retained
reimport/actions and 300-frame grounding pass. The final log is
BUILD/no-import/blending-mutant-proof-final.log; disposable asset identity
68c9f808-17ab-423c-b532-ee1595d9a095. No source files changed after that run.

## 2026-10-02 owner importer acceptance and paired blending Test Level
Owner reports ALL animation files imported, assigned actions and played correctly;
with a nav grid, Character idles, walks, runs toward Player and attacks.
Owner also confirms a fresh Level includes sky, sunlight and default nav grid.
The older runtime-proof fixture intentionally lacks normal fresh-Level defaults.

Prepared ignored BUILD/no-import/BlendingTest (no production source changes):
RenegadeStudio_BlendingTest.exe is byte-identical to accepted AnimationImport
Studio (SHA256 34AB99682403DB624C5182FAE2E82F440A8B2095EC2674406B6DA84BBA7F5561).
Runtime/RenegadeRuntime.exe is byte-identical to crossfade Runtime_Blending
(SHA256 BAC9ED6BC29CC44CE1E6F891BFC1E65218B881D6620675B8D386F1480C664C4E),
implementation bea6313e2ebfa270fc955b4016f9e28215d5140e; prior handoff 0d4bcc5.
Copied support Content/shaders/BuildInputs/DX compiler; original binaries untouched.
Copied external-mutant-proof project to BUILD/no-import/blending-owner-project.
Original editor PID 63288 had unsaved fresh-Level edits: cancelled its close
prompt, preserved it open and minimized. New paired editor PID 51056 is open.

Native Studio PLAY launched PID 7280, parent 51056, using the bundle Runtime
and copied-project Intermediate/TestLevelSnapshots/1790939239452207-0000000000.
GET http://127.0.0.1:38742/snapshot: startup SUCCESS, scene loaded, Player spawned,
Character synced, mutant idle playing; no missing animation request.
Normal CloseMainWindow on child Runtime returned paired Studio to READY /
TEST LEVEL COMPLETED; child exited. Screenshots visually inspected:
BUILD/no-import/import-proof-blending-pair-editor.png (fresh Level defaults),
import-proof-blending-pair-runtime.png and import-proof-blending-pair-return.png.
Older copied runtime-proof has four clips and no nav grid, so this verifies launch,
startup and return, not owner pursuit blending acceptance. Owner must repeat the
configured pursuit scenario with this paired build; Asset/Character selector next.
No new code/build required: reused tested binary hashes. No push, merge or release gate.
## 2026-10-02 owner-scene partial-track crossfade repair
Implementation: 5fdd4c677bbffbad6472484690c51ad2d70b6cf1 on
feature/character-animation-crossfades (no push/merge).
Changed RuntimeCharacterAnimation.h, RuntimeLiveDiagnostics.cpp,
CharacterAnimationBlendProof.h, ModelImporterRebuildGraphicsProof.cpp,
docs/ARCHITECTURE.md and docs/FEATURE_MATRIX.csv REN-AI-007.
Owner reported Run -> three attacks without blending -> stretch Idle loop.
Live original test PID 44348 was the correct paired Runtime. Player had taken
five hits and was dead (0 health), explaining return to Idle after combat.
Captured its disposable Test Level under BUILD/no-import/blending-owner-captured.
Two AI Characters; 13 assigned clips. Native inspection found 93-track source
clips and 90-track retargeted clips: matching-coverage guard caused immediate
switches between those groups. Native controllers had no competing animation list.
Runtime startup now completes assigned transform coverage using constant tracks
from the target's initial local pose; native blending remains authoritative.
Only transient Runtime scene changes; saved Levels/importer products untouched.
Event/non-transform tracks still use guarded fallback. Counters expose fades,
incompatible switches and per-Character blend status through live diagnostics.

Release MSBuild.exe (VS18 BuildTools) commands, from repository root:
BUILD/no-import/Runtime/RenegadeRuntime.vcxproj /m:2 /verbosity:quiet /nologo
/p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false
/p:TargetName=RenegadeRuntime_BlendingFix -> PASS, build log owner-fix-runtime.
Same switches without TargetName on RenegadeModelImporterRebuildGraphicsProof.vcxproj
and Tests/RenegadeCharacterAiAnimationTests.vcxproj -> PASS.
Logs under BUILD/no-import/blending-owner-*-build.log; existing MSB8029 warnings.
ctest.exe --test-dir BUILD/no-import -C Release -R
'ModelImporterRebuild|CharacterAiAnimation|HumanoidRetarget' --output-on-failure
-> final PASS 9/9, 13.71s; log blending-owner-ctest-final.log.Native proof executable command (repository root):
BUILD/no-import/Release/RenegadeModelImporterRebuildGraphicsProof.exe
--inspect-scene BUILD/no-import/blending-owner-captured/Content/Scenes/TestLevel.wiscene
BUILD/no-import/blending-owner-fixed-proof-final
-> PASS exit 0; log blending-owner-fixed-proof-final.log.
Synthetic native partial translation/rotation/scale midpoint, repeated sampling,
completion and idempotent coverage tests pass. Existing lifecycle checks pass.
Both owner Characters pass Idle/Walk/Run/Attack/Attack/Idle render checks.
Initial inspection fixture produced blank images; corrected isolation retains
referenced meshes and disables physics only in preview clones. Added pixel-content
assertion so blank frames fail. Final character-1 run-attack-half, run-attack and
attack-attack-half PNGs read and visually inspected: visible distinct poses.
The source captured WISCENE is never saved or edited by inspection.

Replaced only paired BUILD/no-import/BlendingTest/Runtime/RenegadeRuntime.exe
with new Runtime_BlendingFix; preserved former bundle runtime as
Runtime/RenegadeRuntime_PreCoverageFix.exe. New SHA256:
E9EAA5EC91DCFE51BD5102969B7C7FD28376CC1EA004CFA81101D9B838172AE7.
Original accepted Studio and original Runtime binaries remain untouched.
Native Studio PLAY launched new Runtime PID 35704, parent 51056. Startup SUCCESS;
13 clips; five crossfade transitions; zero incompatible transitions or missing
requests; five hits and Player dead. Evidence: blending-owner-runtime-fixed-
diagnostics.json, import-proof-blending-coverage-runtime.png and coverage-return.
Runtime closed normally, Studio returned to the same owner Level ready to Play.
Standalone visual screenshot did not frame the Character; visual transition proof
comes from the inspected isolated owner-character renders, not this screenshot.
Owner subjective gameplay acceptance is still required. Fixed durations remain
0.20s loops, 0.08s actions, 0.05s Death. Idle variant scheduling/loop policy remains
basic; this repair does not change which assigned Idle variant is selected.
No Debug/full CI/package-export/layered blending claims or release gate completion.
## 2026-10-03 main CI repair integration and Alpha Playability handoff

PR #176 (`fix/main-ci-green`) was verified with four green pull-request checks
and merged into `main` as merge commit
`3bb768b148da590ea9dd7aa47fbb28006c0508d6`.

The repair addressed three clean-run CI assumptions exposed after the large
Character/importer integration:
- EngineBridge Debug required MSVC `/bigobj` for large translation units;
- the Studio CI aggregate build had to include every registered test executable
  it later asks CTest to run;
- the rebuilt importer graphics proof had to distinguish hosted-headless PNG
  readback from owner-hardware visible-model pixel acceptance.

The final PR head `f0b4f815f345dd1765bd4fde3c8ce623bea912e5`
passed Windows baseline Debug/Release and Renegade Studio Debug/Release on a
clean GitHub checkout. Local owner-hardware validation separately retained the
full rendered-model visibility assertion, and the hosted-headless importer group
passed 7/7 while preserving import/commit/reopen coverage.
The post-merge `main` Windows baseline and Renegade Studio workflows were
triggered automatically for `3bb768b`; they were still in progress when this
documentation handoff section was written and must be checked before calling the
merged main globally green.

The active programme is now **Alpha Playability — Player Arms & Combat**.
Canonical design authority:
`docs/PLAYER_ARMS_COMBAT_FRAMEWORK.md`.

The programme extends the existing Player rather than replacing it. Both hands
are first-class; combat families share equipment/action/projectile/effect
foundations. Required reference families are firearm, directional sword + shield,
bow, crossbow and magic. Directional melee uses attack/guard directions and swept
weapon collision rather than a generic centre-camera melee ray.

The first implementation gate is **P1 — First-person Arms Rig**. A dedicated
owner combat-feel gate is mandatory later for recoil, camera response, animation
timing, SFX, particles, muzzle flash, projectile/impact feel, directional melee,
parries/guards, bow/crossbow timing and spellcasting presentation. Automated
damage/state correctness is not sufficient acceptance.

## 2026-10-03 P1 recovery and honest acceptance checkpoint

Implementation baseline: 51513b95e98e6f962c0052da3f92f1b8c279b280
on feature/p1-first-person-arms-rig. Local recovery reference:
recovery/p1-baseline-20261003. The recorded origin branch points at the same
baseline; no fetch/push/merge was performed by this recovery.

Read docs/P1_STATUS_AND_RECOVERY.md before resuming P1. It inventories the four
implementation commits, source capabilities, actual test coverage, failed
experimental Runtime evidence, missing acceptance and the next bounded task.

Preserved temporary RuntimeApplication diagnostic source and patch under ignored
BUILD/renegade/p1-recovery-20261003-2217, then restored RuntimeApplication.cpp.
Foreground policy already matches baseline. Runtime, EngineBridge, Studio and
Tests now have no source diff against 51513b9. Experimental BUILD projects and
asset variants remain preserved and unaccepted; original owner arms are untouched.
Generated Tools/__pycache__ was not committed.

Changed documentation: README.md, docs/ROADMAP.md, docs/FEATURE_MATRIX.csv,
docs/P1_STATUS_AND_RECOVERY.md and this handoff. No new gameplay implementation.

Exact commands, hashes and evidence limits are in the status document.
VS18 BuildTools local incremental Release and Debug Runtime builds passed;
Release and Debug P1 executable builds passed. P1 tests passed 2/2 per
configuration (0.17s each). Related Release regression passed 9/9 (0.39s),
including the P1 pair. Existing MSB8029 warnings remain. Wicked source/pin clean.
Logs and preserved patch: BUILD/renegade/p1-recovery-20261003-2217/.

No Runtime window launched during this recovery. No direct UI/save/reopen,
real-arms visual fix, independently exported game or clean-CI claim. Synthetic
product round-trip tests do not prove live rendered arms. P1 is NOT accepted.

Next task: stabilize one fixed no-imported-arms fixture and prove grounded Player,
visible landmarks and camera/rig agreement before examining one immutable arms
asset. Stop at the first failed isolation check. Do not rotate/re-export more
variants while replacing the fixture. Verify the coordinate/rest/animation basis
before correction. Subsequent owner exact-build visual and packaged parity
verification are required; do not move to P2 on test passes alone.

## 2026-10-03 P1 no-imported-arms placeholder proof

Owner requested that supplied arms stop being used. Generated a separate
BUILD/renegade/p1-placeholder-proof project with no arms assignment or imported
assets and no Story Flow override. Baseline Release Runtime now visibly shows
solid floor, three landmarks and orange/blue proxies extending from the lower
view. Two inspected captures 62.07 seconds apart retain world framing.
Diagnostics: Player spawned; proxy geometry true; imported asset loaded false;
Character count zero. Runtime left open for owner movement/look verification.

Added Tests/PlayerViewRigFixture.cpp and optional Windows
RenegadePlayerViewRigFixture CMake target to preserve the no-asset scene generator.
See docs/P1_PLACEHOLDER_PROOF.md for commands, hash, evidence and limits.
Configure and generator Release build pass; generator save/reload passes on
separate output; P1 Release tests 2/2 pass. Existing MSB8029 warnings remain.
No production Runtime/Player/View Rig code changed; original arms untouched.
No skinned-arms or live movement/pitch/yaw or full P1 acceptance claim.

## 2026-10-03 owner placeholder movement and idle acceptance

At 22:32 Europe/London owner reports movement is fluid and idle works well on
the placeholder arms. This verifies movement feel and procedural idle on the
baseline Release Runtime (51513b9 source; hash in P1_PLACEHOLDER_PROOF.md).
No imported skeletal asset is involved. Existing running fixture unchanged.
Explicit look extremes, sprint, jump/landing, pause/resume and R reset are the
remaining immediate owner checks. Input bindings confirmed from the fixture's
governed GameplayInput file; checklist added to docs/P1_PLACEHOLDER_PROOF.md.
No implementation change or full P1 acceptance.


## 2026-10-03 directional proxy stutter repair (owner verified)

Owner clarified that the other placeholder checks work, but moving in a
direction makes the proxies stutter. This supersedes broad movement acceptance.
A native 300-frame Scene/Jolt test reproduces a 0.0498593-unit camera/rig-root
position mismatch at 75 Hz rendering and 120 Hz physics, with forward/backward
and sideways walking/sprinting. The mismatch equals raw versus interpolated
Player position; this is not evidence of a defective imported arms asset.

ApplyRuntimePlayerCamera now prefers the post-Scene::Update Player transform,
matching the attached rig's interpolated presentation position. Physics remains
movement authority, with raw position retained only as a missing-transform
camera fallback. The placeholder scene, geometry and idle behaviour are unchanged.
Tests/PlayerViewRigFixture.cpp retains the native movement regression and rejects
both camera/rig drift and an inconclusive run without interpolation divergence.
Build/test results and executable identity are recorded in P1_PLACEHOLDER_PROOF.md.
No upstream Wicked changes, push, merge or full P1 acceptance.

At 22:50 Europe/London the owner tested the repaired Release Runtime and reported
"stutter is gone". Directional placeholder movement is accepted on this fixture.
Release and Debug native movement proofs pass with maximum camera/rig error
2.38419e-07 and real interpolation divergence 0.0498593. Both configurations'
Player/arms/snapshot checks pass 5/5. P1 skinned-asset/package acceptance remains open.


## 2026-10-03 proxy snapshot and detached package proof

Camera repair source remains 4dd9953 and owner-verified Runtime is preserved.
Added Tests/PlayerViewRigParityFixture.cpp and its optional Windows manual target
in Tests/CMakeLists.txt. No gameplay production code changed.
Production TestLevelSnapshotService snapshot saved/reloaded and launched in
standalone Runtime; assignment remains empty and control scene path unchanged.
StageWindowsGameBuild plus ApplyWindowsGameExecutableIdentity and stage validation
pass. Copied package launches from unrelated working directory with no arguments;
bootstrap verifies package integrity and DX12 success, live diagnostics resolve
only detached GameData paths. Descriptor/scene/input hashes match the control.
Snapshot and package screenshots visually inspected with identical proxy framing.
Commands, hashes and evidence are in docs/P1_PLACEHOLDER_PROOF.md. An initial
stage-only trial failed the expected Gate 3 schema check; corrected tool now
finishes executable identity before producing the launchable package.

Updated docs/P1_STATUS_AND_RECOVERY.md so the supplied owner/GGMAX arms are
explicitly excluded and live camera agreement is no longer described as untested.
Next: owner checks directional movement, pause/resume and R reset in the open
P1ProxyParity.exe package. Then validate the actual Studio Test Level/Build Game
UI workflow. Real skinned arms, hand-bone sockets and asset/package closure remain
unaccepted. This fixed three-input manual plan is not a new general packager.
No push, merge or P1 gate acceptance.

## 2026-10-03 23:05 Europe/London: packaged proxy controls accepted

After being asked to check movement, Escape pause/resume and R reset in the
open detached package, the owner reported "everything works". These requested
checks are accepted for this exact proxy package. Proof tooling source: ee79134;
camera repair source: 4dd9953. Package executable SHA-256:
a5532514ed33f33cdb8c554808cb1e72de13fb54ac3d863c094dc64e60e47ace
No gameplay code or binary changed after this acceptance. Preserve the package
and fixed control fixture. Next bounded task is the actual Studio Test Level
and Build Game UI workflow. Real skinned arms, skeletal animation, animated
hand-bone sockets and real-asset dependency closure remain unaccepted; P1 remains
open. Do not resume the withdrawn owner/GGMAX arms experiment.

## 2026-10-03 Studio Test Level stale bundled Runtime repaired

Owner reported no arms through Studio Test Level. Process inspection confirmed
the correct proxy snapshot, but Studio selected its bundled Runtime from
16 September, before P1. Stale bundle hash:
56f7ac45a73ba17e03bcfbcda33f0274b53eee167e08fe8b2e2ff0b7615d171b
Closed obsolete child PID 59816 and detached package PID 26420, preserved the
old executable in the ignored proof folder, then synchronized the verified
Runtime executable, dxcompiler and Content into Studio/Release/Runtime.
Source and bundle now both hash:
69536da66fd6f40ebc392387ddeb0ce4e218f4373f0a74465f45b2e9524d1b92
No production source change. Tools/Build-Studio-Windows.ps1 already performs
this synchronization; direct-target builds had bypassed its packaging step.
Build Game also prefers this bundle. Live Runtime diagnostics initially belonged
to detached PID 26420 rather than Test Level PID 59816: match diagnostic PID to
child PID before drawing conclusions. Owner button retry remains pending.

## 2026-10-03 23:23 Europe/London: Studio Test Level gameplay accepted

After synchronizing Studio's bundled Runtime, owner retried the Test Level
button and reported "yes everything works". Requested movement/look/sprint/jump,
pause/resume and reset checks accepted for the proxy fixture through Studio.
Live Runtime PID 48644 matches Studio's child PID, loads the new TestLevel
snapshot, reports startup success, proxies enabled and no imported arms.
Evidence: BUILD/renegade/p1-placeholder-proof/studio-test-accepted.json.
Studio's captured ready flag remains false while its update is suspended;
this does not establish readiness-indicator acceptance. Gameplay owner result
is accepted; actual Studio Build Game export remains next. Source checkpoint
5d4d985 records bundle repair; verified Runtime hash unchanged. Full P1 open.

## 2026-10-03 Studio export-test project prepared

Added Tools/StoryFlow/Create-P1ExportFixture.ps1. It refuses existing destinations,
creates fresh stable IDs, copies only the accepted scene/input unchanged, creates
scene identity metadata and a three-node/two-route Story Flow, and declares the
input map Always Include. Owner project is the ignored
BUILD/renegade/p1-studio-export-proof/P1ExportProof.renegade. Control project and
accepted Runtime/Studio binaries were not changed.
Extended Tests/PlayerViewRigParityFixture.cpp with --inspect-export-project,
calling the real PrepareWindowsGameBuildProjectState readiness boundary with a
hidden GPU context and synchronized teardown. Release helper build passes;
owner export project and a second fresh generator-validation project both pass
InspectProject/dependency/registry/route preflight, exit 0, one Level completion.
The generated scene/input SHA-256 match the control. git diff --check passes.
Commands and logs are in docs/P1_PLACEHOLDER_PROOF.md. Early helper trials failed
before document newline/GPU teardown corrections and are not acceptance evidence.
Next: owner closes Test Level, opens P1ExportProof in Studio, uses Build Game and
checks the resulting exported game's movement/look/idle/pause/reset. Export may
briefly open Runtime for automatic smoke validation. This preflight is not an
actual export or full P1 acceptance. No supplied arms resumed; no push or merge.


## 2026-10-03 P1 native hand socket binding checkpoint

Implementation commit: bab0de7 on feature/p1-first-person-arms-rig. Prior owner
Studio export-launch confirmation is recorded by 757ee6d: actual P1 Export Proof
Windows build launches/rendered DX12 at 75 FPS with both proxies and landmarks.

Changed Runtime/src/RuntimePlayerViewRig.h and RuntimePlayerViewAsset.h: explicit
primary/off-hand/support native boolean anchor metadata binds sockets to bones or
child grips in the view-model skeleton. Missing roles retain original offsets;
duplicate/non-skeletal/cyclic anchors fail before mutation and failed asset commit
retains proxies. Native Wicked animation/hierarchy remains the sole pose authority.
No movement/camera contract change or supplied owner/GGMAX arms use.

Extended PlayerViewRigTests and manual PlayerViewRigFixture; added shared generated
native skeleton and GPU proof headers. Updated P1_STATUS_AND_RECOVERY, ROADMAP,
ARCHITECTURE and FEATURE_MATRIX; full contract, commands, hashes and limits are in
docs/P1_HAND_SOCKET_BINDINGS.md. Runtime/tests/fixture build Release and Debug exit
0. Final related CTest checks pass 5/5 per configuration. Packaged governed rasset
loader binds remapped anchors; save/reload, atomic rejection, fallback and cleanup
checks pass. Generated rigid geometry is not a production skinned arms asset.

Manual --socket-proof DX12 runs 300 frames of translation/rotation Idle Walk Sprint,
forward/backward/sideways movement and pitch/yaw in both configurations. Socket
matrix error 0; camera/rig error 2.38419e-07; actual animated local Z range 0.0592m;
pause matrix/timer errors 0; three captures per configuration; exit 0. Rendered
captures inspected with yellow primary, green off-hand and purple support markers.
Evidence in ignored BUILD/renegade/p1-socket-proof; build logs under p1-socket prefixes.
New Release standalone Runtime starts the accepted unchanged proxy project in DX12,
then closes only its new test window cleanly, exit 0. This is startup regression,
not owner controls acceptance of the new binding build. Existing MSB8029 remains.

Runtime candidate hashes: Release
3b3e0bbddcdd7cab9251bca92b7e64ee2c63b7406aac5107d10d24513f0968ae;
Debug de0d86e3f5ad45d6507bbc3934743df7aa63789f059126ce06b20aea53925f3d.
Studio's bundle and owner-approved export deliberately remain at the previously
accepted camera-sync binary; they do not contain this new socket candidate yet.
Wicked source/pin unchanged; no push/merge. Untracked Tools/__pycache__ and log.txt
left alone. P1 remains open; this is C++/native metadata exposure only.

Next bounded outcome: command-backed Studio primary/off-hand/support bone/grip
selection and grip offsets with Undo/Redo and governed asset save/reopen. Then use
one agreed separately generated/authored skinned view model for owner verification;
preserve the accepted proxy control. Do not resume the withdrawn supplied arms.

## P1 native Hand Grips editor checkpoint — 2026-10-04

Code and documentation commit: 7a105d8dcb1b6db188fc006e0432127b0d972cab
on feature/p1-first-person-arms-rig. No push or merge performed.

Outcome: Player's governed arms selector now offers EDIT HAND GRIPS. Native
Studio widgets expose primary/off-hand/support roles, native bone hierarchy
choices and local position/rotation offsets. The private bridge working copy
has its own CommandService Undo/Redo history. Save journals the .rasset product,
managed projection and registry together and retains hand_grips in the import
recipe. Close discards unsaved changes. Bones are identified by canonical
hierarchy-name arrays, excluding the creator transform wrapper; ECS IDs are not
persisted. Invalid/missing/ambiguous bones and malformed ownership markers fail
before mutation. DirectX-compatible rotation decomposition fixed the nonzero
three-axis roundtrip mismatch found during this session.

Validation:
- EngineBridge, Runtime, PlayerViewRigTests, PlayerViewRigFixture and Studio
  build Release and Debug, exit 0. MSB8029 and existing C4834 warnings remain.
- Tests target now uses /bigobj for Debug JSON-generated sections. After CMake
  regeneration rerun MSBuild so it reads the new project, rather than executing
  an already-loaded stale project definition.
- Related CTest selector Phase6Gate1Player|PlayerViewRig|TestLevelSnapshot|
  ReusableAssetTests: 6/6 per configuration.
- New headless checks: offsets and rotation, validation without partial mutation,
  Undo/Redo and saved-state boundary, injected AfterReplace(index 1) failure and
  byte-for-byte three-file rollback, successful retry, native reopen, creator
  recipe reapplication and stale external-product rejection.
- Release/Debug authored-grip DX12 proof: 300 native animated motion frames;
  socket matrix error 0, camera error 2.38419e-07 metres, animated bone Z range
  0.0592 metres, pause matrix/timer errors 0, three captures each, exit 0.
  The animated-range probe now reads the bone parent of the authored anchor;
  the grip transform's local offset is correctly static.
- Native Release Studio on an isolated synthetic fixture: selected product,
  opened real Hand Grips controls, slider changed primary Z 0.24 -> 0.583,
  SAVE persisted 0.5829999446868896, Close/reopen restored 0.583 with clean history.
  Player scroll layout now contains the arms selector and grip button; scene
  gizmo/outline are suppressed while this editor is open. Native UI capture:
  BUILD/renegade/p1-hand-grips-proof/studio-reopened.png.
- Debug Studio launch from the isolated proof working directory hit a Wicked
  DX12 graphics-pipeline assertion at wiGraphicsDevice_DX12.cpp:3939 before
  editor entry. Root cause not established. Debug build/headless/GPU evidence
  must not be described as Debug Studio startup acceptance.

Exact build commands and limits are in docs/P1_HAND_GRIP_EDITOR.md. Ignored logs
use BUILD/renegade/p1-grips prefixes; tests/GPU/UI fixture artifacts are under
BUILD/renegade/p1-hand-grips-proof. Release Studio was built with
/p:TargetName=RenegadeStudioHandGrips because owner's original Studio was running
and its executable was locked. Existing owner Studio, proxy control project,
Runtime bundle and accepted exported game were not replaced. Discarded only
agent-owned test windows during UI verification.

SHA-256:
Studio Release candidate:
7EA283E885D82A0FE6A1188660E57F138624E6BA987F5461C02BFB7CB2A078DF
Runtime Release:
47EFF1BDA3795FB70CFB1FDF4803AFDA11932C8AD7FEB34F6831D19326286539
Runtime Debug:
0E1FC32C4A81C911CC74E34F01FC83583BEA0F8770870FAECFC2CA7EC182650F

Binaries were compiled from checkpoint changes on parent 2f8662d before the
source commit, so their displayed source revision remains that parent. No owner
exact-commit verification is inferred. The two-bone native test product is
synthetic; it is not a real FBX import or proof of skinned deformation.

P1 remains IN PROGRESS. Original owner/GGMAX arms remain withdrawn. Next bounded
outcome: owner verify the native grip editor, resolve Debug Studio startup
assertion if reproducible from its normal resource directory, then agree on a
separately generated/authored skinned view model for rendered acceptance.
No production grip IK, weapon behaviour or P1 completion is claimed.

## Studio DX12 startup assertion repaired - 4 October 2026

Branch: feature/p1-first-person-arms-rig.
Source/diagnostic commit: 1446cef7be51f8edf1c3c2675d4f37d45a27f7e9.
Parent: f737777. No push or merge.

Confirmed the former Debug failure at wiGraphicsDevice_DX12.cpp:3939 is
CreateCommandSignature, not pipeline creation. Native DX12 debug layer reports
#743: root parameter slot 0 was not declared to hold constants. CDB stack reaches
StudioRenderPath::LoadGridResources. RenegadeGrid and RenegadeImGui signatures
placed CBV(b0) first, conflicting with the pinned backend's counted draw setup.

Changed all four Studio/shaders/RenegadeGrid*/RenegadeImGui* HLSL files to reserve
one root-constant DWORD at slot 0 using native b999; retained matching VS/PS
signatures and b0 binding. Upstream source, submodule, arms/gameplay, accepted
owner project and exported game are unchanged. Corrected the earlier misleading
grid-shader comment and P1_HAND_GRIP_EDITOR assertion description.

Added Tests/StudioShaderDx12Proof.cpp and its manual Windows GPU target in
Tests/CMakeLists.txt. Updated docs/FEATURE_MATRIX.csv, docs/P1_HAND_GRIP_EDITOR.md
and docs/STUDIO_DX12_SHADER_ASSERT.md. The latter records exact diagnosis,
commands, logs and acceptance boundaries.

Validation (from repository root; CL=/MP4 for proof and Release Studio):
- cmake -S . -B BUILD/renegade: exit 0.
- MSBuild BUILD/renegade/Tests/RenegadeStudioShaderDx12Proof.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false: exit 0. Repeat Configuration=Release: exit 0.
- BUILD/renegade/Tests/Debug/RenegadeStudioShaderDx12Proof.exe Studio/shaders: exit 0. Repeat Tests/Release: exit 0. All four shaders compile and pass real Wicked CreateShader/command-signature creation; both pipeline descriptions accepted.
- Original shaders under CDB reproduce #743. Corrected grid plus original ImGui: grid passes; ImGui fails identically; direct negative process exit 2170.
- MSBuild BUILD/renegade/Studio/RenegadeStudio.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false: exit 0. Repeat Release with /p:TargetName=RenegadeStudioDx12Fix: exit 0.
- ctest --test-dir BUILD/renegade -C Debug -R "Phase6Gate1Player|PlayerViewRig|TestLevelSnapshot|ReusableAssetTests" --output-on-failure: 6/6. Repeat -C Release: 6/6.
- CDB -c g with debugdevice: Debug reaches Project Hub, loads isolated HandGripsProof, renders floor/landmarks/grid and opens native Hand Grips with saved primary Z=0.583. Zero DX12 errors/breaks; two nonfatal #680 depth-view pipeline warnings remain. Release reaches rendered welcome screen; no DX12 validation messages in captured startup.
- All four source/deployed shader SHA-256 pairs match per configuration; git diff --check passes.

Native captures: BUILD/renegade/p1-hand-grips-proof/dx12-debug-grid.png and
dx12-debug-hand-grips.png. Application CDB logs: dx12-studio-Debug.txt and
dx12-studio-Release.txt there. Build/proof/negative/CTest logs use dx12-* under
BUILD/renegade. Agent-owned debugger sessions stopped after capture; owner's
original Studio and earlier Release grip candidate remain open.

Debug Studio SHA-256:
928016EFCF63041419D0BCDB4B01850C67522CDD8C89222EC5D266B677556A92
Release RenegadeStudioDx12Fix SHA-256:
A225220EFEBDD487372E86B940E89D1CE5FC20BB52B296AF1823A5796CBCC7EE
Shader deployment is part of the repair; exe hash alone is insufficient. Binaries
were built on parent f737777 with this patch; displayed revision remains parent.
No exact-source-commit owner acceptance is inferred.

Risks/next: synthetic copied fixture emits unrelated Story Flow stable-identity
errors and is not a production project acceptance. Nonfatal #680 warnings remain.
This bounded startup fix does not close P1 or prove real skinned arms. Next is
independent exact-commit verification and owner acceptance of native grip editing
on an appropriate governed asset; withdrawn original/GGMAX arms remain withdrawn.

## P1 isolated textured shotgun assembly proof - 4 October 2026

Source/proof checkpoint: a858e87b73ff3d632b7391834f60e99d6781fd0b.
Branch feature/p1-first-person-arms-rig; parent e8df1b5. No push or merge.
User authorized inspection/preview using supplied SawedOffShotgun pack, rather
than the earlier full-body Shotgun Animset. Original withdrawn arms remain unused.

Changed Tests/FirstPersonAssemblyGraphicsProof.cpp, Tests/CMakeLists.txt and
docs/P1_SHOTGUN_ASSEMBLY_PROOF.md. Added a manual Windows DX12 native diagnostic
and optional interactive view; no Studio/Runtime/Player/production importer or
upstream source/pin changes. No feature-exposure change; FEATURE_MATRIX and
production P1 status remain unchanged.

Owner-supplied FBXs found locally; four idle/reload inputs match uploaded bytes.
Copied pack and separate Manny D/N textures into ignored input tree. Weapon and
shell textures were already supplied. FBXs contain stale original-author paths;
governed PrepareModel correctly refuses unresolved dependencies. Fixture reports
that refusal, uses raw native converter and explicitly relinks supplied D/N
textures plus weapon/shell ORM maps. It does NOT prove governed asset import,
material relink UI, retained-source reimport or packaged game integration.

Native conversion: arms161 bones/483 channels; weapon7 bones/21 channels.
Arms Idle7s, weapon Idle0.666667s; both Reload3s. Separate skeletons preserved.
Direct identity weapon_r attachment was visibly wrong. Fixture derives a
provisional offset from right-hand and weapon Handle reference pivots at time0,
then retains native hierarchy attachment. Not an authored socket/IK solution.
Fixed first-person camera inspects original import coordinates; production
axis normalization/camera-relative placement remains unresolved.

Evidence:
- cmake -S . -B BUILD/renegade: exit0.
- CL=/MP4; MSBuild BUILD/renegade/Tests/RenegadeFirstPersonAssemblyGraphicsProof.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false: exit0, existing MSB8029 warnings.
- From BUILD/renegade/Tests/Release:
  RenegadeFirstPersonAssemblyGraphicsProof.exe ../../p1-shotgun-proof/input ../../p1-shotgun-proof/captures: exit0 with explicit texture relink including ORM.
- Overview and FP captures at0,.6,1.2,2,2.9s; Idle/Reload WISCENE saves reopened and rendered at1.2s.
- Textured idle FP, reload FP and reopened overview inspected: arms beneath
  camera, gun forward, red shells and wood/arm details present, reload opens gun.
- --view starts paused; R restarts both reload tracks, Space pauses/resumes,
  Escape closes. Initial blurry64px canvas corrected by refreshing app window
  canvas and render-path size after resize. Corrected live window inspected
  sharply at larger size. NO keyboard/mouse automation; owner controls pending.

Power cut interrupted first viewer-check attempt. User restarted Desktop Commander
with established npx.cmd remote launcher; connection restored and resumed above.
Only earlier agent-owned preview closed for size fix. Final proof viewer remains
open for owner (PID18824 at checkpoint; do not assume PID after a restart).
No running owner Studio was detected at initial inspection; accepted proxy
project/export/Runtime were not modified.

Ignored evidence BUILD/renegade/p1-shotgun-proof: input copies, captures,
textured-proof.log, viewer.log and viewer-window.png. Build log
BUILD/renegade/p1-shotgun-build.log.
Final Release proof exe SHA-256:
8C92B649D9279B5853DBD76B2D3A01C6B1B0AC1AB6A639FC3BC488827DCF8282.
Built from pre-commit working changes; no independent exact-commit gate claimed.
No third-party assets committed.

P1 remains IN PROGRESS. Final grip fit, loose-shell attachment/visibility events
and ammo/action timing need inspection; Unreal montage text is not a complete
demo Blueprint setup. Next: owner inspect R/Space/Escape preview, resolve exact
authored attachment/timeline, then bounded governed assembly import/relink/save
workflow, followed by existing Player View Rig integration. Do not broaden into
inventory/pickups or replace working Player controls at this checkpoint.

## P1 proof lateral grip correction - 4 October 2026

Source checkpoint 9af049296ad681f11a18bbfa5df06e58b5a229ff; parent ce2a2af.
Owner confirms animations work very well; reports gun/shells right of hands.
Uploaded screenshot read locally successfully despite displayed missing-file error.
Changed Tests/FirstPersonAssemblyGraphicsProof.cpp and
docs/P1_SHOTGUN_ASSEMBLY_PROOF.md: +0.025m imported-X correction to entire weapon
root before local attachment derivation (camera-right is -X). Shells follow
existing native weapon skeleton; clips/hands/camera/gameplay unchanged.

Same Release MSBuild command from prior checkpoint exits0; full textured capture,
WISCENE save/reopen/render proof exits0. Logs alignment-proof.log and existing
p1-shotgun-build.log. Idle and reload1.2s/2.0s FP renders visually inspected:
lateral mismatch reduced; exact final fit remains owner-pending. Agent-owned
previous preview closed through CloseMainWindow, corrected --view reopened.
No mouse/keyboard automation, push, merge, upstream or production Player changes.

Corrected Release proof SHA-256:
B539693C7A1A25E7CB42D735FF715875D332550BE56F070858FFA19DEB96F953.
Pre-commit compiled source content; owner exact-commit verification not inferred.
Animations accepted by owner, corrected alignment not yet accepted; P1 still
IN PROGRESS. Next assess grip/shell contacts through full reload with owner.

## Authored shotgun attachment restored - 4 October 2026

Source checkpoint 7c4a9762c864029f09b9449452e8aeda562edb99; parent 56d5b93.
Owner explicitly rejects previous 25mm calibration: stock rests on back of hand,
reload shells hover below palm. Animation quality accepted; grip NOT accepted.
Changed Tests/FirstPersonAssemblyGraphicsProof.cpp and
docs/P1_SHOTGUN_ASSEMBLY_PROOF.md only. Read-only original Unreal package
inspection located BP_DemoCharacter SKM_Weapon_GEN_VARIABLE serial export
369438..370399: AttachToName ik_hand_gun; location
(-3.466970,-27.336276,4.505738)cm, Rotator(6.552304,-182.929938,-10.254553).
Proof replaces guessed weapon_r placement with this exact converted local
transform. Skeleton GripPoint is middle_01_r, not demo attachment.
No original asset, upstream, production Studio/Runtime/Player change.

Same bounded Release MSBuild command recorded above: exit0. Full paired
captures and WISCENE save/reopen/render: exit0, authored-proof.log.
Idle, reload1.2s/2s and new live viewer visually inspected: fit improved,
owner acceptance pending. No original Unreal/Wicked Editor parity claimed.
Current exe SHA256 A81A3421BEBD20CD4F52E74B8F46736153B7ECA85A84098E252868A431641E20.
Built pre-commit source content; no independent exact-commit gate verification.
git diff --check clean. Existing untracked Tools/__pycache__/ and log.txt left.
Only earlier agent diagnostic closed via CloseMainWindow. New preview left open,
PID6632 at checkpoint; use process name/title, not a stale PID. R reload,
Space pause/resume, Escape close. No keyboard/mouse automation, push or merge.

P1 remains IN PROGRESS. Next owner inspect actual grip and shell contacts through
reload; resolve any remaining authored event/visibility behavior before governed
assembly import/relink/save workflow and existing Player View Rig integration.

## Owner acceptance and assembly import foundation - 4 October 2026

Source checkpoint e329ac830be1c7d79cd0a76656b1b4cc88894f94; parent 1ffe86e.
Owner accepts authored shotgun diagnostic grip and reload ("almost perfect",
then confirms no remaining mismatch). Baseline source 7c4a976; diagnostic exe
A81A3421BEBD20CD4F52E74B8F46736153B7ECA85A84098E252868A431641E20.
This is visual diagnostic acceptance, not production P1 completion.

Changed shared import/recipe/commit boundaries for explicit material-slot texture
relinks and native matching-rig clip ingestion; added MatchingRigAnimationService,
headless rejection/recipe tests, workflow/cold proof modes and a separate manual
workflow target. Studio's existing ADD ANIMATION now automatically preserves
matching native rigs, with prior humanoid retarget fallback retained. Texture
relink bridge API has no creator UI yet. Full changed-file list: git show --stat
e329ac8. README, architecture, roadmap, feature matrix and P1 docs updated.
Canonical detailed limits/commands: docs/P1_ASSEMBLY_IMPORT_FOUNDATION.md.

Release configure and builds of Bridge, Studio, matching-rig tests, workflow
proof, importer graphics proof and PlayerViewRig tests: exit0. Same MSBuild
/m:2 /nologo /verbosity:quiet /p:Configuration=Release /p:Platform=x64
/p:BuildProjectReferences=false with CL=/MP4.
CTest selection recorded in p1-workflow-ctest.log: six of six pass (matching-rig,
PlayerViewRig tests/contract, GLB importer, animated Character and cold reopen).
Real-pack --workflow exits0: both rigs/Idle+Reload clips retained, committed,
stable-ID reopened, reconstructed from retained recipes and reassembled using
accepted authored parent/transform. Duplicate relinks and disposable changed-
texture commit reject without product. Maximum clone normalization 1.78814e-7
for arms, zero for weapon; strict indices/binds preserved, no retargeting.
--workflow-reopen exits0 in a separate process; all FP frames require lit model
pixels, all captures require visible pixels. Fresh-process reload1.2s inspected
via unique cold-verified-reload-1743.png after stale same-path image display.
Final --workflow rerun with final proof executable exits0.

Studio exe SHA256 7390E00685B8DE7DB509AA6ECF2B3989F8036C7486F11257C78F0DEE1E27D5FA.
Workflow exe SHA256 9CE51204BC79A3729E06C7875284A1ACB8FAC78830CF864A0C785E3C84F7E168.
Compiled before commit (final newline-only normalization); exact-commit
independent acceptance/CI and Debug not claimed. Existing warnings remain.
Existing ReusableAssetReimportRecipeTests build blocked by removed
CreatorModelMaterialPreparationService.h from prior importer rebuild; recorded,
not silently counted passing or repaired by this scoped change.
Deep initial proof project hit Windows staging-path limit; final ignored project
uses BUILD/p1wf short destination. No production path-length fix claimed.

Evidence: BUILD/p1wf plus BUILD/renegade/p1-shotgun-proof/workflow-proof.log,
workflow-cold-proof.log and p1-workflow-*.log. Original assets/Unreal projects
untouched; licensed files not committed. No mouse/keyboard automation, new
Runtime launch, upstream change, push or merge. Accepted diagnostic viewer stays
open (6632 at prior checkpoint; resolve by process/title). Studio built but not
restarted. Untracked Tools/__pycache__/ and log.txt preserved.

Next bounded outcome: native Studio first-person assembly controls over these
bridge APIs; choose retained arms/weapon parts, explicit parent bone and authored
transform, paired semantic clip preview, persist/reopen one assembly product.
Then connect that assembly to the existing Player Start and prove camera-relative
skin, Test Level and Build Game parity. Do not treat the current two Character-kind
rigged-part test products as a final Weapon Asset/NPC workflow, and do not broaden
into inventory/ammo/fire gameplay. P1 remains IN PROGRESS.

## Studio assembly authoring recovered - 4 October 2026

Base source checkpoint 6562c81; implementation checkpoint is the commit containing
this section (resolve with git log). Branch feature/p1-first-person-arms-rig.
Recovered uncommitted assembly candidate rather than rebuilding accepted imports.
Changed-file inventory: git show --stat at that implementation commit. New bridge
FirstPersonAssemblyService, Studio FirstPersonAssemblyEditor, recipe tests and
workflow proof additions expose retained part selection, explicit attachment,
paired native preview, transactional assembly save/reopen and command-backed
Player Start assignment. README, architecture, roadmap and feature ledger updated.

Recovered evidence: p1assembly-ctest.log has seven passing Release checks;
p1assembly-final-save.log and p1assembly-final-cold.log report exact recipe reopen,
both rigs and paired camera rendering. Source-unavailable proof is separately
recorded in p1assembly-source-unavailable.log. These are prior-session results,
not newly rerun or independent acceptance. Cold Reload-1.200000.png visually
inspected during recovery: textured hands, weapon and shells are visible.
Studio panel screenshot studio-assembly-12.png exposed dark image styling,
parent-label overlap and clipped status. Applied the existing importer theme
exception to assemblyImage_ (white sprites and disableBackground), shortened the
parent label and expanded panel height. Also restart completed paired previews
on Play and report assignment failure accurately after successful product save.
Final UI appearance still requires direct inspection; no visual success inferred.

Recovery build command from repo: set CL=/MP4; MSBuild executable from VS18
BuildTools Current/Bin on BUILD/renegade/Studio/RenegadeStudio.vcxproj with /m:2
/nologo /verbosity:quiet /p:Configuration=Release /p:Platform=x64
/p:BuildProjectReferences=false /p:TargetName=RenegadeStudioAssemblyRecovery.
Exit 0, p1assembly-recovery-studio-final.log. Alternate exe name leaves the
already-running Studio untouched. SHA256:
FC38B3B890EAEA0488391C086139EE510172C59C49C314BB34A4CA5C0EBE7815.
Existing MSB8029 temporary-directory warnings remain. No application launched,
closed or controlled during recovery. No upstream change, push or merge.
Initial ctest invocation failed because PATH lacks CTest; do not count stale
LASTEXITCODE from that invocation. Correct explicit VS18 CTest binary used for
--test-dir BUILD/renegade -C Release -R
'FirstPersonAssemblySettings|MatchingRigAnimation|RenegadePlayerViewRig'
--output-on-failure; result in p1assembly-recovery-ctest.log.

P1 remains IN PROGRESS. Next: inspect the rebuilt Studio panel with the owner,
then extend existing RuntimePlayerViewAnimation to evaluate the assembly arms
and weapon tracks together (currently treated as action variants), preserving
camera/controller ownership. Prove real assembly Test Level and Build Game parity.
Do not broaden into ammo, firing, inventory or pickups. General texture relink UI,
draft Undo/Redo and assembly update/rebuild remain outstanding. Existing
Tools/__pycache__/ and log.txt stay untracked.

Corrected recovery CTest invocation exits 0: four of four pass, 0.34 seconds
(PlayerViewRig tests and source contract, MatchingRigAnimation, assembly settings).
git diff --check passes; Git warns only about LF-to-CRLF normalization in appended
documentation. Recovery build is pre-commit source-identical code; independent
exact-commit verification and owner panel inspection remain open.

Exact recovered assembly implementation commit: 2be899e. The following checkpoint
commit changes documentation only. Release recovery executable and four passing
checks correspond to the implementation code; final Studio visual inspection and
Runtime paired-action integration remain pending.

## Owner assembly preview acceptance and name-field repair - 4 October 2026

Owner reports everything works as expected in recovery build after testing native
assembly preview. Then reports renamed asset name did not save. Read-only project
inspection confirms Shotgun Assembly Verified.rasset and its recipe/projection/
thumbnail exist; a later First Person Assembly product also exists. Root cause:
OpenAssemblyEditor always resets the name input to the default despite reopening
the assigned assembly recipe. Studio/src/FirstPersonAssemblyEditor.cpp now derives
the reopened name from the assigned product's registered path stem. Stable asset
identity, product files and owner level remain unchanged. Save still creates a new
product; in-place asset renaming is not introduced. Name-field visual acceptance
and level persistence after owner actions remain unverified.

Build: same VS18 Release MSBuild command recorded above, with
/p:TargetName=RenegadeStudioAssemblyNameFix; log p1assembly-name-fix.log. Alternate
exe avoids changing the owner's running recovery build. No automatic restart.

Name-field repair Release build exits 0; git diff --check passes. Existing MSB8029
warnings remain. Commit containing this section is the bounded repair checkpoint.

## Runtime paired assembly movement - 4 October 2026

Base e7be162; implementation is the commit containing this section. Owner confirms
native assembly controls work as expected, then confirms selecting the named asset
and saving the level. Read-only real-product proof resolves the saved named assembly
assignment. P1 remains IN PROGRESS; no gameplay reload or release gate closure.

Changed RuntimePlayerViewAnimation.h: recognize explicit assembly root marker and
track roles, require one arms/weapon pair per movement action and an Idle pair,
set both paused native timers from one clock before the existing Wicked scene
update, suppress root motion and unused action tracks, hold shorter clips until
whole-pair wrap, preserve clocks across movement fallbacks, reset both tracks.
Legacy single-rig variant/crossfade path is retained. Pair-to-pair transitions
currently switch immediately; paired crossfades remain deferred. Diagnostics adds
paired-assembly and active-track count without per-frame clock event spam.
Tests/PlayerViewRigTests.cpp covers native pose evaluation of both tracks with
unequal starts/durations, no double timer advance, pause, wrap, fallback, action
switch/reset, missing/duplicate partners and native serialized entity remapping.
Tests/FirstPersonAssemblyGraphicsProof.cpp adds --runtime-assembly and
--runtime-package proof modes. README, roadmap, architecture, feature matrix and
P1_ASSEMBLY_AUTHORING.md updated. Full changed-file inventory: git show --stat.

VS18 Release MSBuild (same executable and CL=/MP4 as prior checkpoint): targets
Tests/RenegadePlayerViewRigTests, Tests/RenegadeFirstPersonAssemblyWorkflowProof,
Runtime/RenegadeRuntime with /m:2 /nologo /verbosity:quiet /p:Configuration=Release
/p:Platform=x64 /p:BuildProjectReferences=false: all exit 0. Final logs:
BUILD/renegade/p1paired-final-<target>.log. Explicit VS18 CTest --test-dir
BUILD/renegade -C Release -R
'FirstPersonAssemblySettings|MatchingRigAnimation|RenegadePlayerViewRig'
--output-on-failure: 4/4 pass, 0.41 seconds, p1paired-ctest.log.

Manual real-product proof commands, from repo:
BUILD/renegade/Tests/Release/RenegadeFirstPersonAssemblyWorkflowProof.exe
BUILD/p1wf/project-ad198d5b/AssemblyProof.renegade BUILD/p1paired --runtime-assembly
exits 0 (p1paired-project-proof.log). Saves disposable Test Level snapshot and
verifies assigned stable ID survives snapshot loading; no owner level rewrite.
Same exe with BUILD/p1paired/isolated-package BUILD/p1paired/cold-final
--runtime-package exits 0 (p1paired-cold-final.log) in a separate process.
Isolated asset fixture has only product and manifest: no retained-source rigs,
recipes or textures. Both modes verify two armatures, no second physics/Character,
paired rendering through the native Runtime loader/controller, pause and cleanup.
Cold paired-1.100000.png visually inspected: textured arms and shotgun visible.
This tests packaged loading, not the actual Build Game UI/export acceptance.

Initial headless native-pose test used full Scene::Update without a graphics device
and crashed; corrected to the pinned native animation dependency scan/update and
job wait. Initial proof compile hit ambiguous Translate initializer; corrected to
explicit XMFLOAT3. Initial diagnostic build rejected float DiagnosticValue;
removed the continuous clock observation. Final builds/tests above pass. Existing
MSB8029 warnings remain. Debug, full suite, CI and independent exact-commit review
not claimed. git diff --check passes.

Studio had stale Release/Runtime/RenegadeRuntime.exe, preferred over sibling Runtime.
After confirming no Runtime process running, copied final executable to that
existing Studio launch path; both SHA256:
352B73620B68A6B9D4B7D383E46825D9556F0B8BAD1501CB15AF99BB0274026F.
Studio remains open; no live Runtime launched or user app closed by this work.
No upstream change, push, merge or licensed asset commit. Existing untracked
Tools/__pycache__/ and log.txt preserved. Next owner closes assembly panel and
uses Studio PLAY to verify real camera-relative arms/weapon rendering and look;
then actual Build Game parity. Read diagnostics for paired=true and active_tracks=2.
Reload/Attack/Equip remain preview-only. No inventory/ammo/fire scope expansion.

Exact Runtime paired movement implementation checkpoint: 31263ba. This following
checkpoint is documentation-only; compiled code matches that implementation.
Independent exact-commit and owner Test Level/actual export acceptance remained open at that checkpoint.

## Owner real assembly Test Level acceptance - 4 October 2026

Implementation 31263ba; preceding documentation checkpoint 363c37e. Owner reports
"everything looks great" after the requested Studio PLAY camera/look check.
Recovered screenshot shows Renegade Runtime - Assembly Proof [DX12], textured
arms/gloves and shotgun held together in the foreground, and 75 FPS. This records
bounded owner Test Level visual acceptance; movement timing is covered by the
previous native proofs, not inferred from a still image. Runtime was no longer
running at this read-only follow-up, so live paired/active-track diagnostics were
not captured. Studio's Runtime executable still hashes to
352B73620B68A6B9D4B7D383E46825D9556F0B8BAD1501CB15AF99BB0274026F.

Changed HANDOFF.md, docs/P1_ASSEMBLY_AUTHORING.md and docs/FEATURE_MATRIX.csv only.
Validation: git diff --check; no new implementation or rebuild required. No app
launched or closed, owner level edited, upstream change, push or merge. Existing
Tools/__pycache__/ and log.txt preserved. P1 remains IN PROGRESS. Next: actual
Studio BUILD > BUILD WINDOWS GAME export and owner launch parity for this real
assembly; independent exact-commit verification remains pending. Gameplay reload,
firing and paired crossfades are not claimed.

## Owner actual Assembly Proof export acceptance - 4 October 2026

Following a277ec8, original deep-root Studio export failed at Gate 2 copying the
retained shotgun ORM texture. Source exists and about 44 GB free; failed staging
path measured 265 characters. Prepared separate short-root project via robocopy
/E /XD Builds Intermediate Saved /R:1 /W:1; all 49 copied files hash-identical.
First retry still opened original project, confirmed by live Studio diagnostics.
Owner then opened short-root copy and reports "the build works". Recovered image
shows exported Assembly Proof Runtime DX12 with textured arms and shotgun, 74 FPS,
and export folder Explorer behind it. Read-only inspection confirms promoted
Assembly Proof.exe plus build-report and package-manifest. Export executable SHA256:
E9DA5D6F437D55F9B8A72E232A4B24F90A98E38F172AE9067DC4C72905E17E8A.
Build report status gate5_validated_for_final_path; package isolation and smoke
passed_gate4, safe rebuild passed_gate5, distribution_ready=false. Its configured
revision remains 6562c812 (running Studio configuration metadata); do not treat
that as an independent exact-31263ba verification.

This records bounded actual Studio export launch/visual acceptance for this real
assembly, alongside prior Test Level acceptance. No gameplay reload/fire claim or
whole P1 gate closure. Path-length workaround succeeded; durable long-path handling
and OS-error diagnostics remain outstanding. Earlier HANDOFF append attempts were
blocked by a file-sharing lock; this entry records those outcomes now. Changed
HANDOFF, assembly authoring status and feature ledger only; git diff --check.
No code rebuild, user app launch/closure, upstream edits, push or merge. Next:
independent exact-commit verification and remaining P1 authoring lifecycle work.
Existing Tools/__pycache__/ and log.txt remain untracked.


## Windows long-leaf staging copy repair - 4 October 2026

Base 93dfd33; implementation is the commit containing this checkpoint. Changed
EngineBridge/src/BuildStageService.cpp, Tests/BuildStageTests.cpp and status docs.
Windows staging copy and digest use explicit extended absolute native paths at
local I/O boundaries (drive and UNC forms); returned paths/manifests retain their
portable existing representation. Approved-plan checks, source symlink rejection,
no-overwrite copy and post-copy digest comparison remain in place. Copy failures
now include OS error value/message. No upstream, controller or serialized change.

Regression stages a 312-character leaf path with legal <=96-character components,
checks identical SHA256 and absence of extended prefix in portable manifest,
validates the complete stage and cleans through an extended-path fixture root.
Original copy call reproduced failure (OS error 3), exit 1; corrected call passes.
Initial fixture used an overlong component rejected by Gate 1; corrected to legal
components. Initial long-fixture cleanup threw; corrected cleanup preserves test
failure reporting. First manual test invocation used relative document paths;
corrected to absolute fixture path. Final results below supersede these attempts.

VS18 MSBuild with CL=/MP4, /m:2 /nologo /verbosity:quiet /p:Configuration=Release
/p:Platform=x64 /p:BuildProjectReferences=false: EngineBridge/RenegadeEngineBridge,
RenegadeBuildStageTests, Studio/RenegadeStudio all exit 0. Studio uses
/p:TargetName=RenegadeStudioLongPath to preserve running owner editor. Logs:
BUILD/renegade/longpath-final-<target>.log. Explicit VS18 CTest --test-dir
BUILD/renegade -C Release -R '^RenegadeBuildStageTests$' --output-on-failure:
1/1 passes, 0.23 seconds; covers existing collision/tamper/stale-source checks.
git diff --check passes. Existing MSB8029 warnings. No new Debug/full-suite/CI
or independent verification claim. No app launched or closed and no push/merge.

Next owner saves/closes existing Studio, then launches Release
RenegadeStudioLongPath.exe and opens original deep-root AssemblyProof.renegade
for actual BUILD WINDOWS GAME retry. Short-root successful export remains valid;
new original-path UI/export acceptance is still pending. This is a scoped staging
file-copy/hash repair, not universal long-path support across every engine API.
Existing Tools/__pycache__/ and log.txt preserved untracked.


## Correct long-path package integrity beyond staging - 4 October 2026

Base 0014527. Owner retest of original deep-root project overrides earlier staging
success: copy passed but package validation rejected the same texture as missing.
Prior fix/test scope was incomplete. Shared private WindowsFileIoPath.h now owns
local extended path conversion for staging and integrity. Integrity probes and
canonical containment use the same extended representation; digest retains the
absolute-input requirement. Public result root remains ordinary path. Existing
symlink, traversal, duplicate, extra, missing and tamper checks remain authoritative.

Changed BuildStageService.cpp, new WindowsFileIoPath.h, PackageIntegrityService.cpp,
BuildStageTests.cpp, PackageIntegrityTests.cpp and status docs. Long fixture now
runs actual Gate 4 integrity after stage validation; fake-executable fixture
upgrades only package manifest schema to 2, without claiming real Gate 3 identity.
PackageIntegrityTests --validate-package <absolute-candidate> provides read-only
real artifact verification. Initial tests exposed canonical prefix normalization
and Gate 2 versus Gate 3 schema mismatch; fixed both. Final two tests pass.

VS18 MSBuild with CL=/MP4, /m:2 /nologo /verbosity:quiet /p:Configuration=Release
/p:Platform=x64 /p:BuildProjectReferences=false: EngineBridge/RenegadeEngineBridge,
RenegadeBuildStageTests, RenegadePackageIntegrityTests, Studio/RenegadeStudio all
exit 0. Studio /p:TargetName=RenegadeStudioLongPathComplete preserves running app.
Logs BUILD/renegade/longpath-integrity-final-<target>.log; validator rebuild log
longpath-actual-validator-build.log. Explicit VS18 CTest --test-dir BUILD/renegade
-C Release -R 'RenegadeBuildStageTests|RenegadePackageIntegrity'
--output-on-failure: 2/2 pass, 0.35s. Includes 312-character staged file integrity
and existing package rejection tests. git diff --check passes. MSB8029 remains.

Read-only validator of owner's actual failed candidate
Assembly Proof Windows Build.studio-1102be708a10-67ac: PASS, 35 files, exit 0.
Launched actual staged Assembly Proof.exe from detached temp working directory
with dx12 --flow-outcome=next --renegade-smoke-autoplay --renegade-smoke-exit:
exit 0, RuntimeBootstrap evidence status PASS, package_integrity PASS, DX12 STARTED,
smoke_status PASS, terminal Complete Game. Initial manual smoke omitted required
next outcome and exited 27 flow_not_complete; corrected invocation above passed.
Owner warned before automatic test window. No owner level changes, promotion,
upstream changes, push or merge. Full suite/Debug/independent verification not claimed.

New Studio SHA256 FEBE6A58506F8B8B9E3144AE590FAC545C6D6190CC3D6031CE62E642664EFFBE.
Next owner saves/closes current Studio, launch RenegadeStudioLongPathComplete,
then original deep-root project actual Build Windows Game UI retry. Original-path
promotion and owner UI acceptance still pending; short-root acceptance preserved.


## Original deep-root Studio export success - 4 October 2026

Implementation cf6bd66. Owner supplied screenshot after using verified
RenegadeStudioLongPathComplete: BUILD COMPLETE, 23 seconds, original project
Builds/Windows output. Read actual promoted build-report: status
 gate5_validated_for_final_path, stage_only=false, package_isolation and smoke
passed_gate4, safe_rebuild passed_gate5, directory-rename promotion. Read-only
RenegadePackageIntegrityTests --validate-package <original-final-output> passes
35 files, exit 0. Deep-root export blocker is now verified resolved through the
actual Studio workflow. Prior short-root standalone visual acceptance retained;
no new manual foreground visual test inferred from Build Complete screenshot.
Configured report revision still 6562c812 (stale configure metadata), so independent
exact-commit verification remains outstanding. distribution_ready=false remains.
No code changes, app launch/closure, push or merge in this acceptance step.
Status docs updated; git diff --check. P1 authoring lifecycle and gameplay work
remain as previously documented. Existing untracked files preserved.

## Assembly save/update and draft history - 4 October 2026

Implementation: 419fa655484945cf8543eeb8d2b8c9840368b0fc on
feature/p1-first-person-arms-rig. Local commit only; no push/merge.
The following documentation checkpoint records this implementation.

Changed: FirstPersonAssemblyService.h/.cpp, FirstPersonAssemblyEditor.cpp,
StudioApplication.h/.cpp, FirstPersonAssemblyGraphicsProof.cpp,
FirstPersonAssemblySettingsTests.cpp; README, architecture, roadmap, feature ledger
and P1_ASSEMBLY_AUTHORING documentation.

SAVE CHANGES rebuilds the assigned assembly with the same product/recipe IDs and
paths. Player Start assignment is preserved. SAVE AS NEW creates and assigns a
variant through the existing scene command. Native draft UNDO/REDO snapshots
parts, parent, weapon/view transforms and clip pairs, including incomplete
selections; save marks the history boundary. Updating requires the original
registered and disk product hash. All replacements use ProjectDocumentTransaction.
Opening the panel refreshes retained assets after dependency-only build scans.
Unrelated stale/missing import provenance is preserved rather than requiring
every historic import to be current.

Windows x64 Release validation (installed MSBuild; CL=/MP4):
- MSBuild BUILD/renegade/EngineBridge/RenegadeEngineBridge.vcxproj
- MSBuild BUILD/renegade/Tests/RenegadeFirstPersonAssemblySettingsTests.vcxproj
- MSBuild BUILD/renegade/Tests/RenegadeFirstPersonAssemblyWorkflowProof.vcxproj
- MSBuild BUILD/renegade/Studio/RenegadeStudio.vcxproj with
  /p:TargetName=RenegadeStudioAssemblyLifecycle
All use /m:2 /nologo /verbosity:quiet /p:Configuration=Release /p:Platform=x64
/p:BuildProjectReferences=false. All exit 0; known MSB8029 warnings remain.
- ctest --test-dir BUILD/renegade -C Release -R
  "FirstPersonAssemblySettings|MatchingRigAnimation|RenegadePlayerViewRig"
  --output-on-failure: 4/4 pass, final run 0.33s.
- BUILD/renegade/Tests/Release/RenegadeFirstPersonAssemblyWorkflowProof.exe
  BUILD/assembly-lifecycle-project-final/AssemblyProof.renegade
  BUILD/assembly-lifecycle-proof-final --update-assembly: exit 0.
  Disposable owner-project copy only. Same product/recipe IDs, exact reopen,
  injected AfterReplace failure and byte rollback, stale original hash rejection,
  separate Save-as-new ID and unchanged level bytes pass. Updated product passes
  production Test Level snapshot and paired Runtime pose/pause/cleanup checks.
- Same proof executable with BUILD/assembly-lifecycle-proof-final/isolated-package
  BUILD/assembly-lifecycle-proof-final/cold --runtime-package: exit 0, 2.31s.
  Fresh-process packaged native loading and paired Runtime checks pass.
- Updated-assembly render inspected: textured arms/shotgun present.
- git diff --check passes.

New Studio SHA256:
778E3DE0E32E09E4C2756B2A5DE5BB078D683AAC3A1D02028311F42057E3FABA
Executable: BUILD/renegade/Studio/Release/RenegadeStudioAssemblyLifecycle.exe.
Opened this alternate build for owner checking; older LongPathComplete editor
was not closed or overwritten. Owner original project was not changed by proofs.

Next: owner checks offset Undo/Redo, UPDATE PREVIEW, SAVE CHANGES and panel reopen,
then unique Copy name + SAVE AS NEW and level save. New UI acceptance and
independent exact-commit verification remain pending. P1 gate remains open.
Runtime paired crossfades, reload/fire gameplay and general texture-relink UI
remain outside this slice.

## Complete shotgun preview library and automatic refresh - 4 October 2026

Implementation: 34297443c03c986935690e4fc1dd53886d94d0d6 on
feature/p1-first-person-arms-rig. Local commit; no push or merge.
Owner accepts prior 419fa65 lifecycle UI: "yup, all that works as expected".
Owner requested live offset feedback because the cleared image looked broken,
then chose "Complete animation library" as tonight's goal.

Changed: FirstPersonAssemblyService.h/.cpp (shared 14-action whitelist);
FirstPersonAssemblyEditor.cpp, StudioApplication.h/.cpp (three six-row action
pages, 150ms debounced safe-point rebuild, retain last valid image until ready,
block stale draft saves and preserve preview action/time/play state);
FirstPersonAssemblyGraphicsProof.cpp and SettingsTests.cpp; README, architecture,
roadmap, feature matrix and P1 assembly authoring instructions.

Exact Windows x64 Release build flags:
CL=/MP4; MSBuild /m:2 /nologo /verbosity:quiet /p:Configuration=Release
/p:Platform=x64 /p:BuildProjectReferences=false.
Targets: BUILD/renegade/EngineBridge/RenegadeEngineBridge.vcxproj;
BUILD/renegade/Tests/RenegadeFirstPersonAssemblySettingsTests.vcxproj;
BUILD/renegade/Tests/RenegadeFirstPersonAssemblyWorkflowProof.vcxproj;
BUILD/renegade/Studio/RenegadeStudio.vcxproj with
/p:TargetName=RenegadeStudioFullArmsLibraryReady. All exit 0.
Known MSB8029 and unrelated Studio nodiscard warnings remain.
CTest --test-dir BUILD/renegade -C Release -R
"FirstPersonAssemblySettings|MatchingRigAnimation|RenegadePlayerViewRig"
--output-on-failure: 4/4 pass, 0.45s. Includes all 14 action recipe roundtrip.

Manual proof executable:
BUILD/renegade/Tests/Release/RenegadeFirstPersonAssemblyWorkflowProof.exe
- BUILD/renegade/p1-shotgun-proof/input BUILD/arms-full-library-ready
  --full-library: PASS. 14 arms clips and 4 weapon clips, native matching-rig
  ingestion, unchanged skin-index/inverse-bind evidence, retained-source recipe
  reconstruction and governed commit/reopen. Descriptive native clip names
  survive retention instead of displaying 18 generic "Unreal Take" labels.
- BUILD/arms-full-library-ready/ArmsLibrary.renegade
  BUILD/full-arms-ready-cold-preview --full-library-reopen: PASS.
  14 semantic pairs, 28 tracks, two armatures and ten valid retained textures.
  Every action rendered at start/midpoint/near end; draw/holster legitimately
  permit out-of-view frames. Final build/import/cold proof chain exit 0, 36.73s.
- Same descriptor BUILD/full-arms-ready-runtime --runtime-assembly: PASS.
  Production Test Level snapshot, paired movement pose/pause/cleanup.
- BUILD/full-arms-ready-runtime/isolated-package
  BUILD/full-arms-ready-runtime/cold --runtime-package: PASS in a fresh process.
  Final launch and Runtime proof chain exit 0, 19.43s.
- Rendered aim, fire, sprint, jump, draw/holster, reload and partial reload poses
  visually inspected. Native UI inspection caught overlapping labels caused by
  Window child visibility propagation; selected page visibility is now enforced
  each frame. Final native first-page layout has six distinct labelled rows.
- Native offset changed temporarily: prior image remained during "Updating
  preview...", then the new offset rendered without pressing UPDATE PREVIEW.
  Undo was used to restore the original draft. No assembly save was performed.
- git diff --check passes.

Ready Studio: BUILD/renegade/Studio/Release/RenegadeStudioFullArmsLibraryReady.exe
SHA256 D807AFEDB336EBD763FF157C708C19A7922901CA39978AA7D4C7AC6CD95292E3.
Ready fixture: BUILD/arms-full-library-ready/ArmsLibrary.renegade
Project ID 3720e96a-6cf6-48f9-8988-c5744445a95a.
Assembly ID 07858b1d-4b5a-4be9-90ab-c9dd5629761c.
Existing level was copied as template into the new fixture; owner original
project and prior accepted executable were not overwritten.
Our preliminary full-library editor was closed after inspection; the owner's
AssemblyLifecycle editor was left open. Corrected Ready editor has the new
project loaded, Player Start selected, assembly preview open.

Next owner check: use Preview action + PLAY for the 14 supplied actions; check
small offsets refresh automatically and page selection is clean. Aim/jump,
equip/fire/reload remain authoring previews; only Idle/Walk/Run are wired to
existing gameplay movement. Input-driven equipment/actions, damage/ammo/recoil
and paired crossfades remain later roadmap work. New native UI owner acceptance,
actual full-library Build Game export and independent exact-head verification
remain pending. P1 gate remains open.


## Runtime fire/reload animation input - 4 October 2026

Implementation: 090e589d96cee00408e5c238dc3ca732705b116f on
feature/p1-first-person-arms-rig. Local only; no push or merge.
Owner requested left mouse fire and R reload; confirms "yep, that works"
after the rebuilt Runtime was opened on the full-library Test Level snapshot.

Changed GameplayInputService.h/.cpp, RuntimeApplication.cpp,
RuntimePlayerViewAnimation.h, RuntimePlayerViewRig.h; PlayerViewRigTests.cpp,
Phase6Gate2InputTests.cpp and Tests/CMakeLists.txt; README, architecture, roadmap,
feature matrix and assembly authoring docs. Input tests were previously not
registered; now registered and included in the CI bridge test aggregate.

Fire is a discrete left mouse press; Reload is R; Reset is F8. Old version-1
input maps gain Fire/Reload defaults in memory and move the old default R reset
to F8. Custom bindings remain authored. Paired Attack/Reload plays once on the
existing shared native clock, shorter tracks hold, Reload wins simultaneous
presses, busy actions reject retriggers, pause freezes time, and completion
returns to current movement. Missing assigned pairs do not imitate firing with
Idle. Damage, ammunition, audio, effects and recoil remain later combat work.

Owner screenshot caught stale standalone Runtime: old binary rejected new
assembly actions with "Unknown assembly action" and displayed proxy blocks.
Rebuilt Runtime now loads the governed full-library asset successfully.
The earlier bootstrap proof linked updated headers but did not establish that
the actual Studio-launched executable was current; native launch supersedes
that earlier evidence.

Build commands: CL=/MP4; MSBuild /m:2 /nologo /verbosity:quiet
/p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false.
Targets BUILD/renegade/EngineBridge/RenegadeEngineBridge.vcxproj,
BUILD/renegade/Tests/RenegadePlayerViewRigTests.vcxproj,
BUILD/renegade/Runtime/RenegadeRuntime.vcxproj and
BUILD/renegade/Tests/RenegadeGameplayInputTests.vcxproj. All exit 0.
CMake -S . -B BUILD/renegade configured the new input test.
CTest --test-dir BUILD/renegade -C Release -R
"GameplayInputTests|RenegadePlayerViewRig" --output-on-failure: 3/3 PASS, 0.75s.
Regressions cover legacy binding migration and input persistence, paired action
priority, one-shot completion, short-track hold, pause, retrigger rejection,
second-shot restart and return to movement. git diff --check passes.

Copied new Runtime executable to Studio's existing Release/Runtime launch path.
Both binaries SHA256:
A02504741931FF039730A6BB77F50D2A0195E3B6D4401B46537CBDB8F2F9994C.
Native standalone DX12 launched the same TestLevel.renegade snapshot from
BUILD/arms-full-library-ready/Intermediate/TestLevelSnapshots.
Runtime log shows loaded governed first-person arms asset and no load error.
Native R key check rendered reload with aligned hands, gun and shells instead
of resetting. Owner independently accepts the controls on this build.
The existing Ready Studio was left open; updated Runtime was opened for testing.
No authored scene/assembly change was needed.

Next: broader action routing (aim/equipment/jump) and later actual combat systems.
Current accepted slice is input-driven animation, not a complete firearm.
Full-library actual Build Game export and independent exact-commit overall P1
verification remain pending; no release gate is closed.


## Two-shell shotgun and partial reload - 4 October 2026

Implementation b52e4804d5b8263fb034b7783f03535274113f73, local only.
Owner requested firing to stop after two shots and partial reload after one.
Changed RuntimePlayerViewAnimation.h, RuntimePlayerViewRig.h,
PlayerViewRigTests.cpp, README, architecture, roadmap, assembly authoring docs
and feature matrix. Existing player, native pair clocks and input service remain.

The bounded shotgun prototype initializes two shells. An accepted Attack
consumes one only after its explicit native pair is successfully requested.
Empty fire is ignored. R with one shell uses ReloadPartial; empty uses Reload;
full-capacity reload is ignored. A missing partial pair may use an assigned full
reload. Reload completion restores two, with no early refill, pause refill or
busy action consumption. Reset/reinitialization restores the count.
Reserve ammo, creator weapon definitions, HUD and damage remain later work.

CL=/MP4; MSBuild /m:2 /nologo /verbosity:quiet /p:Configuration=Release
/p:Platform=x64 /p:BuildProjectReferences=false:
BUILD/renegade/Tests/RenegadePlayerViewRigTests.vcxproj and
BUILD/renegade/Runtime/RenegadeRuntime.vcxproj exit 0.
CTest --test-dir BUILD/renegade -C Release -R
"GameplayInputTests|RenegadePlayerViewRig" --output-on-failure: 3/3 PASS, 0.71s.
Tests prove two-shot exhaustion, blocked third shot, full refill at completion,
one-shot partial pair selection, frozen paused reload, full-capacity rejection
and existing movement/busy action regressions. git diff --check passes.

Copied Runtime to the actual Studio Release/Runtime launch location.
SHA256 4D8FD0FBBBB466512D29C5366C3655A23ED23D4D6331E076D03156DAFA3448F9.
Fresh native DX12 launched the same full-library Test Level snapshot.
Left mouse once, then R visibly renders the partial reload with one shell
being handled; native screenshot inspected. Updated Runtime is open for owner
checks, existing Studio left open. No scene/assembly save was needed.
Owner acceptance of this new capacity/partial reload build is pending.
No gate closure, push or merge. Next check: two shots then blocked third, R full
reload; one shot then R partial; both restore two usable shots.


## Right-mouse aiming - 4 October 2026

Implementation ec86f172a413b965916b6f2de52058d51492589e, local only.
Owner confirms prior two-shot limit and partial/full reload were already checked.
This slice adds Aim input on right mouse hold, with backward-compatible default
for existing version-1 input documents. Existing Player View controller recognizes
explicit AimIn/AimOut/AimAttack track pairs. Hold plays AimIn once then retains
its final pose; release plays AimOut then movement resumes. Aimed fire consumes
the same two-shell count. Reload lowers sights and resumes AimIn if still held.
Busy actions finish before the next hold/release transition; pause freezes them.
No FOV/camera change, ammo reserve, damage or crossfade scope is claimed.

Changed GameplayInputService.h/.cpp, RuntimeApplication.cpp,
RuntimePlayerViewAnimation.h, RuntimePlayerViewRig.h, input and paired animation
tests, README, architecture, roadmap, authoring docs and feature matrix.
Exact Release build flags: CL=/MP4; MSBuild /m:2 /nologo /verbosity:quiet
/p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false.
Targets EngineBridge/RenegadeEngineBridge, Tests/RenegadePlayerViewRigTests,
Tests/RenegadeGameplayInputTests, Runtime/RenegadeRuntime under BUILD/renegade.
CTest --test-dir BUILD/renegade -C Release -R
"GameplayInputTests|RenegadePlayerViewRig" --output-on-failure.
Regressions cover aim-in timing and terminal hold, pause, aimed fire ammo,
release queued during firing, return to movement and reload resuming held aim.


All four Release targets exit 0; build/test chain 196.73s.
CTest 3/3 PASS, 0.78s; git diff --check passes.
Deployed Runtime to Studio Release/Runtime launch location, SHA256:
0430928D63B4CBDE6AE572EB7ACBF43AAF0E8CDF9B0D94A9E7FCEAAFDEA4858E.
The prior temporary snapshot had been cleaned up; direct old-path launch failed.
Closed that failed launch and used the real Studio PLAY control to create a new
snapshot and launch the updated Runtime. Correct full-library project stayed open.
Native right mouse hold visibly centers the shotgun in the authored sight pose;
left fire then release returns to the hip stance. Both captures visually checked.
Current Runtime and existing Ready Studio left open for owner review.
No project, scene or assembly edits/saves needed. New aim owner acceptance pending.
No gate closure, push or merge. Next: owner aim/reload feel check, then further
equipment/jump routing or weapon definition work within the canonical roadmap.

## 2026-10-05 Equipment, jump and view-model collision correction

Implementation commit: f87795a13ebb2fc16dfc2a38c8b00cef65f1809c.
Owner accepted prior aim behavior and requested Equip, Unequip and all jump stages.
Q toggles authored paired equip/holster clips, terminal holster holds hidden,
and holstered firing/reload/aim are blocked without losing shell count.
Space uses the existing capsule jump input; Jolt ground support drives paired
JumpStart, JumpLoop and JumpLand. Pause freezes action progression.

Changed GameplayInputService.h/.cpp, RuntimeApplication.cpp,
RuntimePlayerViewAnimation.h, RuntimePlayerViewRig.h, RuntimePlayerViewAsset.h,
FirstPersonAssemblyGraphicsProof.cpp, Phase6Gate2InputTests.cpp,
PlayerViewRigTests.cpp, README, architecture, roadmap, assembly authoring docs
and feature matrix. The private playground is a separate descriptor/scene under
BUILD/arms-full-library-ready, retaining the real arms assignment. Original
ArmsLibrary scene remains untouched. Native save/reopen checks retain the floor
rigid body and Player Start assignment.

Owner reported bouncing followed by endless falling. Real physics trace
reproduced spontaneous sideways/upward impulses before jump input. Imported
humanoid ragdoll colliders were pushing the authoritative capsule. View-model
sanitization now disables ragdolls and removes soft-body physics alongside
character/rigid-body/collider components. Regression checks cover the retained
humanoid being disabled. No Wicked source or submodule change.

Exact Release build flags: CL=/MP4; MSBuild /m:2 /nologo /verbosity:quiet
/p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false.
Final corrected Runtime and PlayerViewRigTests target build chain exit 0, 24.76s.
CTest --test-dir BUILD/renegade -C Release -R
"GameplayInputTests|RenegadePlayerViewRig" --output-on-failure:
3/3 PASS, 0.61s.
Final FirstPersonAssemblyWorkflowProof target build and executable:
BUILD/renegade/Tests/Release/RenegadeFirstPersonAssemblyWorkflowProof.exe
BUILD/arms-full-library-ready/ArmsLibrary.renegade
BUILD/arms-full-library-ready/Content/Scenes --jump-playground
exit 0, 14.80s. Real Jolt takeoff/airborne/landing stages pass and the final
capsule remains supported, stationary, at the floor, with no horizontal drift.
Earlier failing diagnostic builds were corrected before this result.
git diff --check passes.

Deployed Runtime at Studio Release/Runtime launch location, SHA256:
DE42ACB21368353D291230B2A198E0E5D60254470C3F6583A9D1D7EE907CB04A.
Warned before replacing the previous Runtime; existing Ready Studio stays open.
Corrected Shotgun Arms Playground DX12 Runtime left open. Native Q holster/equip
and Space start/loop/land captures taken; final grounded floor and arms visually
inspected. Owner review of this corrected build pending.
Lighting in this diagnostic playground is bright; polish remains separate.
No overall release gate closure, push or merge. Next: owner confirm corrected
jump stability and equipment behavior, then weapon definition work per roadmap.

## 2026-10-05 Editable assembly firearm settings

Implementation commit: 4a28bcdd3fe5245bb9e7f1e21e07a0834ebcd70c.
Owner confirmed previous equipment/jump fix works. Continued with one bounded
outcome: persisted capacity, minimum shot interval, and partial reload policy.
Changed bridge FirearmSettings/assembly service, native Studio assembly panel,
Runtime paired animation controller, settings/rig/native workflow tests, README
and canonical architecture/roadmap/feature matrix/assembly documentation.
New docs/PLAYER_AUTHORING_CONTINUATION.md records remaining player authoring work.
Legacy assemblies retain capacity 2, interval 0, partial reload enabled.
Settings remain shared by assembly; independent equipment assets and ammo reserve
are future work. Reload still requires an assigned action.

Release x64 build chain: bridge, assembly settings tests, player view rig tests,
Runtime, Studio, assembly workflow proof: exit 0, 160.69s.
MSBuild flags: CL=/MP4 /m:2 /nologo /verbosity:quiet
/p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false.
CTest --test-dir BUILD/renegade -C Release -R
"FirstPersonAssemblySettings|GameplayInputTests|RenegadePlayerViewRig"
--output-on-failure: 4/4 PASS, 0.82s.
BUILD/renegade/Tests/Release/RenegadeFirstPersonAssemblyWorkflowProof.exe
BUILD/arms-weapon-settings-proof/ArmsPlayground.renegade
BUILD/weapon-settings-evidence --update-assembly
then same executable BUILD/weapon-settings-evidence/isolated-package
BUILD/weapon-settings-package-evidence --runtime-package:
combined exit 0, 36.78s. Same ID update, exact reopen, stale rejection, rollback,
save-as-new, native snapshot and isolated package paired runtime load pass.
Proof profile capacity=4, interval=1.25, partial=false; original fixture untouched.
Native Studio Weapon Settings visually inspected; numeric edit commits with
physical Enter and Undo restores capacity 2. Automated instantaneous SendKeys
Enter was missed by frame polling. Preview refreshes on edits.
Standalone Runtime launched from BUILD/renegade/Runtime/Release with --project
BUILD/arms-weapon-settings-proof/ArmsPlayground.renegade dx12.
Grounded arms/playground visually inspected. Private native input scripts exercise
fire/reload; exact capacity/cooldown/partial behavior verified by rig tests.
Diagnostic playground lighting remains washed out as previously documented.
Initial deployment-location launch exited early; direct Runtime build works.
Do not count initial stale screenshots as evidence.
git diff --check passes. No release gate closure, push, or merge.
Next: owner review native settings, then reusable player inspector/prefab work.

## 2026-10-05 Selected Player Start wireframe capsule

Implementation commit df95fe31d9c1923895208144a61331d8570ba0e4.
Owner explicitly requested the concept-art wireframe capsule. Studio Render()
now queues an orange wireframe only for a selected Player Start, before the
normal 3D render and after assembly preview rendering. Uses sanitized bridge
controller settings and PlayerCapsuleTotalHeight; feet are world translation.
Wicked DrawCapsule expects outer base/tip, so top is feet + total height.
Upright/unscaled matches runtime player policy. Overlay is transient and absent
during Test Level and project hub; no scene objects or persistence change.
Changed StudioApplication.cpp, architecture, assembly authoring, continuation
documentation and Player Start feature-matrix row.

Release Studio build:
MSBuild BUILD/renegade/Studio/RenegadeStudio.vcxproj /m:2 /nologo
/verbosity:quiet /p:Configuration=Release /p:Platform=x64
/p:BuildProjectReferences=false with CL=/MP4.
Exit 0, 18.67s; existing MSB8029 and unrelated nodiscard warning only.
git diff --check passes. Native DX12 build copied to
BUILD/renegade/Studio/Release/RenegadeStudioPlayerCapsule.exe and opened.
Loaded ArmsLibrary through hub and Story Flow, selected Player Start.
Orange rounded capsule visually inspected at spawn alongside native gizmo.
Owner review of size editing and Undo/Redo remains pending; drawing reads live
settings every frame. No new serialized state or gameplay behavior in this change.
Existing editor/runtime processes were retained. New capsule build left open.
No overall release gate closure, push or merge.
Next: owner review capsule, then reusable player inspector/prefab work.

## 2026-10-05 Capsule camera-motion display repair

Owner reports capsule visually splitting while navigating and suggests 3D asset.
Implementation ff4b3b875551d9f5ee58c431d229e0e4c7549e49 replaces queued
DrawCapsule debug-world rendering with connected capsule geometry projected
during Studio Compose beside gizmo, after temporal scene postprocessing.
Wicked wiRenderPath3D.cpp draws debug world before Postprocess_TemporalAA;
thin debug lines accumulating through temporal history are the likely cause.
New display uses 48-segment rings, eight meridians, smooth hemispherical caps,
1.5 logical-pixel orange lines. Reads live controller dimensions and spawn
translation. Clips camera planes and viewport; hides during hub/Test Level and
assembly/grip workspaces. No serialized object or imported mesh asset added.
Changed StudioApplication.cpp and existing capsule docs/feature matrix.

Same Studio Release x64 MSBuild command/flags as prior capsule handoff:
exit 0, 19.04s. Existing warnings only. git diff --check passes.
Native BUILD/renegade/Studio/Release/RenegadeStudioStableCapsule.exe opened.
ArmsLibrary.renegade original fixture opened directly via native project dialog.
Static selected rounded capsule visually inspected, approximately 75 FPS.
Automated motion captures repeatedly lost Player Start selection and cannot
verify the owner's reported moving-capsule defect. Owner movement review pending;
do not claim this motion behavior passed based on unrelated captures.
Build left open on ArmsLibrary. Existing editors and Runtime retained.
No persistence or gameplay changes; no overall gate closure, push or merge.
Next: owner select Player Start and navigate to verify connected capsule,
then continue reusable player inspector/prefab setup.

## 2026-10-05 Window resize input alignment repair, owner accepted

Implementation 4a38305584e779131a4b21fed287b98673c78314.
Owner reported hierarchy header only toggles at top and viewport icons have
large left offset. Window procedure skipped SetWindow on background resize/DPI
change. Stale swapchain/canvas could stretch the displayed editor while pointer
coordinates follow current client size. Refresh now runs even while inactive
(except minimized), and on focus recovery. Changed main_Windows.cpp and
architecture. Temporary click-coordinate probe was removed before final build.

Studio Release x64 MSBuild BUILD/renegade/Studio/RenegadeStudio.vcxproj
/m:2 /nologo /verbosity:quiet /p:Configuration=Release /p:Platform=x64
/p:BuildProjectReferences=false with CL=/MP4: exit 0, 18.17s.
git diff --check passes.
Opened BUILD/renegade/Studio/Release/RenegadeStudioCoordinateFix.exe DX12.
Reopened ArmsLibrary original fixture. Hub centre Open Project button works.
Native hierarchy lower/header-centre clicks toggle Characters; Player Start
row selects with Inspector/capsule. Native capture inspection confirms layout
no longer stretched. Initial viewport-icon script used stale position after
camera movement; do not count that capture as successful icon evidence.
Owner explicitly reports "that works fine now, thanks" at 10:25 UK time,
confirming reported selection alignment repair. Corrected build left open.
No serialized or gameplay state changes. No overall release gate closure,
push or merge. Next: continue player inspector/prefab authoring per continuation doc.

## 2026-10-05 — Always-visible player capsule
Implementation commit: `0fa05df1cb59d9f06116922b66459611b471556e`.
Owner requests the capsule remain visible without selection, including future full player prefabs.
Changed Studio/src/StudioApplication.cpp, docs/ARCHITECTURE.md and docs/FEATURE_MATRIX.csv.
Compose resolves the governed Player Start independently of selection, respects hierarchy visibility, draws cyan normally/orange selected, and uses the same live controller dimensions. Removed the duplicate older selected-only ring guide from scene-icon handling.
Validation: `powershell -NoProfile -ExecutionPolicy Bypass -File BUILD/build_capsule.ps1` builds Windows x64 Release Studio with MSBuild /m:2 /p:BuildProjectReferences=false, exit 0, 18.71 seconds; existing MSB8029 and C4834 warnings. `git diff --check` passes.
Native DX12 visual inspection: reopened ArmsLibrary, unselected cyan capsule visible with empty Inspector (BUILD/capsule-unselected.png); selected orange capsule visible with Player Start Inspector (BUILD/full-arms-window.png). Correct build RenegadeStudioAlwaysCapsule.exe remains open. Previous editor retained.
No serialized or Runtime changes, so save/reload and standalone gameplay checks are not applicable. Prefab-backed governed starts inherit this display; reusable player prefab authoring remains future work. Owner acceptance and overall release gate remain pending. Next: reusable player prefab setup.

## 2026-10-05 — Capsule selection replaces Player Start icon
Implementation commit: `567b9ecdcfda276619a1c504da4f6700350af973`.
Owner accepted always-visible capsule and requested selecting it directly plus removal of the redundant Player Start icon.
Changed Studio/src/StudioApplication.cpp/.h, Studio/src/MarkerIconOverlay.cpp, docs/ARCHITECTURE.md and docs/FEATURE_MATRIX.csv. Camera pick ray intersects native upright capsule using the same feet/total-height/radius as Compose. Interior and wire edges select the governed entity and refresh Inspector/hierarchy/gizmo. Removed Player Start billboard emission and ground arrow. Other marker kinds retain their existing workflow.
Validation: `powershell -NoProfile -ExecutionPolicy Bypass -File BUILD/build_capsule.ps1`, Windows x64 Release Studio MSBuild /m:2 /p:BuildProjectReferences=false, exit 0 in 63.54 seconds; existing MSB8029/C4834 warnings. `git diff --check` passes.
Native DX12 opened ArmsLibrary through Hub/Story Flow: cyan unselected capsule with no Player Start icon/arrow (BUILD/capsule-only-unselected.png). Clicked capsule interior at screen 950,540: orange selection, Player Start hierarchy row and Inspector, transform gizmo visible (BUILD/full-arms-window.png), visually inspected PASS. Adjacent light icon remains. RenegadeStudioPickCapsule.exe remains open, earlier editors retained.
No serialized or gameplay changes: save/reload and standalone tests not applicable. Capsule picking intentionally follows editor guide semantics without world-depth occlusion. Owner acceptance and independent release gate remain pending. Next: reusable player prefab authoring.


## 2026-10-05 — Reusable player prefab authoring and Save freeze repair
Implementation commit: `774e8e4d912944e06e5c6372a865cac472bd49c5`.
PlayerPrefabService adds strict versioned .rplayerprefab assets, transactional registry save/rollback, controller and arms defaults, undoable assignment, serialized origin/baseline, local override comparison and Reset. Spawn transform remains local. Studio adds selector, Save As Player Prefab, Reset and status in dynamic Inspector layout. Dependency provider discovers Scene -> prefab -> default arms; Test Level snapshots retain default arms even when local arms are NONE. Changed EngineBridge, Studio, Tests CMake/source and README/architecture/feature matrix/player continuation/roadmap; see commit for exact files.
Owner reported Save froze the editor at 11:05 UK. Windows Application events 1001 AppHangB1 / 1002 confirm RenegadeStudioPlayerPrefabFinal.exe hang and closure. Refresh used ComboBox::SetSelected, which invokes OnSelect and subscribed another apply during safe-point callback dispatch. Fixed using SetSelectedWithoutCallback; user selection still applies normally. Initial automated service checks missed this native UI defect. Initial prefab layout overlap also caught visually and repaired in S1BInspectorSectionMigration.
Build: BUILD/build_player_prefab.ps1 configures CMake and builds Bridge, PlayerPrefabTests, Studio, Runtime and FirstPersonAssemblyWorkflowProof Windows x64 Release; final full-target pass exit 0 / 27.82s. Later proof extension build exit 0 / 12.04s. Final Studio callback fix BUILD/build_capsule.ps1 exit 0 / 10.41s; existing MSB8029/C4834 warnings only. git diff --check passes.
Verification: BUILD/verify_player_prefab.ps1 exit 0 / 14.88s. Five CTests pass (0.49s): Phase6Gate1Player, PlayerViewRig, PlayerViewRigSourceContract, FirstPersonAssemblySettings, PlayerPrefabTests. Native GPU proof clone BUILD/arms-player-prefab-proof passes cold save/reopen, dependency closure, local NONE override retaining default arms in snapshot, real paired Runtime project/snapshot loading and cleanup, capacity-two weapon settings, isolated packaged scene/prefab defaults and paired arms. Evidence BUILD/player-prefab-evidence. Isolated package proof is not full Build Game staged/promotion acceptance.
Native corrected DX12 RenegadeStudioPlayerPrefabFinal.exe: opened original BUILD/arms-full-library-ready/ArmsLibrary.renegade, selected Player Start; Inspector controls no longer overlap. Save As Player Prefab completed, assigned Player Start // 3d4e8140 and remained responsive at 75 FPS. Changed capsule radius via slider -> LOCAL OVERRIDES and larger guide; Reset -> DEFAULTS/original dimensions. Clicked native SAVE then REOPEN; selected hierarchy Player Start and inspected persisted prefab/defaults/arms. Original project also retains asset a57a0cbf from owner's pre-fix save; no deletion performed. Corrected build remains open on saved Player Start. Earlier test direct Runtime grounded/fire/reload check passed, fixture lighting remains washed out; closed only that test Runtime to unblock relink.
Each save creates a new immutable reusable defaults asset. Live propagation, asset-browser drag placement, multi-weapon loadout and full Build Game promotion remain future work. No overall release gate closure, push or merge. Next: owner review corrected Save flow, then continue player prefab placement/inspector workflow.


## 2026-10-05 — PR 178 Studio CI repair
Implementation commit: `5a9caf1` (Align Studio CI with player browser workflow and build prefab tests), following player-browser implementation `ce10105`.
Inspected GitHub Studio run 37308122911: Debug and Release fail during CTest, after compilation; Windows baseline run 37308122891 passes both configurations. Failure set: Phase5Gate3SourceContract expects nine Add items including retired Player Start, Phase6Gate1SourceContract expects retired Player Start arrow/Add workflow, and RenegadePlayerPrefabTests executable is absent from the explicit CI build target set.
Preserved and completed existing local fixes in Tests/CMakeLists.txt (prefab test dependency of bridge test aggregate), Tests/Phase5Gate3SourceContract.cmake (eight Add entries), Tests/Phase6Gate1SourceContract.cmake (browser drop, capsule and prefab card requirements). Tools/Build-Studio-Windows.ps1 now explicitly builds RenegadePlayerPrefabTests and records that target in build evidence. No runtime/editor feature change in this repair.
Validation: BUILD/validate_prefab_ci.ps1 configures CMake -S . -B BUILD/renegade -A x64 -DRENEGADE_EMBED_SHADERS=ON, builds RenegadePlayerPrefabTests --parallel 4 in Debug and Release, then CTest -R 'RenegadePlayerPrefabTests|RenegadePhase5Gate3SourceContract|RenegadePhase6Gate1SourceContract' --output-on-failure in each configuration. Exit 0, 682.34s including Release bridge recompilation. Debug 3/3 pass (0.41s); Release 3/3 pass (0.38s). Existing MSB8029 and C4834 warnings only. Generated bridge-test project includes prefab-test reference. git diff --check passes. Full fresh hosted Studio workflow remains required; focused local passes do not claim full CI acceptance or release gate closure.
Next: push repair on existing feature branch and inspect new PR 178 Debug/Release jobs. Do not merge until required checks pass. Preexisting Tools/__pycache__ and log.txt remain untouched.

## 2026-10-05 Test Level gameplay input snapshot recovery

Resumed feature/p2-equipment-actions at c07877a with the existing uncommitted
P2 equipment persistence/UI candidate retained. Bounded outcome: restore native
Test Level startup on the previously rejected deeply nested validation project.
RuntimeBootstrap.log identified ProjectRejected (22): GameplayInput defaults were
absent from the snapshot; Runtime's journal .writing path exceeded Windows path
limits. Completed existing SnapshotGameplayInput candidate in
EngineBridge/src/TestLevelSnapshotService.cpp; snapshots copy validated authored
input bytes or generate defaults under a short temporary root and copy them.
Source project input is not changed. No Runtime/player architecture replacement.
Tests/TestLevelSnapshotRuntimeTests.cpp fixes a non-static Cleanup call and verifies
Runtime EnsureGameplayInputMap reads defaults without creating them, source has
no new default document, and custom bytes/mouse sensitivity remain unchanged.

Validation: Release bridge MSBuild then powershell -NoProfile -ExecutionPolicy
Bypass -File BUILD/p2_snapshot_build.ps1. Final chain exit 0 / 14.10s;
RenegadeTestLevelSnapshotRuntimeTests 1/1 PASS, 0.25s; Studio Release rebuilt.
Existing MSB8029 warnings only. git diff --check passes.
Native DX12: rebuilt Studio PID 40740, validation project opened via Hub and Story
Flow, PLAY launched Runtime child PID 23200. Bootstrap PASS/SUCCESS/exit_code=0.
Live diagnostics confirm same build identity, real Studio child, player spawned,
scene loaded, paired rig/animation initialized, two active Idle tracks and no
error events. BUILD/p2-snapshot-recovery-diagnostics.json records evidence.
BUILD/p2-snapshot-runtime-success.png visually inspected: shotgun/arms present,
75 FPS; existing washed-out fixture lighting remains. This is startup/rig evidence,
not new combat, gameplay feel, or equipment action acceptance.

Only snapshot source, snapshot regression and this handoff form the bounded
recovery commit. Other existing P2 changes remain uncommitted and preserved;
P2 implementation/feature matrix working copies updated with recovery status.
No push/merge or independent release-gate closure. Next: equipment dependency
closure in Test Level (snapshot inventory had no .requipment documents), then
semantic equipment Runtime routing and packaged gameplay acceptance per P2.

## 2026-10-05 P2 equipment persistence and snapshot closure checkpoint

Continued from 0d9e067. Preserved/completed the existing equipment asset and
Starting Equipment UI candidate. EquipmentAssetService journals immutable
.requipment assets; PlayerService retains primary/off-hand StableIds in WISCENE;
PlayerPrefabService schema v2 preserves slots with v1 compatibility. Registry,
asset catalogue and dependency provider expose equipment and presentation edges.
Studio creates definitions from governed assemblies and applies hand-compatible
loadouts with existing command history. Native UI save/reopen evidence for this
candidate was recorded by the preceding session in P2 implementation document.

New TestLevelSnapshotService closure copies resolved level equipment and prefab
default equipment, including displaced defaults, and reuses governed arms/product
texture closure for presentation. Copied definitions reload against the snapshot
registry. Invalid/missing equipment fails snapshot creation. Regression preserves
sword/shield prefab defaults and local two-hand bow override; snapshot prefab
cold load, unchanged definition bytes and invalid-loadout rejection pass.
FirstPersonAssemblyGraphicsProof --equipment-snapshot clones supplied project
Content and registry, saves a real shotgun definition, assigns it in memory,
creates a snapshot, cold-loads the definition and paired presentation, and reopens
native level loadout. Original creator project remains untouched by this proof.

Validation: BUILD/build_p2_snapshot_equipment.ps1 exit 0 / 10.93s; snapshot
CTest 1/1 PASS / 0.65s. BUILD/p2_snapshot_finish_build.ps1 rebuilt Release equipment
and prefab tests, Runtime and Studio: exit 0 / 17.71s. Focused CTest 4/4 PASS
(snapshot, prefab, EquipmentActionState, EquipmentAsset) / 1.31s.
BUILD/p2_equipment_proof.ps1 builds RenegadeFirstPersonAssemblyWorkflowProof and
runs supplied validation descriptor with --equipment-snapshot into
BUILD/p2-equipment-snapshot-evidence: exit 0 / 21.02s; EQUIPMENT SNAPSHOT PASS.
Build flags CL=/MP4, MSBuild /m:2 /verbosity:quiet /nologo
/p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false.
Existing MSB8029 warnings. No claims of new combat feel or full Build Game parity.

Owner reaffirmed approved Player Camera Preview inset at lower-right of scene
viewport. PLAYER_AUTHORING_CONTINUATION.md records world + equipped arms/weapon,
authored facing/eye height/FOV, preview-only paused gameplay. This inset is still
unimplemented; isolated Assembly preview and separate Runtime do not fulfill it.

Changed bridge equipment/player/prefab/catalogue/dependency/snapshot files,
Studio equipment panel/Inspector and CMake, equipment/prefab/snapshot/graphics
proof tests and CMake, plus changelog/architecture/roadmap/feature matrix/P2/player
continuation documents. Unrelated Tools/__pycache__ and log.txt remain untouched.
Next: semantic equipment Runtime ownership/action routing, then owner gameplay
and packaged acceptance. P2 and independent release gate remain open.

Final TestLevel native smoke: snapshot load ready and simultaneous attack/block plus lowering observed in events; strict automated script assertion interrupted by foreground input changes, so no clean editor-button parity claim. Sword grip lowered 2cm; owner visual clipping acknowledged.

Native mouse sequence PASS: four successive attacks while shield held, BUILD/sword-variants-native-events.json.

Final native mouse PASS: four selected full-charge releases, independent held block and low-charge quick tap. Evidence BUILD/directional-native-events.json. Alternate Studio Release build also passes. Runtime left open on the updated owner project.


## Sword/shield pose avoidance checkpoint - 2026-10-06
Implementation commit: 3160f690badea5fc45c92b39964bc86b3c6c2d7e.
Changed: FirstPersonAssemblyService.h/.cpp, FirstPersonHandAssemblyPreparation.h,
PlayerViewHandAnimation.h, new PlayerViewHandAvoidance.h, RuntimeLiveDiagnostics.cpp,
FirstPersonAssemblySettingsTests.cpp, PlayerViewRigTests.cpp, new
PlayerViewHandAvoidanceTests.h, SwordShieldPlayableProof.h, ARCHITECTURE and FEATURE_MATRIX.
Optional schema-v2 collision proxies persist through settings and generated native
metadata. Old v1/v2 content remains disabled by default. CPU native masked evaluation
plus bounded native CCD corrects the primary arm before world hierarchy/skinning.
Wrist orientation, bone lengths, off-hand isolation and zero-dt pose are tested.
Finite local/world and decomposition guards reject singular native IK; unresolved
contacts restore authored pose and increment diagnostics. No upstream changes.
Five focused Release CTests pass: PlayerViewRig, source contract,
FirstPersonAssemblySettings, EquipmentActionState, EquipmentAsset.
Commands: BUILD/collision_verify.ps1 builds bridge and proof/tests then runs
five CTests and the real-pack fixture. Final run exit0 / 33.11s; five CTests 0.85s.
BUILD/collision_final.ps1 rebuilds Bridge/Runtime and incremental alternate Studio,
then relaunches the player. Clean alternate Studio exit0 / 113.25s.
Native rotations preserve authored lengths and scale. A near-contact fallback
permits up to 4cm of shoulder-root translation within the authored total bound.
MSBuild /m:2 /verbosity:quiet /nologo /p:Configuration=Release /p:Platform=x64
/p:BuildProjectReferences=false with CL=/MP4. Existing MSB8029/C4834 warnings.
Collision fixture: RENEGADE_HAND_COLLISION=1
BUILD/renegade/Tests/Release/RenegadeFirstPersonAssemblyWorkflowProof.exe
BUILD/sword-ue-proof4 BUILD/sword-collision-proof9 --sword-playable.
Result: 965 corrected frames, zero unresolved; saved asset reopen and TestLevel closure pass.
Straight-chain finite pose and bone-length regression passes in PlayerViewRig.
Final Bridge/Runtime/alternate Studio refresh passed: collision_final.ps1 exit0 / 87.98s.
Final native mouse collision_focus.ps1 exit0 / 27.10s. Player Runtime remains open.
Native mouse proof BUILD/collision_focus.ps1 -> collision-native-events.json:
four charged directional releases, shield phase2, quick low-charge tap,
avoidance enabled, corrections nonzero, all observed unresolved counters zero.
Rendered four directional release poses inspected; native player idle image inspected.
Desktop SwordShieldTest replaced with collision-enabled proof4 project; old
blending-only project retained beside it. Existing launcher preserved. Player left open.
Risks: conservative box corners; discrete frame contacts can miss fast swept crossings;
no joint/pole limits; unreachable contacts can retain clipping via safe fallback.
This fixes presentation against the player's shield only. Damage, NPC/world collision,
stamina/parry, generic proxy editing UI and production combat acceptance remain open.
No release gate closure or universal weapon collision claim. Owner gameplay feedback
and independent exact-commit review are next; refine contact continuity if needed.

## Directional melee chaining checkpoint - 2026-10-06
Implementation commit: c2ffb4f64c046f8c123232ca621161880244d7b6.
Changed RuntimeApplication.cpp, RuntimeEquipmentLoadout.h, RuntimeLiveDiagnostics.cpp,
EquipmentAssetTests.cpp, new EquipmentMeleeChainTests.h, ARCHITECTURE.md and FEATURE_MATRIX.csv.
One follow-up can be prepared in the final 0.30s of a directional release or recovery.
Released requests expire after 0.75s of gameplay time; held requests carry their
direction and capped charge into the next charge. Existing authored action phases
remain authoritative. Shield ownership is independent. Cancel/reload/equip/item
replacement clear the queue; pause/nonfinite dt freeze progress.
Released queued charge is pinned through authored preparation/windup until dispatch.
The HUD exposes PREPARE NEXT and queued direction/charge. Damage is unchanged.

Verification: BUILD/chain_build.ps1 builds Release x64 Runtime, then
BUILD/chain_tests.ps1 builds RenegadeEquipmentAssetTests and runs
ctest -C Release --output-on-failure --timeout 30 -R "FirstPersonAssemblySettings|PlayerViewRig|Equipment"
from BUILD/renegade. Final build exit0 / 27.40s, all five focused CTests pass / 0.77s.
MSBuild /m:2 /verbosity:quiet /nologo /p:Configuration=Release /p:Platform=x64
/p:BuildProjectReferences=false; CL=/MP4. Existing MSB8029 warnings.
Tests include held/released queues, exact single dispatch, authored recovery,
preparation/windup charge retention, cancel/equip/item change, expiry, pause/NaN
and unchanged nondirectional routing. No serialized state was added.
Native BUILD/chain_focus.ps1 -> chain_native.py -> chain-native-events.json:
final exit0 / 8.58s. Short charged right follow-up then held full-charge stab,
two chain dispatches, shield phase2 throughout. Runtime left open.
Final idle render inspected. Window Alt-menu activation froze the initial test's
frame loop; clearing the menu and removing Alt from the focus harness resolved it.
Snapshot sampling is coarse; short follow-up was below 75% charge in native proof.
Unit tests explicitly verify a released request waits through recovery.

Collision limitation: an earlier opening left strike observed one unresolved
bounded correction (authored-pose fallback). Final native run counter stayed 1
from start through both chains, so chaining introduced no new unresolved correction
in that run. This is not a universal zero-clipping claim; investigate contact
continuity during rapid shield/strike transitions next if owner observes clipping.
Player preview authoring controls, NPC damage/stamina/parry and production combat
acceptance remain separate follow-ups. No release gate closure or editor parity claim.
Unrelated Tools/__pycache__ and log.txt untouched. Next: owner chain feel review,
then movement/transition checks and targeted collision continuity refinement.

## Independent hand assembly authoring checkpoint - 2026-10-06
Implementation commit: 744f9d63f24296a2b60bc32a9a9331786e45d0b2.
Changed FirstPersonAssemblyService settings/preparation, PlayerViewHandAnimation
and avoidance, Runtime timing/HUD/routing, Studio assembly panels and native preview,
three test fixtures, README, ARCHITECTURE, ROADMAP, FEATURE_MATRIX and new
docs/P2_ASSEMBLY_HAND_AUTHORING.md. No upstream changes.
Select Player Start -> ASSEMBLY -> HAND / MELEE SETUP. Five pages expose off-hand
mesh/grip/roots, uniform weapon scales, twelve directional stages, three block
stages, four fallback attacks, collision proxies and charge/chain/queue timings.
Weapon-only swaps preserve bindings. Arms replacement resets skeleton bindings.
Optional authoring settings retain old recipe defaults and validate finite bounds.
Runtime loads authored timings; preview uses native independent masks/avoidance.
Undo/Redo, automatic preview and journaled save retain all three part hash guards.
SAVE CHANGES retains equipment references; SAVE AS NEW requires equipment
presentations referencing the new identity. Level assignment alone is insufficient.

Verification: BUILD/assembly_build.ps1 builds Release x64 Bridge, Runtime,
FirstPersonAssemblySettingsTests, PlayerViewRigTests, EquipmentAssetTests,
FirstPersonAssemblyWorkflowProof and alternate Studio.
MSBuild /m:2 /verbosity:quiet /nologo /p:Configuration=Release /p:Platform=x64
/p:BuildProjectReferences=false, CL=/MP4.
Final production build exit0 /115.25s; all five focused CTests pass /0.78s:
ctest --test-dir BUILD/renegade -C Release
-R "FirstPersonAssemblySettings|PlayerViewRig|Equipment" --output-on-failure --timeout 30.
Final Studio-only button/layout refresh exit0 /16.35s.
Existing MSB8029/C4834 warnings. git diff --check passed.

Real supplied-pack proof: RENEGADE_ASSEMBLY_SWAP=BUILD/replacement-sword.glb,
RENEGADE_HAND_COLLISION=1; WorkflowProof BUILD/sword-ue-proof4
BUILD/sword-assembly-authoring-proof5 --sword-playable.
Final exit0 /22.04s: governed import of a different, longer sword; retained asset
update and cold reopen; scale/timing metadata; charge cap; all twelve directional
preview stages, four variants and three shield stages; saved new assembly;
965 corrected contacts and zero unresolved; saved level/snapshot closure.
Replacement held-shield strike PNG visually inspected, nonempty pixel assertion passes.
Earlier empty captures were a proof harness omission of EVENT_THREAD_SAFE_POINT;
the final harness supplies the same event as native Studio. Production preview
was visually correct throughout.

Native Studio on BUILD/assembly-hand-ui-project disposable copy: numeric full
charge edit to 1.25, saved product/source projection inspected, panel closed and
reopened with 1.25 retained. All directional/block mappings retained. Attachments,
Idle, held-shield strike, Directional, Block, Collision and Timing views inspected.
Undo restored an accidental scale slider click; no oversized mesh was saved.
Window title click brings setup ahead of the inspector after inspector interaction;
normal native window priority applies. Broader responsive UX remains owner review.
Owner's preexisting Studio and original desktop project were not modified by UI tests.
Updated Studio remains open on the disposable copy; original project Runtime open.

Native BUILD/chain_focus.ps1 exit0 /8.48s: released follow-up, held charged chain,
single dispatch, shield independence; unresolved counters 0 at start/chain/end.
Default original loadout remains playable. No universal no-clipping claim:
conservative discrete proxies and bounded correction retain authored fallback
for unreachable contacts. No NPC damage, stamina, parry, world collision or new
movement routing. Independent exact-commit review remains required; no release
gate or Wicked Editor parity closure. Unrelated Tools/__pycache__ and log.txt untouched.
Next: owner authoring/mesh-swap review, then movement/transition checks and targeted
contact continuity refinement if observed. Damage awaits NPC system integration.


## Owner fantasy sword mesh swap - 2026-10-06
Engine implementation unchanged at fcac40314952edb3f5d6130d93906f904c6da56d.
Owner supplied fantasy-sword.zip, SHA256
b57474266f43b2bd9898844743a033964d044d99985a3fbb46e74dce7a4ae82f.
The matching local Downloads copy was inspected in Blender 5.1.1. Static FBX:
1042 source vertices, original bounds about 5.72m end-to-end. Prepared GLB
uses 0.18 source scale (about 1.03m overall), blade +Y and upper-grip pivot,
with supplied base colour, metallic, roughness, normal and emissive maps.
Original ZIP/FBX/textures untouched; preparation scripts and GLB are in BUILD.

RENEGADE_ASSEMBLY_SWAP=BUILD/fantasy-sword.glb; RENEGADE_HAND_COLLISION=1;
RENEGADE_IMPORT_FOLDERS=1; WorkflowProof BUILD/sword-ue-proof4
BUILD/fantasy-sword-proof1 --sword-playable: exit0 /12.61s.
Custom folder/weapon role/tag/move rollback/cold placement checks and all
replacement masked preview slots passed. The fixture restores its original
playable assembly after checking the replacement: its 965 corrected/zero
unresolved counter therefore describes the original sword, not this new mesh.

Native Studio opened the disposable proof project, selected the imported
Replacement Sword, LOAD PARTS, and saved changes to the existing assigned
assembly. Animation mappings, shield, default scales and charge/chain timing
retained. Blade proxy edited to base (0,0.10,0), tip (0,0.797,0), radius 0.037m;
source projection inspected after journaled save. The updated project was copied
to a separate owner desktop FantasySwordTest, with a Play Fantasy Sword.cmd
launcher and prepared Fantasy Sword.glb for reuse. The original SwordShieldTest
was not modified. Native standalone cold-loaded this desktop copy; rendered
grip, metal blade and decorated guard inspected.
BUILD/chain_focus.ps1 against the new standalone: exit0 /8.95s, released
follow-up and held charged stab, two single chain dispatches, shield independence,
unresolved counters 0 -> 0 -> 0. Evidence BUILD/fantasy-sword-native-events.json
and BUILD/fantasy-sword-proof1.log. This is bounded presentation acceptance,
not universal no-clipping, damage collision, or NPC combat validation.
New Runtime left open for owner feedback. Only handoff documentation changed;
no new engine implementation, no push and no release gate closure.


## 2026-10-06 — Solid Player Start facing guides

Base implementation commit 4f16567; resulting implementation commit is recorded
by the following documentation-only checkpoint. Changed StudioApplication.cpp/.h,
ARCHITECTURE.md, FEATURE_MATRIX.csv and PLAYER_AUTHORING_CONTINUATION.md.
Player Start retains its selectable capsule and now renders a solid camera body,
grip, viewfinder, stepped lens and raised arrow in Studio's native depth-tested
scene pass. Both follow resolved Runtime spawn yaw; camera height follows the
sanitized authored eye height. Grid visibility does not control these guides.
No scene entities, asset IDs, serialized schema or Runtime behavior changed.

Release x64 Studio MSBuild with BuildProjectReferences=false, /m:2, CL=/MP4 and
OutDir=BUILD/player-solid-marker-studio: exit0, 75.40s. Existing MSB8029 temporary
output warnings and C4834 at the existing importer return-value site remain.
Native DX12 Studio opened BUILD/player-marker-ui-project, selected Player Start,
focused it and visually verified solid camera and extruded arrow. Owner replied
"much better thanks". Final cleanup restores the unchanged capsule edge signature
and corrects geometry documentation; final rebuild evidence follows below.
Rotation/save/reopen interaction was not exercised in this visual pass; these
markers consume the existing resolved authored transform rather than persisting
new state. Independent exact-commit verification/release gates remain open.
No upstream edits, no push. Next: owner continuation of equipment work.

Implementation commit: 6135b2d5395b4718b3adf198a95a08805307a919.
Final Release x64 Studio rebuild with the same flags and
OutDir=BUILD/player-markers-final: exit0 /20.36s; log
BUILD/player-markers-final-build.log. CTest --test-dir BUILD/renegade -C Release
-R 'PlayerFoundation|PlayerPrefab' --output-on-failure: PlayerPrefabTests 1/1
passed /0.32s (only prefab test matched). git diff --check passed.
The visually accepted solid-marker Studio remains open; final binary is also
available in BUILD/player-markers-final. No UI interaction after owner acceptance.


## P2 owner acceptance and P3 continuation - 2026-10-06

Acceptance/documentation commit c1375eb9770cea6b9a2a9ecfa89eb9f64211ab4c;
implementation through 6135b2d5395b4718b3adf198a95a08805307a919.
Owner confirmed movement and Build Game both act as expected and authorized
updating documentation, pushing the P2 branch and starting CI on PR #180.
Owner intends to begin P3 in a separate chat. README, ROADMAP, P2 implementation,
player continuation/UX follow-up, FEATURE_MATRIX and hand authoring documentation
now record the accepted bounded checkpoint; docs/P3_CONTINUATION_HANDOFF.md gives
the new chat its starting authority, scope, branch policy and remaining limits.
Historical checks remain dated evidence, superseded by the new current checkpoint.

Validation for this documentation-only change: git diff --check passed.
Prior compiled implementation/build/native/focused regression evidence is above;
no new source changes or rebuild required. Owner's exported executable hash/source
revision was not independently captured. CI and independent exact-head review
remain required; no merge or full P2/Alpha release gate closure claimed.
Existing PR #180 description is being replaced to match the complete branch.
Commands: git fetch origin; gh pr view 180; git push origin
feature/p2-equipment-actions; gh pr edit 180 --title ... --body-file
BUILD/p2-pr-body.md; gh pr checks 180; gh run list --branch
feature/p2-equipment-actions. GitHub links/status are reported to the owner after
push. Unrelated Tools/__pycache__ and log.txt remain untouched.
Next: P3 shared hits/projectiles/impact framework on a separate dependent branch
(or updated main after P2 merge). Read docs/P3_CONTINUATION_HANDOFF.md first.


## P3 initial simulation foundation - 2026-10-06

Implementation commit 4eaf5ec5371df730e631a7ca8a52acaceb8ac80e.
Branch feature/p3-projectile-impact depends on P2 PR #180 head
cde41068fcf38b8dd293a9defc85ebbf93435dcc. PR #180 remains open;
four Windows checks were pending when inspected. No merge or gate closure.

Changed CMakeLists.txt; ProjectileSimulation.h; ProjectileSimulation.cmake;
ProjectileSimulationTests.cpp; ARCHITECTURE.md; FEATURE_MATRIX.csv; ROADMAP.md;
P3_PROJECTILE_IMPACT_IMPLEMENTATION.md. Added bridge-owned transient point
projectile simulation with bounded travel steps, explicit source attribution,
gravity/lifetime, injected segment-query contract and typed single impact output.
Existing health/events/physics remain authoritative. This is not live gameplay.

Commands:
cmake --build BUILD/renegade --config Release
 --target RenegadeProjectileSimulationTests -- /m:2 /verbosity:minimal
exit 0 /8.99s; BUILD/p3-foundation-build.log. Same Debug build exit 0;
BUILD/p3-foundation-debug-build.log. Existing MSB8029 warnings remain.
ctest --test-dir BUILD/renegade -C Release
 -R '^RenegadeProjectileSimulationTests$' --output-on-failure
1/1 passed /0.23s total. Same Debug test 1/1 passed /0.09s.
git diff --check passed.

Tests use deterministic query callbacks; no native world collision, weapon input,
Character damage, effects, persistence, Test Level or packaged Runtime behavior
has been added or verified. No owner visual/gameplay acceptance claimed.
Unrelated Tools/__pycache__/ and log.txt untouched. Studio not closed or replaced.

Next: native nearest-contact query and source-hierarchy exclusion with coverage
proof for scene/Jolt/terrain/animated Character targets; map contact to governed
target/surface identity and existing ApplyAttributedCombatDamage; integrate Runtime
pause/reset and semantic actions; then Test Level and packaged-runtime parity.
Read docs/P3_PROJECTILE_IMPACT_IMPLEMENTATION.md for audit and limits.


## P3 native scene contacts and Character damage boundary - 2026-10-06

Implementation commit bc470286122bedf4345250237dcd4701637cfead on
feature/p3-projectile-impact, still dependent on P2 PR #180. Changed
RuntimeProjectileWorld.h, ProjectileWorldTests.cpp, ProjectileSimulation.cmake,
ARCHITECTURE.md, ROADMAP.md, FEATURE_MATRIX.csv and P3 implementation document.

Native Scene ray/overlap queries exclude the explicitly bound shooter hierarchy,
choose nearest eligible contact, resolve Character child hits to their governed
root and retain material-subset stable identity. Origin overlap stops at time
zero; coincident sphere-centre undefined normals use an incoming-facing fallback.
Character damage delegates to existing ApplyAttributedCombatDamage and existing
health/death/perception/ai.damage ownership. No second Player/physics/damage stack.

Commands:
cmake --build BUILD/renegade --config Release --target
 RenegadeProjectileWorldTests -- /m:2 /p:BuildProjectReferences=false
 /verbosity:minimal
Final Release build/test cycle exit0 /9.20s; BUILD/p3-world-final-build.log.
Same Debug build/test cycle exit0 /6.16s; BUILD/p3-world-debug-build.log.
Retained unchanged P2 engine/bridge dependencies used; no clean full Runtime build.
Existing MSB8029 warnings remain.
ctest --test-dir BUILD/renegade -C Release -R
 'RenegadeProjectile(Simulation|World)Tests|RenegadeCharacterAiCombatTests'
 --output-on-failure: 3/3 passed /0.14s.
Same Debug projectile-only expression: 2/2 passed /0.14s.
git diff --check passed.

Intermediate origin-overlap test failed because native overlap normal was
undefined at the collider centre; final implementation fixes and passes it.
Fixture directly populates native CPU collider query caches and uses real Scene
BVH/primitive queries. It proves ten-owner-child exclusion, nearest world cover,
Character child resolution, bounded segment and origin overlap, health/death/
legitimate knowledge/events, source/faction/self/dead-target rejection and material
identity lookup. It does not prove Scene::Update query-cache construction,
animated mesh/terrain/Jolt coverage, owner gameplay or package behavior.

This adapter is not yet installed into Runtime action/update lifecycle.
Next: physics-only Jolt blocker coverage and real actor owner bindings;
then Runtime lifecycle/semantic emission and Test Level plus independent
package proof. Impact profiles/effects/persisted asset definitions remain open.
No release gate closure. Owner Studio left untouched. Tools/__pycache__/ and
log.txt remain unrelated. P2 baseline Debug and Release checks passed when
last inspected; two Studio checks still pending.
