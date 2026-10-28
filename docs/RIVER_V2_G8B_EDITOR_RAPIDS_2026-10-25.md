# River V2 G8B: opt-in measured-bed rapids integration

2026-10-25. Engineering validation PASS; private candidate ready for owner review. No release declaration. Owner accepted exact G8A preview then authorized integration. HEAD remains 11bbb90e4214776d5e51c8bc4cba2e7f1ccde296; no commit/push/merge.

## Implemented behavior

FLOW Inspector adds Depth-driven rapids toggle and Rapids foam amount 0..2. Receiver owns shared settings; selected inlet retains independent speed. New metadata JSON version2 uses existing carrying UUID/key renegade.river.flow.v1, adding rapids=false and rapidIntensity=1. Version1 reads preserve accepted behavior. Edits use SetRiverFlowProfileCommand; Undo restores exact original metadata. No ephemeral GPU resources serialize. One flat two-inlet confluence only; single-river rapids edits and non-owner shared edits are refused.

Optional depth mode gathers the same statically selected geometry for fingerprint and snapshot, including fully submerged rigid geometry within supported four-metre depth. Terrain, bounds, hierarchy and dynamic/foreground exclusions remain governed. Immutable mesh hashes are cached by edit revision; native animated UV Z/W excluded. Geometry or enable edits rebuild once. Contact-width/rapid-amount edits verify exact topology/face signature and replace only source map/parameter binding, preserving density, clock and state index. GPU source.w is the rapid source; three accepted contact channels retained. Four-channel finite-volume CS and accepted source-dependent normal steepness staged through owned scripts; Wicked source/pin unchanged. Existing shader bytecode is preserved exactly. Separate RiverRapidsPS/CS files serve opt-in mode; original RiverWaterwayFoamPS/CS remain the exact accepted G7C bytecode. Two pipelines are cached independently and selected by profile. Existing canonical built-in content identity is retained; no dependency guard relaxation.

## Physical scope

Prescribed flat free surface with conservative depth-weighted steady discharge projection and calibrated Froude/bed-disturbance aeration proxy. Not free-surface hydraulics, Saint-Venant, hydraulic jumps, standing-wave geometry, spray, unsteady wake turbulence or mountain rivers. Minimum resolved depth remains 0.10m; maximum depth4m; missing beds fail closed. Amount scales rapid emission only. Existing general Foam intensity affects visible foam; Foam lifespan controls decay. Foam toggle affects total foam.

## Real-scene regression found and repaired

Actual copied editor terrain yielded wet Bank cell XZ(-0.608606,63.913109), waterY=-0.15, actual native triangleY=-0.150042. Prototype discarded submerged samples shallower than0.1mm before applying its thin-layer floor, incorrectly reporting missing bed. Depth raster now samples the same float cell positions as the classifier and uses its1e-5m wet/solid epsilon before regularisation. CPU regressions cover42-micrometre, coplanar and within-epsilon wet beds; above-water-only data still refused. No fallback/invented bed was added.

## Commands and evidence

All commands run on DESIGN, Release x64/DX12. Relative B = BUILD/river-v2-G8B-20261025-editor in experimental source repo. Existing integration build cache BUILD/g6 reused.

- powershell.exe -NoProfile -ExecutionPolicy Bypass -File B/build.ps1. Initial integration build0,306.9947842s;4/4CPU PASS3.69s. Final thin-bank repair build0,55.4329329s;4/4CPU PASS3.62s. Configure/build/ctest logs in B.
- B/run-gpu.ps1 before thin-bank fix: legacy0/22.51s;rapids0/24.15s;fresh reopen0/22.95s. Scoped native geometry maxerror7.62939e-6m, native planar reflection visible. Profile edits including rapid amount zero and Undo preserve frozen GPU density; inlet speed commit/Undo rebuild exactly once. Animated cache0rebuilds, mean preparation0.0857/0.1138/0.0923ms respectively. Superseded by final separate-variant suite below.
- cmake --build BUILD/g6 --config Release --target RenegadeRiverWaterwayNativeProof RenegadeRiverFlowRuntimeLifecycleProof RenegadeRiverFlowPackageProof --parallel2:0. Adds actual editor-scene native bed preflight and unsaved rapid-amount TestLevel snapshot case.
- Initial actual-bed preflight intentionally FAIL closed; exact diagnostic retained in B/actual-cell-diagnostic.log. Superseded by completed actual-scene/UI/Runtime/export checks below.
- Build-extra test initially failed to find private json.hpp; corrected explicit test include path. Final strengthened4/4CPU PASS3.47s before thin-bank extra regression; final4/4 includes regression.

