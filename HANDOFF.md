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

### Creator model importer removal checkpoint — 30 September 2026

Branch: `feature/remove-importer-completely`, isolated Windows worktree `renegade-no-import`, based on accepted V4 commit `5152874`. This is a temporary no-model-import reset, not the replacement importer. Main and the previous experimental worktrees were not edited.

The old guided model/character importer was physically removed from Studio: its state, preview window/dashboard, callbacks, stage controls, commit path, ADD menu action, Asset Browser model import/reimport controls, reference FBX and package copy rule. The bridge model import transaction, prepared conversion entry points, external animation/material preparation and model reimport methods were deleted. Existing `.rasset` read and placement utilities remain so old projects can still open and place their assets. Independent texture, video, audio and specialist resource workflows are outside this model importer reset.

Windows Release `RenegadeStudio` compilation and link passed before the bridge deletion; the first post-build step failed only because the CMake copy command still referenced the deleted reference FBX. That rule is now removed. The final incremental Windows Release `RenegadeStudio` build after bridge deletion passed with exit code 0 and produced `BUILD/no-import/Studio/Release/RenegadeStudio.exe`. The executable launched to Project Hub on DX12. No new importer or UI has been started. DX12 editor inspection passed: an existing project opened with scene 9.WISCENE and a populated Project Assets pane; ADD showed no IMPORT MODEL entry, and the Asset Browser showed no model IMPORT or REIMPORT controls. This verifies the visible paths in the local executable, not owner acceptance or all historical import tests.

### New model importer rebuild checkpoint — 30 September 2026

Implementation commit `9543ace` on branch `feature/model-importer-rebuild` starts at no-import baseline `4d2bcda`; no main, other worktrees, or pinned Wicked submodule edits. `ModelImportCandidateService` now validates a selected GLB, converts it into an isolated heap-backed Wicked Scene with an initialized graphics device, records source byte/fingerprint evidence before and after conversion, reports converter failures, and rejects a candidate without mesh/object content. No Studio action, preview, package commit or new asset creation is exposed at this checkpoint.

Changed files: `EngineBridge/include/renegade/bridge/ModelImportCandidateService.h`, `EngineBridge/src/ModelImportCandidateService.cpp`, `EngineBridge/CMakeLists.txt`, `docs/MODEL_IMPORTER_REBUILD.md`, `docs/ARCHITECTURE.md`, and this handoff. The acceptance contract demands a governed `.rasset` transaction, current catalogue card, exact reopened placement, scene Save/Reopen, and a real native UI check before any READY or completion claim. The GLB source dependency closure must be checked at commit; a GLB extension alone does not guarantee embedded images. Character/animation controls remain out of this first gate.

Local Windows command: `cmake --build BUILD/no-import --config Release --target RenegadeEngineBridge --parallel 2` (VS18 CMake) passed with exit code 0; the immediate incremental repeat also passed with exit code 0 after final source transfer. `git diff --check` passed. No new GPU conversion proof or Studio interaction has run. Next: implement and test the commit/reopen transaction using the retained asset contract. Do not claim the importer works in Studio yet.

### Static GLB transaction build checkpoint - 30 September 2026

Implementation commits 46b8432 and aa9da5c on feature/model-importer-rebuild. ModelImportCommitService stages a
self-contained static GLB source, WISCENE-backed .rasset, managed projection,
registry and metadata in one project transaction. It refuses external URI
references, source fingerprint changes, rig/animation payload, existing paths
and recovery tombstones. After commit it checks the exact asset document,
current catalogue entry and stable-ID placement loader. A post-commit failure
reports committed=true and does not invite a blind retry.

Tests/ModelImporterRebuildGraphicsProof.cpp and two tiny GLB fixtures cover
actual conversion, external-reference rejection, commit/reopen and duplicate
refusal. Windows VS18 Release RenegadeEngineBridge target build passed
(exit 0, 619.46 s). RenegadeModelImporterRebuildGraphicsProof target build
passed (exit 0, 8.61 s). The first graphics run exposed a ReadBytes bug:
istreambuf_iterator did not set eofbit, causing a false source-change failure.
The reader now uses a sized binary read. VS18 Release rebuilt the proof
(exit 0, 650.92 s), and the focused graphics proof passed (exit 0, 0.66 s)
in BUILD/no-import/model-import-rebuild-proof. It covered static GLB conversion,
external-URI refusal, governed commit, exact asset reopen, current catalogue,
stable-ID placement preparation and duplicate refusal. Studio UI, scene
Save/Reopen, project reopen and creator acceptance remain unproven. Next wire
native Studio controls, then test the built UI and the full acceptance sequence.
Main and other worktrees remain untouched.

