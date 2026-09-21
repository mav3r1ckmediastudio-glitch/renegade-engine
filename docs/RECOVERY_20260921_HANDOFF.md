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
