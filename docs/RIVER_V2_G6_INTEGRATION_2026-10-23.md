> 2026-10-24: owner accepted this repaired G6 candidate: "it works beautifully". Earlier OPEN labels below are historical. Remaining untested limits are unchanged.

> Acceptance repairs supersede candidate path, performance and texture status: see RIVER_V2_G6_REPAIR_2026-10-23.md. Owner G6 acceptance remains OPEN.

# River V2 G6 governed integration candidate — 2026-10-23

Implementation and automated engineering checks PASS on DESIGN. G6 owner manipulation/visual acceptance remains OPEN. This is an isolated candidate, not an accepted replacement Studio release.

## Authority and preservation

The owner accepted isolated G5 visually ("im actually quite impressed"), asked why its throwaway level had no editable spline, then authorized "continue" after G6 was explained. G1–G5 authority remains docs/CODEX_RIVER_V2_IMPLEMENTATION_HANDOFF_2026-10-23.md. No G7, mountain streams, tributaries or waterfalls. No commits, push, git merge, deployment, Unreal content, overlay meshes, foam collars or noise-only foam source.

Candidate: C:\Users\paulw\source\repos\renegade-river-v2-integration-20261023, feature/river-v2-g6-20261023, HEAD 11bbb90e4214776d5e51c8bc4cba2e7f1ccde296. Source changes are uncommitted. Private Wicked pin 3a800b7134aafe58461093c8abb2e274d4e64033; copied dirty Wicked sources match all 2143 original files, with no changes. Original native objectHF.hlsli SHA256 27d73178076aa7333374bef9a162e996629db01e749556881236d978daf41ce5.

Compared original river dirty tree against current accepted combined importer/terrain/probe work, retaining combined live fixes and river additions by bounded file assembly from common base 5f42a21e15a3a72b37f00939a5880455a59dfdd0. No branch merge. The two overlaps were identical GPU bypass/comment material and append-only test target additions; both retained. Assembly and combined documentation histories are recorded in assembly.json and source-baselines.json under the evidence directory. No older Studio was presented as including newer fixes. Accepted probe live behaviour is retained.

Before documentation updates, all 1562 baseline river source files and 1468 combined source files were byte-identical, with zero missing files. Original river HEAD and original combined HEAD unchanged. Original Wicked dirty patch equals its G5 baseline exactly. Note: combined Wicked folder has no independent Git metadata; git -C there climbs to the parent, so its parent HEAD is NOT a Wicked pin.

## Behaviour

Select a native WATER river or a control point below it. Expand the FLOW inspector section (SPLINE FLOW heading). Enable spline flow explicitly for existing scenes. Controls: arrows, contact foam, reverse, reset, speed, intensity, lifespan, turbulence, world-metric contact width, breakup threshold and base ribbon width. Base width is full LOCAL width before parent/node scaling. Existing native spline gizmos and smooth neighbourhood command remain the editing authority. Width changes use the existing linked-bed recomposition command.

River Flow is opt-in. Existing materials and scenes retain original WATER when disabled. Missing/corrupt/unsupported profiles fail closed with inspector feedback. Another custom material shader is refused. Dynamic/skinned/morphing obstacle surfaces are excluded; only static actual transformed LOD0 triangle intersections supply contact foam. Separate rock objects generate separate bounded wakes. This is a kinematic transport approximation, not Navier–Stokes or connected-waterway physics.

The bridge owns flow validation, geometry gathering, GPU resources and native rendering bindings. Studio owns widgets and command dispatch. Runtime uses the same renderer. Both apply custom WATER material fields only within native Update/Render scopes, restoring original shader/userdata afterward; saves, snapshots, duplication and Undo see native authored materials. The owned shader is built from the accepted G5 WATER extension; original Wicked source and optics remain intact.

## Persistent schema and lifecycle

