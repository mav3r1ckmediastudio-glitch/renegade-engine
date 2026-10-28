# G7D exported river normal reference repair — 2026-10-25

## Outcome

Resolved the relocated standalone missing waterNormalMap.jpg reference. Actual Build Game PASS33s; relocated native DX12 game visually verified75FPS with WATER normals/foam and textured rocks. Runtime resource log has no missing/failed/not-found/error lines; bootstrap package integrity PASS. Runtime closed normally exit0. All four editable owner baseline files remain byte-identical, including scene geometry/settings and registry. Studio remains the private Drawing Candidate, PID17628; creator-launch.json authoritative.

## Implementation

BuildStageService has an optional cookProjectCopy callback after its original source-hash/copy verification and before any manifest generation. It hashes the cooked regular file, so content/package manifests and downstream integrity checks govern the actual cooked bytes. Failure aborts staging and promotion. Default callers retain the original byte-copy behavior.
WindowsGameBuildController supplies a bounded native scene cook. It deserializes a detached source scene, identifies only WATER normal-map references fs::equivalent to the explicitly governed built-in map, and writes their package-relative paths into the staged copy. Custom maps are not matched by basename. The source archive directory preserves unrelated scene-relative references. Archive discard RAII prevents source writes even on serialization exceptions. Native WATER archive mask retained; no shader or Wicked changes.

Changed code: EngineBridge/include/renegade/bridge/BuildStageService.h; EngineBridge/src/BuildStageService.cpp; Studio/src/WindowsGameBuildController.cpp; Tests/BuildStageTests.cpp. Backups in evidence/before. Source HEAD11bbb90e4214776d5e51c8bc4cba2e7f1ccde296 remains dirty; no commits/push/merge.

## Commands and evidence

- powershell.exe -NoProfile -ExecutionPolicy Bypass -File B/build-export-remap.ps1
- cmake --build I/BUILD/g6 --config Release --target RenegadeStudio RenegadeBuildStageTests --parallel 2
- ctest --test-dir I/BUILD/g6 -C Release -R ^RenegadeBuildStageTests$ --output-on-failure
- Final build exit0; tests1/1PASS .38s; combined final command12.536948s. build.log/tests.log/build-result.json retained.
- New test cases: cooked scene bytes receive new manifest hashes; source scene unchanged; post-cook tampering rejected; refused cooking aborts before promotion. Closed test fixture read handle before Windows cleanup (initial test failure fixed).
- Initial attempts hit librarian C0000006/C000007F disk-space failures and a ComponentManager iteration compile error; fixed iteration and regenerated only interrupted build-cache library from retained objects. NTFS-compressed explicitly owned generated files with SHA256 unchanged, no owner/accepted artifacts removed. Interrupted logs retained. Final build.log contains no linker exception.
- Actual Studio Story Flow BUILD GAME on private RiverV2Review completed33s, automatic native DX12 smoke/isolation/test parity PASS. Evidence build-complete-33seconds.png and relocated-package.json/build_report. distribution_ready remains false; this is engineering validation, not a full release approval.
- C:/Python314/python.exe -X utf8 B/launch-remap-relocation.py moved actual exported package to evidence/relocated/Deep/RiverV2Review and verified every file hash. Launched RiverV2Review.exe dx12 from evidence/unrelated-working-directory. Original published path absent after move.
- C:/Python314/python.exe -X utf8 B/verify-remap-live.py verified package bootstrap root/integrity, zero resource errors, unchanged four owner files and exact cooked-scene manifest SHA256. RuntimePID27036; normal CloseMainWindow exit0.
- Visual evidence relocated-water-rocks-foam-75fps.png. Retained RuntimeBootstrap.log, WickedRuntime.log, validation.json, runtime-exit.json. Owner scene SHA cef23f9afec41d0c7ed8ea70a06369a7e49bec073cc29e127e1c5ae9ac23f156; cooked scene SHA5a8e9e73186fab328131c7df482b94608c5f6dc784db06c29fcd79d8e1477277.

I=C:/Users/paulw/source/repos/renegade-river-v2-integration-20261023
B=C:/Users/paulw/source/repos/renegade-spline-water-20261020/BUILD/river-v2-G7D-20261024-authoring
Evidence=B/normal-export-remap-20261025

## Limits / next work

This closes the one observed exported built-in river-normal reference failure. It is not a general remapper for arbitrary outside-project/custom assets; existing outside-project guards remain. Original development files remained available; relocation plus package-relative resource paths/log validation was tested, not OS-level file-access denial. Prior save/reopen, TestLevel, CPU and GPU controls tests remain valid; rendering/solver code unchanged. Wake turbulence disabled until real wake/mixing implementation. Mountain/height/rapids remain paused; no full G7D gate acceptance or release declaration. Protected builds and Wicked diff must remain unchanged.
