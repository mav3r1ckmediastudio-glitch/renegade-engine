# Renegade Studio — complete Character A6 recovery handoff

Date: 2026-09-21. Status: source integration checkpoint; **not a validated build or feature acceptance**.
Repository: `mav3r1ckmediastudio-glitch/renegade-engine`.
Branch: `feature/complete-a6-recovery-20260921`.
Worktree: `C:\Users\paulw\source\repos\renegade-complete-a6-recovery`.
Verified remote `main` baseline: `57086b61dce0bfef22606a6707d6cafd74e82cfe`.
Original local `main`: `5732d8d460610045275a9149252e47f0a3d75a39`; do not change it.
Original hierarchy worktree: `C:\Users\paulw\source\repos\renegade-editor-hierarchy-selection` at `af35de9e97141e5270edfc356f69047c28a35019`; preserve its 80 changed entries, including untracked `log.txt`.
A6 source branch: `feature/character-ai06-native-animation` at `7f9358305ff57b7c32f595dc81353bb62e2f3b52`; preserve it.

## Completed source-recovery operations

1. Created this isolated worktree from cached `origin/main` at `57086b6`; did not fetch, pull or modify `main`.
2. Cherry-picked hierarchy commits `7224a01`, `a57acae` and `af35de9`, yielding `ab3fcce`, `50bb3b9` and `6d6c798`.
3. Captured the original 78-file staged patch and 5-file unstaged patch in binary-correct form **outside the repository** at `C:\Users\paulw\source\repos\renegade-recovery-{staged,unstaged}-20260921.patch`.
4. Applied both patches to this isolated worktree; confirmed SHA-256 equality for key imported files and confirmed both snapshot patches reproduce byte-for-byte against the original worktree.
5. Recorded the complete, still-unvalidated source snapshot in commit `2163aef6fb125f00d2edd52f4b73046d8d77c730`.
6. Cherry-picked A6 commit `7f93583` without conflict, yielding `e60614a3e458a8c1c47fc0de6888b09e9f6ffd73`.
7. Preserved original hierarchy worktree, its index, all untracked logs and the original `main`. No PR merged and no remote push performed yet.

## Critical boundaries

Never reset, force-push, checkout or modify `main`; never discard or overwrite original worktrees. Do not merge a PR without owner approval. Do local builds, not routine GitHub Actions. Compilation and synthetic tests alone do not prove editor behavior. Do not upload the owner's private FBX, project, scene, screenshots or diagnostic logs.
## Functionality to retain

Merged importer baseline: PR #171 and #172, both present in `57086b6`. Native six-stage importer, thumbnail capture, governed `.rasset`, embedded/external animations, retargeting, model/character choice, material previews, source provenance, isolated preview and asset placement all require real owner regression tests.
Recovered character programme: A1–A5 authoring, profiles, factions, patrol, weapons and combat, Character Prefab, Undo/Redo and Runtime; these were in the original hierarchy worktree's staged changes, **not** in its committed HEAD.
Hierarchy repair: terrain and character logical root/counts, correct root selection, viewport reveal, double-click framing, valid expansion, selector eligibility and real Filter field rendering. The extra five unstaged repairs were also recovered (Studio Chrome popup scissor, final import-page reflow, prefab API compatibility, CMake inspector inclusion and test variable correction).
A6 commit adds `Runtime/src/RuntimeCharacterAnimation.h`, `Tests/CharacterAiAnimationTests.cpp`, Runtime wiring and CMake registration. It is **not** proof of running/attacking in the real Mutant scene.

## Known risks / required investigation

- The A6 animation lookup groups walking and running under one Locomotion semantic and infers intent from clip names rather than an explicit authorable semantic Animation Set.
- In `RuntimeCharacterAnimation.h`, a one-shot Attack can be interrupted by Idle/Locomotion at the following update. Verify and fix duration/priority without breaking deterministic native playback.
- Verify whether the imported character actually has a usable configured intrinsic/external weapon and emits shots/attacks; animation code cannot create combat events.
- `SceneService::ListEntities()` currently considers only depth-zero visible items logical roots; verify internal terrain chunks and imported character are actually parented under a single root.
- Investigate duplicate `MutantMesh` and repeated animation sources at importer data, ECS and UI separately; do not delete real source data blindly.
- Character placement contains a payload local-origin normalization; investigate the `(0,0,0)` conversion regression with a real model and protect original world position.
- No complete current-head Studio or Runtime compile, CTest, owner hands-on or packaged TestGame verification yet.

