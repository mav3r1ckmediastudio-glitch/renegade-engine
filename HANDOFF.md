# Renegade Engine — Current Handoff

## Character Importer action assignment - 22 September 2026

On isolated `feature/complete-a6-recovery-20260921`, the first explicit Character action assignment slice is implemented locally. Animations-page choices persist on governed native clip metadata and A6 reads them instead of guessing filenames; the base reference-pose clip is not a fallback Idle. Repeated attack requests no longer interrupt an active one-shot. Runtime and focused tests built Release; 3/3 selected tests passed, plus an actual owner Mutant four-clip governed import/reopen/AI graphics proof passed. Studio Release ClCompile passed; a separate `RenegadeStudio_ActionAssignments.exe` also linked successfully without replacing the running Studio. NOT a finished importer UI or owner acceptance. Read `docs/CHARACTER_ACTION_IMPORTER_HANDOFF.md` for scope, validation, unimplemented slots/frame-number/custom-action UI and next safe steps. No main edits, owner-project mutation, push or merge.

## Editor logical hierarchy and selector repair — 21 September 2026

**Branch:** `feature/editor-logical-hierarchy-selection`
**Worktree:** `C:\\Users\\paulw\\source\\repos\\renegade-editor-hierarchy-selection`
**Base:** `origin/main` at `57086b61dce0bfef22606a6707d6cafd74e82cfe`

### Completed implementation

- `SceneService::ListEntities()` now labels only hierarchy roots as logical editor assets while retaining every descendant for intentional hierarchy expansion.
- Category headers count logical assets only, fixing internal terrain chunks and imported character payload nodes inflating category counts.
- Generic scene-reference selectors exclude non-logical descendants, preventing internal terrain chunks and imported child entities from being offered as authoring targets.
- Hierarchy expansion/collapse responds only on its chevron hit area; clicking the row performs selection without unexpectedly changing disclosure.
- The native ComboBox filter input is rendered directly, restoring caret, keyboard focus, typed text and native filtering visibility.

### Validation

- `cmake --build BUILD\\hierarchy --config Release --target RenegadeBridgeTests --parallel 4` passed (full first build; 807.97 s).
- `cmake --build BUILD\\hierarchy --config Release --target RenegadeStudio --parallel 2` passed (270.12 s).
- `ctest --test-dir BUILD\\hierarchy -C Release -R '^RenegadeBridgeTests$' --output-on-failure` passed 1/1.
- `git diff --check` passed.

### Character-root and multi-clip placement repair

- Character inspectors now resolve a selected presentation mesh through its authoritative native `MeshComponent::armatureID`, so selecting the logical character root retains humanoid/IK/look-at controls even when the mesh and rig are separate hierarchy branches.
- Multi-action imports now remain paused on placement. A single-action import still auto-plays; a character action library no longer evaluates and blends every clip simultaneously, which was forcing poses and causing the observed FPS collapse.
- The owner's existing `Mutant001.rasset.json` was inspected without modification: it contains 21 actions and 1,971 channels, including ten repeated action names. The placement fix protects that existing asset immediately; duplicate source-action consolidation remains a follow-up importer data-quality repair.

### Validation

- `cmake --build BUILD\\hierarchy --config Release --target RenegadeBridgeTests --parallel 4` passed after this repair.
- `ctest --test-dir BUILD\\hierarchy -C Release -R '^(RenegadeBridgeTests|RenegadePhase7Gate7BHumanoidRetargetTests|RenegadePhase7Gate7BSourceContract)$' --output-on-failure` passed 3/3.
- `cmake --build BUILD\\hierarchy --config Release --target RenegadeStudio --parallel 2` passed after this repair.
- `git diff --check` passed.
- Untracked local-only `log.txt` contains four test-generated `Scene::Serialize` timing lines. It is intentionally not staged or committed.

### Required owner confirmation

Open the freshly built `BUILD\\hierarchy\\Studio\\Release\\RenegadeStudio.exe` and validate an existing Terrain and imported `Mutant.fbx` scene: one logical root/count per object; chevron-only expansion; viewport-to-root reveal; double-click framing; character/weapon selector eligibility; filter keyboard/caret interaction; character-root Inspector controls; and FPS/pose with the multi-clip character placed. Existing already-imported duplicate clips remain until reimport/data consolidation. No merge is authorised.


