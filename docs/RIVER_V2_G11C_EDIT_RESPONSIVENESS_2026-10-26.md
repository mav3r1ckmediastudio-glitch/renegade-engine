# G11C course waves and obstacle edit responsiveness
2026-10-26. Integration HEAD 11bbb90e4214776d5e51c8bc4cba2e7f1ccde296 plus preserved dirty files. No commit/push/merge.

Owner reports regular full-width humps and roughly one-minute freezes when placing or moving a rock in the G11B candidate. The course motion field assigned phase only from along-course travel, feeding a transverse coherent sinusoidal displacement. Course motion displacement is now disabled; existing detailed flowing normal-map sampling, native WATER optics, smooth cascade geometry, landing/downstream foam and concealed banks/caps remain.

Static-source changes previously synchronously rebuilt classification, measured depth and pressure projection from Prepare. Source-only course edits now gather immutable world-space triangle snapshots and submit CPU-only domain construction to a promise-owned worker. No live scene, Entry or GPU references cross that boundary. The previous valid rendered domain remains active while working. Only a result matching the latest source fingerprint is published; stale results are discarded and the newest state queued. Source failures are reported without retrying the same failed fingerprint every frame. Worker lifetime is independent of scene/renderer destruction. Source capture and final GPU publication remain on the owning thread. Initial/structural builds remain synchronous.

The pressure matrix now caches its invariant face conductance once instead of repeating harmonic-depth divisions inside every iterative multiply. Creator asset drag preview/cleanup additionally suspends river preparation, matching existing gizmo suspension. No new UI or serialized schema.

Changed: EngineBridge/src/RiverCourseRenderer.cpp; EngineBridge/src/RiverGradedTransport.h; Studio/src/StudioApplication.cpp; optional Tests/RiverCourseRockProof.cpp and its Tests/CMakeLists.txt registration.

Evidence: original spline-water BUILD/river-v2-G11C-20261026-regressions.
- Separate Release Studio/Runtime build PASS (build-fix.log).
- 19/19 rebuilt river regressions PASS, 13.21 seconds (regressions-final.log). Initial attempt was Not Run because the redirected output directory did not yet contain rebuilt test binaries; retained regressions.log is NOT pass evidence.
- Real terrain bank/cap, node edits/Undo/Redo, native regeneration recovery, foam amount/history, zero-speed and native save proof PASS (bank-checks.log). All 90 upper-edge terrain ray checks retained; ~76.94FPS steady scene.
- Fresh process native reopen PASS, authoredNodes=7 (fresh-reopen.log).
- Actual big_rock_1b and new rock payload: 15,672 vertices / 24,648 triangles. Three repeated obstacle moves publish updated flow, ~1.07-1.15seconds completion with maximum frame ~0.122seconds (fixed-rock.log). New-rock final test changes pose again while work is pending: stale result discarded, exactly one final publication, ~2.05seconds total and max frame ~0.119seconds (new-rock-final.log).
- Test Game real Runtime held 30 seconds; readiness, return edit/Undo, snapshot identity and cleanup PASS (lifecycle.log).
- course.png and course-overview.png under bank-checks inspected: no regular transverse humps; smooth fall/foam and concealed endpoints retained.
- Initial baseline asset-placement harness failed on copied manifest/registry project identity mismatch. Final reproduction reads the validated native RASSET payload directly, in memory; owner assets are never rewritten. baseline-rock.log is NOT baseline timing evidence.
- Old G11B Studio SHA256 remains 03a752f66e5246e9ca1b5dc6888b59e599c2f09e45fb868548cdc8c257370c23.

Owner live scene and old editor are preserved. New review is a separate on-disk project copy; no owner save/reset/close. Owner visual and actual mouse drag acceptance remain OPEN. This does not claim exact reproduction of every one-minute stall: the measured isolated scene verifies the corrected blocking path with the actual mesh. Cold/structural synchronous builds, capture/publication hitch, previous-flow rendering while worker runs, existing elevated-course/reflection/width/static-bed limits remain. No general wake/free-surface solver or release certification. Mountain streams remain paused.

Public Build Game/package startup smoke PASS: package-final.log. Initial package.log failed because the new output directory lacked the governed Runtime subfolder; copied existing support resources into the new candidate and replaced Runtime EXE and river Content with the newly built versions. Old candidate was not modified. Package created only under the separate repair-review project.