## Next execution steps

1. Initialize pinned submodules **only inside this isolated worktree**; verify Wicked pin `3a800b7134aafe58461093c8abb2e274d4e64033` and imnodes `eb36902c892548ef94f88f51ad7e7c9c7058a71c`.
2. Configure local VS18 build using existing project settings, preferably `cmake -S . -B BUILD/recovery -G "Visual Studio 18 2026" -DRENEGADE_EMBED_SHADERS=ON`; check actual generator and dependencies first.
3. Build `RenegadeStudio`, `RenegadeRuntime` and focused AI/hierarchy/importer tests locally; record full exit codes and test counts. Never treat an existing binary in a different worktree as this recovery build.
4. Fix discovered compile/test issues on this branch only; then conduct visual and owner acceptance including Mutant import/thumbnail, hierarchy, terrain, world position, attack/run clips and saved/TestGame behavior.
5. Commit verified fixes at sensible checkpoints and push this branch alone, never `main`. Maintain exact build commands/results here.

## Local validation ledger — 21 September

- Pinned submodules initialized in this worktree only; Wicked `3a800b7`, imnodes `eb36902`, nested imnodes vcpkg `fba75d0`. `git submodule update --init --recursive --jobs 2` exited 0.
- Configure PASS (exit 0): `"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S . -B BUILD/recovery -G "Visual Studio 18 2026" -DRENEGADE_EMBED_SHADERS=ON`, executed with this worktree as source.
- Initial focused 7 source-contract run: 5 passed, AI-04 and AI-05 failed because they looked in `Studio/MarkerIcons.cmake` for source registrations actually owned by root `CMakeLists.txt`.
- Updated only `Tests/CharacterAiDecisionSourceContract.cmake` and `Tests/CharacterAiCombatSourceContract.cmake` to include the actual root CMake source registrations as well as the previous file; both tests still require the corresponding Inspector sources.
- Rerun PASS: `ctest --test-dir BUILD/recovery -C Release -R '^Renegade(CharacterAi.*SourceContract|CW04CharacterPlacementSourceContract|CW05CharacterPrefabSourceContract)$' --output-on-failure` -> **7/7 pass**, exit 0. This is source-contract evidence, **not** runtime behavior or compilation evidence.
- Full first local Release build command: `cmake --build BUILD/recovery --config Release --target RenegadeStudio RenegadeRuntime RenegadeCharacterAiAnimationTests --parallel 4`. It has reached EngineBridge compilation; **no exit code yet at the time of this handoff entry**. Read Desktop Commander process session PID `22332` if still alive; always verify its final output.

## Full-build recovery after compiler crash

- First full local Release command (`cmake --build BUILD/recovery --config Release --target RenegadeStudio RenegadeRuntime RenegadeCharacterAiAnimationTests --parallel 4`) ended with `RECOVERY_BUILD_EXIT=1`: MSBuild `MSB6006`, `CL.exe` exit `-1073740791` while compiling `ScreenRenderer/RenegadeScreenRenderer.vcxproj`. Wicked Engine, the entire `RenegadeEngineBridge.lib` and several dependencies had compiled; do **not** call the full build successful.
- Isolated `cmake --build BUILD/recovery --config Release --target RenegadeScreenRenderer --parallel 1` PASS (`SCREEN_RENDERER_SERIAL_RETRY_EXIT=0`) without changing any source, proving the previous compiler crash was not a deterministic ScreenRenderer source error on this retry.
- Started serial retry of full `RenegadeStudio RenegadeRuntime RenegadeCharacterAiAnimationTests` targets (`--parallel 1`); Desktop Commander session PID `29860`. Inspect final `RECOVERY_SERIAL_BUILD_EXIT` before claiming success or running compiled tests.
- Additional source-contract suite PASS: `ctest --test-dir BUILD/recovery -C Release -R '^Renegade(CharacterWorkflowSourceContract|SceneUiGate[23456]SourceContract)$' --output-on-failure` -> 6/6, exit 0. Together with previous seven checks, 13/13 **source contracts** passed, not full functional acceptance.

## Reconnected local build and isolated repairs — late 21 September