## Studio custom marker icons — 21 September 2026

Branch `codex/studio-custom-icons` starts at importer acceptance commit `bc11c845d81f402ff35bae2d27f7932ddbe64cd0`. The owner-provided `editor.zip` was extracted into `Studio/Content` with its `editor/markericons/*.png` paths preserved. `Studio/MarkerIcons.cmake` now copies those 17 loose PNG files into the compiled Studio `Content/editor/markericons` folder, and the shared local/CI Studio packaging script verifies the same complete set in both compiled and packaged Content. The marker source contract validates the exact file set and PNG signatures. No ZIP is copied into a build or package.

Verification: the archive-to-source SHA-256 comparison passed for all 17 files; `RenegadeMarkerIconAssets` built successfully in Release; `ctest --test-dir BUILD/renegade -C Release -R '^RenegadeMarkerIconsSourceContract$' --output-on-failure` passed 1/1; and a source-to-compiled-to-package proof passed all 17 SHA-256 comparisons while the packaging PowerShell parsed successfully. A fresh full `RenegadeStudio` Release build reached Wicked's `WickedEngine_emb_shaders` target and then CL.exe exited with `-1073740791`; the focused asset target and contract remain green, and the failure occurred outside changed code before Studio compilation. Changed files are `Studio/Content/editor/markericons/*.png`, `Studio/MarkerIcons.cmake`, `Tests/MarkerIconsSourceContract.cmake`, `Tools/Build-Studio-Windows.ps1`, and this handoff.

The owner-supplied PR58 Gate 2C Release package also provided the accepted startup media. The three loose files now live under `Studio/assets/startup`: the 8,338,556-byte logo reveal MP4, 2,441,958-byte identity-handshake MP4, and 2,764,854-byte final-frame BMP. CMake's existing Gate 2A/2C rules copy them into compiled `Content/startup`; the shared local/CI packaging script now requires all three in both compiled and packaged Studio Content. They add 13,545,368 source bytes and do not embed in or enlarge `RenegadeStudio.exe`.

## Importer v3 native UI fidelity — active draft programme

- Branch: `feature/importer-v3-native-ui-fidelity` from `main` merge `2c9c92195f72d868066177c5136d891860fa4873`; remote `https://github.com/mav3r1ckmediastudio-glitch/renegade-engine.git`; latest pushed implementation checkpoint `622c34787578f847ce2ee98349526e4da7e2badc`.
- First checkpoint suppresses authored-level marker overlays during the transient importer preview and changes the existing importer grid to a neutral low-contrast studio floor. It does not alter import/retarget/persistence behaviour or the pinned Wicked dependency.
- The pending playback-footer styling checkpoint turns the real `PLAY SELECTED CLIP` control into a full-width native strip between Previous/Next. It delegates to the existing native animation preview, is disabled unless a Character has a selectable clip, and introduces no simulated transport or invented playhead.
- Read `docs/importer-v3/IMPORTER_V3_HANDOFF.md` for complete current importer evidence and the exact next action. A focused Windows build and visual inspection of the pushed exact head remain required.

