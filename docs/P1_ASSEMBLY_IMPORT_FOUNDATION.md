# P1 assembly import foundation

4 October 2026. Branch feature/p1-first-person-arms-rig. P1 remains IN PROGRESS.

## Owner-accepted baseline

Owner accepts shotgun grip, shell contact and reload animation in the authored
attachment diagnostic built from source checkpoint 7c4a976. Its executable hash
is recorded in P1_SHOTGUN_ASSEMBLY_PROOF.md. This accepts that diagnostic visual
result; it does not establish production Player integration or a completed P1 gate.

## Implemented import boundary

ModelImportCandidateService now accepts explicit FBX material-index/texture-slot
relinks. Each selected file is snapshotted, decoded under a unique preview key,
retained in SourceAssets/Models and included in the journaled model transaction.
Unreplaced dependencies retain the existing strict source-folder checks.
Duplicate or absent material slots fail; changed dependencies reject commit.
Original downloads and Unreal projects remain untouched.

MatchingRigAnimationService transfers native animation channels and keyframe data
onto one matching skeleton. It validates unique bone names, parent topology and
inverse binds (0.0002 matrix-element tolerance), rejects unsupported tracks and
copies no meshes, materials or extra skeletons. External clip import first tries
this exact-rig route, then retains the existing humanoid-retarget route when it
does not match. Candidate mutation occurs on an isolated working copy.

Durable recipes record matching_rig and texture_relinks with project-relative
retained source paths. Recipe reconstruction loads matching clips directly.
Existing recipes default to the prior retarget behavior. Matching-rig clip import
is available through Studio's existing ADD ANIMATION interaction; explicit
material relinking currently has a bridge API and proof, not a creator UI.

## Real-pack evidence

The manual --workflow proof imports each Idle FBX with selected supplied textures,
appends the separate Reload FBX without humanoid retargeting, commits two rigged
products through the existing model/Character transaction and reopens each by
stable ID. It reconstructs both products from retained FBXs plus durable recipes.
The paired scene then uses the owner-accepted ik_hand_gun attachment.

Arms: 161 bones / 2 clips. Weapon: 7 bones / 2 clips. Bone-index and inverse-bind
fingerprints are unchanged. Native scene cloning renormalizes weights: measured
maximum arm-weight difference 1.78814e-7; weapon difference zero. The proof permits
at most 1e-6, rather than incorrectly requiring byte-identical normalized floats.

Duplicate texture mappings and a changed disposable texture dependency are rejected
without committing a product. Both paired actions render at 0, 0.6, 1.2, 2, 2.9s;
assembly WISCENE save/reopen renders again. A separate --workflow-reopen process
loads only retained products and renders reload again. Captures require visible
pixels, including lit model pixels for the first-person fixture. Fresh-process
reload at 1.2s was visually inspected under a new evidence filename to avoid
stale image display caching. No original-editor parity claim is made.

## Commands and results

Configure: cmake -S . -B BUILD/renegade, exit 0.
Release builds use CL=/MP4 and:
MSBuild <target.vcxproj> /m:2 /nologo /verbosity:quiet
/p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false

Targets built successfully: RenegadeEngineBridge, RenegadeStudio,
RenegadeMatchingRigAnimationTests, RenegadeFirstPersonAssemblyWorkflowProof,
RenegadeModelImporterRebuildGraphicsProof, RenegadePlayerViewRigTests.

CTest Release selection:
MatchingRigAnimation|RenegadePlayerViewRigTests|PlayerViewRigSourceContract|
RenegadeModelImporterRebuildGraphicsProof|RenegadeModelImporterRebuild_animated_character
Six tests pass, including animated Character cold reopen.

From BUILD/renegade/Tests/Release:
RenegadeFirstPersonAssemblyWorkflowProof.exe ../../p1-shotgun-proof/input ../../../p1wf --workflow
RenegadeFirstPersonAssemblyWorkflowProof.exe <retained-project> ../../../p1wf/cold --workflow-reopen
Both exit 0. Use the project path recorded in BUILD/p1wf/latest-project.txt.

Ignored evidence: BUILD/p1wf, BUILD/renegade/p1-shotgun-proof/workflow-proof.log,
workflow-cold-proof.log and BUILD/renegade/p1-workflow-*.log.
An initially deeper fixture destination hit the existing Windows path-length
limit during transaction staging; the final disposable project uses a short path.

Existing RenegadeReusableAssetReimportRecipeTests could not compile because it
references removed CreatorModelMaterialPreparationService.h. This is a prior
importer-rebuild issue, not a passing regression or a full-suite claim.
Existing MSB8029/C4834 warnings remain. Debug and hosted CI were not run here.

Release Studio SHA256:
7390E00685B8DE7DB509AA6ECF2B3989F8036C7486F11257C78F0DEE1E27D5FA
Workflow proof SHA256:
9CE51204BC79A3729E06C7875284A1ACB8FAC78830CF864A0C785E3C84F7E168
Compiled working source before committing; independent exact-commit gate pending.

## Next bounded outcome

Provide a Studio first-person assembly authoring surface: choose retained arms and
weapon products, specify the weapon parent bone and authored local transform,
pair semantic clips, preview simultaneous rig playback, save/reopen one reusable
assembly, then select it on the existing Player Start. Preserve the accepted
attachment and player controller. The temporary rigged-part proof uses existing
Character-kind products; it does not expose a first-class Weapon Asset or place an
NPC weapon in a level. Assembly classification and paired-clip ownership remain
to implement before production Player use.

No fire/reload gameplay, inventory, ammo events, pickups, retarget UI, IK solver,
standalone real-arms acceptance or completed P1 gate is claimed by this step.
