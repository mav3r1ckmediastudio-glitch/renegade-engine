> Follow-up: the normal-reference resource-closure failure recorded below is now fixed and relocation verified. See RIVER_V2_G7D_EXPORT_NORMAL_REPAIR_2026-10-25.md. Historical results retained.

# G7D consolidated lifecycle validation — 2026-10-25 DESIGN

All remaining lifecycle checks executed in one work session. Export portability is NOT fully passed. No release closure or independent acceptance claimed.

## Results

- Save/reopen PASS: saved latest owner edits, confirmed clean tab; reopened native river/foam and both textured rocks at75FPS. Evidence save-reopen-rock-textures-75fps.png.
- Actual Play/Test Level PASS: private Runtime PID5868 launched against Intermediate/TestLevelSnapshots/1792884015796167-0000000000/TestLevel.renegade; textured rocks, WATER and foam75FPS. Closed only test Runtime; Studio displayed TEST LEVEL COMPLETED. Evidence test-level-rock-textures-75fps.png.
- First actual Build Windows Game correctly refused outside_project: native river normal map from development Content/river/waterNormalMap.jpg. Registered that exact existing built-in in Studio/src/WindowsGameBuildController.cpp governed bundle list. No general outside-project guard weakening. Generated private BuildInputs licence/readme files staged; export-support.json records hashes. This bundle declaration fixes collection, but DOES NOT repair old serialized material reference.
- Studio build PASS: powershell.exe -NoProfile -ExecutionPolicy Bypass -File B/build-normal-export-fix.ps1; underlying cmake --build C:/Users/paulw/source/repos/renegade-river-v2-integration-20261023/BUILD/g6 --config Release --target RenegadeStudio --parallel 2. lifecycle-20261025/build-normal-export-fix.log records successful output. Runtime unchanged.
- Build Windows Game retry completed23s. build-report.json status gate5_validated_for_final_path; DX12 smoke/test-all parity/isolation passed_gate4, safe rebuild passed_gate5, distribution_ready=false. Duplicate dependency warnings retained. Automatic smoke did NOT catch the missing normal reference; do not equate its PASS with complete resource closure.
- Relocation/integrity PASS: moved published package (no duplicate) to lifecycle-20261025/relocated/Deep/RiverV2Review; all hashes identical. launch-relocated-export.py launches RiverV2Review.exe dx12 from unrelated-working-directory. PID16100, bootstrap package integrity PASS, Story Flow entered River from relocated GameData. Visual75FPS with textured rocks, water and foam. Closed normally exit0. Evidence relocated-standalone-rock-water-foam-75fps.png; relocated-RuntimeBootstrap.log.
- Relocated resource closure FAIL: retained WickedRuntime log has exactly one File not found warning for legacy development-relative waterNormalMap.jpg under the relocated scene path. Bundled file exists but the material reference was not remapped. Visible normals on other materials do not prove this affected river material is correct. Needs a bounded packaging/material-reference normalization fix and a repeat relocation/log check. Original development files remained available; this was not OS-level file-access-denial verification.

## Preservation and existing engineering checks

Scene, asset metadata and descriptor SHA256 unchanged from saved latest-owner baseline. AssetRegistry refreshed during Build Game to add recovered big_rock PNG dependencies/reusable asset graph; registry-after-export.diff records exact expected changes. Do not restore old registry or overwrite owner scene.
Previous final controls checks PASS: Studio+Runtime build0/41.74s;2 CPU tests1.80s; GPU proof0/22.05s. Native bank maxerror7.62939e-6m; frozen contact edit retains exact foam history; inlet speed commit/Undo each rebuild once;30 animated frames zero domain rebuilds, Prepare mean.0727ms.
Final preservation audit audit-cache-and-caps.py: protected G6/G7C hashes true, repo heads/branches preserved, Wicked pin and private diff preserved; no unexpected source changes; git diff --check0. No commits, pushes or merges.
Wake turbulence remains disabled/unimplemented; mountain streams/height/rapids paused. Private Drawing Candidate remains usable; exported package is test evidence, NOT an accepted release.

B=C:/Users/paulw/source/repos/renegade-spline-water-20261020/BUILD/river-v2-G7D-20261024-authoring