**Date:** 12 September 2026  
**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`  
**Merged baseline:** PR #156 — Phase 7F native mesh blending parity  
**Merged commit:** `3d305be84fedf73f5b3cfbb0522be2a732c1adca`  
**Active repair branch:** `repair/phase7-integrated-audit`  
**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Programme state

**Phase 6 — Playable Core is accepted and closed.** PR #148 remains the accepted Phase 6 exit baseline, including native navigation, repaired terrain/rigid-body contact and the owner's successful packaged mini-game acceptance.

**Phase 7A–7F are merged, but Phase 7 is temporarily reopened for integrated acceptance repair.** The individual gates intentionally deferred owner testing until the end of the sequence. The resulting integrated audit found three real contract defects in 7B, 7D and 7E. They are repaired together on `repair/phase7-integrated-audit` so one exact branch can receive the final Windows CI and owner acceptance.

Do not begin Phase 8 from this work until that repair PR passes both CI and owner acceptance.

## Merged Phase 7 sequence

- **PR #151 — Phase 7A:** native `AnimationComponent` playback, pause/stop, scrub, range, speed/blend, loop/ping-pong/play-once and guarded root-motion controls.
- **PR #152 — Phase 7B:** humanoid auto-map/manual correction, ResetPose and native baked retarget from WISCENE/FBX/GLTF/GLB/VRM/VRMA.
- **PR #153 — Phase 7C:** native IK, humanoid look-at and expression authoring.
- **PR #154 — Phase 7D:** native timeline/channel/sampler/keyframe authoring.
- **PR #155 — Phase 7E:** HairParticle, ForceField, Video, Spline, Gaussian Splat and remaining Terrain specialist exposure.
- **PR #156 — Phase 7F:** native mesh-blend material/global render-path exposure.

All of these remain part of the final owner smoke test; the repair does not replace accepted Wicked-native ownership with parallel Renegade runtimes.

## Integrated repair now implemented

### 7B — retarget Undo/Redo no longer depends on the source file

The initial operation still imports the creator source and calls Wicked's native baked retarget. Once that succeeds, `RetargetHumanoidAnimationsCommand` captures the created native `AnimationComponent` state and every referenced baked `AnimationDataComponent` at their entity IDs.

Undo removes the command-owned clips/data. Redo restores those snapshots directly and fails closed if an entity ID has been reused. Redo therefore does not reopen or reinterpret the source FBX/GLTF/GLB/VRM/VRMA/WISCENE.

Changing the humanoid bone map invalidates the old native ragdoll body/joint cache so the current mapping can be rebuilt rather than continuing with bodies tied to the previous map.

### 7D — timeline keys/events now follow native-safe ordering

Recording uses chronological insert-or-replace. Payload chunks move with their timestamps. Recording the same zero-payload event at the same time is a no-op. Event mutations reset Wicked's `next_event` traversal cursor.

`CLOSE LOOP` skips Event channels and only closes value continuity. It can no longer manufacture an extra SOUND PLAY/STOP event at the seam.

SCRIPT PLAY/STOP has been removed from the creator picker and is rejected by the bridge because Renegade's accepted creator scripting authority is `.rscripts`, not Wicked `ScriptComponent`.

A blank scene can create an undoable native clip through **NEW CLIP**.

### 7E — Video now uses governed project ownership

`ADOPT MP4` retains the selected creator source under `SourceAssets/Video`, imports an authoritative LP08 product under `Content/Video/*.rasset`, and binds the native `VideoComponent` through serializable StableId metadata instead of an absolute source path.

Studio restores that binding after Scene open/project adoption/reload. Test Level restores it from the active project. Build dependency extraction adds required governed Video products to the closure. Packaged Runtime resolves the video through the content manifest and `.rasset` payload, not the creator machine's original MP4.

Loop/transport remain native VideoComponent controls. Regression coverage explicitly protects governed StableId metadata through Loop Undo/Redo.

## Current verification state

The branch has been deliberately kept free of PR-triggered Windows CI while implementation and static audit are completed. Source inspection has confirmed:

- the new Video service is present in EngineBridge ownership;
- `CreateVideoInstance` usage matches the pinned Wicked bool-returning API;
- resource dependency extraction handles video-only as well as texture+video scenes;
- authoring and packaged Runtime each have explicit governed video restoration;
- Studio uses the project/Scene lifecycle rather than per-frame video repair;
- governed Video loop edits do not enter the filename/resource replacement path;
- 7D timeline implementation and executable tests agree on chronological/event semantics; and
- 7B source contracts require snapshot-based deterministic Redo rather than reopening source input.

This is still **not an acceptance claim**. The branch must compile and run the Windows test suite, and the creator-facing behaviours need owner proof with real character/video content.

## Exact next action

Finish the documentation/evidence ledger on the repair branch, then open one PR targeting `main`. That PR is the intended expensive CI boundary.

If the exact PR head is green, owner-test the artifact using [`docs/PHASE7_INTEGRATED_REPAIR_AUDIT.md`](docs/PHASE7_INTEGRATED_REPAIR_AUDIT.md):

1. 7B real humanoid retarget -> Undo -> make original source unavailable -> Redo -> play -> save/reopen.
2. 7D NEW CLIP -> record out-of-order keys -> SOUND PLAY event -> Close Loop -> prove no seam duplicate -> Undo/Redo -> save/reopen.
3. 7E ADOPT MP4 -> transport/seek/Loop -> Loop Undo/Redo -> save/reopen after original MP4 is unavailable -> Test Level -> Build Game -> standalone playback.
4. Quick regression smoke of 7A–7F Inspector surfaces, including 7C character controls, 7E Hair/Force/Spline/Gaussian/Terrain and 7F mesh blending.

Do **not** merge on green CI alone. Any owner-visible failure remains a Phase 7 repair blocker.

## Deliberate boundaries still in force

- One future shared ZoneService for reusable trigger volumes; do not recreate audio-only/objective-only zones.
- Ground navigation from Phase 6 remains accepted; flying/swimming NPC navigation requires a separate 3D movement/navigation design.
- Player arms, weapons, combat and production enemy AI are outside this repair.
- Creator-facing VSync control remains deferred.
- Wicked Video audio-track playback is not claimed.
- Timeline SCRIPT PLAY/STOP remains deferred until `.rscripts` has an explicit timeline adapter.
- Commercial redistribution/release packaging clearance remains separate from engineering Build Game acceptance.

## Canonical references

- [`README.md`](README.md) — product/build entry point.
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — current programme state and acceptance boundary.
- [`docs/MASTER_PLAN.md`](docs/MASTER_PLAN.md) — long-range programme.
- [`docs/PHASE7_INTEGRATED_REPAIR_AUDIT.md`](docs/PHASE7_INTEGRATED_REPAIR_AUDIT.md) — authoritative Phase 7 repair architecture and owner-test contract.
- [`docs/PHASE6_CAPABILITY_AUDIT.md`](docs/PHASE6_CAPABILITY_AUDIT.md) — accepted Phase 6 exit contract.
- [`docs/PHASE6_NATIVE_NAVIGATION_STAGING.md`](docs/PHASE6_NATIVE_NAVIGATION_STAGING.md) — accepted Phase 6 navigation architecture.
- [`docs/FEATURE_MATRIX.csv`](docs/FEATURE_MATRIX.csv) — capability evidence ledger.
- [`docs/AI_WORKFLOW.md`](docs/AI_WORKFLOW.md) — implementation/handover rules.

## Importer v3 — local Codex/ChatGPT continuation

A separate `feature/importer-v3-local-handoff` worktree based on `main` is prepared for the approved native importer redesign. Read [`CODEX_IMPORTER_V3_START.md`](CODEX_IMPORTER_V3_START.md), [`docs/importer-v3/CODEX_IMPLEMENTATION_BRIEF.md`](docs/importer-v3/CODEX_IMPLEMENTATION_BRIEF.md) and the live [`docs/importer-v3/IMPORTER_V3_HANDOFF.md`](docs/importer-v3/IMPORTER_V3_HANDOFF.md) before any work. The approved HTML and older contextual handoff are available **only in the Git-ignored local `docs/importer-v3/reference/` directory**; do not add them to version control or upload private assets. This entry records preparation only: no importer code, build or owner acceptance has been completed on this branch. Preserve all other worktrees and PRs. Codex should work locally in small commits with explicit testing and an up-to-date handoff so ChatGPT can continue the exact branch when usage runs out. No remote push/CI/PR without owner approval.

### Importer v3 local continuation — 18 September 2026

The dedicated branch now has a native Model verification WIP checkpoint: `a3eaf4a2af24c0cd837c05eee52e06bbcbed1382`. Changed files are `EngineBridge/include/renegade/bridge/{CreatorAssetWorkflowService,ReusableAssetService}.h`, `EngineBridge/src/{CreatorAssetWorkflowService,ReusableAssetService}.cpp`, `Studio/src/StudioApplication.cpp`, and `Tests/CreatorAssetWorkflowGraphicsProof.cpp`. It reopens a committed `.rasset`, checks identity/hash and catalogue presence, retains source after a post-commit failure, reports detailed native failure context, and starts preview at original source scale. A subsequent scene-reopen verification edit remains to be compiled and committed. `git diff --check` passed before the first commit. VS18 Release Studio build command was `cmake --build BUILD/renegade --config Release --target RenegadeStudio --parallel 4`; the sandboxed attempt failed with MSB4184 on Windows SDK access, and the authorized retry was still compiling when this note was written. No Model fixture test, visual audit, owner import, save/reopen or Runtime test has passed yet. The underlying owner-observed missing `.rasset` cause is unknown. See `docs/importer-v3/IMPORTER_V3_HANDOFF.md` for the live commands, results, risks and exact next action. No other worktree, pinned submodule, active binary or remote PR was changed.

**Final importer v3 local result at 13:48 UTC:** later implementation commits `64697c155c3fcc2d94aff007d41deb3a4f3bc2c6` and `ea043688ba646a2f17c636b5ef68382777109c43` added serialized scene/material reopen through normal stable-ID placement and a resizable native right inspector. Exact VS18 Release commands `cmake --build BUILD/renegade --config Release --target RenegadeStudio RenegadeCreatorAssetWorkflowGraphicsProof --parallel 4` and, after the inspector edit, `cmake --build BUILD/renegade --config Release --target RenegadeStudio --parallel 4` both passed (exit 0). `ctest --test-dir BUILD/renegade -C Release -R '^RenegadeCreatorAssetWorkflowGraphicsProof$' --output-on-failure` passed 1/1 with filesystem/graphics access (0.71 seconds); its sandboxed run failed containment before test execution. This proves a non-private FBX fixture `.rasset` commit/catalogue/scene reopen path, not the owner's Mutant import or v3 visual/Character parity. Modified implementation files are listed in the live importer handoff. Risks: the missing-asset symptom's root cause remains unknown, the right inspector has not had visual inspection, and the approved v3 stage navigation, isolated preview and external-animation playback are still outstanding. Exact next task is in `docs/importer-v3/IMPORTER_V3_HANDOFF.md`; preserve this branch and build output. Do not mark the importer accepted.

**Owner correction after this checkpoint:** asset import has been confirmed fixed by the owner. The earlier missing-`.rasset` concern is historical, not a current blocker. This message did not specify the tested build or asset set, so the confirmation is recorded as owner evidence without attributing it to a particular local commit. Native v3 layout, isolated preview, Character external-animation playback/retargeting and visual acceptance remain open. The next assistant should follow the updated importer handoff rather than repeat the old missing-asset diagnosis.

**Importer v3 continuation at 14:11 UTC:** local implementation commit `690cf96877e3b99375826b0d7cb5e466780786a3` changes `Studio/src/StudioApplication.cpp`, `Studio/src/StudioApplication.h` and `Studio/src/CreatorImportPreviewWindow.h`. The old section dropdown is replaced by six clickable right-inspector headings around the active page; Model/Character choice selects the governed destination, Model skips Rig/Animations, Transform includes preview lighting, and Rig shows measured source counts with an explicit unverified-mapping warning. VS18 Release command `cmake --build BUILD/renegade --config Release --target RenegadeStudio --parallel 4` passed (exit 0); `git diff --check` passed. No native visual inspection or Character external-animation proof was run. Risk is heading/body clipping or spacing until native inspection; preview still uses the active scene. The exact next action and remaining work are in `docs/importer-v3/IMPORTER_V3_HANDOFF.md`. No other worktree, active binary, pinned submodule, or remote PR was touched.

**Importer v3 continuation at 14:20 UTC:** implementation commit `3d08f4e71c207c89dac3e38713cabeef18942ac5` adds a read-only first-armature humanoid auto-map diagnostic in the Rig stage, reporting mapped slots and missing required bones through the existing bridge service. VS18 `MSBuild.exe BUILD\\renegade\\Studio\\RenegadeStudio.vcxproj /t:ClCompile /p:Configuration=Release /p:Platform=x64 /m:4 /v:minimal` passed (exit 0, existing C4834 warning), without linking over the running branch executable. `git diff --check` passed. Native visual inspection of the previous linked build confirmed Asset Setup, Model/Character choice, Transform stage interaction and an internally scrolling inspector. It also revealed a material failure: the authored level grid and an AUDIO marker remain visible behind the import preview; isolation is unfinished. The new diagnostic has not yet been seen in the running app. No save/reopen, Character external animation or Runtime check occurred. See the importer-specific handoff for exact next actions and limitations; no remote or pinned submodule changed.

**Importer v3 ChatGPT takeover (18 September 2026):** Local WIP implementation `c9467f414597ff93ad3738b859fe4dd5b4104f86` introduces a separate native preview scene, prevents importing preview/light into the authored level and editor Undo, redirects preview material/lighting/transform edits, suppresses editor overlays/input, and retains preview camera navigation. VS18 Release Studio ClCompile passed twice (exit 0); focused `RenegadeCreatorAssetWorkflowGraphicsProof` passed 1/1 (0.66s); `git diff --check` passed. This commit is NOT linked or visually tested: the v3 worktree Studio executable is running, and must not be overwritten or terminated without owner approval. Owner-confirmed merged asset-import repair has not been merged/rebased into this older-base worktree. Character external-animation queue, retarget, playback and save/reopen remain outstanding. Full commands, boundaries and next action: `docs/importer-v3/IMPORTER_V3_HANDOFF.md`. No push, PR, CI or other worktree changes.

**Importer v3 Character animation checkpoint (18 September 2026):** `314e0276b71c04e0efb3038a80e6dd6f7106f95e` adds native embedded clip preview transport; `71975ae106c04d453a2f0763030d149045273d32` adds isolated external animation queue, opt-in native humanoid source mapping, baked Wicked retarget and fingerprint-guarded commit. VS18 Release Studio ClCompile PASS and focused synthetic native retarget test PASS 1/1; Model RAsset proof separately PASS 1/1 (an earlier combined run had an intermittent journal-file I/O failure). **No new Studio link/visual proof, external FBX Character RAsset save/reopen, owner test or Runtime proof:** the v3 Studio binary remains in use and must not be overwritten. Original asset-import repair accepted by owner and merged separately; exact integration in this older v3 worktree unverified. See `docs/importer-v3/IMPORTER_V3_HANDOFF.md` for commands, modified files, constraints and next action. No remote changes.

### Importer v3 PR preparation - 19 September 2026

- `feature/importer-v3-local-handoff` now incorporates `origin/main` `8522e850` (#170) through merge commit `c7acb9c` and restores documented Wicked pin `3a800b7` through `c499b2a`. The previous `f540315` diagnostic submodule revision is excluded from the final PR diff.
- Owner confirmed successful native Character import and drag/drop; the latest fresh import was 3.477 seconds confirm-to-editor, and warmed drag preparation 95-268 ms. Eight focused tests and real Mutant/external-walk disposable proof passed before branch reconciliation. VS18 CMake configure against the reconciled source passed; matching Release Runtime rebuild is in progress. See `docs/importer-v3/IMPORTER_V3_HANDOFF.md` for exact evidence and next steps.
- Full Windows PR CI, matching standalone Runtime playback/save-reopen and native functional UI inspection remain acceptance gates. Cosmetic polish may follow separately, but do not claim full native visual or external animation parity or merge on CI alone. No private assets/screenshots or untracked logs staged; Studio executable has not been overwritten.


### Importer v3 UI-fidelity local build correction — 20 September 2026

The active remote UI branch is `feature/importer-v3-native-ui-fidelity` (draft PR #172). A GitHub Studio Debug failure was traced to a private visibility error for the existing `DiagnosticImportActive()` query used by the importer overlay suppression. The declaration is now public in `Studio/src/StudioApplication.h`. Local Visual Studio 18 Release configuration and `cmake --build BUILD/renegade --config Release --target RenegadeStudio --parallel 2` passed and linked the dedicated UI-fidelity worktree executable. The protected historical Studio process was left untouched. Routine UI work is local-build first; do not trigger GitHub Actions for normal visual iteration. The importer-specific handoff records the exact next action and PR status.

### Importer v3 3D reference checkpoint — 20 September 2026

Owner-supplied `Male_Reference.fbx` is committed publicly to the UI-fidelity branch as `Studio/assets/importer/male_reference.fbx` (`c2966f78654c730c5fba5957d6c498af165f5724`). The currently uncommitted native integration loads it only into the isolated preview scene, replaces the old white 2D reference load, wires the existing visibility controls, and copies the FBX beside the Studio executable. Local source-only Release compile passed (`MSBuild ... /t:ClCompile ... /m:2 /v:minimal`, exit 0; existing C4834 warning only); no Studio executable was overwritten because the dedicated UI-fidelity instance remains running. This is not yet visually accepted: the next action is commit/push the integrated code, then inspect the exact fresh executable after the owner closes it. No CI was triggered.

### PR #172 navigation timeout repair - 21 September 2026

Implementation commit: `6a9a34893f8c79ea18e22ce564c36b87286c9056`, based on owner-accepted importer head `5c1dd52aa761dd0c0082ed6c7a4496da405d0011`.

Changed only `Tests/Phase6NativeNavigationTests.cpp` and `Tests/Phase6NativeNavigation.cmake`. A scope guard finishes Wicked jobs and flushes the backlog after scene/command teardown but before CRT static destruction, including early failure returns. The navigation checks and real multithreaded voxelization remain intact. The test now has a 60-second timeout instead of inheriting CTest's 1,500-second default. No Studio, Runtime, bridge, asset, upstream source or submodule-pointer change.

Diagnosis: GitHub Studio Debug run `35541788311`, job `106160660605`, timed out test 80 after 1,500.01 seconds; the other 169 tests had no failures (one package test skipped). Locally, the unmodified executable timed out at 60 seconds in both Release and Debug. CDB stacks confirmed the main thread held the CRT on-exit lock while joining `wi::backlog::AsyncWriter::Stop`; the writer was blocked in `atexit` from `AsyncWriter::WriterLoop` while registering its function-static queue destructor. This is a shutdown race, not a slow path query or importer UI regression.

Local Windows VS18 evidence:

- Baseline Debug build: `cmake --build BUILD/renegade --config Debug --target RenegadePhase6NativeNavigationTests --parallel 2` passed.
- Baseline tests: `ctest --test-dir BUILD/renegade -C Debug -R '^RenegadePhase6NativeNavigationTests$' --timeout 60 --repeat until-fail:10 --output-on-failure` timed out on run 1; Release also timed out with `--timeout 60`.
- Fixed test builds: `MSBuild.exe BUILD/renegade/RenegadePhase6NativeNavigationTests.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /m:2 /v:minimal` and the same command with `Configuration=Release` passed against the already-built unchanged dependencies. An unnecessary full dependency rebuild following CMake regeneration was stopped before these focused builds.
- Fixed repetition: `ctest --test-dir BUILD/renegade -C Debug -R '^RenegadePhase6NativeNavigation' --repeat until-fail:50 --output-on-failure` passed 50 native tests and 50 source-contract runs (4.68 seconds total). The identical Release command passed 50 plus 50 runs (3.40 seconds total).
- `RenegadeReusableAssetReimportRecipeTests` passed in Release (0.10 seconds).
- The existing `RenegadeCreatorAssetWorkflowGraphicsProof` failed twice under its default long output path with a staged journal file-creation error. Running the same binary and fixtures with a shorter output directory passed: `BUILD/renegade/Release/RenegadeCreatorAssetWorkflowGraphicsProof.exe Tests/Fixtures/LP07/maya_cube_6100_ascii.fbx Tests/Fixtures/LP07/maya_transformed_skin_7700_ascii.fbx BUILD/nav172-proof`. No importer code was changed to accommodate this local path limitation.
- `git diff --check` passed. Diagnostic/build logs remain in ignored `BUILD/navigation-*` files; no private assets or logs were staged.

Next gate: push the implementation and this evidence to PR #172, then inspect all four exact-head Windows checks. CI is pending at this handoff; do not describe it as passed or merge before required checks pass. The accepted importer UI, existing Studio executables and other worktrees were preserved. This focused test lifecycle workaround does not claim to repair Wicked's general asynchronous-logger implementation.
