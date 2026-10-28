# G13E12 — Deep Windows project-path packaging and relocated native Runtime verification
UK session overnight 27–28 October 2026, DESIGN. Isolated worktree `feature/river-v2-g6-20261023`, original HEAD `11bbb90`. No commit, push, merge, reset, stash or owner-scene overwrite.

## Owner-approved boundaries
G13C owner-approved asymmetric, collision-guided waterfall and restrained non-spinning mist unchanged. G11 native river shading/foam unchanged. G13E10 node-width gizmo and inspector already accepted by owner; unchanged in this gate. Test fixtures use separate project copies and non-redistribution-cleared rock art.

## Previously failing deep-path scene now PASS
- Exact disposable `BUILD/waterfall-placeable/evidence/G13E11-Verification/Project/Waterfall.renegade` under deep repo path. Release `RenegadeWaterfallRuntimeLifecycleProof.exe ... --combined` verifies one authored native waterfall, one G11 river, one native terrain, six spline nodes, selected node width scale 1.5, stable UUID-anchored whitewater, linked bed, no fake pool. Save and Test Game snapshot retain all plus history.
- Most recent **long-path** proof: `BUILD/waterfall-placeable/evidence/G13E11-Verification/longpath-r14.exit=0` and `longpath-r14.log`. Actual governed Windows Game Build Gates 1–5 succeeded, promoted to `BUILD/waterfall-placeable/evidence/G13E11-Verification/Project/Builds/Windows/G13E9 Waterfall + River Review Windows Build`.
- Final build-report: `status=gate5_validated_for_final_path`, `smoke_test=passed_gate4`, `package_isolation=passed_gate4`, `safe_rebuild=passed_gate5`, `distribution_ready=false` as required for a technical fixture.
- Earlier r5/r6/r7/r8/r9/r10/r11/r12/r13 failures are retained for diagnosis; **r14 is the first successful deep-path end-to-end proof**.

## Scoped fixes, no security bypass
1. `EngineBridge/src/BuildStageService.cpp`: extended Windows I/O for nested staging file/directory creation and actual-file tree enumeration. Hash verification, safe relative names, symlink and collision rejection preserved.
2. `Studio/src/WindowsGameBuildController.cpp`: package-cooked WISCENE save uses extended I/O. Smoke launch on staged executable beyond legacy MAX_PATH uses OS-reported short alias (8.3) of the SAME existing file, not a copied substitute; path must resolve to same non-symlink regular executable, short enough for CreateProcessW, else actionable fail. DX12 smoke job containment unchanged.
3. `EngineBridge/src/BuildIdentityService.cpp`: for deep paths, Win32 resource stamping uses an OS-unique temporary short file before copying the stamped bytes back to the governed staged executable; Stage3 digest is refreshed and checked. Avoids EndUpdateResourceW error 122 at long destination.
4. `Runtime/src/RuntimePackageBootstrap.cpp`: strict same-file identity check (extended-path `std::filesystem::equivalent`) permits 8.3 process aliases only when they refer to the exact manifest-named, non-symlinked executable within the integrity-validated package. Ordinary basename check still applies. Initial failed runtime alias tests used an OUTDATED bundled Runtime, now corrected.
5. `Studio/src/WindowsGameBuildController.cpp` Runtime source selection now prefers freshly compiled sibling `RenegadeRuntime.exe` in CMake output ahead of a potentially stale `Runtime/RenegadeRuntime.exe` bundled copy. Packaged Studio still falls back to its governed `Runtime/` copy. This was the underlying blocker in r9-r12: the package repeatedly contained a stale binary.

## Independent relocated execution
- Copied final promoted Windows game to `BUILD/G13E12/RelocatedWindows`, leaving original intact. `BUILD/G13E12/relocated-smoke.exit=0`: package integrity PASS, DX12 graphics STARTED, Story Flow to Complete Game, native package-relative launch.
- `BUILD/G13E12/relocated-native.exit=0`, `relocated-native.log`, `relocated-native.png`: frame-240 DX12 image captured, `authored=1 active=1 emitters=19 capture=1`; all reported material resource references valid=1 inside relocated game. Rendered test fixture, not owner gameplay scene.
- Source-internal paths not required at Runtime; only project fixtures are packaged, no source project altered and public distribution is NOT cleared.

## Regression
- Built `RenegadeBuildPromotionTests`, `RenegadeBuildStageTests`, `RenegadeWindowsGameBuildProjectTests`, `RenegadePackageBootstrapTests`, `RenegadePackageIntegrityTests`, `RenegadeStandalonePackageTests` on current source, Release, build exit0.
- Focused CTest selection now **39/39 PASS**, including all nine Gate5 promotion/rollback/incomplete/lock/recovery cases, package bootstrap/integrity/standalone, snapshots, river/foam/waterline/rapids, and waterfall tests. `BUILD/G13E12/ctest-regressions.exit=0` and `ctest-regressions.log`; total 32.99s.
- No new river/waterfall artistic changes, no main merge.

## Remaining separate acceptance
- Owner review in actual Studio Test Game camera and close-up placement, including junction water appearance; screenshot of a disposable fixture is not final visual sign-off.
- Real gameplay frame pacing, GPU VRAM/long-session performance with camera moving and 361 native terrain chunks; 240-frame screenshot alone is not a performance test.
- **Asset provenance clarified by owner (2026-10-28):** the waterfall artwork, including the fixture rocks/textures used in this proof, was originally created for this Renegade work by the assistant, not licensed from third parties. No waterfall-art licensing blocker is asserted. `distribution_ready=false` is the Windows test/package gate's technical non-release state, **not** a finding of missing artwork rights. Public shipping still requires ordinary release QA/approval.
- Before any scoped merge: independent diff audit against extensive pre-existing dirty worktree and explicit owner approval.
