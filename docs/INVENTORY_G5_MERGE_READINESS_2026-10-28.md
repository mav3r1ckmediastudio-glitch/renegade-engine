# Renegade inventory — final review / merge preparation (28 October 2026)

**Working branch:** `feature/inventory-core-g1-20261026`, HEAD `b2da8466c0726ca94becb30445afd0149afce792`, with G5 changes still UNCOMMITTED and unstaged.
**Remote target:** `origin/main` at `9b8ff46b09c5c71f24caac134cf21dac293d360f` as inspected 28 October 2026.
**Do not merge, push, stage, or commit until the owner explicitly approves.**

## User-approved gameplay in owner DX12 testing
- Backpack ↔ chest transfer by mouse, E and arrows; one item per ordinary click, drag/Shift-drag behaviour from accepted G4/G5G checks.
- Native authored collectable pickups, item catalogue, PNG slot icon and quantity, permission/capacity and world pickup registration.
- Player right-click context actions: Use, Drop One, Drop Stack; chest Take One/Take Stack. Owner said dropping works and is fine.
- Live wound-and-heal test in disposable G5 packaged Runtime: health 50/100 -> 100/100, real HUD bar fills, one health potion removed from backpack (3 -> 2), chest unchanged. Live screenshots at `BUILD/G5PackagePlay/g5i-live-start-50hp.png` and `BUILD/G5PackagePlay/g5i-health-after-potion.png`.
- Full-health potion correctly refused; no silent consumption.
- World pickup dynamic drop quantity, saved v2 state, v1 backward load, denied withdrawal rollback and full-inventory collection refusal proven in native tests.
- Resolved transient old 'HEALTH FULL' status persistence on inventory reopen.

## Project and packaging validation
- `RenegadeStudio.exe` output restored to the canonical name CI expects; previously accepted Studio V4 candidate preserved separately in BUILD.
- Release Studio and Release Runtime built successfully in `BUILD/inventory-full`. Release Studio `startup-smoke` exit 0.
- Windows production package G5I health QA V2: `BUILD/G5PackagePlay/Builds/Windows/.renegade-staging/Renegade G5 Packaged Inventory Proof Windows Build.g5h-health-hud-20261028-v2/G5InventoryProof.exe`, 19 dependency nodes, 38 governed files, manifest and integrity pass, editable project health Screen and inventory UI included.
- Latest focused CTest log: `BUILD/inventory-signoff-final-release-ctest.log` (run after only copy change to tooltip; check result).
- Debug Studio and Debug Runtime local build log: `BUILD/inventory-final-debug-build.log` (wait for final EXIT line before claiming green).
- The four hosted GitHub checks are NOT considered green until a clean committed PR runs them; local build validation is not identical to CI.

## Integration hazards — MUST exclude or reconcile
- The worktree's `WickedEngine` submodule is **dirty but at the correct pinned commit** `3a800b7134aafe58461093c8abb2e274d4e64033`. Its changed weather/sky/aurora and Milky Way shader sources are NOT inventory work. Do not stage/add them or reset them: preserve the other agent's work.
- Root `log.txt` is untracked, a local runtime log; exclude from a commit.
- The remote main target is **three commits ahead** of local main, including environment probe merge PR #185 and pinned DirectXMath build script work. Do not assume a simple fast-forward.
- `git merge-tree --write-tree origin/main HEAD` reports documentation conflicts in `HANDOFF.md`, `docs/ARCHITECTURE.md`, and `docs/FEATURE_MATRIX.csv`; this checks the committed G1-G4 base only, not uncommitted G5. `Studio/src/StudioApplication.cpp` is an overlap from main and G5 edits and requires review on integration.
- Resolve docs by preserving the newer `origin/main` content and appending only current Inventory G5 facts; NEVER replace the environment-probe record with an older branch copy.
- After approval: isolate the selected inventory source/docs/tests, omit Wicked and runtime logs, commit on feature branch, reconcile against freshest `origin/main`, push reviewed PR, run Studio Debug + Release and Runtime Debug + Release CI, approve merge only after verified green checks.

## Explicitly deferred (not silently shipped)
- Creator-chosen custom 3D world pickup prefab and rigidbody impulse (current default drop is visible, collectible neutral 3D sphere).
- Generic item Use behaviour for weapons/equipment/scripts beyond the implemented RestoreHealth effect; item effect must be defined to enable use.
- Multi-scene Story Flow quick-save and resume; G5 direct-scene F5/F9 already governed and tested.
- Visual owner click-through of Studio's project Inventory Screen selector (the authoring writer, validation and package closure are unit-tested); may be evaluated as the first scenario in the coming full UI Designer audit.
- General UI Designer consistency/performance/accessibility audit across the entire engine is NEXT, not part of Inventory G5 implementation.