### Native static GLB Studio entry checkpoint - 30 September 2026

The isolated rebuild branch now has a real ADD > IMPORT STATIC GLB action, a
Windows GLB file picker, isolated conversion evidence, native asset-name input,
and IMPORT ASSET / CANCEL widgets. Commit calls the governed bridge transaction
at a Wicked thread-safe point; success refreshes and reveals the exact stable-ID
asset card. A committed-but-reveal-failed result is reported without suggesting
a retry. This is a static-model slice only, not the finished importer.

Windows VS18 Release RenegadeStudio built and linked (exit 0, 354.98 s); the
incremental reveal change rebuilt and linked (exit 0, 15.90 s). The executable
launched on DX12; ADD menu and its IMPORT STATIC GLB item were visible and a
click opened the native GLB picker. The previously open 9.WISCENE project was
automatically restored. No import was committed and no scene was saved. The
owner resumed desktop activity during the picker check, so interactive work
stopped and the test Studio process was closed. Candidate panel, cancel,
commit, asset reveal, placement and save/reopen have NOT been verified in the
built Studio. The panel currently reports structural counts; an actual model
render preview remains to build. Do not claim this UI READY or merge it.

Next, on a clear desktop, create a disposable Studio project and click through
file selection, evidence, name editing, cancel, commit, asset card, placement,
Undo/Redo and scene/project reopen. Then implement and verify an isolated 3D
model preview before owner acceptance. Main and other worktrees remain untouched.

### Static GLB drag repair - 1 October 2026

Implementation commit `5e0444c` on `feature/model-importer-rebuild` restores
Studio's per-frame `UpdateCreatorAssetDragPreview` call after GUI callbacks
and before chrome's consumed-pointer guard. The no-import reset accidentally
removed this existing placement block along with vegetation ticking, dirty
workspace layout handling and viewport bounds refresh; all four are restored.
The owner imported Bow 05 successfully but could not drag its CURRENT asset
card into the scene. This missing update explains the queued drag/drop failure.

Changed implementation: `Studio/src/StudioApplication.cpp` and
`Tests/ModelImporterRebuildGraphicsProof.cpp`. The graphics proof now executes
actual reusable placement, Undo, Redo, WISCENE save/reopen, stable instance
identity, mesh/object counts and wrapper position checks.

Windows VS18 command: `MSBuild BUILD/no-import/Studio/RenegadeStudio.vcxproj
/p:Configuration=Release /p:Platform=x64 /p:TargetName=RenegadeStudio_DragRepair
/p:BuildProjectReferences=false /m:2 /verbosity:minimal` passed (exit 0,
15.52 s). The side-by-side executable is in `BUILD/no-import/Studio/Release`.
The original Studio process remains open and predates this fix; no scene was
closed, saved or edited. Build warnings MSB8029 and existing C4834 remain.

VS18 CMake command: `cmake --build BUILD/no-import --config Release --target
RenegadeModelImporterRebuildGraphicsProof --parallel 2` passed (exit 0,
12.66 s). From `BUILD/no-import/Release`, ran
`RenegadeModelImporterRebuildGraphicsProof <static_triangle.glb>
<external_uri_triangle.glb> <BUILD/no-import/model-import-rebuild-proof>`
and then the same command with the owner's retained `Bow 05.glb` source and
`BUILD/no-import/model-import-bow-placement-proof`. Both printed placement/
Undo/Redo/save/reopen PASS; combined process exit 0 (1.47 s). Only disposable
BUILD projects were written. The owner's project was read for its GLB source.
`git diff --check` passed.

Remaining: save/close the old Studio safely, launch the DragRepair executable,
and visually test the exact asset card drag, drop, Undo/Redo and Save/Reopen.
Do not force-close a process with potentially unsaved work. Warn the owner
before mouse/keyboard interaction. Thumbnail generation and a real importer
model preview remain unfinished. Native acceptance and independent exact-head
verification remain pending; this is not a READY importer claim.

### Owner drag acceptance and preview implementation WIP - 1 October 2026

The owner saved/closed the old Studio. Launched the side-by-side
`RenegadeStudio_DragRepair.exe`; PID 62800 restored 9.WISCENE in DX12.
Warned before desktop control, dragged the CURRENT Bow 05 card into viewport,
and the owner immediately confirmed: "that now works". No scene save was
performed by the agent. User was released to resume desktop use.

