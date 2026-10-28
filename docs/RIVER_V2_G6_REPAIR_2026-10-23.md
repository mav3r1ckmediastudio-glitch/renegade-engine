> 2026-10-24: owner accepted this repaired G6 candidate: "it works beautifully". Earlier OPEN labels below are historical. Remaining untested limits are unchanged.

# River V2 G6 acceptance repairs — 2026-10-23

G6 owner acceptance remains OPEN. This supersedes the original G6 report's candidate path and incomplete visible Runtime evidence. G7 and mountain streams remain paused. No commits, pushes or merges.

## Reported failures and fixes

The desktop review scene still referenced imported PNGs under an older disposable fixture. Earlier export adoption had repaired a different project copy. Corrected review: `C:\Users\paulw\source\repos\renegade-spline-water-20261020\BUILD\river-v2-G6-20261023-2135\g6-review-repair\RiverV2Review\RiverV2Review.renegade`. It was copied from the owner's last saved review, with transforms, native spline geometry, widths, metadata, persistent IDs, two lights and one camera preserved. Eight texture payloads were adopted by exact basename; duplicate water-normal candidates were byte identical. All 21 populated native material texture slots have valid GPU resources. The original review project was not overwritten; unsaved edits are not inferred to be part of this copy.

The actual Runtime baseline repeatedly spent 725–730 ms in RiverFlowRenderer::Prepare. Animated foreground equipment and above-water objects invalidated the static contact fingerprint every frame; global object count also invalidated for unrelated additions. CPU field/contact rebuilds caused the collapse, rather than WATER shader cost.

New EngineBridge/src/RiverFlowSourceSelection.h supplies the SAME membership predicate to fingerprinting and gathering. Excludes foreground, hidden, skinned/morphed, dynamic rigidbody/character ancestors, WATER and out-of-corridor objects. Native world bounds are checked each frame, so a real static rock entering the water is detected. Terrain above the plane remains eligible for solid-bank classification. Removed global object-count invalidation. No HLSL, WATER optics, foam quality, texture resolution or simulation-quality reduction.

Runtime/src/RuntimeApplication.{h,cpp} and RuntimeLiveDiagnostics.cpp expose read-only Prepare duration, cached rebuild count and submitted frame count via existing diagnostics. No GPU readback is added.

## Final verification on DESIGN

Evidence root: `C:\Users\paulw\source\repos\renegade-spline-water-20261020\BUILD\river-v2-G6-20261023-2135`. Scripts contain exact absolute commands, working directories, arguments and output paths.

- `powershell -NoProfile -ExecutionPolicy Bypass -File cache-fix-final-build.ps1`: Release Studio, Runtime and proof targets EXIT 0. Subsequent changes only removed abandoned manual proof targets and updated docs.
- `powershell -NoProfile -ExecutionPolicy Bypass -File repair-ctest-final.ps1`: EXIT 0; 84/84 CTests, including new source selection tests (14 checks). Source-contract and importer/snapshot/runtime regressions included.
- `powershell -NoProfile -ExecutionPolicy Bypass -File cache-regression-final.ps1`: actual native DX12 cache regression PASS. Moving foreground for 60 frames, removing it and moving an above-water rock do not rebuild. Rock entering water, moving and removal each rebuild once. Foam remains finite and positive. Final rebuilds 1→2→3→4; capture cache-regression-final/cache-regression.png.
- `python sample-fixed-testgame.py`: actual TestLevelSnapshotService and Runtime process lifecycle PASS, readiness, stop cleanup, return-edit and Undo. After initial warm-up 73.927 FPS with zero repeated static rebuilds. Full samples in runtime-perf-fixed-testgame.json and raw-v2.json.
- `python check-visible-river.py`: actual maximised Runtime, client 1920×1009, visible downward river capture inspected. 75.32–75.55 FPS, Prepare 0.016–0.048 ms, zero rebuilds across measured views. Actual downward pitch ~0.619 rad; requested view labels are not calibrated camera orientations. runtime-fixed-visible-v2.log/json and runtime-fixed-down.png show textured rocks and retained WATER/foam.
- `powershell -NoProfile -ExecutionPolicy Bypass -File package-repair.ps1`: actual BuildActiveWindowsGame EXIT 0; package promoted under corrected project's Builds/Windows. Final Runtime, adopted texture closure and owned river CSOs included. Package hash verification is recorded separately in preservation-repair-final.json.
- `python deploy-repair.py`: candidate deployment PASS; Studio and both Runtime locations match final build hashes. Only candidate recent-project section prepended corrected project, retaining previous entries and all settings. Previous binaries/INI retained in candidate-before-repairs-v1; hashes in candidate-repair-deployment.json. Same desktop shortcut: Renegade River V2 G6 - Editable Studio.

## Discarded attempts and remaining limits

A manual render staging harness failed native graphics initialization (compute PSO allocation/device loss); no timing from it is accepted. A focus-interrupted first visible sampling run is also discarded. Replaced by the actual Runtime and Test Game measurements above. Both logs and staging source are archived under failed-runtime-render-harness.

The one-off texture repair utility checked all 21 slots and produced the repaired archive, but three versions crashed during process teardown (0xc0000005); these are NOT full passes. Its source and CMake target have been removed from the integration source and archived under texture-repair-helper-provenance. Fresh native render proof and actual full Runtime independently reopened and rendered the corrected project successfully; final public Build Game succeeded. These utility failures do not justify claiming every graphics lifecycle is certified.

Cold shader/first-field initialization may take around ten seconds. Genuine static geometry or river edits still synchronously rebuild (~0.7 seconds for this fixture); interactive dynamic/deforming obstacle support, connected waterways, device-loss recovery and uncapped GPU benchmarks remain unverified. 75 FPS is the measured capped fixture result, not a general performance guarantee. Owner G6 manipulation/Undo/reopen and visual acceptance remain OPEN.

Original source/owner projects and Wicked modifications preserved; separate final repair audits record exact hashes and unchanged HEADs.

Final handover: repaired candidate Studio PID18732 opened the corrected project and River scene through the real Project Hub/Journey UI, with zero undo entries; left open for owner. Screenshot candidate-repair-level-loaded.png records native scene, textured terrain and 75FPS. repair-final-source-check.ps1 reconfigured final CMake after removal of transient helpers and git diff --check EXIT0. owner-review-preservation-repair.json confirms every original review input file remains unchanged. Preservation audit: original river1562 checked (only HANDOFF.md navigation changed), combined1468 unchanged, native Wicked2143 identical to private copy and original dirty patch equals G5 baseline; final package has no hash errors.
