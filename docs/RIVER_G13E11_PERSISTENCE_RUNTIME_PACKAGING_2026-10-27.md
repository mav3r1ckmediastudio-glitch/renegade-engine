# G13E11 — Combined waterfall + river native persistence, Windows game build and relocated DX12 verification
Date 2026-10-27, DESIGN. Branch `feature/river-v2-g6-20261023`, HEAD `11bbb90`; owner G13C appearance, G11 water art and owner-accepted G13E10 width UX preserved. No merge, commit, push, reset, stash or public redistribution.

## Confirmed green gates
- Fresh independent native WISCENE open of an exact copy of the G13E9 accepted project: `BUILD/waterfall-placeable/evidence/G13E11-Verification/fresh-reopen.exit=0`. One actual G13 waterfall, one native G11 river, one native terrain, six spline points, second node scale X 1.5, UUID-anchored source whitewater, no fake plunge-pool quad.
- Expanded `Tests/WaterfallRuntimeLifecycleProof.cpp` with test-only `--combined`: asserts real river/terrain, linked native bed, node width and stable foam source before and after Test Game snapshot and native scene save. Snapshot leaves history/path unchanged and produces no duplicate emitters. Only disposable fixture cliff/water textures are copied project-local; no production asset security guard altered.
- Actual Windows Game Build on **short isolated path** `BUILD/G13E11/Project`: `lifecycle.exit=0`; Windows Gate 5 report status `gate5_validated_for_final_path`, DX12 Gate 4 smoke pass and package isolation pass, `distribution_ready=false`. Packaged path: `BUILD/G13E11/Project/Builds/Windows/G13E11 Combined Windows Build`.
- Package copied to `BUILD/G13E11/RelocatedWindows`, completely separate executable location. `relocated-flow-smoke.exit=0`, RuntimeBootstrap log confirms DX12 STARTED, package integrity PASS, complete-game terminal reached, smoke_status PASS. An initial invocation without needed `--flow-outcome=level.complete` returned 27 (incomplete Story Flow), then correct outcome passed. Do not erase earlier failure.
- Independent relocated native scene render: `BUILD/G13E11/relocated-native.exit=0`, `relocated-native.log`, `relocated-native.png`. Native waterfall renderer authored=1 active=1 emitters=19, frame-240 image captured, every listed external/material texture valid=1 and relocated under package path. Quick process working-set sample 11 measurements: ~1543 MB peak. This is **system RAM process working set, not GPU VRAM, gameplay FPS or a long endurance benchmark**.
- Regression of production staging/identity changes on separate new short-path fixture `BUILD/G13E11b/Project`: `BUILD/G13E11b/lifecycle.exit=0` again, actual Gate5 promoted. No modification to previously validated G13E11 package.
- Built missing `RenegadeGate4SceneFixture.exe` target, then targeted CTest `RenegadeBuildStageTests`, `RenegadeGate4SceneFixtureTests`, `RenegadeWindowsGameBuildProjectTests`: 3/3 PASS, `BUILD/waterfall-placeable/evidence/G13E11-Verification/ctest-stage-r2.exit=0`. Previous combined 27-test run yielded 25 passes + 2 **Not Run** when fixture executable was absent; do not represent it as 27/27 green. River/waterfall suite had previously passed 24/24 in G13E10.
- `git diff --check` exit 0 (nonfatal existing CRLF notices).

## Windows long-path caveat — OPEN / not a release blocker for short-path fixture
The identical combined scene in nested `BUILD/waterfall-placeable/evidence/G13E11-Verification/Project` initially failed Windows staging due long `Content/terrain/default_grass` destination (about 282 characters). Short-path fixture (about 225) built successfully.
- `EngineBridge/src/BuildStageService.cpp` now applies existing `detail::FileIoPath` extended Windows I/O to nested output directory/file creation and tree enumeration; all hash checks, non-symlink checks and canonical destination collision rejection remain.
- `Studio/src/WindowsGameBuildController.cpp`: package-local cooked WISCENE save uses extended I/O path, source remains untouched.
- `EngineBridge/src/BuildIdentityService.cpp`: staged output validation/read/write and executable resource BeginUpdateResource use same local extended paths.
- Deep-path retests progressed from Stage 2 copy failure to cooked scene save, Stage 2 validation, and Gate 3 stamping. **Latest `longpath-r5.exit=1`: Windows EndUpdateResourceW returned error 122 during identity stamping.** This remains unresolved; do not claim arbitrary-length project builds work. The short-path build and post-change short-path regression are both green. Preserve failing evidence in `BUILD/waterfall-placeable/evidence/G13E11-Verification`.

## Still open / acceptance
1. Owner Test Game close-up review of the landing/river and after actual save/reopen, plus real gameplay frame pacing / VRAM and longer session under moving camera/obstacles. The automated native process is not equivalent to owner gameplay FPS.
2. Safe packaging from arbitrarily nested long project paths; Gate 3 Win32 error 122 requires a separate bounded platform fix, not an altered G13C waterfall or unsafe file verifier bypass.
3. **2026-10-28 owner clarification:** waterfall rocks and associated artwork were originally created for Renegade with the assistant; no third-party licensing concern is asserted. `distribution_ready=false` is a technical non-release flag for the fixture, not a copyright finding. Ordinary public-alpha release QA remains separate.
4. Scoped integration review of extensive dirty source and explicit owner approval before merging into main.

## Latest handoff
Continue from owner-accepted G13E10 and this G13E11 evidence. Keep work on `feature/river-v2-g6-20261023`, preserve any owner windows, content and unrelated source. Do not rework the waterfall appearance or width controls. Do not claim the remaining long-path / performance / rights gates are closed.

## Final focused regression check
Built the missing Gate4 fixture, then reran the full focused staging + native river + waterfall + snapshot CTest selection: **27/27 PASS**, 14.33s, `BUILD/waterfall-placeable/evidence/G13E11-Verification/ctest-final-27.exit=0` and `ctest-final-27.log`. This supersedes the earlier 25-pass/2-not-run test session, but not the still-open long-path Gate3 Win32 error 122.