## Final local validation update (28 October)
- Canonical Release Studio and Runtime built successfully after removing the temporary `OUTPUT_NAME`; the prior original Studio executable was backed up locally as `BUILD/inventory-full/Studio/Release/RenegadeStudioBeforeG5Final20261028.exe`, while the V4 QA candidate remains independently available.
- Canonical Release Studio `startup-smoke` EXIT=0.
- Final 28/28 Release regression tests PASS, log `BUILD/inventory-signoff-final-release-ctest.log`. `git diff --check` EXIT=0. Hover instructions now say 'RIGHT-CLICK FOR OPTIONS'.
- Fresh final standalone staging `BUILD/G5PackagePlay/Builds/Windows/.renegade-staging/Renegade G5 Packaged Inventory Proof Windows Build.g5h-inventory-signoff-20261028-v3` PASS: 19 dependencies, 38 files, authored inventory binding, health HUD, item catalogue, PNG icon; log `BUILD/inventory-signoff-stage-final.log`.
- Do NOT compare the final packaged EXE hash directly against raw `RenegadeRuntime.exe`: production `ApplyWindowsGameExecutableIdentity` writes the project's icon/version resources into the staged executable and changes its hash/length. The production stage identity/manifest/integrity validator **passed** after that transformation.
- Fetched `origin/main` read-only to `9b8ff46b09c5c71f24caac134cf21dac293d360f`. Committed G1-G4 branch base dry-run merge-tree reports only docs conflicts (HANDOFF, ARCHITECTURE, FEATURE_MATRIX); G5 uncommitted StudioApplication overlap with new environment-probe work must be reviewed during actual integration. No checkout, reset, merge, stage, push or commit performed.
- Local Debug Studio+Runtime cold compilation still running at time of this note; DO NOT imply it or hosted 4-check CI passed until verified.

## 2026-10-28 INVENTORY SIGN-OFF PREP: local 4-target builds GREEN, pending PR approval

User agreed to finish inventory acceptance and prepare for UI Designer audit. G5 gameplay was owner-accepted (E/arrow/mouse interactions, drag/Shift-drag, Drop works); production live DX12 potion test 50->100 HP and backpack 3->2 confirmed with HUD screen capture. Final hover text corrected to RIGHT-CLICK FOR OPTIONS in both graphical inventory grid variants. Release Studio output name restored from temporary review V4 alias to canonical RenegadeStudio.exe, preserving V4 as a separate existing file; older canonical executable was copied to RenegadeStudioBeforeG5Final20261028.exe before local rebuild (BUILD only).

LOCAL BUILDS: Release Studio + Runtime EXIT=0, log BUILD/inventory-final-canonical-release-build.log. Debug Studio + Runtime EXIT=0, log BUILD/inventory-final-debug-verify.log (initial cold compile in BUILD/inventory-final-debug-build.log); DEBUG/RELEASE Studio startup-smoke both EXIT=0. Focused Release CTest 28/28 PASS, log BUILD/inventory-signoff-final-release-ctest.log. git diff --check 0. Production Build Game fresh sign-off stage 19 dependencies, 38 governed files, identity/integrity validation PASS; log BUILD/inventory-signoff-stage-final.log. Path BUILD/G5PackagePlay/Builds/Windows/.renegade-staging/Renegade G5 Packaged Inventory Proof Windows Build.g5h-inventory-signoff-20261028-v3. Staged G5InventoryProof.exe has a different SHA and length from raw Runtime because production ApplyWindowsGameExecutableIdentity rewrites icon/version resources; this is expected, stage integrity validation succeeded.

GIT INTEGRATION: Current inventory feature HEAD b2da8466 with G5 edits still uncommitted and unstaged. Fetched current origin/main 9b8ff46b (three newer commits vs local main); dry-run merge-tree of committed G1-G4 reported documentation conflicts in HANDOFF.md, docs/ARCHITECTURE.md, docs/FEATURE_MATRIX.csv. StudioApplication.cpp has overlaps in file paths but separate diff hunks from the new environment-probe changes. Keep both. WickedEngine at correct pinned gitlink 3a800b7 but its working files have unrelated night-sky/aurora changes: NEVER stage or reset them. Root log.txt untracked: exclude. Docs/INVENTORY_G5_MERGE_READINESS_2026-10-28.md records the exact safe integration plan. No git stage/commit/push/merge performed. Hosted GitHub Studio/Runtime Debug+Release checks NOT run because branch not pushed.

Explicitly deferred: creator-selected 3D world-drop prefab and rigidbody impulse (currently accepted collectible green sphere), non-potion generic scripted/equipment Use, Story Flow multi-level quicksave (direct-scene F5/F9 supported). Owner Studio Inventory Screen picker visual click-through not directly accepted, writer/inventory package tests green; include in first full UI Designer audit. NEXT: seek owner approval before committing & opening PR, reconcile main, run 4 hosted CI checks, merge only after explicit approval. UI Designer audit in a fresh scope after inventory integration.

## 2026-10-28 15:07+ — Upstream river/waterfall PR #186 merged: inventory integration baseline changed

Owner reports river merged to main. Verified through remote fetch WITHOUT checkout/rebase/merge/stage/commit: `origin/main=150eca4b761d3f62f122df3ab5e8536ec084f926`, merge PR #186; previous `origin/main=9b8ff46`. Remote main is now **9 commits ahead** of inventory common ancestor `11bbb90e`, inventory feature branch is **36 commits ahead** of common ancestor, HEAD still `b2da8466` plus UNCOMMITTED G5 files. All inventory source and Wicked working tree left unchanged.