Native MetadataComponent string key renegade.river.flow.v1, JSON version 1:
```json
{"version":1,"enabled":true,"foam":true,"reversed":false,"speed":1.0,"intensity":1.0,"lifetime":3.2,"turbulence":1.0,"sourceWidth":0.04,"threshold":0.0}
```
Identity is the carrying entity's existing persistent UUID; no second embedded UUID that could become stale after duplicate. Absent metadata defaults disabled. Maximum 4096 bytes; all fields required when present; version mismatch, nonfinite numbers and out-of-range values fail closed. Bounds: speed0–3m/s, intensity0–2, lifespan0.5–6s, turbulence0–1.5, sourceWidth0.01–0.08m, threshold±0.15. Exact metadata Undo/Redo preserves unrelated values. Reset restores supported defaults and leaves flow enabled.

Textures, descriptors, density/age/material-coordinate history are transient per render path and river UUID. Up to16 active rivers. Scene replacement clears resources safely. Monotonic CommandService revision avoids missed edits when undo-stack counts repeat. Static geometry fingerprints inspect component transforms and candidate bounds each frame; mesh vertices are hashed only on changed command epoch/counts and only in candidate corridor sources, shared across rivers. No every-frame full terrain/mesh traversal. Rock move/rotate/scale/remove, material UV, spline and width/profile changes invalidate relevant entries. Terrain worker mutation defers rendering/cache work until settled. Gizmo and vegetation interactions suspend V2 preview; native WATER displays during drag and rebuild happens on release.

History persists across profile/rock changes if world bounds are identical; domain changes reset safely instead of relocating density. Actual inherited native geometry supplies the field. G1 native spline oracle tests remain included. No independent approximate spline is substituted.

## Files added or directly changed for G6

- EngineBridge/include/renegade/bridge/RiverFlowRenderer.h; src/RiverFlowRenderer.cpp; src/RiverFlowProfileService.cpp; private src/RiverFlowWaterline.h, RiverFlowTransport.h, RiverFlowGather.inc.
- EngineBridge/include/renegade/bridge/CommandService.h and src/CommandService.cpp: monotonic successful-edit revision.
- EngineBridge/CMakeLists.txt: bridge sources plus owned shader staging/deployment.
- Studio/src/RiverFlowStudioIntegration.cpp; StudioApplication.h/.cpp; TerrainInspectorSections.cpp; CMakeLists.txt.
- Studio/src/WindowsGameBuildController.cpp: two owned flow shader support resources only.
- Runtime/src/RuntimeApplication.h/.cpp; Runtime/CMakeLists.txt.
- Tests/RiverFlowProfileTests.cpp; RiverFlowIntegrationProof.cpp; RiverFlowScalabilityProof.cpp; RiverFlowPackageProof.cpp; RiverFlowRuntimeLifecycleProof.cpp; Tests/CMakeLists.txt.
- Tools/prepare_river_g6_package_proof.py: BUILD-only staging of actual build/lifecycle sources with window visibility adaptations. Production sources unchanged by that script.
- Tests/WD01VegetationSourceContract.cmake and SceneUiGate3SourceContract.cmake: stale accepted-layout assertions corrected after demonstrating the exact same failures in the untouched original river worktree.
- This report, docs/ARCHITECTURE.md, docs/FEATURE_MATRIX.csv and HANDOFF.md. Original river HANDOFF receives a navigation pointer only.

All inherited dirty importer/terrain/probe/river features are also present in the integration worktree; its broad Git diff is not a G6-only diff. See assembly.json for provenance.

## Exact build and verification

Evidence root: C:\Users\paulw\source\repos\renegade-spline-water-20261020\BUILD\river-v2-G6-20261023-2135.

CMake executable:
C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe

Initial configure in configure-build.ps1 uses Visual Studio 18 2026, x64, CMAKE_CXX_FLAGS="/DWIN32 /D_WINDOWS /GR /EHsc /MP2 /IC:/Users/paulw/source/repos/renegade-river-v2-integration-20261023/WickedEngine/WickedEngine/Utility/DirectXMath".
Final coherent core build: integration-build-v8.ps1. From candidate root:
```powershell
cmake -S . -B BUILD/g6
cmake --build BUILD/g6 --config Release --target RenegadeRiverFlowProfileTests RenegadeRiverFlowIntegrationProof RenegadeRiverFlowScalabilityProof RenegadeStudio RenegadeRuntime RenegadeRiverFlowFieldTests RenegadeRiverWaterlineTests RenegadeRiverFoamTransportTests RenegadeRiverSplineTests RenegadeRiverFoamTests RenegadeRiverUnderwaterTests RenegadeBridgeTests RenegadeImportTests RenegadeReusableAssetTests RenegadeModelImporterFailureAdapterTests RenegadeTestLevelSnapshotTests RenegadeTestLevelSnapshotRuntimeTests RenegadeTestLevelRuntimeProcessTests --parallel 2
ctest --test-dir BUILD/g6 -C Release -R 'RenegadeRiver.*Tests|RenegadeBridgeTests|RenegadeModelImport|RenegadeImportTests|RenegadeReusableAssetTests|RenegadeTestLevelSnapshot|RenegadeTestLevelRuntimeProcessTests|SourceContract' --output-on-failure
```
Core build EXIT0; final CTest83/83 PASS in30.04s (integration-ctest-final.log). New profile suite27 assertions PASS. One initial MSVC19.51 ICE in unchanged BuildExecutableTests.cpp retried successfully unchanged in v8; do not use earlier incoherent v3 binaries after the revision header changed.

Additional proof builds:
```powershell
cmake --build BUILD/g6 --config Release --target RenegadeRiverFlowRuntimeLifecycleProof RenegadeRiverFlowPackageProof --parallel 2
cmake --build BUILD/g6 --config Release --target RenegadeRiverFlowScalabilityProof --parallel 2
```
lifecycle-build.log, lifecycle-build-v2.log, stress-final-build.log: EXIT0.

Actual DX12 native proof commands and working directories are verbatim in run-proof-v7.ps1, package-v5.ps1, lifecycle-run-v1.ps1 and stress-final.ps1. Source scene is original BUILD/river-v2-G5-20261023-2015/polish-proof/native-contact-foam.wiscene. All evidence is task-owned.

- proof-v7.log: native bindings scoped on every frame, enable/disable, native spline edit, linked width edit, Undo/Redo, reverse, no static rebuild on unrelated edit or suspended drag, native archive profile and stable identity checks PASS; shader_errors0. Enabled foam mass494.948/max2.35119; cached entry remains rebuild1. Spline edit changes actual rendered geometry and rebuilds. Reverse retains finite transported foam.
- reopen-v7.log: fresh process rebuild PASS; mass323.993/max2.94168.
- proof-v7/native-before.png vs native-restored.png: exact RGB match, max delta0 (proof-v7-pixels.json). Enabled, spline-edited, width-edited and reversed captures inspected. The old throwaway terrain/camera composition is a fixture, not final scenery.
- lifecycle-v1.log: actual TestLevelSnapshotService + actual ready-event/JobObject/cleanup process implementation with hidden child, native alwaysactive argument. Unsaved reversed profile/UUID/native material in snapshot checked. Runtime PID43528 reports ready; runs3s; stop removes owned snapshot. Further edit plus two Undo operations PASS; editor path/history preserved. Runtime/Logs/RuntimeBootstrap.log statusPASS, DX12 STARTED,76entities. This is process/load evidence, not a completed visible Runtime walkthrough.
- package-v5.log: actual BuildActiveWindowsGame adapter with only hidden progress/child visibility adaptation: dependency closure, identity stamp, real exported Runtime smoke, final promotion PASS. Output candidate BUILD/g6-export-input/RiverV2Review/Builds/Windows/RiverV2Review Windows Build. build-report status gate5_validated_for_final_path, smokeDX12 passed, distribution_ready remainsfalse by existing policy.
- Package manifest all listed payload hashes match; both new CSOs present. relocation-result.json: copied complete export to evidence/relocated-player-v1 and ran its actual RiverV2Review.exe from new cwd; EXIT0. No external dependency allowlist broadened.
- The copied fixture initially failed dependency closure because old proof-relative texture names escaped its root. Adopted8 texture names/payloads into task-owned Content/Textures, public save; explicit known native waterNormalMap.jpg fallback from accepted foam-repair Content only when embedded payload absent. Existing owner/live project not rewritten. Initial fixture prep v3 invalid-resource access was corrected with IsValid before GetFileData; v4 records the missing normal. These are retained failed iterations, not hidden successes. No production loader policy changed.
- BUILD-only adapter provenance is in BUILD/g6/river-g6-package-stage/{provenance,lifecycle-provenance}.json.