- Desktop Commander reconnected after a dropped session. The previous serial build's final status was not recoverable; an incremental serial retry hit `LNK2019` because `StudioApplication.obj` was newer than its edited source and still referenced the old hierarchy callback signature. Removed **only that generated object** inside `BUILD/recovery` and rebuilt the three targets serially. `RenegadeStudio.exe`, `RenegadeRuntime.exe` and `RenegadeCharacterAiAnimationTests.exe` all linked, final CMake build **exit 0**.
- Ran 12 selected CTest cases: five AI source contracts, character workflow, five scene UI gates and the original native AI06 animation test: **12/12 passed**. This predates the subsequent extended AI06 behaviour test changes below.
- Isolated, initially uncommitted edits now cover CharacterComponent root categorisation, a synthetic native hierarchy assertion, hierarchy double-click framing of aggregate descendant bounds, and A6 separate Run/Walk semantic, Run fallback, native one-shot Attack/Hit/Reload protection. Extended A6 native tests for these behaviour changes; **their new test build/run remains pending until explicitly verified**.
- `RenegadeBridgeTests` target's broad dependency build suffered another unrelated `CL.exe` crash (`-1073741819`) in `RenegadeAssetRegistryTests`; build was still progressing after that failure. Capture final status, retry affected compiler project serially if needed, then verify Bridge tests and AI06 tests. Do not misreport unrelated compiler crashes as a passed test.
- All work above is in `renegade-complete-a6-recovery` only. Real imported Mutant, thumbnail, head retarget, terrain scene, world placement, full gameplay and packaged build **remain unverified by owner**.

## Native A6 retest and durable checkpoint

- Committed and pushed WIP checkpoint `3814375` to **only** `feature/complete-a6-recovery-20260921`; no PR or main changes. It includes the isolated hierarchy/AI edits and original extended test source. The test initially failed to compile due to a missing `missingBefore` local in the new test block; fixed it in the subsequent uncommitted edit (source unchanged elsewhere).
- Rebuilt `RenegadeCharacterAiAnimationTests` Release after that test fix: **build exit 0**. Its CTest **1/1 PASS** now covers Run inference, native attack shot and play-once persistence, switch to Chase/Run after action finishes, Patrol/Walk, Run-to-Walk fallback, and missing optional Reload clip.
- Re-ran 11 selected source contracts after AI06 edits: **11/11 PASS**. A fresh serial `RenegadeStudio RenegadeRuntime` rebuild was started to ensure the changed AI06 header also compiles in both complete binaries; capture its final exit before claiming a new whole-binary success.
- `RenegadeBridgeTests` broad-dependency build was still running. Its first pass encountered transient unrelated `CL.exe` access-violation exit `-1073741819` in `RenegadeAssetRegistryTests`; its final exit and native hierarchy assertion must be recorded separately.

## Completed focused validation after WIP checkpoint

- Rebuilt both complete **RenegadeStudio** and **RenegadeRuntime** Release targets *after* the Run/one-shot A6 changes: serial CMake build **exit 0**; both new binaries linked in `BUILD/recovery/Studio/Release` and `BUILD/recovery/Runtime/Release`.
- Rebuilt new AI06 native test Release target **exit 0**. CTest AI06 **1/1 PASS**. Built `RenegadeBridgeTests.vcxproj` directly with project references disabled (the EngineBridge dependency had already linked): **exit 0**; CTest selected **Bridge + AI06 2/2 PASS**, including synthetic imported Character root/descendant assertions.
- Eleven selected AI/workflow/Scene UI source contracts re-ran **11/11 PASS**. These automated checks do not replace opening the Studio, using the actual Mutant FBX, checking thumbnail/head/position/terrain or testing the packaged TestGame.
- Separately, a broad `cmake --build ... --target RenegadeBridgeTests` dependency build was running and had encountered `CL.exe` crash in `RenegadeAssetRegistryTests` plus an internal compiler error while building unrelated `RenegadeRagdollPhysicsTests`. This is **not** a clean all-target build; inspect its final process exit, do not disguise those failures. The focused native Bridge executable itself built and passed.

## Focused importer, retarget, placement and AI regressions

