# P7 crossbow asset preparation checkpoint

## Scope and baseline

The owner requests remaining combat capabilities before another broad gameplay/feel session. Continue focused automated, persistence and package checks. Original P1-P10 gates and acceptance criteria remain unchanged. Crossbow precedes magic by owner instruction; this does not close or renumber any gate.

Baseline: `9e7c20d81543abf0346fd230f84075cfb7f1fb0d`. Branch: `checkpoint/a-mainline-integration`. No production gameplay implementation has changed in this preparation stage.

## Asset export evidence

The supplied Unreal pack was copied into a disposable Unreal 5.7 project under ignored `BUILD/crossbow-export/`, at its original `/Game/LightCrossbow` mount. Original source files and existing projects were preserved.

The initial NullRHI export asserted during mesh export. The offscreen DX12 retry completed with exit 0. Two unsupported TGA mask exports were retried as PNG successfully. `export-manifest.json` records 39 exported files with hashes and zero unresolved export errors. Blueprints were not translated into engine gameplay code.

- Arms mesh and six clips: Fire, Idle, Run, JumpStart, JumpLoop, JumpEnd. Fire lasts approximately 0.267 seconds.
- Crossbow mesh and native Fire (0.625 seconds) and Reload (1 second) clips.
- Bolt mesh, material textures, firing WAV and supporting textures.
- All 13 FBX exports imported successfully in Blender. Arms have 67 bones; the weapon has Trigger, StringMid and Handle bones.
- Material/UV audit: visible arms, crossbow and bolt meshes have UV layers; all 25 exported texture images decode successfully. Evidence: `material-uv-inspection.json` and matching log.
- Crossbow FBX contains LOD0, LOD1 and LOD2. Bolt FBX includes UCX collision geometry. Governed import must select visible geometry appropriately rather than render these overlapping meshes together.

No dedicated arms reload clip was found. Blueprint name/reference inspection identifies PlayAnimation, TryFire and TryReload but is not a complete graph/timing audit. Weapon reload animation alone does not establish P7 cock/load hand choreography.

## Mainline verification

The canonical Release/Debug run completed successfully at 2026-10-10T20:57:30.4163210Z. Studio and Runtime compiled in both configurations, all 212 CTest tests passed in each, and both packages passed dependency and registry init/update/move/reopen checks. The final manifest is retained in ignored `artifacts/checkpoint-a/mainline-final-build-result.json`.

An earlier run failed to create a journal beneath a long Desktop Commander cache path. Its evidence remains retained; the successful rerun used a fresh short artifact directory. No timeout or acceptance requirement was relaxed. This establishes the technical baseline, without closing an original gate or replacing independent acceptance review.

## Next bounded outcome

1. Mainline technical verification is complete; preserve its exact-build evidence.
2. Prepare governed arms, weapon and bolt assets, material mapping and a first-person assembly in a disposable project. Verify native rig matching and filter redundant LOD/collision geometry.
3. Audit P2 action persistence and native animation completion before adding reusable Cock -> Load Bolt -> Ready staging. Do not add a crossbow-specific Player controller or second animation evaluator.
4. Verify save/reopen, cancellation, pause/reset and packaging. Flag the missing arms reload choreography when presentation testing becomes necessary.

P7 remains open until its original acceptance criteria are demonstrated on an exact build and independently verified. Private pack exports remain machine-local under ignored BUILD.

## Prepared bolt and reload integration audit

`BUILD/crossbow-export/prepared/Bolt.glb` now contains one visible mesh, with UCX collision geometry removed, two UV layers and one PBR material. Blender cold reopen succeeded; dimensions remain approximately 0.3113 x 0.0427 x 0.0405 metres. SHA256: `43d2452a5e850bdaf57c2b722471fa296740e0bc9a973d2bcf5bbd128376a516`. Normal-map green was inverted under the Unreal DirectX convention assumption; native render inspection must confirm shading before acceptance. This prepared source is not yet a governed Renegade projectile asset.

`PlayerViewAnimation.h` currently refills loaded ammunition to capacity when Reload/ReloadPartial completes (pairedTime reaches clip duration). Crossbow staged reload must reconcile this native completion path with P2 action authority. A semantic state-only implementation would be insufficient and could refill early. Cock/load cancellation must preserve explicit loaded/cocked state until successful stage completion.

The complete Release/Debug package result is recorded above. P7 remains open.