## Preservation

B/before stores narrow edited-source snapshots. B/*-status/head/submodules captures dirty baselines. B/audit-current.json confirms owner four project files exact, protected executables exact, heads/pins unchanged and diffcheck0. Compression of8 generated libraries preserved SHA256 exactly, recovering build headroom without deletions. Manifests B/space-preservation.json and extra-compression.json. Old open candidate and accepted G8A preview unchanged. New review project copied separately; source project never altered by G8B tests. New candidate hashes B/candidate-package.json.


## Final validation and review candidate

Separate-variant final build: powershell.exe -NoProfile -ExecutionPolicy Bypass -File B/build-variants-final.ps1: exit0,33.1728572s;4/4CPU PASS3.47s. Original accepted contact shaders retain SHA256 PSdf5ef04473ee80ea9e162ca2a20ace569351b0cfbe6dec449af42ccf883ae015 / CS83a52c87795babefadce95b5bc410b1eec5c7dad8242b1f4e2a713151ba48055.

B/run-review-gpu.ps1: legacy0/22.0991s;rapids0/26.9445s;fresh reopen0/25.6362s. Rapidity/legacy shader toggle and Undo each rebuild exactly once. Frozen amount0 edit and Undo retain density exactly. Receiver/source speed changes and Undo each rebuild once and resume foam. Animated UV0rebuilds;mean Prepare0.08763/0.11539/0.08392ms. Native-before/restored RGB pixel-identical in all three cases; actual native planar reflection visible.

Actual native editor bed proof command:
RenegadeRiverWaterwayNativeProof.exe --rapids-prepare <copied-project-scene> <fresh-output-in-same-scene-directory>.
PASS0/5.00s;50037wet cells;Q0.238544m3/s;maxFr4.22575;rapidSource1.38319;max divergence1.18744e-08. Review-only bed depth0.45m and both inlets0.35m/s. Original owner terrain/settings untouched. Output must serialize in final scene directory; copying a saved archive between different directories without reference rebasing is invalid.

Actual editor: PID12404; creator-launch.json authoritative; copied RiverRapidsReview project. Selected RiverWater5 Point2 initially; amount1->0.406667, Undo->1, Edit/Redo->0.406667; toggleOFF returns contact-only mode and Undo restores measured-depth mode. Saved then File/Reopen; shared receiver controls reappear correctly even selecting sourceRiverWater6 Point2. Saved0.406667 survives, Ready75FPS. Actual PLAY child8432 launched through TestLevelSnapshots and ready-event handshake, screenshot75FPS with native water/foam/textured rocks; closed through its owned main window. Studio reports TEST LEVEL COMPLETED/75FPS; snapshot cleaned. No resource error matches in Studio or actual Play log. Evidence editor-reopened-shared-rapids0p4067.png,actual-play-rapids75fps.png,editor-test-level-return75fps.png,editor-review-ready75fps.png.

Final unsaved-setting lifecycle API test:
RenegadeRiverFlowRuntimeLifecycleProof.exe <copied-project-descriptor> <private-Runtime> --rapids.
PASS0/8.03s; unsaved rapid amount0.5 carried into disposable snapshot, child readiness, return/edit/Undo/history and cleanup PASS. B/runtime-lifecycle-final.log.

Actual BuildActiveWindowsGame path through relocated package helper: B/creator-bin-v1/RenegadeRiverFlowPackageProof.exe <copied-project-descriptor>. PASS0/22.73s; promoted real game. B/launch-relocated.py moved all package bytes into another Deep directory and launched from unrelated CWD, verified unchanged file hashes. Runtime29900 visibly75FPS, native water/foam/textures retained, normal exit0. RuntimeBootstrap package_integrity/status PASS; WickedRuntime no missing/failed image/resource/shader error matches. Logs copied to B/relocation. Export used rapid amount1; subsequent editor amount0.406667 verified separately by actual Play and save/reopen. No false claim that the earlier export already contains later authoring edits.

Current private Studio SHA256:351b6fdc9b039bc108acef4d6e5cfe5693cebb9571e7c9ba9331ddb1178268b2.
Current private Runtime SHA256:08bd00e1e5a44be4a81d497bdc8a3682c7394d9d2d9934d5266eaab0b5e3216e.
B/candidate-package-final.json and validation-final.json are authoritative. Desktop shortcut: Renegade River V2 - Rapids Editor Candidate. Uses idempotent owned-process opener B/open-editor.py. Old Drawing Candidate and accepted G8A preview remain intact. UI left open, sourceRiver6Point2 selected, FLOW expanded; amount0.406667 intentionally reduces demonstration coverage. Public default remains1.

### Failed attempts and open risks

Early CLI export helper was run from Tests/Release and could not locate its module-relative Content; copied helper into new private package. Freezing per-candidate river folders also broke canonical built-in normal identity. A hardlink experiment preserved bytes but did not preserve canonical paths; unprivileged file symlink unavailable(error1314), prior hardlink restored immediately. Final solution uses separate opt-in shader names, exact accepted legacy outputs and original canonical content junctions. No production dependency-policy weakening. Old pinned copies retained with manifests; all legacy-byte comparisons PASS. Review scene is now serialized in its final directory. The final actual export succeeds without missing-resource warnings. Early CLI error paths exited0xC0000005 after reporting refusal; no attribution of that failure path to the renderer is established.

One isolated binding-proof startup reported bad allocation. Immediate same-binary retry passed, followed by all three fresh final suite cases. Cause is unproven; no claim that a generic startup allocation issue was fixed. No new release gate closure. Larger networks, continuous bed deeper than4m, dynamic obstacles, free-surface hydraulics, standing waves, spray and actual unsteady wake solver remain unsupported.

### Changed source files for G8B

EngineBridge/include/renegade/bridge/RiverFlowRenderer.h;
EngineBridge/src/RiverFlowProfileService.cpp;
EngineBridge/src/RiverFlowSourceSelection.h;
EngineBridge/src/RiverFlowGather.inc;
EngineBridge/src/RiverWaterwayRenderer.cpp;
EngineBridge/src/RiverRapidsTransport.h;
EngineBridge/CMakeLists.txt;
Tools/prepare_river_rapids_shading.py(new);
Studio/src/StudioApplication.h;
Studio/src/RiverFlowStudioIntegration.cpp;
Studio/src/TerrainInspectorSections.cpp;
Studio/src/WindowsGameBuildController.cpp(explicit new shader bundle entries only);
Tests/RiverWaterwayTests.cpp;
Tests/RiverFlowSourceSelectionTests.cpp;
Tests/RiverRapidsTests.cpp;
Tests/RiverWaterwayBindingProof.cpp;
Tests/RiverWaterwayNativeProof.cpp;
Tests/RiverFlowRuntimeLifecycleProof.cpp;
HANDOFF.md;docs/ARCHITECTURE.md;docs/FEATURE_MATRIX.csv;this report;G8A owner-acceptance note.

Preservation final: B/preservation-final.json PASS, unexpected_changed[], private Wicked diff/pin/heads/branches and accepted G6/G7C bytes unchanged. Original owner four project files SHA unchanged. Reversible NTFS compression of generated libraries, own relocated package and12 older generated proof archives verifies exact SHA256 before/after; no deletions. Last older-scene pass recovered1,832,976,384bytes; final headroom approximately3.4GiB. Manifests retained.