- Built Release `RenegadeCW04CharacterPlacementTests`, `RenegadeCW05CharacterPrefabTests`, and `RenegadeCreatorExternalAnimationImportTests` via their respective VS18 generated project files with `/p:BuildProjectReferences=false /m:1`: each build **exit 0**, referencing the already-built bridge libraries.
- Consolidated CTest `ctest --test-dir BUILD/recovery -C Release --output-on-failure -R '^RenegadeCW04CharacterPlacementTests$|^RenegadeCW05CharacterPrefabTests$|^RenegadeCreatorExternalAnimationImportTests$|^RenegadePhase7Gate7BHumanoidRetargetTests$|^RenegadeImportTests$|^RenegadeBridgeTests$|^RenegadeCharacterAiAnimationTests$'` **7/7 PASS**, covering native authored placement, prefab, external animation import, humanoid retarget, import, synthetic hierarchy and A6 animation. Together with the previously repeated 11 source contracts, 18 selected checks passed across separately executed suites, not a full CTest run.
- IMPORTANT: Synthetic tests do not prove user's actual Mutant character import UI, thumbnail rendering, original head, preserved placement, terrain hierarchy or visible run/attack gameplay. Those require owner GUI/asset acceptance and packaged TestGame. The unrelated broad-dependency build remains uncertified because of observed CL.exe crashes/ICE.

## 22 September — verified local integration checkpoint

- The prior serial all-target build first exited 1 on MSVC `CL.exe` access violation / ICE while compiling AssetRegistry and Ragdoll tests. An isolated serial Ragdoll retry succeeded. An unchanged-source `cmake --build BUILD/recovery --config Release --parallel 1` then **passed for the complete default ALL_BUILD target**, exit 0. The intermittent compiler crashes should be recorded, not mislabelled as deterministic source failures.
- Repaired the Review/importer geometry: destination occupies y=266–298, so the former title y=284 overlapped it. Review title now y=312, thumbnail y=358 and review scroll body height 640. The Scene UI Gate 6 source contract includes a new dedicated bounds contract; positive test passed and deliberate restoration of the old title position failed as expected. Owner visual review is still required.
- `SceneService::ListEntities()` now suppresses Wicked's generated terrain chunk-group subtree **from hierarchy presentation only**, preserving the authored Terrain root and all native scene components. Native Bridge test adds a Terrain root, chunk-group and 48 generated children, verifies only Terrain root is presented and native chunks remain in the actual scene. Studio/Runtime/Bridge targets compiled; seven focused regression tests passed.
- Found the owner's actual untouched Mutant FBX at `C:\Users\paulw\Downloads\Creature NPC Pack\Mutant.fbx`, plus walk/run/swiping animations in the same directory. The graphics-backed FBX proof converted, saved and reopened the real 19,805,233-byte Mutant; rig had 37 bones, 111 animation channels and 192 keyframes. No owner project or source files were modified.
- Extended the isolated graphics-backed Creator workflow proof to accept multiple external source FBXs. Real Mutant plus `mutant walking.fbx`, `mutant run.fbx`, `mutant swiping.fbx` all retargeted into **one prepared Character** and passed governed Character import, registry/product verification and reopened WISCENE native-clip preservation. The reopened native clip names were precisely `[Mutant] [mutant walking] [mutant run] [mutant swiping]` (graphical proof exit 0).
- Those real names exposed an AI06 semantic omission: `swiping` did not match Attack. Added `swipe` and `swiping` to native Attack inference and a regression for the exact real `mutant swiping` name. Rebuilt Runtime and AI06 test; focused A6 native test passed. This proves clip recognition, **not** that the owner's placed NPC has valid combat authoring or actually attacks in TestGame.
- Other relevant checks: 7/7 native AI A1–A6 tests passed, standalone LP07 FBX graphics CTest passed, 7/7 focused Bridge/Review/AI06/importer/character tests passed after terrain and Review fixes, and real Mutant import/retarget graphics proofs passed. Scope and results are tracked separately from visual owner acceptance.
- Pending manual release acceptance: open the actual Mutant in the current recovery Studio, capture thumbnail and confirm Review control hit targets; inspect world placement, head/rig, scene hierarchy and filter; play TestGame to inspect run, swipe/attack and combat weapon setup. Do not merge to main without these checks.
- Completed a fresh current-head default ALL_BUILD successfully before running the complete 199-case Release CTest. Its first run passed 198/199, failing the old LP07 Gate 5 graphics proof only when it used its overly long default output path. The journal filename measured exactly **260 characters**, hitting the Windows path-length limit; the same proof with a shorter isolated output path had already passed repeatedly.
- Shortened **only** the disposable CTest proof destination in `Tests/LP07Gate5.cmake` from `lp07-gate5-creator-asset-proof-output` to `lp07-g5`. Regenerated VS18 CMake (exit 0) and reran the previously failing exact CTest; it **passed 1/1**. Full 199-case suite rerun was subsequently started; consult the process result or `BUILD/recovery/full-ctest-fixed-20260922.txt` for its final count. Do not describe it as green without checking.
- The actual input Mutant, walking, run and swiping FBXs remain untouched in Downloads; proof output is exclusively under this recovery worktree's ignored `BUILD/recovery`. UI interaction, terrain visual performance, and TestGame combat need owner verification despite native tests.