Next authorized work is real importer model preview and asset thumbnails.
Uncommitted WIP adds `Studio/src/ModelImportPreview.h/.cpp`: private cloned
scene, auto-framed camera, fixed neutral lighting, offscreen 512x320 render,
rotation and PNG capture. Native panel has image plus real rotation buttons.
The commit service accepts an optional rendered PNG and includes thumbnail
and managed projection path in the same project transaction. Graphics proof
now renders/captures the preview and checks transaction thumbnail decode.
At this checkpoint the extended proof Release build is running; no preview
render, native preview click or thumbnail transaction success claimed yet.
Do not replace/close the running drag-repaired Studio without warning and
protecting the owner's potentially unsaved scene.

### Rendered model preview and thumbnail checkpoint - 1 October 2026

Supersedes the preview WIP checkpoint above. Implementation commit is recorded
in the following exact-commit checkpoint after the implementation is committed.

Changed: Studio ModelImportPreview.h/.cpp, StudioApplication.h/.cpp and CMake;
EngineBridge ModelImportCommitService header/source; Tests rebuild CMake and
graphics proof; MODEL_IMPORTER_REBUILD, ARCHITECTURE, FEATURE_MATRIX and HANDOFF.

The preview renders a private scene clone at 512x320 with auto-framed camera
and neutral illumination; native rotation changes only its camera. Studio
waits for usable frames then freezes the texture until rotation. The global
GUI theme had darkened the image; the image widget now bypasses tint and
background blur. Asset name positioning also keeps its label inside the panel.
No Wicked source or submodule pointer change. No forced GPU waits were added
to Studio. Captured PNG validation and persistence share the governed model
transaction; the existing Asset Browser reads the sibling thumbnail.

Commands/results (Windows VS18, Release x64):
- MSBuild BUILD/no-import/Studio/RenegadeStudio.vcxproj
  /p:Configuration=Release /p:Platform=x64
  /p:TargetName=RenegadeStudio_ModelPreview /p:BuildProjectReferences=false
  /m:1 /verbosity:quiet: exit 0, 18.61 seconds. Existing MSB8029/C4834 remain.
- MSBuild BUILD/no-import/RenegadeModelImporterRebuildGraphicsProof.vcxproj
  /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false
  /m:1 /verbosity:quiet: exit 0.
- From BUILD/no-import/Release, RenegadeModelImporterRebuildGraphicsProof
  <static_triangle.glb> <external_uri_triangle.glb>
  <BUILD/no-import/model-import-rebuild-proof>, then the same command with
  the owner's retained Bow 05.glb source and model-import-bow-placement-proof:
  combined exit 0. Triangle contrasting pixel counts 13293/4230 before/after
  rotation; Bow 2892/2574. Conversion, candidate isolation, changed rotated
  image, malformed PNG/external URI rejection, transaction thumbnail decode,
  stable-ID placement, Undo/Redo and WISCENE Save/Reopen all pass.
- ctest --test-dir BUILD/no-import -C Release --output-on-failure
  -R '^RenegadeModelImporterRebuildGraphicsProof$': 1/1 passed, 2.11 seconds.

The graphics harness now fires EVENT_THREAD_SAFE_POINT like Application,
installing asynchronously compiled pipelines before counting rendered frames.
It initializes components before conversion and drains pending pipeline work
before Application destruction, fixing the proof's earlier teardown crash.
Both generated PNGs and the corrected native Bow preview were visually
inspected. Only disposable BUILD projects were written by the proof.
A disposable native proof descriptor was prepared but native commit/card
verification was not completed: the owner resumed desktop use and 9.WISCENE
had unsaved edits. Desktop input stopped; no authored scene was saved/closed.

Next: owner or different reviewer verifies this exact implementation commit
using VERIFICATION_CHECKLIST, clicks rotation/cancel, imports into a disposable
project, confirms the thumbnail card and project reopen. Warn before desktop
input and protect unsaved work. Old thumbnails are not auto-regenerated.
Static self-contained GLB to Content/Models only; no FBX, rigs or animation.
This is an implemented/tested candidate, not a completed release gate.\n
### Exact implementation commit checkpoint - 1 October 2026

Implementation: fd8b24718085669a41f8fad1fe01f41eb52dcc7a
(Add isolated rendered GLB preview and transactional thumbnails).
The running RenegadeStudio_ModelPreview Release executable was built from
this implementation's source before its commit. All build/proof/visual
evidence and pending native checks are listed in the preceding checkpoint.
git diff --check passed before commit. This follow-up changes documentation
only. Independent verification must target fd8b247; no release gate accepted.\n
### Recovery-session native verification - 1 October 2026

Reviewed implementation: fd8b24718085669a41f8fad1fe01f41eb52dcc7a.
Repository HEAD before this documentation update:
462bf3f (Record exact preview implementation and acceptance handoff), on
feature/model-importer-rebuild. Working tree and Wicked submodule were clean.
Verifier: a different Codex conversation, recovering the interrupted session;
no implementation code was changed in this verification.