## Scaling and limits

stress-final.log: one actual rock cluster duplicated across16 native rivers; all16 geometries remain in scene. Runs0,1,4,16,4,1,0 after warmup;150 measured frames each, native DX12 timestamp queries around graphics-queue foam dispatch, CPU timer around Prepare. All active river foam states finite/positive; zero static rebuilds; shader_errors0.

| Active | Prepare CPU ms/frame | Foam graphics-queue span ms/frame | Initial setup ms |
| --- | --- | --- | --- |
| 0 | 0.105–0.157 | 0.00004–0.00012 | 54 |
| 1 | 0.133–0.167 | 0.014–0.540 | 1159 |
| 4 | 0.157–0.215 | 0.277–0.328 | 2237 |
| 16 | 0.229 | 1.099 | 8890 |

Measurements share DESIGN with owner applications. GPU spans include graphics-queue scheduling interference; broad one-river variance prevents treating these as isolated kernel benchmarks. Display/frame cap remains about75.5fps, so capped FPS is not proof of zero overhead. GPU-only normal/foam pixel shading cost was not isolated. New flow textures consume about20MiB per active river (five1024x256 RGBA32F), about320MiB for16, excluding native rendering.

Rebuild is synchronous and bounded but noticeably stalls on release: roughly0.7–1.2s for one fixture; about8.9s initial16 allocation/build. Drag preview suspension avoids rebuilding each intermediate gizmo sample, but asynchronous rebuilding is not implemented. Owner authoring-feel acceptance remains required. Flat static rivers only; dynamic/deformed obstacles, connected water, device loss and complex field stress beyond limits are not certified.

## Review session and remaining acceptance

Tested candidate bin evidence/studio-bin-v1/RenegadeStudio.exe with complete Runtime subfolder. Separate copied creator project evidence/proof-v7/RiverV2Review/RiverV2Review.renegade. Review camera River Review Camera looks at River Water5 rock cluster. Studio visual water/foam was inspected and captured (studio-review-camera.png). Candidate source/binary core was not changed afterward.

Owner became active in Studio PID23608, with unsaved changes. Input automation stopped; no close, save, revert or replacement of that session was performed by this task. PID23608 was absent at final verification; its owner editing/save outcome is not inferred. Therefore actual GUI drag/reverse/Undo/close-reopen and visible standalone walkthrough are OPEN despite bridge/native/process proofs. Device loss was not forced. No G6 acceptance invented. Keep G7 and mountains paused.

Desktop shortcut: Renegade River V2 G6 - Editable Studio. last-check.json verifies the shortcut candidate core Studio/Runtime executables match the final Release build; git diff --check EXIT0. preservation-final.json confirms only the intended original HANDOFF navigation addition, combined source unchanged and all packaged payload hashes valid.

For owner review: select River Water5 (or expand its control points), expand FLOW, toggle FLOW ARROWS, manipulate existing spline gizmos; reverse and Undo; save/reopen when ready; Test Game and Build Game from this isolated candidate. Do not replace newer accepted builds or move this dirty work into another branch without permission.