## Full local release validation — final 22 September result

- **Complete `ALL_BUILD` Release build: PASS**, native CMake exited 0 after serial retry. Current sources include Review layout, native terrain hierarchy, real Mutant swipe semantic and the shorter test-only journal path.
- **Complete Release CTest: 199/199 PASS, exit 0**, `ctest --test-dir BUILD/recovery -C Release --output-on-failure -j 1 --timeout 180`. Full output is retained locally at ignored `BUILD/recovery/full-ctest-fixed-20260922.txt` (45.03 seconds). The initial 198/199 run and its repeatable exactly-260-character Windows MAX_PATH failure are preserved in the earlier ledger for honesty.
- The real owner Mutant multi-source graphical import (`Mutant.fbx` plus walking, run, swiping FBXs) separately passed, with all three native clips persisted in the governed Character. This does **not** replace hands-on Studio UI and playable TestGame checks.

## 22 September — real placement and Character action isolation

- Found a second placement path: `PlaceImportedModelCommand` published only local placement, while `PlaceReusableModelCommand` (the Character-asset wrapper path) also left its world transform stale until the next Scene update. Both now explicitly call `UpdateTransform()` immediately after setting the creator-requested local position. This prevents native Runtime initialization from observing the pre-placement world origin.
- A graphics-backed proof using the owner's **actual Mutant.fbx** and walking/run/swiping FBXs now verifies the reopened Character can be placed at the exact nonzero world coordinates `(17, 3, -9)` immediately after `PlaceImportedModelCommand::Execute()`. The same proof still verifies all three imported actions are retained. This does not replace hands-on TestGame verification of controller movement.
- Corrected another `PlaceReusableModelCommand` regression: it previously called `Play()` on every newly merged animation, combining exclusive Idle/Run/attack actions. It now autoplays only when there is exactly one imported animation; multi-action libraries stay stopped for Runtime AI to select one.
- Strengthened `RenegadeCW04CharacterPlacementTests` with immediate world-coordinate assertions for a promoted Character wrapper and with an imported two-action fixture asserting that neither clip automatically plays. New focused Release build succeeded and the two relevant CTest cases passed **2/2**. Owner visual acceptance of geometry and live animation is outstanding.
- The **previous** full ALL_BUILD and 199/199 CTest checkpoint is at pushed `9905879`. The newly modified placement code is being rebuilt and must get its own final all-target build/CTest result before calling the current head fully green.

## Final local placement-regression validation

- After the `ImportService.cpp` and `ReusableAssetInstanceService.cpp` changes, the targeted Release rebuild linked `RenegadeCW04CharacterPlacementTests.exe`, `RenegadeCreatorAssetWorkflowGraphicsProof.exe`, `RenegadeStudio.exe` and `RenegadeRuntime.exe` successfully (build process exit 0).
- Native world-placement/animation-isolation and Creator graphics CTest **2/2 PASS**, exit 0. A fresh graphical proof using the real Mutant plus all three external FBXs **PASS**, exit 0; persisted clip names match `[Mutant] [mutant walking] [mutant run] [mutant swiping]`.
- Rebuilt the complete default VS18 Release `ALL_BUILD` target **PASS, exit 0**. Re-ran complete Release CTest **199/199 PASS, exit 0**, 51.57 seconds; full output is in ignored local `BUILD/recovery/final-world-all-199-ctest.log`.
- No GUI owner-acceptance claim: native tests cannot demonstrate thumbnail hit targets, original mutant head, actual terrain performance, visible Run/Swipe, combat authoring or playable TestGame. Open `BUILD/recovery/Studio/Release/RenegadeStudio.exe` from **this recovery worktree**, not an earlier CI artifact. Verify each issue in an owner scene before requesting merge.

## 22 September — owner reports recovery build does not fix Studio behaviour