## Retained native rigs and attachment audit

`Tools/Prepare-Crossbow-Reference.py` runs with Blender factory startup and `--python-exit-code 1`. It preserves the original six arms animation FBXs byte-for-byte. The arms base receives only a diffuse material correction, checked by a parsed structural digest excluding materials. Rebaking the arms through Blender was rejected because native skinned vertices separated from the bones despite matching rig fingerprints.

The weapon preparation removes redundant LODs and grafts original Unreal animation curves onto its retained LOD0 mesh and skin binding. No curve rebake or humanoid retarget fallback is used. Idle is an explicitly synthesized constant first-fire pose, not an authored idle clip. The preparation tool completed successfully; native import accepted six arms and three weapon clips with matching rigs, unchanged inverse-bind and skin-index fingerprints, correct durations, and successful retained-source recipe rebuilds.

The manual proof is in `Tests/CrossbowAssetGraphicsProof.h`, enabled through `RenegadeFirstPersonAssemblyGraphicsProof --crossbow-assets` and `--crossbow-reopen`. Private pack outputs stay ignored. Its current status is diagnostic: passing persistence checks does not establish visual or gameplay acceptance.

A template-only Blueprint hierarchy query incorrectly suggested a component-root weapon mount. An instantiated actor query confirms `BP_Crossbow` attaches at `GripPoint`, a socket on `middle_01_r`; the bolt attaches at the separate `Bolt` socket. The authored socket, child actor offset and weapon component scale must be converted into the retained FBX bone frame. The saved assembly now cold-reopens in a fresh native process with the authored socket conversion and no settings override. Idle, Attack at 0.12 seconds and Reload at 0.5 seconds each produce nonempty first-person renders. The diagnostic camera uses 80 degrees; the original pack camera uses 75. This proves retained native presentation and persistence, not playable P7 acceptance. No crossbow controller, projectile delivery asset, staged reload or playable package has been added in this preparation stage.

## Completed bounded asset checkpoint

Preparation: `blender --background --factory-startup --python-exit-code 1 --python Tools/Prepare-Crossbow-Reference.py -- BUILD/crossbow-export` completed without a Python exception. Native proof target compiled in Release. Fresh governed asset import and assembly save/read/prepare passed in `native-project-11`; separate-process cold reopen passed in `reopen-final-11`. Evidence is private and ignored. The pixel checks are enabled for the final cold-reopen captures; earlier allow-empty diagnostic captures are not acceptance evidence.

Visual review confirms the crossbow is in the first-person view at its source GripPoint mount. Idle hands and the short fire clip are visible; mechanical Reload retains Idle arms because the source pack has no dedicated arms reload. Neither the gray mannequin material correction nor a mechanical reload pose establishes final shading or hand choreography acceptance.

Next bounded implementation is the reusable action-stage contract: Cock completion must mark cocked but remain unloaded; Load Bolt completion alone may refill the one-bolt capacity. Existing native animation completion currently refills Reload automatically, so staged authoring, runtime dispatch and native completion must change together. Cancellation/resume, pause/reset, serialized stage save/reopen and a non-crossbow staged-action case must be checked before connecting a saved Bolt projectile and equipment fixture. Broad owner feel testing remains deferred; flag presentation testing when real hand choreography is available.

## Staged integration follow-up

Equipment schema v4 now supports reusable Cock and Load Bolt stages. Native paired completion advances ownership; only final Load Bolt refills. Completed Cock progress resumes for the same item and owning hand until session reset. A three-stage non-crossbow firearm exercises the same contract. No second skeletal evaluator or dedicated crossbow Player controller was added.

The fresh native-project-13 fixture saves governed projectile/equipment and PlayerStart scene. GripPoint lookup now checks exact terminal middle_01_r: substring lookup could select a descendant finger. Runtime and direct preview PSP poses agree. The real native proof releases once at 0.12 seconds, hits a scene target, retains the bolt and removes it on reset. Cock/loading-placeholder/Ready and bolt captures are visually inspected.

Current centred Bolt.glb SHA256: 9f7133eb77bed6ba42f5564f4722c9fb8b2077894a51c26ca81e9852acd53952. Native governed import/render now verified. Earlier hash and no-production-change statements describe the preceding preparation checkpoint. Firing sound binding, standalone evidence and owner presentation acceptance remain pending. Load Bolt explicitly retains Idle arms. Original P7 criteria remain unchanged and open.