CRITICAL: main's WickedEngine submodule **gitlink changed** from `3a800b7134aafe58461093c8abb2e274d4e64033` to `ceb816b844b4ab2fa56a159ac60a4f65c931bdfc` (river-enabled fork branch `renegade-rocky-riverbed-v1`). This inventory worktree's Wicked submodule remains at the OLD gitlink and contains independent dirty aurora/Milky Way/shader edits; DO NOT CHECKOUT, RESET, OR OVERWRITE IT. Old inventory Debug/Release 4-target builds and 28/28 tests and 38-file packaged game were run **against OLD pinned Wicked**, NOT against combined river+inventory integration. Must validate both together on clean new pinned fork after isolated integration.

`git merge-tree --write-tree origin/main HEAD` for *committed* inventory G1-G4 produced **5 confirmed conflicts**: EngineBridge/CMakeLists.txt; Runtime/src/RuntimeApplication.cpp; HANDOFF.md; docs/ARCHITECTURE.md; docs/FEATURE_MATRIX.csv. This is NOT a dry run of G5 UNCOMMITTED changes; further conflicts/behaviour risks exist. Path-overlap analysis found 14 overlapping files across old/new feature work: EngineBridge/CMakeLists.txt, EngineBridge/src/TestLevelSnapshotService.cpp, HANDOFF.md, Runtime/CMakeLists.txt, Runtime/src/RuntimeApplication.cpp, Runtime/src/RuntimeApplication.h, Runtime/src/main_Windows.cpp, Studio/CMakeLists.txt, Studio/src/StudioApplication.cpp, Studio/src/StudioApplication.h, Tests/CMakeLists.txt, WickedEngine, docs/ARCHITECTURE.md, docs/FEATURE_MATRIX.csv. Studio sources auto-merged on G1-G4 but G5 changes overlap and require manual review. Root CMake CI configuration and river shaders/assets changed heavily. Confirm both existing inv and new river CI test registrations in final combined CMake; preserve river shader runtime staging and new fork pin.

MERGE PLAN after EXPLICIT OWNER APPROVAL only: (1) preserve current inventory tree and separate dirty night-sky Wicked tree; (2) isolate inventory changes on branch without accidentally including unrelated Wicked or log.txt; (3) create fresh **separate** integration worktree from newest origin/main at new fork gitlink, apply reviewed G1-G5 inventory changes, resolve conflicts by retaining BOTH river and inventory runtime/studio/CMake logic, plus updated docs, never choose whole-file ours/theirs; (4) compile Studio and Runtime Debug+Release and run inventory, river/waterfall and packaging regressions, live DX12 proof; (5) push PR and verify all 4 hosted CI gates; (6) only merge with owner's specific approval. Current inventory is feature-complete for old fork but **not yet river-integrated or merge-ready**.

Existing readiness doc docs/INVENTORY_G5_MERGE_READINESS_2026-10-28.md must reflect this new river merge; prior statement 'three newer main commits/document-only conflicts' is superseded. No local modifications to branch beyond this handoff note.

## 2026-10-28 — CMake workarounds recovered from earlier development chats, verified on DESIGN

User reminded us two working local build routes already exist; DO NOT waste time reinstalling VS/CMake or assuming only one route. On 2026-10-21, the river work successfully bypassed problematic CMake regeneration by directly invoking **MSBuild 18.6.3** on already-generated `BUILD\renegade` Visual Studio project files, with Release Studio and river/terrain tests passing. On 2026-10-22, the team discovered the VS2022 Community-bundled CMake executable is **0 bytes**, then installed official Kitware **CMake 4.4.4** via WinGet and explicitly ran it to configure the Visual Studio 2022 generator; Release builds passed. Both tools remain present and verified on DESIGN 2026-10-28:
- CMake: `C:\Users\paulw\AppData\Local\Microsoft\WinGet\Packages\Kitware.CMake_Microsoft.Winget.Source_8wekyb3d8bbwe\cmake-4.4.4-windows-x86_64\bin\cmake.exe` (13,465,160 bytes; reports version 4.4.4).
- MSBuild: `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe` (reports 18.6.3.22110).
- Broken bundled CMake: `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe` (still **0 bytes**).
- Existing inventory build cache already points `CMAKE_COMMAND` to the correct Kitware 4.4.4 and `CMAKE_GENERATOR` to `Visual Studio 17 2022`.

**Recommended when integrating inventory into freshly merged river main:** configure a **new isolated build directory** against new main and Wicked pin `ceb816b`, invoking Kitware 4.4.4 by its full path. Once generated, invoke MSBuild 18 directly if CMake regeneration begins failing, instead of driving the zero-byte bundled executable. Do not reuse old inventory/river `BUILD\renegade` build trees against a different source root or changed fork pin. Keep `CMAKE_SUPPRESS_REGENERATION=ON` if the regenerated ZERO_CHECK/VerifyGlobs route is defective, and avoid mixed versions inside one CMakeCache. No code/tree/submodule/branch merges were performed while checking these.
