# Inventory G1–G5 + PR #186 integration validation

Status: LOCAL RELEASE REGRESSION 280/280 PASS; 4 final Studio/Runtime Debug/Release builds PASS; live DX12 river, waterfall and inventory visually approved. Hosted CI/PR/merge pending owner authorisation. No commit, push or merge.

## Provenance and isolation

- Branch: `integration/inventory-river-20261028`, fresh independent worktree.
- Refreshed main: `150eca4b761d3f62f122df3ab5e8536ec084f926` (PR #186).
- Inventory HEAD: `b2da8466c0726ca94becb30445afd0149afce792`; common ancestor `11bbb90e4214776d5e51c8bc4cba2e7f1ccde296`.
- All 84 modified/untracked G5 source, test and documentation files copied to an outside-repository snapshot; source/destination SHA-256 verified. Tracked binary patch, original status, manifest and unrelated Wicked file hashes retained separately.
- 158 inventory paths reconciled from the complete accepted final tree against main using the common ancestor. No temporary commits, cherry-pick commits, stash, reset, clean or changes to original worktree content.
- `.gitmodules` and Wicked gitlink retained from main: `ceb816b844b4ab2fa56a159ac60a4f65c931bdfc`, fork branch `renegade-rocky-riverbed-v1`.
- New Wicked checkout was clean before configuration. Shared superproject submodule configuration initially cloned from historical upstream URL; corrected ONLY the new submodule's origin to the fork. The exact checked-out commit was already correct.
- Configure applies the existing main CMake night-sky/backlog/physics overlays inside the new submodule. These generated modifications are expected; the original unrelated dirty submodule remains protected.

## Conflict decisions

Every conflict had an empty common-ancestor segment (two independent insertions). Kept both inserted implementations, then reviewed surrounding control flow.

| File | Resolution |
| --- | --- |
| EngineBridge/CMakeLists.txt | Retain all river/course/waterway/foam/swimming/underwater and waterfall sources, then all inventory services and adapters. Existing shader/resource generators and deployment helpers retained. |
| Runtime/src/RuntimeApplication.cpp | Keep G13E14 opt-in native-player movement trace declarations and G5 world-pickup frame/modal focus declarations together. Inventory inputs, script gating, pickup pre-upload lifecycle, save/load, combat-health authority and river render lifecycle retained. |
| Runtime/src/main_Windows.cpp | Keep G13E13 opt-in perf probe and G5 explicit health-proof flag. Configure native save directory from LOCALAPPDATA. |
| HANDOFF.md | Retain main environment-probe record and complete inventory history; add current integration status separately. |
| docs/ARCHITECTURE.md | Retain main probe frame-boundary rule and appended inventory architecture. |
| docs/FEATURE_MATRIX.csv | Retain environment-probe row plus inventory row; historical acceptance claims do not certify this integration. |

Auto-merged overlapping files independently inspected: TestLevelSnapshotService (inventory companions alongside waterfall/river snapshot handling), Runtime CMake/header, Studio CMake/header/application, Tests CMake. Studio river width-axis logic, vegetation integration and deferred env-probe creation are retained. Inventory pickup editor and project Screen assignment are additive.

Root CMake's existing river/UI CI dependencies are unchanged. Added all 23 registered inventory/world-pickup/save native test targets to RenegadeStudioCI, so an unfiltered CTest will not encounter newly registered but unbuilt inventory tests.

## Test fixture corrections

Six G5 tools hardcoded the old worktree name. Replaced that pathname test with a shared guard requiring the exact compiled source BUILD directory, expected sandbox leaf, explicit disposable marker and exact known test-project identity. Existing refusal-to-overwrite checks remain. No user project is eligible merely by matching a name.

Package proof now derives Renegade/Wicked revision macros from the integration configure rather than historical G1–G4/old-Wicked constants. Uncommitted integration content must additionally be identified by the final diff/source manifest; HEAD alone does not identify this candidate.

## Builds and checks

Evidence directory: `BUILD/integration-evidence`.

- Working Kitware CMake 4.4.4, Visual Studio 17 2022 generator, x64, fresh `BUILD/inventory-river-validation`, `CMAKE_SUPPRESS_REGENERATION=ON`.
- Initial configure exit 0 (`configure.log`); configure after fixture/CI corrections exit 0 (`configure-final.log`).
- Release RenegadeStudioCI build running, parallel 3 (`release-ci-build.log`). No successful target or test result claimed yet.
- Debug, startup smoke, inventory/river/full regression, combined DX12 gameplay and actual Windows packaging remain pending.

## Acceptance boundary

The full UI Designer audit is out of scope. Historical inventory and river owner acceptance is preserved, but combined build/gameplay/packaging acceptance is not yet established. Story Flow multi-level save/resume, arbitrary item Use, custom world-drop prefabs and rigid-body toss remain deferred.

## Build interruption and disk-space correction

- Initial CMake-driven Release CI build used VS2022 MSBuild 17.14.51. It was deliberately interrupted before completion to switch the serial cold compile to documented MSBuild 18.6.3.22110 with CL=/MP4 and /m:3. Initial log retained as release-ci-build-initial-interrupted.log; no success claimed.
- MSBuild 18 completed Wicked/bridge compilation and began registered test builds, then C: exhausted available space. Multiple tests reported MSVC C1085 'No space left on device'. Remote process completion reports exit 4294967295. This is a FAILED build, not a passing partial run. Log: release-ci-build-msbuild18.log.
- Recovery moves ONLY this integration's generated inventory-river-validation directory to the spare E: volume, retaining its exact logical C: path via a junction. Original project/source/cache root and generator stay identical. Every generated file is SHA-256 checked before removing its duplicate from C:. The 1.74 GB bridge library was verified and removed from C: early to restore free space while remaining files copied; its exact E: copy is retained. No original worktree files, user assets, other builds or backup snapshots are removed.
- Canonical README and UPSTREAM_SYNC documents still described the historical official Wicked baseline despite PR #186's fork pin. Corrected their active pin authority to main's existing fork; historical baseline notes retained.

## Additional integration validation under investigation

Static review found Test Level projects only project inventory stack/permission companions; their G5 catalogue, assigned Screen and icon closure may be omitted. The new combined native proof now explicitly checks these files in the real snapshot. This is not yet a reproduced failure or a correction claim; verify before changing production code.

## Reproduced Test Level G5 closure defect and correction

- Build relocation completed: all 4,105 generated files SHA-256 verified; logical cache/source paths unchanged through the junction; C: regained 7,179,776,000 bytes immediately after relocation. Full hash ledger retained on the build volume.
- New combined proof initially failed C++ compilation (C2668 ambiguous TransformComponent::Translate initializer list). Corrected the proof to pass an explicit DirectX::XMFLOAT3. Initial compile log retained; corrected compile exit 0.
- Running the compiled proof on the copied 81-chunk combined scene preserved the authored river/waterfall/terrain and stable pickup through native save/reopen, then FAILED with 'Snapshot omitted G5 input: Content/Gameplay/Inventory.renegade-items' (combined-author-before-correction.log). This reproduces a real gameplay lifecycle gap, independent of textual merge success.
- Corrected TestLevelSnapshotService to discover G5 content through the existing InventoryDependencyProvider and RuntimeScreenDependencyProvider. It copies the catalogue, item icons, assigned Screen and its complete resource/theme closure into the disposable shadow project; existing renamed stack/permission companions remain under their original service. Project identity, governed Content paths, dependency diagnostics and normal failure cleanup remain enforced. Inventory-free projects remain optional.
- Extended existing TestLevelSnapshotRuntimeTests with byte-for-byte G5 Screen/catalogue/icon/stack parity, assignment identity, missing-icon refusal, foreign catalogue refusal, staging cleanup and original author scene/history preservation. No existing assertions or test registrations removed.
- Restored only the newly created disposable combined fixture's scene from its river baseline for a clean author-proof rerun. Original accepted river/inventory projects unchanged.
- Corrected Release CI rebuild now running: release-ci-build-recovered.log. Final proof and tests pending; no pass inferred from the implementation.

## 28 October, 17:25+ — owner-reported raised river plane in combined Test Game

- Owner screenshot of combined DX12 Runtime showed the G11 surface visibly raised over the native carved terrain. Existing proof only counted 1 terrain, 1 spline/6 nodes and 1 waterfall: it did **not** compare surface height with banks.
- Added read-only `--diagnose` mode to the existing disposable-only `Tests/InventoryRiverCombinedProof.cpp`. Direct saved-scene inspection: 81 native terrain chunks, river layer linked, depth=1.25 m, bank=2 m; all six spline-node world heights initially +0.015 m, while uncarved surrounding ground was about -0.55 m. The 0.565 m relative height caused the raised-water appearance; not a new shader/render fork regression.
- Backed up ONLY the disposable proof scene byte-for-byte at `BUILD/integration-evidence/river-fixture-prealignment-20261028.wiscene`, SHA256 `AEB94979873A4DCA6EE37FA7456FDFCCA85057BA9CE2E454F7FF4C8718D07F02`. No original river, inventory, user scene, or Wicked source changed.
- Added guarded `--align-fixture` mode for that fixture: checked existing G5 author marker/one linked watercourse/six unchanged node heights, moved all six spline nodes down 0.535 m via native SetTransformCommand, called RecomposeRiverTerrainLayers, saved/reopened successfully. It is strictly a disposable test-scene correction, **not a global engine default change**.
- After read-back: node surface height -0.520 m; nearest carved channel floor approximately -1.69 to -1.77 m, unchanged 1.25 m linked depth, 81 chunks and the existing waterfall preserved. Evidence: `river-alignment-run.log`, `river-after-alignment.log`.
- Added guarded `--refresh-snapshot` mode and regenerated actual native Test Level snapshot (with inventory catalogue, assigned Screen, potion icon) rather than reusing the outdated pre-alignment shadow project. `river-refreshed-snapshot.log` PASS.
- Opened new Release DX12 Runtime from refreshed snapshot; `aligned-live-runtime.png`: 50/100 starting health, water aligned with terrain banks, ~75 FPS. Visual remains a deliberately bare/flat demo scene, not final river-art approval.
- Retested one E pickup (3 potions in backpack), I to open Screen, right-click Drop One (stack 3->2 and real world item notification), right-click Use (health 50->100; stack 2->1), and full-health Use refusal (health stays 100; stack stays 1; explicit POTION KEPT). Evidence `aligned-after-pickup-inventory.png`, `aligned-inventory-healed.png` (drop), `aligned-inventory-healed-confirmed.png`, `aligned-inventory-full-health-refusal.png`. This runtime remains a test, not feature/PR sign-off.
- Broader Release CTest previously finished 267/280 passed, 13 failures (`release-full-ctest.log`): projectile Test Level optional-inventory snapshot failure, plus 12 build-stage/package-integrity/standalone/promotion-fixture failures. These are NOT green and must be investigated without weakening package integrity checks; they are not explained away by the water-fixture correction. Release selected inventory and river tests, Studio startup, and the tested package path had passed independently. Debug CI build was ongoing at last review.
- No commit, push, PR or merge approved/performed. Main PR #186, approved inventory source, the original dirty night-sky Wicked checkout, and the required river-pinned fork remain protected. Full UI Designer audit is separate work.

## G11B river-to-G13 waterfall landing — 28 October late DX12 correction

- Owner confirmed G11B banked/rounded river fixed visually, then noted the raised waterfall and subsequent **disappeared waterfall** after the first downward landing edit.
- Identified original regression precisely using native opt-in Runtime `RENEGADE_WATERFALL_RUNTIME_PROOF`: one authored waterfall, **0 active/0 emitters**, `WATERFALL ERROR // Obstacle paths could not reach landing; move landing or crest`. Original authoring/river/rocks existed; native renderer refused **the entire fall** because even one of 32 physics-guided strands failed to reach the river.
- Read-only native actual scene trajectory survey `Tests/InventoryRiverCombinedProof.cpp --survey-falls`: **20/32 lanes reach receiving river at Y -0.85m**, **12/32 outer lanes collide with surrounding bank near Y -0.485m** and exhaust the 650-step trajectory budget. First physical low-bank contacts ~steps 218–352. The original renderer all-lanes-must-reach precondition wrongly discarded the otherwise valid 20 wet streams and the entire waterfall.
- In isolated integration worktree, `EngineBridge/src/WaterfallCollisionPaths.h::ResolveWaterfallBankTerminations` now preserves all actual river-landed trajectories, ends only exhausted/physically bank-contacted outer lanes at first nearby low solid-bank hit, and refuses invalid/no-contact/all-dry conditions without changing source. For mixed cases it removes the discarded dry-land runoff from impact VFX sources. The original geometry/shaders/textures/crest/width/riverbed/water material are unchanged.
- `EngineBridge/src/PlaceableWaterfallRenderer.cpp` uses this bounded resolver instead of aborting the whole waterfall when a bank catches an outer strand. Valid all-wet falls are unchanged. `Tests/PlaceableWaterfallTests.cpp` now checks mixed wet/bank response and all-wet original parity, fails-closed invalid inputs. Native Release test binary `BUILD/waterfall-placeable/RenegadePlaceableWaterfallTests.exe` exited **0**, reporting `WATERFALL_MIXED_RIVER_BANK_TERMINATION=PASS` and geometry 90/90 cases.
- Release Runtime rebuilt; first attempted link failed only because a stale OLD combined-test Runtime was still holding `RenegadeRuntime.exe` open (LNK1104). That old test was identified by its exact snapshot command line and closed. Second MSBuild Release Runtime link `waterfall-bank-runtime-relink.log` **exit 0**.
- Fresh hidden independent actual DX12 player Runtime with waterfall diagnostics `waterfall-bank-restored-native-proof.txt`: **authored=1, active=1, emitters=19, capture=1, status=Waterfall ready // 1 instance(s)**. Captured `waterfall-bank-restored-native-proof.png`; original falling-water strands, rock guidance and foam/spray visible again.
- Closed the owned hidden GPU proof. Opened a fresh visible Release DX12 Test Game with corrected waterfall/river and inventory snapshot, PID from `BUILD/integration-evidence/aligned-live-runtime.pid`. Screenshot `waterfall-restored-visible-runtime.png` shows **falling waterfall and spray again**, recessed riverbanks and 50 HP at ~75 FPS. This is functional verification, not new owner aesthetic sign-off.
- The G13E14 81-chunk disposable authoring proof in `Tests/WaterfallMultiSceneProof.cpp` now creates its river at shelf -0.30m (`surfaceOffsetMeters=-0.85f`) and lowers only the waterfall landing to -0.85m; original crest and other scene variants preserved. Combined snapshot refresh checks native bank containment and correct waterfall/river landing coordinates.
- All scene changes are confined to disposable `BUILD/G5PackagePlay` and have original/pre-correction byte-for-byte archived WISCENE backups under `BUILD/integration-evidence`. No reset/rebase/discard/stash of unrelated night-sky/aurora/Wicked changes. **No commit, push or merge.** Previous full-suite 13 Release failures and Debug completion still require independent evaluation before integration sign-off.

## Final local integration gates - 28 October 2026, 18:50 GMT

Owner inspected final corrected Test Game at ~18:23 and approved the recessed, rounded, covered G11B river and visibly restored G13 collision-guided waterfall; G5 50->100 healing, item drop and full-health refusal had already passed the same integration Runtime. Visual approval of the proof is NOT authorisation to merge.

**Release regression baseline**: CMake 4.4.4 `ctest --test-dir E:/RenegadeIntegration/inventory-river-20261028/build -C Release --output-on-failure --parallel 1` with `TEMP=TMP=E:\RenegadeTestTmp`, running from the actual E: build working directory, completed **280/280 PASS, 0 FAIL, exit 0**. Evidence: `BUILD/integration-evidence/release-all-short-temp-final.log`, `.exit`; total test time 574.76 seconds. This includes all registered G5 inventory, UI Designer, Test Level snapshot/Runtime, G11 river and G13 waterfall tests and package/promotion acceptance tests. No assertions/guards relaxed.

**Failure diagnosis closed**: earlier unfiltered run 267/280 failed because 12 long-path package/promotion fixture tests used an unsuitable TEMP/cwd and the projectile executable had been built at 16:12 before the unsaved-scene G5 snapshot guard was corrected at 16:28. All 13 were reproduced/isolated. Projectile Release test target rebuilt against current code and passed (`projectile-rebuilt-ctest.log`). The shorter E: temp location was proved with S5 reusable-owner, Build Stage and Package Integrity 3/3 (`short-temp-preflight.log`), then the full unfiltered suite passed. Earlier exploratory long temp run's S5 journal-path failure was an environment path problem, not a production code change.

**Four final build outputs** (current integration source after G11B/G13 correction):
- Release Studio `RenegadeStudio.exe`: MSBuild 18, `release-studio-after-waterfall-fix.log/.exit` = 0.
- Release Runtime `RenegadeRuntime.exe`: `waterfall-bank-runtime-relink.log` builds `Runtime/Release/RenegadeRuntime.exe`, command returned exit 0; the resulting executable was launched in DX12 and native status proof read authored=1, active=1, emitters=19, ready.
- Debug Runtime `RenegadeRuntime.exe`: full dependency rebuild of current EngineBridge, `debug-runtime-after-waterfall-fix.log/.exit` = 0.
- Debug Studio `RenegadeStudio.exe`: relink against rebuilt Debug EngineBridge, `debug-studio-after-waterfall-fix.log/.exit` = 0.

**Additional native tests**: `RenegadePlaceableWaterfallTests` Debug rebuilt and executed: `WATERFALL_MIXED_RIVER_BANK_TERMINATION=PASS`, `PLACEABLE_WATERFALL_GEOMETRY_PASS cases=90`, exit 0 (`debug-waterfall-unit-build.log/.exit`, `debug-waterfall-unit-test.log/.exit`). Release version also passed earlier (`waterfall-bank-unit-test.log`). Newly edited G13E14 combined authoring proof `Tests/WaterfallMultiSceneProof.cpp` freshly compiled in Release, exit 0 (`final-waterfall-authoring-proof-compile.log/.exit`). Existing approved live fixture was not overwritten to re-author from scratch.

**Safety final audit**: HEAD remains original main PR #186 SHA `150eca4b761d3f62f122df3ab5e8536ec084f926`, branch `integration/inventory-river-20261028`; index staged count 0; `git diff --check` emitted no errors; submodule gitlink **and** WickedEngine checkout both `ceb816b844b4ab2fa56a159ac60a4f65c931bdfc`. Uncommitted integrated files deliberately retained. Original separate inventory worktree and original dirty night-sky/aurora Wicked worktree untouched. User's corrected live Release DX12 fixture remained running at final audit (RenegadeRuntime PID 6528). C: free ~34 GB.

**Approval boundary**: All local regression, DX12 gameplay, build-config and targeted packaging gates are green. No hosted GitHub CI, commit, push, PR or merge performed. Require owner's explicit authorisation for source submission/merging. The broad UI Designer audit remains separate, not silently commenced. Previously deferred beyond-G5 features (multi-level save/resume, arbitrary item use, custom world-drop prefabs/rigid-body toss) are not claimed complete.

Final binary SHA-256 manifest: `BUILD/integration-evidence/final-build-binary-hashes-20261028.txt` (all four actual current Studio/Runtime Debug/Release executables, verified bytes and timestamps). Final `git diff --check` exit 0, staged index count 0.