- Owner opened the correct isolated `BUILD/recovery/Studio/Release/RenegadeStudio.exe` (PID 23140 was confirmed running from that exact path). Owner reports none of the apparent fixes worked. **The previous claims about head and overall repairs were premature; 199 green automated tests are not Studio owner acceptance.** No PR or main change is authorised.
- Inspected live Studio `log.txt` without altering owner project: current project is `OneDrive/Desktop/renegade tests/v2`; real import created `Mutfdgdfgfsdggdfgant.rasset` with 14 clips, whereas earlier graphics proof exercised only four. Log reports repeated `Registered reusable model product resolves outside Content or is unavailable`, missing imported `Mutant_diffuse.png` and `Mutant_normal.png` external Mixamo paths, plus missing test snapshot terrain textures. These are observed warnings/failures, not yet root-cause conclusions for all visual symptoms.
- Reproduced a **specific missed head regression** with the owner's untouched real `Mutant.fbx`: graphics test printed `MUTANT PRE-MAPPING HUMANOIDS=1 [lookAt=1 target=0 xyz=0,0,0]` and failed `external-animation mapping restored unwanted default head look-at` (exit 6). The prior source fix in Studio only normalised an earlier preview; the pre-existing native HumanoidComponent was skipped by the late `EnsureHumanoidAnimationSourceMapping` path.
- Isolated repair in `EngineBridge/src/HumanoidRetargetService.cpp`: newly created humanoids have look-at disabled on creation; already-valid imported humanoids with default zero-target look-at are normalised after mapping, without disabling explicit nonzero or entity targets. Native Phase7B test now covers both creation ordering and already-valid-rig path; real graphics proof verifies map and reopened Character head state.
- Focused Phase7B and graphics targets built Release **exit 0**, CTest **2/2 PASS**. The same real FBX + walking/run/swiping proof that failed now **PASS** (exit 0), with all three action clips persisted. This only verifies native state, not the head's visual pose or bone animation in the owner's real Studio scene.
- **Critical: owner still has the old Studio executable running; the latest head source changes are not in that running binary or a newly linked Studio executable yet.** Do not rebuild/overwrite its executable until Studio is closed. Do not ask owner to test the stale build. Other importer/hierarchy/terrain/AI issues remain unverified; request screenshots or a precise failure description and reproduce each on the exact v2 project, never infer a single fix resolves them.

## 2026-09-22 — Owner-reported glide/no transitions: verified follow-up

- Owner tested earlier `9c26063` Studio and reported the head still moving, model gliding and no walk-to-attack interaction. Previous 199/199 pass did not establish usable in-editor behaviour. DO NOT describe the system as visually repaired.
- The importer had TWO unsynchronised Character selectors. Primary CHARACTER button set `importAsCharacter` and destination folder but left durable `assetKind=Model`; importing that way saved a Model inside `Content/Characters`. A real owner project's `Mutfdgdfgfsdggdfgant.rasset.json` shows that inconsistency. Both selectors now update both fields. **Existing incorrectly typed products are not migrated by this change.**
- Added regression to the real-Mutant FBX graphics proof: imports with an actual Character recipe, places the reusable Character wrapper, confirms a native Renegade Character and resolves embedded plus external walk/run/swipe clips. The proof also exercises manually-driven Idle -> Walk -> Run -> Swipe -> Run and real AI foundation/profile/perception/combat/animation integration with authored Enemy + intrinsic Melee. It verifies native clip playback/events, NOT actual rendered pose or the owner's onscreen level.
- AI-06 previously never requested the initial Idle clip because the record defaulted to Idle before any clip had started. It also would not restart an externally stopped locomotion loop. Both have been corrected with regression checks; a headless backlog logging attempt was reverted because it hung the native test. Playback/configuration telemetry now lives in the real Runtime application diagnostics path instead.
- Actual read-only scene audit mode was added to the existing graphics proof. Saved `Content/Scenes/9.wiscene` (21 Sep) has 0 native and 0 authored Characters, 0 clips; older `9` backup has 0 Characters and 12 clips; `Content/Scenes/7.wiscene` has 1 authored `oildrum` Enemy, 0 related clips. These historical saved files are **not** proof of the owner's current unsaved scene. Audit never writes to owner project.
- Corrected Studio and Runtime Release executables built locally. Direct importer source/geometry contract PASS; complete local Release CTest **199/199 PASS** (46.32 seconds). Source test and model-clip checks remain insufficient substitutes for observing rendered animation and UI.
- Original main/hierarchy worktrees and the owner's project assets are untouched; never merge until owner-on-screen validation. Untracked `Tools/__pycache__/` and `log.txt` were present; do not stage/delete them. Build test scripts/logs are under ignored `BUILD/recovery`.
