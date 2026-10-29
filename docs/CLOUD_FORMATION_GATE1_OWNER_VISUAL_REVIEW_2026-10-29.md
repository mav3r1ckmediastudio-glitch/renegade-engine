# Gate 1 cloud visual review — DESIGN — 29 October 2026

**A real Studio visual test is open; Gate 1 remains UNAPPROVED.** This is a preview, not a merge candidate.

## Run / return to review
- Running review executable: BUILD/gate1/Studio/Release/RenegadeStudioCloudReviewG1Oct29.exe, launched with --cloud-body-preview --dx12; review PID at setup 15116.
- Current isolated review project: BUILD/cloud-body-studio-preview/CloudBodyStudioPreview.renegade, Main Level, Environment > Scene Weather.
- To relaunch after closing: double-click BUILD/Run-Cloud-Gate1-Visual-Review.cmd; choose Enter Hub, then Cloud Body Studio Preview, open project and double-click Main Level from Story Flow.
- The review executable is a separately named copy of this worktree's newly linked Release Studio; the original project/other owner Studio binaries are untouched.
- Test Time of Day at midday, sunset and midnight. Clouds > CUMULUS supports Coverage, Base and Depth. Moon > Moon movement has Follow Time of Day or Fixed Position; an enabled automatic Moon can be outside the camera. To check lunar disc set a visible above-horizon Fixed Position and point camera toward it.
- The review scene initially showed 80% Cumulus, base 2280 and depth 3190, daytime 11.67 h and nighttime 0 h rendered in the actual Studio, 50–56 FPS observed on DESIGN at 1920x1080. Frame-rate readings are not controlled performance benchmarks. The user may have changed settings since the screenshot.
- Real GUI screenshot evidence: BUILD/visual-preview-level-viewport.png (night), BUILD/visual-preview-studio-daytime.png (day), BUILD/visual-preview-environment-inspector.png (visible inspector); generated 1920x1080 GPU images BUILD/visual-preview-new-morphology-dx12.

## What changed this continuation
- Tools/night_sky/renegadeCloudBodyTemplate.hlsli has connected 3D thermal-body domain displacement, per-updraft unequal rise (break repeated horizontal tiers), adjusted radial centres, and multiscale subordinate detail, applied in the same baked cache as camera and shadow paths. Previous exact authored source backed up as BUILD/cloud-visual-preview-pre-morphology.hlsli.
- Cumulus complete-body hybrid spatial placement, 4x4 bounds and corrected CMake-to-Wicked generated site displacement stay from the preceding checkpoint.
- Release native NightSky test build exit 0: BUILD/visual-preview-morphology-build.log. Real RenegadeStudio Release link exit 0: BUILD/visual-preview-studio-release-build.log. Focused Release CTest 7/7: BUILD/visual-preview-20261029-focused-ctest.log.
- All original 45 authored source SHA protection and native fallback passed: BUILD/visual-preview-20261029-preservation.log. git diff --check 0. Accepted river Wicked checkout and pin ceb816b844b4ab2fa56a159ac60a4f65c931bdfc intact. No push, commit, PR, merge or changes to unrelated owner project/windows.

## Defects NOT yet accepted
- Distant Cumulus still has recurring layered/horizontal cloud undersides; giant near cloud masses are not fully photographic. Owner should assess in actual Studio before any additional morphology work.
- Four other formation profiles not signed off. 100% full overcast visuals and deep-layer steep-angle variability require visual judgement. Moon cloud scattering is still artistic, and the exact owner's unrelated unsaved Studio scene has not been reproduced; the saved isolated studio Moon outside camera finding is documented in docs/CLOUD_FORMATION_GATE1_HYBRID_MOON_STUDIO_2026-10-29.md.
- No final Debug Studio/Runtime four-app gate, full test matrix, 1440p benchmarks, long wind or packaged release. Do not call Gate 1 complete until owner visually accepts it. No Weather Gates 2–5 or Snow V2.

**Next**: take owner feedback on THIS OPEN preview; address specific visual differences within isolated worktree; then rerun complete validation before discussing PR/merge.