Owner evidence: the owner explicitly confirmed the corrected preview and BOTH
rotation buttons worked; that message had been lost by the frozen conversation.
The recovered desktop history contained the implementation and handoff commits.
There were no active Desktop Commander sessions or Studio/build processes when
recovery began; the old chat spinner did not indicate a running local build.

Independent native checks, Windows 11 Pro 10.0.26200, RTX 4070 Ti driver
32.0.15.9636, DX12, existing VS18 Release preview executable:
- Opened a disposable BUILD/no-import/model-preview-native-proof project.
  Its earlier incomplete descriptor was refused because it lacked startup
  content. Fixed only the disposable fixture by adding Content/Scenes/Preview.wiscene
  copied from the existing Bow placement proof; no product code change needed.
- ADD > IMPORT STATIC GLB selected the retained Bow fixture, rendered the model,
  and left the authored scene untouched. Cancel closed the panel without creating
  a model asset; the scene SHA256 stayed
  B009919E1A692EF69C9D8E7D2FBFEC48DC48E85DF572CE00B5BC6D47304E79E4.
- Reopened the picker, clicked Rotate Right (visible view change), typed Native
  Bow Thumbnail, and clicked Import Asset. Native success status identified
  Content/Models/Native Bow Thumbnail.rasset. The transaction created the source,
  RAsset, managed projection, registry record, and 512x320 PNG.
- Opened Content/Models in Project Assets and visually inspected the Bow thumbnail
  on the new card. Dragged that card into the viewport; model count rose 1 -> 2.
- Clicked native Undo: model count 2 -> 1, Redo available. Clicked native Redo
  in its updated Inspector position: count 1 -> 2. Ctrl+S cleared the dirty marker.
- Closed the saved disposable project through Alt+F4, launched a fresh process,
  opened Preview Proof from Recent Projects, and opened its Main Level. The saved
  scene reopened with model count 2 and clean Undo/Redo history.
- Asset ID a9d94fce-548d-4053-9977-ec9084e0ca46 remained in the reopened registry.
  RAsset, thumbnail and scene files retained these SHA256 hashes after reopen:
  RAsset: 4F91C22C5638B1CCF3CA3EAD54438E5E4D0E8D2E1DF7DD3121875EFD31F8C606
  PNG: C15D8D8701D904761B412F3C0CDCF1994DFB4CC9C16EAC85A74BA9F0E45C0561
  WISCENE: 51E12C4A261CE3363FAD0A21959850F589CBAE2D63800E3DCA2E5B56A98F6450

Evidence under BUILD/no-import (ignored, local only): import-proof-preview.png,
import-proof-cancel.png, import-proof-named.png, import-proof-committed.png,
import-proof-card.png, import-proof-placed.png, import-proof-undo.png,
import-proof-redo-saved.png, import-proof-project-reopened.png,
import-proof-reopened-scene.png. Screenshots were read and visually inspected.
The post-reopen PNG was verified byte-identical; the card screenshot documents
its appearance before reopening, not an additional post-reopen visual assertion.

Automated command (use the CMAKE_CTEST_COMMAND executable from CMakeCache.txt;
ctest is not on the ordinary PowerShell PATH):
ctest --test-dir BUILD/no-import -C Release --output-on-failure
  -R '^RenegadeModelImporterRebuildGraphicsProof$'
Result: 1/1 passed, 2.12 seconds test / 2.22 seconds total.
The reviewed preview executable SHA256 was
20CF05225B30D86FF942B2FFF5FBE5A976F4BDDBA67EE8995CF8B206871A4BAA;
it is the prior implementation build, not a fresh rebuild in this session.
Wicked remains 3a800b7134aafe58461093c8abb2e274d4e64033 with no tracked changes.

Result: PASS WITH LIMITATIONS for the bounded static embedded GLB native
preview/thumbnail/import/placement/save/project-reopen workflow. This completes
the outstanding native checks described in the preceding checkpoint. No global
release gate, full regression suite, new clean clone, FBX/sidecar/rig/animation,
Runtime/package parity, destination selection or old-thumbnail regeneration is
accepted by this result. Existing DX12 startup warnings in the local log remain;
no crash or visual failure was observed in the tested workflow.

Desktop input stopped; the saved disposable Studio session was closed normally.
The owner's authored project was neither opened nor modified. Next bounded work
is importer source-format expansion (FBX / retained dependency support), before
character and animation authoring. Keep the accepted static GLB slice intact.
