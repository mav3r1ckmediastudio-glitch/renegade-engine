## Latest checkpoint: Wander normal role with 2x2 terrain-chunk extent - 2 October 2026

Branch: feature/character-animation-crossfades.

Added Wander as a first-class Character Role/Behaviour dropdown option without
changing existing persisted role values. CharacterAuthoringSettings now owns a
wanderExtentChunks value with default 2; the value persists through native
Character metadata, WISCENE entity serialization, duplication and Character
Prefab serialization. Existing prefabs without the field default to 2.

Runtime Wander uses the existing Wicked CharacterComponent/PathQuery movement
pipeline. At Runtime start it captures a fixed Wander origin from the Character
spawn position and derives world chunk span from the active Terrain chunkScale.
It chooses deterministic meandering destinations inside the fixed configured
extent, rejects/retries unreachable goals through the existing navigation query,
idles for a deterministic 1-4 seconds on arrival, then chooses another point.
Stuck recovery also abandons the current random destination rather than retrying
it forever. Wander uses Locomotion while travelling and Idle while paused.

Combat/threat states still interrupt the normal role. When the threat is dead or
pursuit disengages, NormalRoleIntent resolves back to Wander and requests a fresh
Wander destination. The existing pursuit/vision separation and animation
crossfade implementation were not replaced.

Diagnostics expose first_wander_origin, first_wander_extent_chunks and
first_wander_visit_count alongside existing intent/goal diagnostics.

Validation:
- EngineBridge Release rebuild: exit 0.
- AI Profiles, AI Decision, AI Combat and CW05 Character Prefab executables: pass.
- Focused CTest Profiles/Decision/Combat + source contracts + CW05 Prefab: 8/8 pass.
- Runtime RenegadeRuntime_Wander Release build: exit 0.
- Studio RenegadeStudio_Wander Release build: exit 0.
- git diff --check passes.
Known build warnings are the pre-existing MSB8029 build-directory warnings plus
existing C4834 nodiscard warnings in unrelated RenderSettings/Studio code.
No push or merge.

## Previous checkpoint: patrol/guard return diagnostics and NPC-centred pursuit - 2 October 2026

Branch: feature/character-animation-crossfades.

Pursuit origin is now the Character's own position when hostile engagement begins
(last Runtime position, then authored guard/start post as fallback), not the
player's first-seen position. Crossing Pursuit Radius suppresses Chase/Attack and
returns utility to the configured normal role. Patrol resolves its existing
authored route target; Guard resolves its saved start post. Transition diagnostics
now report pursuit origin, subject, exhausted flag, decision goal and the explicit
reason "Pursuit radius exceeded -> return to <role>".

Focused Character AI Profiles/Decision/Combat Tests + SourceContract: 6/6 pass.
Decision and Studio Release targets compile; Runtime ReturnRole build exists.
git diff --check passes. No push or merge.

## Previous checkpoint: authored pursuit leash and disengagement - 2 October 2026

Branch: feature/character-animation-crossfades.

Added a profile-driven Pursuit Radius (default 45 m) to Advanced AI. Runtime
latches the hostile engagement origin, suppresses Chase/Attack once the target
moves beyond the authored radius, and allows re-engagement if the target comes
back inside. The existing normal-role utility then resumes Guard/Patrol/Idle
behaviour; existing search/death handling remains intact. AI-05 combat scoring
respects the same exhausted pursuit subject so Attack cannot override disengage.

Changed: CharacterProfileService.h, RuntimeCharacterDecision.h,
RuntimeCombatDecision.h, AICharacterInspector.cpp, CharacterAiDecisionTests.cpp,
CharacterAiCombatTests.cpp. Studio and Runtime v145 Release builds pass. Focused
CTest: CharacterAi Profiles/Decision/Combat Tests + SourceContract = 6/6 pass.
git diff --check passes. No push or merge.

## Previous checkpoint: continuous action tails and post-combat roles - 2 October 2026

Branch: feature/character-animation-crossfades.
Implementation: e98deef50b7368c6593d191a4949df1ac45cb3ff (following 59420a5).
The following documentation commit records this exact implementation. No push
or merge. Independent exact-commit verification remains pending.

Owner reported: "that is significantly better" during native gameplay of the
continuous-tail build. Final self-sound exclusion was then tested locally;
that incremental behaviour fix has not received separate owner acceptance.

Changed files: Runtime/src/RuntimeCharacterAnimation.h, RuntimeCharacterDecision.h,
RuntimeCharacterPerception.h, RuntimeCombatDecision.h, RuntimeCombatService.h,
RuntimeLiveDiagnostics.cpp; Tests/CharacterAiAnimationTests.cpp,
CharacterAiCombatTests.cpp, CharacterAiDecisionTests.cpp,
CharacterAiPerceptionTests.cpp, CharacterAnimationBlendProof.h,
ModelImporterRebuildGraphicsProof.cpp; docs/ARCHITECTURE.md and FEATURE_MATRIX.csv.

Melee playback now overlaps the outgoing action's moving tail, including a sole
attack replay with two independent native timers. Attack transition: 0.18s.
Quietest rotational motion among assigned Idle clips becomes the base loop;
other Idle clips are occasional 12-second variations, play once, then fade back.
Dead player knowledge is excluded, perception stops observing dead players, and
self-generated attack sounds cannot create search memories. Patrol requires an
assigned route; Guard remembers and returns to its initial post. No route was
invented or added to the owner Level.

Windows x64 Release commands (installed VS18 MSBuild and CTest):
- MSBuild BUILD/no-import/Runtime/RenegadeRuntime.vcxproj /m:2 /verbosity:quiet /nologo /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /p:TargetName=RenegadeRuntime_NaturalPlayback
- MSBuild BUILD/no-import/RenegadeCharacterAiPerceptionTests.vcxproj with the same Release flags (no TargetName); likewise root DecisionTests and CombatTests targets.
- MSBuild BUILD/no-import/Tests/RenegadeCharacterAiAnimationTests.vcxproj with the same Release flags (no TargetName).
- MSBuild BUILD/no-import/RenegadeModelImporterRebuildGraphicsProof.vcxproj with the same Release flags (no TargetName).
- ctest --test-dir BUILD/no-import -C Release -R "CharacterAi(Animation|Decision|Combat|Perception)(Tests|SourceContract)|ModelImporterRebuild|HumanoidRetarget" --output-on-failure
- BUILD/no-import/Release/RenegadeModelImporterRebuildGraphicsProof.exe --inspect-scene BUILD/no-import/blending-owner-captured/Content/Scenes/TestLevel.wiscene BUILD/no-import/natural-playback-owner-proof

Final Runtime, perception, decision and combat builds exit 0. Animation and
native graphics proof builds pass. CTest 15/15, exit 0, 15.70s; includes existing
cold-reopen importer checks. Synthetic native TRS blending, action-tail overlap,
sole-attack replay, idle variation, dead-target role return and self-sound checks
pass. Captured owner scene native render proof passes, with nonblank textured
poses inspected. Actual idle scores: breathing 1.09575, flex 19.485, stretch 9.59737.
No serialized authoring changes; owner Level was not saved or overwritten.

Native Test Level final run: two characters, 13 assigned clips; five hits;
Attack -> Idle at about 7.95s when player health reaches zero; breathing at 10.42s;
flex once at 22.34s; breathing again at 26.94s. No Search after death. Existing
older character has only the stretch idle assigned and therefore retains it.
Screens from continuous gameplay were inspected; owner confirms substantially
better motion. Owner confirmation does not replace independent exact-head review.
Logs and private captures remain ignored under BUILD/no-import/natural-*.

Staged bundle: BUILD/no-import/BlendingTest/Runtime/RenegadeRuntime.exe.
SHA256 CDD417C6862A724ED049DDDCF0FF47076DAF5CE7A6BB6EB39169F49296A6729A.
Paired Studio remains the accepted importer bytes. Original Studio and Runtime
SHA256 remain unchanged (34AB9968...F5561 and 0F097DB8...B4C). Earlier bundle
runtime is preserved as RenegadeRuntime_PreNaturalPlayback.exe. Editor remains
open with owner setup; no original importer executable was replaced.

Limits: idle selection is a motion heuristic, no authored base-idle UI yet;
fixed fade durations; no gait phase synchronization, layers, additive blending
or blend spaces. Melee damage still uses the existing fire event, not authored
contact markers. Events/non-transform coverage keep guarded immediate fallback.
Next: owner verify this final bundle, especially route Patrol/Guard return, and
independent exact-commit review before merge or broader acceptance claims.
# Renegade Engine — Current Handoff

## Current importer recovery: static FBX - 1 October 2026

Branch: feature/model-importer-rebuild. Base: 8749a20, the recorded independent
native GLB verification. This checkpoint adds static FBX with embedded and
source-folder-relative textures while preserving the accepted GLB preview and
rotation workflow. Implementation commit: 07c21c9da696d8181626c92fda67b0e8fdbf98ef.
The following documentation-only commit records that exact checkpoint. No push, merge, owner-project mutation or global release claim.

### Implementation and files

- EngineBridge ModelImportCandidateService header/source: PrepareStaticModel,
  FBX dependency snapshots through ufbx, isolated conversion, exact-byte decoding
  with unique preview resource keys, and source/dependency change refusal.
- EngineBridge ModelImportCommitService header/source: CommitStaticModel,
  retained FBX/texture bundle, source registry records, cloned material relocation,
  embedded payload serialization with resource-mode restoration and paths based
  at the retained source bundle. Empty-directory cleanup preserves files and
  nonempty recovery folders. Existing strict GLB entry points remain.
- StudioApplication.cpp and RenegadeStudioChrome.cpp: static-model picker and
  summary accept GLB/FBX using the same working native preview/name/rotation/
  commit/cancel controls.
- Tests/ModelImporterRebuildGraphicsProof.cpp and ModelImporterRebuild.cmake:
  external/embedded FBX proof and fresh-process reopen cases. Generated fixture
  FBXs, checker PNG, Blender generator and provenance are under
  Tests/fixtures/Importer. Exporter-written machine paths are removed.
- docs/MODEL_IMPORTER_REBUILD.md, ARCHITECTURE.md and FEATURE_MATRIX.csv record
  the contract, evidence and outstanding acceptance.

### Build and automated evidence

VS18 Windows x64 Release, DX12, RTX 4070 Ti. Wicked remains pinned and clean at
3a800b7134aafe58461093c8abb2e274d4e64033.

Using the installed VS18 MSBuild executable, these sequential final commands
all returned exit 0 (BuildProjectReferences=false requires explicitly building
the bridge first so dependent executables link the current library):

    MSBuild BUILD/no-import/EngineBridge/RenegadeEngineBridge.vcxproj /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /m:1 /verbosity:quiet /nologo
    MSBuild BUILD/no-import/RenegadeModelImporterRebuildGraphicsProof.vcxproj /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /m:1 /verbosity:quiet /nologo
    MSBuild BUILD/no-import/Studio/RenegadeStudio.vcxproj /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /p:TargetName=RenegadeStudio_FbxImport /m:1 /verbosity:quiet /nologo
    ctest --test-dir BUILD/no-import -C Release -R RenegadeModelImporterRebuild --output-on-failure

CTest passed 5/5, exit 0, 9.29 seconds: existing GLB; external-texture FBX;
external FBX cold reopen; embedded-texture FBX; embedded FBX cold reopen.
The final sequential build-and-test process took 28.14 seconds. Existing
MSB8029 intermediate-directory warnings remain; earlier full Studio compilation
also reported existing ignored-nodiscard warnings.

The external proof rejects missing or changed texture bytes before producing a
product. Both FBX proofs verify preview pixels and rotation, retained texture
bytes, candidate isolation, thumbnail decode, placement Undo/Redo and WISCENE
save/reopen. They delete their disposable original source copies; separate
processes then verify textured placement and reconversion from retained source.
The embedded fixture's generated thumbnail was visually inspected.

Earlier failing runs are retained as diagnostic evidence, not acceptance:
an unsupported retained_dependencies recipe key was removed; a dependent build
initially linked the stale bridge library. Subsequent tests exposed texture loss
because the payload used the Intermediate directory as its relative path base.
The final implementation uses the source bundle base and embeds resource bytes;
all five cases now pass. Logs are ignored BUILD/no-import/texture-fix-*.log and
the CTest output, not committed machine-path transcripts.

### Native Studio evidence

Executable: BUILD/no-import/Studio/Release/RenegadeStudio_FbxImport.exe.
SHA-256: 492c645c608624203612ec09707f8bfd93c957526746ef7034d2f3360d79028d.
Built from the implementation changes over base 8749a20; the diagnostic revision
in the executable remains that base, so use this hash to identify the tested file.

In the disposable BUILD/no-import/model-preview-native-proof project:

- ADD > IMPORT STATIC MODEL selected static_textured_cube.fbx.
- Native preview visibly showed its red/blue checker texture; Rotate Right
  changed the view. Scene model count stayed 2 before import and after commit.
- Native name field created FBX Native Proof. Its textured thumbnail card was
  visible in Content/Models and drag/drop created a third logical model.
- Focus showed the textured cube in the viewport. Undo returned count 2;
  Redo returned count 3. Ctrl+S cleared the scene's dirty marker.
- Closed Studio normally and started a fresh process. Opened the same disposable
  project and Level: count 3 and the checker-textured cube survived. Reopened
  Content/Models and visually verified the textured card labelled CURRENT / FBX.
- Closed the test Studio normally after inspection; desktop input is released.

Stable asset ID: fc6efa99-06ff-4e1e-9b65-9e4f7e9eb281.
Thumbnail SHA-256 before/after reopen:
23ebc910d9892986dd4f78719accaac4baa23db65c255841089ffca7b68705cd.
Screenshots are reproducible ignored BUILD/no-import/import-proof-fbx-*.png.
The owner's V2 project was not opened.

### Remaining scope and next task

Static FBX only. External textures must be within the source folder tree; export
relative references. Embedded images need filenames. Ambiguous, missing,
unreadable or changed dependencies fail explicitly. glTF sidecars, arbitrary
outside-folder texture relocation, rig/animation/Character authoring, destination
selection and reimport UX are not part of this result. No Runtime/gameplay proof
or original-Wicked parity claim was made. Existing texture-free GLB tests remain
green; broader importer release acceptance is separate.

Next: owner inspects this exact FBX build with a static model, then independent
exact-commit verification before accepting any release gate. Character/animation
support requires its own bounded authoring and persistence slice. Consult this
entry and MODEL_IMPORTER_REBUILD.md instead of relying on the stalled chat.



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


### Rigged importer and collision recovery checkpoint - 1 October 2026

Implementation commit: `c14884ebdab5c5607891a1bd8f5953f68bc0ead5` on `feature/model-importer-rebuild`.
Base checkpoint: `9b5c5b4`. Main and other worktrees were not edited or merged;
no remote push or CI was triggered. Pinned Wicked source and pointer are unchanged.
The earlier conversation stalled, then the remote device connection stalled;
this recovery finished after reconnection. All implementation WIP is now committed.

Bounded result: native rigged GLB/FBX Character import with preserved skin/bind
payload and embedded clips; isolated clip preview; paused placement; corrected
native Character collision startup in standalone Runtime. The owner-reported
checker cube failure was reproduced: the floor ray hit y=5 but an unseeded
previous object matrix imparted platform inertia, sending the Character to
(1202,1365.47,1803) after 300 frames. Preparing rigid-body surface-query geometry
and seeding previous matrices from current matrices fixes it without a navigation
grid or upstream patch. A zero-time startup update is not a second gameplay loop.

Changed files:
EngineBridge/include/renegade/bridge/ModelAnimationPreviewService.h
EngineBridge/include/renegade/bridge/ModelImportCandidateService.h
EngineBridge/include/renegade/bridge/ModelImportCommitService.h
EngineBridge/src/ModelImportCandidateService.cpp
EngineBridge/src/ModelImportCommitService.cpp
EngineBridge/src/ReusableAssetInstanceService.cpp
Runtime/src/RuntimeCharacterCollision.h
Runtime/src/RuntimeLiveDiagnostics.cpp
Studio/src/CreatorAssetDragPreview.cpp
Studio/src/ModelImportPreview.cpp
Studio/src/ModelImportPreview.h
Studio/src/RenegadeStudioChrome.cpp
Studio/src/StudioApplication.cpp
Studio/src/StudioApplication.h
Tests/ModelImporterRebuild.cmake
Tests/ModelImporterRebuildGraphicsProof.cpp
Tests/fixtures/Importer/FBX_FIXTURES.md
Tests/fixtures/Importer/animated_character.fbx
Tests/fixtures/Importer/generate_character_fixture.py
Tests/fixtures/Importer/rig_checker.png
docs/ARCHITECTURE.md
docs/FEATURE_MATRIX.csv
docs/MODEL_IMPORTER_REBUILD.md

Windows x64/DX12 Release commands, run from repository root unless noted:
- MSBuild.exe BUILD/no-import/RenegadeModelImporterRebuildGraphicsProof.vcxproj
  /m:1 /verbosity:quiet /nologo /p:Configuration=Release /p:Platform=x64
  /p:BuildProjectReferences=false: PASS, exit 0.
- MSBuild.exe BUILD/no-import/Runtime/RenegadeRuntime.vcxproj
  with the same arguments: PASS, exit 0 (11.74 seconds).
- Use the CMAKE_CTEST_COMMAND executable from BUILD/no-import/CMakeCache.txt:
  ctest --test-dir BUILD/no-import -C Release
  -R '^RenegadeModelImporterRebuild' --output-on-failure:
  PASS, 7/7, 17.26 seconds. CTest is not on the ordinary PowerShell PATH.
- From BUILD/no-import/Release:
  RenegadeModelImporterRebuildGraphicsProof.exe --reopen
  ../character-mutant-proof 'SourceAssets/Models/Proof Triangle/Mutant.fbx':
  PASS, exit 0. Both fixture and Mutant have ground_intersect=true and position
  (4,5,6) after 300 updates at 1/60 second. The proof calls the production Runtime
  preparation helper, with no test-only navigation filter or BVH workaround.
- git diff --check and git -C WickedEngine status --short: clean before commit.

Standalone player inspection:
- BUILD/no-import/Runtime/Release/RenegadeRuntime.exe --project
  BUILD/no-import/model-import-animated_character-proof/RuntimeProof.renegade:
  checker cube visibly stable on the floor; Runtime startup SUCCESS and one
  synchronized Character. Saved disposable project and scene loaded in player.
- Same executable with --project
  BUILD/no-import/character-mutant-proof/RuntimeProof.renegade:
  Mutant visible with textures in reference pose on the floor at approximately
  20.69 seconds; startup SUCCESS, one synchronized Character, Player spawned,
  character_scene_sync_failed=false. Screenshot was read and visually inspected.
- Both player windows were closed normally after inspection. No input was sent
  to an authored project. Mutant remains private in ignored BUILD proof files.
- Runtime executable SHA256:
  F378221A069852D03C99F1D1F9B0062B3165C7FCEC51D771849FA5A0136D1807.
  The executable was built before the implementation commit; its embedded build
  revision is historical, so use this binary hash for the inspected build.

Local ignored evidence: collision-recovery-build.log, collision-runtime-build.log,
collision-runtime-screen.png, collision-runtime-diagnostics.json,
collision-mutant-runtime.png, collision-mutant-runtime-diagnostics.json and
Testing/Temporary/LastTest.log beneath BUILD/no-import.

Earlier-session Studio controls/build evidence is carried by the existing
character-studio-final.log and import-proof-character-* images. The recovery did
not repeat every native Studio control interaction. Existing MSB8029 and C4834
warnings remain. No full-suite Debug/Release CI, packaged export, universal FBX
compatibility, or independent exact-commit owner acceptance is claimed.

Next bounded task: external animation import/retarget and explicit per-Character
semantic action slots, starting with a real Idle/Walk example. The current slice
previews embedded clips but does not assign gameplay actions; Mutant's reference
pose in Runtime is expected. Preserve this committed static/rigged import and
collision checkpoint while adding that next slice. Do not mark a release gate
complete without independent verification of the exact implementation commit.

## External Character animation continuation checkpoint (2026-10-01)

Implementation commit: 5a53c404af545a9027b2739ac4224cbf30acce26 on
feature/model-importer-rebuild. Local only; no push, merge or release gate claim.

New Character import exposes ADD ANIMATION FBX and per-clip Unassigned, Idle,
Walk, Run, Attack, Reload, Hit and Death actions in native Wicked controls.
Append retargets an isolated clone, requires one usable humanoid and every source
take, rejects changed sources, and preserves the previous candidate on failure.
Commit retains external bytes under SourceAssets/Animations/Snapshots and writes
action metadata and durable take recipes through the governed transaction.
Legacy external recipes preserve defaults; new recipes opt into source mapping.
Existing valid destination humanoids are reused. New clips remain paused.
The asset name field now preserves edits when Import is clicked without Enter.

Changed files (relative to repository root):
- EngineBridge/include/renegade/bridge/{CreatorModelImportRecipe,HumanoidRetargetService,
  ModelImportCandidateService,ModelImportCommitService}.h
- EngineBridge/src/{CreatorModelImportRecipe,HumanoidRetargetService,
  ModelImportCandidateService,ModelImportCommitService}.cpp
- Studio/src/StudioApplication.{h,cpp}
- Tests/ModelImporterRebuildGraphicsProof.cpp
- docs/{ARCHITECTURE.md,FEATURE_MATRIX.csv,MODEL_IMPORTER_REBUILD.md}

Windows x64/DX12 Release commands follow. MSBuild.exe refers to the installed
Visual Studio BuildTools MSBuild; ctest is CMAKE_CTEST_COMMAND from CMakeCache.txt.

From repository root, each target was built with MSBuild.exe and:
  /m:2 /verbosity:quiet /nologo /p:Configuration=Release /p:Platform=x64
  /p:BuildProjectReferences=false
Targets (all exit 0):
  BUILD/no-import/EngineBridge/RenegadeEngineBridge.vcxproj
  BUILD/no-import/RenegadeModelImporterRebuildGraphicsProof.vcxproj
  BUILD/no-import/RenegadePhase7Gate7BTests.vcxproj
  BUILD/no-import/Runtime/RenegadeRuntime.vcxproj
  BUILD/no-import/Studio/RenegadeStudio.vcxproj
Studio additionally used /p:TargetName=RenegadeStudio_AnimationImport.
Final name-field-only Studio rebuild also passed, exit 0.
  ctest --test-dir BUILD/no-import -C Release
    -R 'RenegadeModelImporterRebuild|RenegadePhase7Gate7BHumanoidRetargetTests'
    --output-on-failure
PASS: 8/8, 14.12 seconds. Final name-field change does not affect bridge tests.

From BUILD/no-import/Release:
  RenegadeModelImporterRebuildGraphicsProof.exe
    '../character-mutant-proof/SourceAssets/Models/Proof Triangle/Mutant.fbx'
    '../../../Tests/fixtures/Importer/external_uri_triangle.glb'
    '../external-mutant-proof' <walk-fbx> <run-fbx> <swipe-fbx> <idle-fbx>
PASS, exit 0. Private owner source paths intentionally omitted. The proof copies
external files into disposable sources before testing. Retained Mutant had
37 bones, five clips, two textures and four external animation dependencies.
The log's "6 textures" label counts all six dependencies, not just textures.

Direct proof verified changed rendered scrub pixels, preview isolation,
Play/Pause/speed, invalid source and unmappable rig refusal, changed-source commit
refusal with no product, retained byte preservation, placement Undo/Redo and
WISCENE reopen, and 300-frame grounding at (4,5,6).
After disposable originals were deleted, retained-source reimport reproduced
clips/actions. Runtime authored Idle/Locomotion/Run/Attack each resolved one
playing variant. Full external retarget reimport was in the same proof process;
do not describe that part as a fresh-process proof.

Native Studio inspection used the side-by-side executable and disposable project.
Walk played and scrubbed visibly, its action survived adding Idle, and native
Idle and Walk assignments persisted into the three-clip Mutant.rasset recipe.
The final executable separately saved Native Name Verified.rasset after typing
the new name and clicking Import without Enter. Screenshots were read and
visually inspected. Studio was closed normally; owner projects were not edited.

Standalone Runtime loaded external-mutant-proof/RuntimeProof.renegade via an
absolute --project argument (relative paths resolve under the executable folder).
Its migrated Story Flow entered Main Level successfully. Mutant was textured
and visibly on the floor. Diagnostics: startup SUCCESS, scene_loaded=true,
character_scene_sync_failed=false, player_spawned=true, one synchronized
Character; first_animation_clip="mutant idle", semantic Idle, playing=true.
Runtime was closed normally after inspection.

Inspected binary SHA256:
- Studio: 34AB99682403DB624C5182FAE2E82F440A8B2095EC2674406B6DA84BBA7F5561
- Runtime: 0F097DB8EDB26468830100074C9BD00F839778DDAD10355E5E496F27EB889B4C
Binaries were built before the implementation commit. Embedded diagnostic
build_commit is historical 9b5c5b4; use these hashes for inspected builds.

Ignored local evidence under BUILD/no-import:
external-final-*.log, external-animation-studio-build-final.log,
external-animation-studio-name-build.log, external-animation-ctest-final.log,
external-mutant-proof.log, import-proof-external-native-*.png,
import-proof-external-name-*.png, import-proof-external-runtime-final.png,
external-runtime-final-diagnostics.json, and the disposable proof project.
git diff --check passed; FEATURE_MATRIX has 16 columns per row; Wicked unchanged.

Limitations: bounded new imports only. No custom slots, clip frame authoring,
existing-Character editing, universal multi-rig compatibility or packaged export
claim. Legacy ReusableAssetReimportRecipeTests and CreatorExternalAnimationImportTests
refer to removed preparation services in this baseline and were not counted;
new optional-recipe checks run in the active graphics proof. Existing MSB8029
and unrelated C4834 warnings remain. No full-suite CI or independent acceptance.

Next task: independent exact-commit inspection and owner acceptance of this
native importer slice, then separately scope existing-Character action editing.
Preserve the implementation commit and this evidence before further changes.

## Native Character crossfade checkpoint (2026-10-02)

Implementation commit: bea6313e2ebfa270fc955b4016f9e28215d5140e.
Branch: feature/character-animation-crossfades, based on importer evidence
checkpoint 6a08fb0. The importer branch and its owner test executables remain
unchanged. No push, merge, or release gate completion.

Crossfade slice:
- AI still chooses actions; native action metadata still selects clips.
- Runtime owns transient logical weights and elapsed simulation time; native
  Wicked evaluates the poses. Sequential amounts use cumulative normalization.
- Matching channel coverage and event-free clips blend; other clips immediately
  switch. Only contributing clips are scanned/sorted per character.
- Loops fade for 0.20s; Attack/Reload/Hit for 0.08s; Death for 0.05s.
- Repeated requests do not restart loops/one-shots. A returning contributing
  loop preserves its timer. Interrupted transitions preserve current weights.
- Hit finishes before a new Attack/Reload; Death interrupts and stays terminal.
  Missing Death stops owned playback once. Missing Idle also stops owned clips.
- Native root motion is disabled for AI-owned clips: Character movement remains
  controller-owned. No speed blend space, gait synchronization, upper-body layer
  or additive animation is claimed. Durations are fixed in this slice.

Changed files: Runtime/src/RuntimeCharacterAnimation.h, RuntimeLiveDiagnostics.cpp;
Tests/CharacterAiAnimationTests.cpp, CharacterAnimationBlendProof.h,
ModelImporterRebuildGraphicsProof.cpp; docs/ARCHITECTURE.md, FEATURE_MATRIX.csv,
MODEL_IMPORTER_REBUILD.md. This handoff is a separate documentation commit.

Exact Windows Release commands, from repository root unless noted:
- Use CMAKE_COMMAND from BUILD/no-import/CMakeCache.txt:
  cmake -S . -B BUILD/no-import: PASS.
- MSBuild.exe BUILD/no-import/Tests/RenegadeCharacterAiAnimationTests.vcxproj
  /m:2 /verbosity:quiet /nologo /p:Configuration=Release /p:Platform=x64
  /p:BuildProjectReferences=false: PASS, exit 0.
- Same MSBuild arguments with
  BUILD/no-import/RenegadeModelImporterRebuildGraphicsProof.vcxproj:
  PASS, exit 0.
- Same arguments with BUILD/no-import/Runtime/RenegadeRuntime.vcxproj
  plus /p:TargetName=RenegadeRuntime_Blending: PASS, exit 0.
- Use CMAKE_CTEST_COMMAND from the cache:
  ctest --test-dir BUILD/no-import -C Release
    -R 'RenegadeModelImporterRebuild|RenegadeCharacterAiAnimationTests|RenegadePhase7Gate7BHumanoidRetargetTests'
    --output-on-failure: PASS, 9/9, 16.14 seconds.
- From BUILD/no-import/Release:
  RenegadeModelImporterRebuildGraphicsProof.exe
    '../character-mutant-proof/SourceAssets/Models/Proof Triangle/Mutant.fbx'
    '../../../Tests/fixtures/Importer/external_uri_triangle.glb'
    '../blending-mutant-proof' <walk-fbx> <run-fbx> <swipe-fbx> <idle-fbx>
  Private source paths deliberately omitted; sources copied to disposable files.
  Native translation, rotation and scale tests verify midpoint/order, no repeated
  frame accumulation, interruption continuity, loop phase, completed Death pose
  and partial-track immediate fallback. Semantic tests cover Hit priority,
  missing Death termination, optional Run fallback and state reset.

Real Mutant rendering proof writes blend-*.png under blending-mutant-proof:
Idle, Idle/Walk halfway, Walk, Walk/Run halfway, Run, Run/Idle halfway and Idle.
These snapshots were opened and visually inspected. Native channels matched,
and intermediate poses showed the expected mixtures. The governed import,
retained-source reimport, placement Undo/Redo/reopen and 300-frame grounding
proofs also run in the same executable.

Standalone inspection:
  BUILD/no-import/Runtime/Release/RenegadeRuntime_Blending.exe --project
  <absolute repository path>/BUILD/no-import/blending-mutant-proof/RuntimeProof.renegade
PASS: startup SUCCESS; scene_loaded=true; character_scene_sync_failed=false;
one synchronized Character; Player spawned; mutant idle playing as Idle.
Mutant was visibly textured and on the floor; screenshot read and inspected.
This standalone inspection exercises saved-scene loading and Idle playback;
transition midpoints are validated by the native graphics proof, not manually
triggered through gameplay in that standalone run. Runtime closed normally.

Blending executable SHA256:
BAC9ED6BC29CC44CE1E6F891BFC1E65218B881D6620675B8D386F1480C664C4E.
Executable built before commit; embedded revision is historical.
Original Runtime hash remains
0F097DB8EDB26468830100074C9BD00F839778DDAD10355E5E496F27EB889B4C;
original Studio_AnimationImport hash remains
34AB99682403DB624C5182FAE2E82F440A8B2095EC2674406B6DA84BBA7F5561.

Ignored evidence under BUILD/no-import: blending-configure.log,
blending-animation-tests-build.log, blending-graphics-build.log,
blending-runtime-build.log, blending-ctest-final.log, blending-mutant-proof*.log,
blending-mutant-proof/blend-*.png, import-proof-blending-runtime-final.png,
blending-runtime-final-diagnostics.json. Private assets remain ignored.
git diff --check passes; all FEATURE_MATRIX nonempty rows have 16 columns;
Wicked submodule source and pointer unchanged. Existing MSB8029 warnings remain.

Limitations and next task:
Fixed durations and matching coverage only. No UI duration authoring, Debug/full
CI, packaged export, crowd-performance claim or independent acceptance.
Owner tests the preserved importer executable first. Independently inspect this
exact blending implementation before integration. Then test AI movement/action
interruptions in an owner-approved disposable Level; separately scope editing
existing Character assignments. No release gate is marked complete.

Final real Mutant proof rerun after the last source/test changes: PASS, exit 0,
6.81 seconds; native crossfade and real-render snapshot checks pass; retained
reimport/actions and 300-frame grounding pass. The final log is
BUILD/no-import/blending-mutant-proof-final.log; disposable asset identity
68c9f808-17ab-423c-b532-ee1595d9a095. No source files changed after that run.

## 2026-10-02 owner importer acceptance and paired blending Test Level
Owner reports ALL animation files imported, assigned actions and played correctly;
with a nav grid, Character idles, walks, runs toward Player and attacks.
Owner also confirms a fresh Level includes sky, sunlight and default nav grid.
The older runtime-proof fixture intentionally lacks normal fresh-Level defaults.

Prepared ignored BUILD/no-import/BlendingTest (no production source changes):
RenegadeStudio_BlendingTest.exe is byte-identical to accepted AnimationImport
Studio (SHA256 34AB99682403DB624C5182FAE2E82F440A8B2095EC2674406B6DA84BBA7F5561).
Runtime/RenegadeRuntime.exe is byte-identical to crossfade Runtime_Blending
(SHA256 BAC9ED6BC29CC44CE1E6F891BFC1E65218B881D6620675B8D386F1480C664C4E),
implementation bea6313e2ebfa270fc955b4016f9e28215d5140e; prior handoff 0d4bcc5.
Copied support Content/shaders/BuildInputs/DX compiler; original binaries untouched.
Copied external-mutant-proof project to BUILD/no-import/blending-owner-project.
Original editor PID 63288 had unsaved fresh-Level edits: cancelled its close
prompt, preserved it open and minimized. New paired editor PID 51056 is open.

Native Studio PLAY launched PID 7280, parent 51056, using the bundle Runtime
and copied-project Intermediate/TestLevelSnapshots/1790939239452207-0000000000.
GET http://127.0.0.1:38742/snapshot: startup SUCCESS, scene loaded, Player spawned,
Character synced, mutant idle playing; no missing animation request.
Normal CloseMainWindow on child Runtime returned paired Studio to READY /
TEST LEVEL COMPLETED; child exited. Screenshots visually inspected:
BUILD/no-import/import-proof-blending-pair-editor.png (fresh Level defaults),
import-proof-blending-pair-runtime.png and import-proof-blending-pair-return.png.
Older copied runtime-proof has four clips and no nav grid, so this verifies launch,
startup and return, not owner pursuit blending acceptance. Owner must repeat the
configured pursuit scenario with this paired build; Asset/Character selector next.
No new code/build required: reused tested binary hashes. No push, merge or release gate.
## 2026-10-02 owner-scene partial-track crossfade repair
Implementation: 5fdd4c677bbffbad6472484690c51ad2d70b6cf1 on
feature/character-animation-crossfades (no push/merge).
Changed RuntimeCharacterAnimation.h, RuntimeLiveDiagnostics.cpp,
CharacterAnimationBlendProof.h, ModelImporterRebuildGraphicsProof.cpp,
docs/ARCHITECTURE.md and docs/FEATURE_MATRIX.csv REN-AI-007.
Owner reported Run -> three attacks without blending -> stretch Idle loop.
Live original test PID 44348 was the correct paired Runtime. Player had taken
five hits and was dead (0 health), explaining return to Idle after combat.
Captured its disposable Test Level under BUILD/no-import/blending-owner-captured.
Two AI Characters; 13 assigned clips. Native inspection found 93-track source
clips and 90-track retargeted clips: matching-coverage guard caused immediate
switches between those groups. Native controllers had no competing animation list.
Runtime startup now completes assigned transform coverage using constant tracks
from the target's initial local pose; native blending remains authoritative.
Only transient Runtime scene changes; saved Levels/importer products untouched.
Event/non-transform tracks still use guarded fallback. Counters expose fades,
incompatible switches and per-Character blend status through live diagnostics.

Release MSBuild.exe (VS18 BuildTools) commands, from repository root:
BUILD/no-import/Runtime/RenegadeRuntime.vcxproj /m:2 /verbosity:quiet /nologo
/p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false
/p:TargetName=RenegadeRuntime_BlendingFix -> PASS, build log owner-fix-runtime.
Same switches without TargetName on RenegadeModelImporterRebuildGraphicsProof.vcxproj
and Tests/RenegadeCharacterAiAnimationTests.vcxproj -> PASS.
Logs under BUILD/no-import/blending-owner-*-build.log; existing MSB8029 warnings.
ctest.exe --test-dir BUILD/no-import -C Release -R
'ModelImporterRebuild|CharacterAiAnimation|HumanoidRetarget' --output-on-failure
-> final PASS 9/9, 13.71s; log blending-owner-ctest-final.log.Native proof executable command (repository root):
BUILD/no-import/Release/RenegadeModelImporterRebuildGraphicsProof.exe
--inspect-scene BUILD/no-import/blending-owner-captured/Content/Scenes/TestLevel.wiscene
BUILD/no-import/blending-owner-fixed-proof-final
-> PASS exit 0; log blending-owner-fixed-proof-final.log.
Synthetic native partial translation/rotation/scale midpoint, repeated sampling,
completion and idempotent coverage tests pass. Existing lifecycle checks pass.
Both owner Characters pass Idle/Walk/Run/Attack/Attack/Idle render checks.
Initial inspection fixture produced blank images; corrected isolation retains
referenced meshes and disables physics only in preview clones. Added pixel-content
assertion so blank frames fail. Final character-1 run-attack-half, run-attack and
attack-attack-half PNGs read and visually inspected: visible distinct poses.
The source captured WISCENE is never saved or edited by inspection.

Replaced only paired BUILD/no-import/BlendingTest/Runtime/RenegadeRuntime.exe
with new Runtime_BlendingFix; preserved former bundle runtime as
Runtime/RenegadeRuntime_PreCoverageFix.exe. New SHA256:
E9EAA5EC91DCFE51BD5102969B7C7FD28376CC1EA004CFA81101D9B838172AE7.
Original accepted Studio and original Runtime binaries remain untouched.
Native Studio PLAY launched new Runtime PID 35704, parent 51056. Startup SUCCESS;
13 clips; five crossfade transitions; zero incompatible transitions or missing
requests; five hits and Player dead. Evidence: blending-owner-runtime-fixed-
diagnostics.json, import-proof-blending-coverage-runtime.png and coverage-return.
Runtime closed normally, Studio returned to the same owner Level ready to Play.
Standalone visual screenshot did not frame the Character; visual transition proof
comes from the inspected isolated owner-character renders, not this screenshot.
Owner subjective gameplay acceptance is still required. Fixed durations remain
0.20s loops, 0.08s actions, 0.05s Death. Idle variant scheduling/loop policy remains
basic; this repair does not change which assigned Idle variant is selected.
No Debug/full CI/package-export/layered blending claims or release gate completion.
## 2026-10-03 main CI repair integration and Alpha Playability handoff

PR #176 (`fix/main-ci-green`) was verified with four green pull-request checks
and merged into `main` as merge commit
`3bb768b148da590ea9dd7aa47fbb28006c0508d6`.

The repair addressed three clean-run CI assumptions exposed after the large
Character/importer integration:
- EngineBridge Debug required MSVC `/bigobj` for large translation units;
- the Studio CI aggregate build had to include every registered test executable
  it later asks CTest to run;
- the rebuilt importer graphics proof had to distinguish hosted-headless PNG
  readback from owner-hardware visible-model pixel acceptance.

The final PR head `f0b4f815f345dd1765bd4fde3c8ce623bea912e5`
passed Windows baseline Debug/Release and Renegade Studio Debug/Release on a
clean GitHub checkout. Local owner-hardware validation separately retained the
full rendered-model visibility assertion, and the hosted-headless importer group
passed 7/7 while preserving import/commit/reopen coverage.
The post-merge `main` Windows baseline and Renegade Studio workflows were
triggered automatically for `3bb768b`; they were still in progress when this
documentation handoff section was written and must be checked before calling the
merged main globally green.

The active programme is now **Alpha Playability — Player Arms & Combat**.
Canonical design authority:
`docs/PLAYER_ARMS_COMBAT_FRAMEWORK.md`.

The programme extends the existing Player rather than replacing it. Both hands
are first-class; combat families share equipment/action/projectile/effect
foundations. Required reference families are firearm, directional sword + shield,
bow, crossbow and magic. Directional melee uses attack/guard directions and swept
weapon collision rather than a generic centre-camera melee ray.

The first implementation gate is **P1 — First-person Arms Rig**. A dedicated
owner combat-feel gate is mandatory later for recoil, camera response, animation
timing, SFX, particles, muzzle flash, projectile/impact feel, directional melee,
parries/guards, bow/crossbow timing and spellcasting presentation. Automated
damage/state correctness is not sufficient acceptance.

## 2026-10-03 P1 recovery and honest acceptance checkpoint

Implementation baseline: 51513b95e98e6f962c0052da3f92f1b8c279b280
on feature/p1-first-person-arms-rig. Local recovery reference:
recovery/p1-baseline-20261003. The recorded origin branch points at the same
baseline; no fetch/push/merge was performed by this recovery.

Read docs/P1_STATUS_AND_RECOVERY.md before resuming P1. It inventories the four
implementation commits, source capabilities, actual test coverage, failed
experimental Runtime evidence, missing acceptance and the next bounded task.

Preserved temporary RuntimeApplication diagnostic source and patch under ignored
BUILD/renegade/p1-recovery-20261003-2217, then restored RuntimeApplication.cpp.
Foreground policy already matches baseline. Runtime, EngineBridge, Studio and
Tests now have no source diff against 51513b9. Experimental BUILD projects and
asset variants remain preserved and unaccepted; original owner arms are untouched.
Generated Tools/__pycache__ was not committed.

Changed documentation: README.md, docs/ROADMAP.md, docs/FEATURE_MATRIX.csv,
docs/P1_STATUS_AND_RECOVERY.md and this handoff. No new gameplay implementation.

Exact commands, hashes and evidence limits are in the status document.
VS18 BuildTools local incremental Release and Debug Runtime builds passed;
Release and Debug P1 executable builds passed. P1 tests passed 2/2 per
configuration (0.17s each). Related Release regression passed 9/9 (0.39s),
including the P1 pair. Existing MSB8029 warnings remain. Wicked source/pin clean.
Logs and preserved patch: BUILD/renegade/p1-recovery-20261003-2217/.

No Runtime window launched during this recovery. No direct UI/save/reopen,
real-arms visual fix, independently exported game or clean-CI claim. Synthetic
product round-trip tests do not prove live rendered arms. P1 is NOT accepted.

Next task: stabilize one fixed no-imported-arms fixture and prove grounded Player,
visible landmarks and camera/rig agreement before examining one immutable arms
asset. Stop at the first failed isolation check. Do not rotate/re-export more
variants while replacing the fixture. Verify the coordinate/rest/animation basis
before correction. Subsequent owner exact-build visual and packaged parity
verification are required; do not move to P2 on test passes alone.

## 2026-10-03 P1 no-imported-arms placeholder proof

Owner requested that supplied arms stop being used. Generated a separate
BUILD/renegade/p1-placeholder-proof project with no arms assignment or imported
assets and no Story Flow override. Baseline Release Runtime now visibly shows
solid floor, three landmarks and orange/blue proxies extending from the lower
view. Two inspected captures 62.07 seconds apart retain world framing.
Diagnostics: Player spawned; proxy geometry true; imported asset loaded false;
Character count zero. Runtime left open for owner movement/look verification.

Added Tests/PlayerViewRigFixture.cpp and optional Windows
RenegadePlayerViewRigFixture CMake target to preserve the no-asset scene generator.
See docs/P1_PLACEHOLDER_PROOF.md for commands, hash, evidence and limits.
Configure and generator Release build pass; generator save/reload passes on
separate output; P1 Release tests 2/2 pass. Existing MSB8029 warnings remain.
No production Runtime/Player/View Rig code changed; original arms untouched.
No skinned-arms or live movement/pitch/yaw or full P1 acceptance claim.

## 2026-10-03 owner placeholder movement and idle acceptance

At 22:32 Europe/London owner reports movement is fluid and idle works well on
the placeholder arms. This verifies movement feel and procedural idle on the
baseline Release Runtime (51513b9 source; hash in P1_PLACEHOLDER_PROOF.md).
No imported skeletal asset is involved. Existing running fixture unchanged.
Explicit look extremes, sprint, jump/landing, pause/resume and R reset are the
remaining immediate owner checks. Input bindings confirmed from the fixture's
governed GameplayInput file; checklist added to docs/P1_PLACEHOLDER_PROOF.md.
No implementation change or full P1 acceptance.


## 2026-10-03 directional proxy stutter repair (owner verified)

Owner clarified that the other placeholder checks work, but moving in a
direction makes the proxies stutter. This supersedes broad movement acceptance.
A native 300-frame Scene/Jolt test reproduces a 0.0498593-unit camera/rig-root
position mismatch at 75 Hz rendering and 120 Hz physics, with forward/backward
and sideways walking/sprinting. The mismatch equals raw versus interpolated
Player position; this is not evidence of a defective imported arms asset.

ApplyRuntimePlayerCamera now prefers the post-Scene::Update Player transform,
matching the attached rig's interpolated presentation position. Physics remains
movement authority, with raw position retained only as a missing-transform
camera fallback. The placeholder scene, geometry and idle behaviour are unchanged.
Tests/PlayerViewRigFixture.cpp retains the native movement regression and rejects
both camera/rig drift and an inconclusive run without interpolation divergence.
Build/test results and executable identity are recorded in P1_PLACEHOLDER_PROOF.md.
No upstream Wicked changes, push, merge or full P1 acceptance.

At 22:50 Europe/London the owner tested the repaired Release Runtime and reported
"stutter is gone". Directional placeholder movement is accepted on this fixture.
Release and Debug native movement proofs pass with maximum camera/rig error
2.38419e-07 and real interpolation divergence 0.0498593. Both configurations'
Player/arms/snapshot checks pass 5/5. P1 skinned-asset/package acceptance remains open.


## 2026-10-03 proxy snapshot and detached package proof

Camera repair source remains 4dd9953 and owner-verified Runtime is preserved.
Added Tests/PlayerViewRigParityFixture.cpp and its optional Windows manual target
in Tests/CMakeLists.txt. No gameplay production code changed.
Production TestLevelSnapshotService snapshot saved/reloaded and launched in
standalone Runtime; assignment remains empty and control scene path unchanged.
StageWindowsGameBuild plus ApplyWindowsGameExecutableIdentity and stage validation
pass. Copied package launches from unrelated working directory with no arguments;
bootstrap verifies package integrity and DX12 success, live diagnostics resolve
only detached GameData paths. Descriptor/scene/input hashes match the control.
Snapshot and package screenshots visually inspected with identical proxy framing.
Commands, hashes and evidence are in docs/P1_PLACEHOLDER_PROOF.md. An initial
stage-only trial failed the expected Gate 3 schema check; corrected tool now
finishes executable identity before producing the launchable package.

Updated docs/P1_STATUS_AND_RECOVERY.md so the supplied owner/GGMAX arms are
explicitly excluded and live camera agreement is no longer described as untested.
Next: owner checks directional movement, pause/resume and R reset in the open
P1ProxyParity.exe package. Then validate the actual Studio Test Level/Build Game
UI workflow. Real skinned arms, hand-bone sockets and asset/package closure remain
unaccepted. This fixed three-input manual plan is not a new general packager.
No push, merge or P1 gate acceptance.

## 2026-10-03 23:05 Europe/London: packaged proxy controls accepted

After being asked to check movement, Escape pause/resume and R reset in the
open detached package, the owner reported "everything works". These requested
checks are accepted for this exact proxy package. Proof tooling source: ee79134;
camera repair source: 4dd9953. Package executable SHA-256:
a5532514ed33f33cdb8c554808cb1e72de13fb54ac3d863c094dc64e60e47ace
No gameplay code or binary changed after this acceptance. Preserve the package
and fixed control fixture. Next bounded task is the actual Studio Test Level
and Build Game UI workflow. Real skinned arms, skeletal animation, animated
hand-bone sockets and real-asset dependency closure remain unaccepted; P1 remains
open. Do not resume the withdrawn owner/GGMAX arms experiment.

## 2026-10-03 Studio Test Level stale bundled Runtime repaired

Owner reported no arms through Studio Test Level. Process inspection confirmed
the correct proxy snapshot, but Studio selected its bundled Runtime from
16 September, before P1. Stale bundle hash:
56f7ac45a73ba17e03bcfbcda33f0274b53eee167e08fe8b2e2ff0b7615d171b
Closed obsolete child PID 59816 and detached package PID 26420, preserved the
old executable in the ignored proof folder, then synchronized the verified
Runtime executable, dxcompiler and Content into Studio/Release/Runtime.
Source and bundle now both hash:
69536da66fd6f40ebc392387ddeb0ce4e218f4373f0a74465f45b2e9524d1b92
No production source change. Tools/Build-Studio-Windows.ps1 already performs
this synchronization; direct-target builds had bypassed its packaging step.
Build Game also prefers this bundle. Live Runtime diagnostics initially belonged
to detached PID 26420 rather than Test Level PID 59816: match diagnostic PID to
child PID before drawing conclusions. Owner button retry remains pending.

## 2026-10-03 23:23 Europe/London: Studio Test Level gameplay accepted

After synchronizing Studio's bundled Runtime, owner retried the Test Level
button and reported "yes everything works". Requested movement/look/sprint/jump,
pause/resume and reset checks accepted for the proxy fixture through Studio.
Live Runtime PID 48644 matches Studio's child PID, loads the new TestLevel
snapshot, reports startup success, proxies enabled and no imported arms.
Evidence: BUILD/renegade/p1-placeholder-proof/studio-test-accepted.json.
Studio's captured ready flag remains false while its update is suspended;
this does not establish readiness-indicator acceptance. Gameplay owner result
is accepted; actual Studio Build Game export remains next. Source checkpoint
5d4d985 records bundle repair; verified Runtime hash unchanged. Full P1 open.

## 2026-10-03 Studio export-test project prepared

Added Tools/StoryFlow/Create-P1ExportFixture.ps1. It refuses existing destinations,
creates fresh stable IDs, copies only the accepted scene/input unchanged, creates
scene identity metadata and a three-node/two-route Story Flow, and declares the
input map Always Include. Owner project is the ignored
BUILD/renegade/p1-studio-export-proof/P1ExportProof.renegade. Control project and
accepted Runtime/Studio binaries were not changed.
Extended Tests/PlayerViewRigParityFixture.cpp with --inspect-export-project,
calling the real PrepareWindowsGameBuildProjectState readiness boundary with a
hidden GPU context and synchronized teardown. Release helper build passes;
owner export project and a second fresh generator-validation project both pass
InspectProject/dependency/registry/route preflight, exit 0, one Level completion.
The generated scene/input SHA-256 match the control. git diff --check passes.
Commands and logs are in docs/P1_PLACEHOLDER_PROOF.md. Early helper trials failed
before document newline/GPU teardown corrections and are not acceptance evidence.
Next: owner closes Test Level, opens P1ExportProof in Studio, uses Build Game and
checks the resulting exported game's movement/look/idle/pause/reset. Export may
briefly open Runtime for automatic smoke validation. This preflight is not an
actual export or full P1 acceptance. No supplied arms resumed; no push or merge.


## 2026-10-03 P1 native hand socket binding checkpoint

Implementation commit: bab0de7 on feature/p1-first-person-arms-rig. Prior owner
Studio export-launch confirmation is recorded by 757ee6d: actual P1 Export Proof
Windows build launches/rendered DX12 at 75 FPS with both proxies and landmarks.

Changed Runtime/src/RuntimePlayerViewRig.h and RuntimePlayerViewAsset.h: explicit
primary/off-hand/support native boolean anchor metadata binds sockets to bones or
child grips in the view-model skeleton. Missing roles retain original offsets;
duplicate/non-skeletal/cyclic anchors fail before mutation and failed asset commit
retains proxies. Native Wicked animation/hierarchy remains the sole pose authority.
No movement/camera contract change or supplied owner/GGMAX arms use.

Extended PlayerViewRigTests and manual PlayerViewRigFixture; added shared generated
native skeleton and GPU proof headers. Updated P1_STATUS_AND_RECOVERY, ROADMAP,
ARCHITECTURE and FEATURE_MATRIX; full contract, commands, hashes and limits are in
docs/P1_HAND_SOCKET_BINDINGS.md. Runtime/tests/fixture build Release and Debug exit
0. Final related CTest checks pass 5/5 per configuration. Packaged governed rasset
loader binds remapped anchors; save/reload, atomic rejection, fallback and cleanup
checks pass. Generated rigid geometry is not a production skinned arms asset.

Manual --socket-proof DX12 runs 300 frames of translation/rotation Idle Walk Sprint,
forward/backward/sideways movement and pitch/yaw in both configurations. Socket
matrix error 0; camera/rig error 2.38419e-07; actual animated local Z range 0.0592m;
pause matrix/timer errors 0; three captures per configuration; exit 0. Rendered
captures inspected with yellow primary, green off-hand and purple support markers.
Evidence in ignored BUILD/renegade/p1-socket-proof; build logs under p1-socket prefixes.
New Release standalone Runtime starts the accepted unchanged proxy project in DX12,
then closes only its new test window cleanly, exit 0. This is startup regression,
not owner controls acceptance of the new binding build. Existing MSB8029 remains.

Runtime candidate hashes: Release
3b3e0bbddcdd7cab9251bca92b7e64ee2c63b7406aac5107d10d24513f0968ae;
Debug de0d86e3f5ad45d6507bbc3934743df7aa63789f059126ce06b20aea53925f3d.
Studio's bundle and owner-approved export deliberately remain at the previously
accepted camera-sync binary; they do not contain this new socket candidate yet.
Wicked source/pin unchanged; no push/merge. Untracked Tools/__pycache__ and log.txt
left alone. P1 remains open; this is C++/native metadata exposure only.

Next bounded outcome: command-backed Studio primary/off-hand/support bone/grip
selection and grip offsets with Undo/Redo and governed asset save/reopen. Then use
one agreed separately generated/authored skinned view model for owner verification;
preserve the accepted proxy control. Do not resume the withdrawn supplied arms.

## P1 native Hand Grips editor checkpoint — 2026-10-04

Code and documentation commit: 7a105d8dcb1b6db188fc006e0432127b0d972cab
on feature/p1-first-person-arms-rig. No push or merge performed.

Outcome: Player's governed arms selector now offers EDIT HAND GRIPS. Native
Studio widgets expose primary/off-hand/support roles, native bone hierarchy
choices and local position/rotation offsets. The private bridge working copy
has its own CommandService Undo/Redo history. Save journals the .rasset product,
managed projection and registry together and retains hand_grips in the import
recipe. Close discards unsaved changes. Bones are identified by canonical
hierarchy-name arrays, excluding the creator transform wrapper; ECS IDs are not
persisted. Invalid/missing/ambiguous bones and malformed ownership markers fail
before mutation. DirectX-compatible rotation decomposition fixed the nonzero
three-axis roundtrip mismatch found during this session.

Validation:
- EngineBridge, Runtime, PlayerViewRigTests, PlayerViewRigFixture and Studio
  build Release and Debug, exit 0. MSB8029 and existing C4834 warnings remain.
- Tests target now uses /bigobj for Debug JSON-generated sections. After CMake
  regeneration rerun MSBuild so it reads the new project, rather than executing
  an already-loaded stale project definition.
- Related CTest selector Phase6Gate1Player|PlayerViewRig|TestLevelSnapshot|
  ReusableAssetTests: 6/6 per configuration.
- New headless checks: offsets and rotation, validation without partial mutation,
  Undo/Redo and saved-state boundary, injected AfterReplace(index 1) failure and
  byte-for-byte three-file rollback, successful retry, native reopen, creator
  recipe reapplication and stale external-product rejection.
- Release/Debug authored-grip DX12 proof: 300 native animated motion frames;
  socket matrix error 0, camera error 2.38419e-07 metres, animated bone Z range
  0.0592 metres, pause matrix/timer errors 0, three captures each, exit 0.
  The animated-range probe now reads the bone parent of the authored anchor;
  the grip transform's local offset is correctly static.
- Native Release Studio on an isolated synthetic fixture: selected product,
  opened real Hand Grips controls, slider changed primary Z 0.24 -> 0.583,
  SAVE persisted 0.5829999446868896, Close/reopen restored 0.583 with clean history.
  Player scroll layout now contains the arms selector and grip button; scene
  gizmo/outline are suppressed while this editor is open. Native UI capture:
  BUILD/renegade/p1-hand-grips-proof/studio-reopened.png.
- Debug Studio launch from the isolated proof working directory hit a Wicked
  DX12 graphics-pipeline assertion at wiGraphicsDevice_DX12.cpp:3939 before
  editor entry. Root cause not established. Debug build/headless/GPU evidence
  must not be described as Debug Studio startup acceptance.

Exact build commands and limits are in docs/P1_HAND_GRIP_EDITOR.md. Ignored logs
use BUILD/renegade/p1-grips prefixes; tests/GPU/UI fixture artifacts are under
BUILD/renegade/p1-hand-grips-proof. Release Studio was built with
/p:TargetName=RenegadeStudioHandGrips because owner's original Studio was running
and its executable was locked. Existing owner Studio, proxy control project,
Runtime bundle and accepted exported game were not replaced. Discarded only
agent-owned test windows during UI verification.

SHA-256:
Studio Release candidate:
7EA283E885D82A0FE6A1188660E57F138624E6BA987F5461C02BFB7CB2A078DF
Runtime Release:
47EFF1BDA3795FB70CFB1FDF4803AFDA11932C8AD7FEB34F6831D19326286539
Runtime Debug:
0E1FC32C4A81C911CC74E34F01FC83583BEA0F8770870FAECFC2CA7EC182650F

Binaries were compiled from checkpoint changes on parent 2f8662d before the
source commit, so their displayed source revision remains that parent. No owner
exact-commit verification is inferred. The two-bone native test product is
synthetic; it is not a real FBX import or proof of skinned deformation.

P1 remains IN PROGRESS. Original owner/GGMAX arms remain withdrawn. Next bounded
outcome: owner verify the native grip editor, resolve Debug Studio startup
assertion if reproducible from its normal resource directory, then agree on a
separately generated/authored skinned view model for rendered acceptance.
No production grip IK, weapon behaviour or P1 completion is claimed.

## Studio DX12 startup assertion repaired - 4 October 2026

Branch: feature/p1-first-person-arms-rig.
Source/diagnostic commit: 1446cef7be51f8edf1c3c2675d4f37d45a27f7e9.
Parent: f737777. No push or merge.

Confirmed the former Debug failure at wiGraphicsDevice_DX12.cpp:3939 is
CreateCommandSignature, not pipeline creation. Native DX12 debug layer reports
#743: root parameter slot 0 was not declared to hold constants. CDB stack reaches
StudioRenderPath::LoadGridResources. RenegadeGrid and RenegadeImGui signatures
placed CBV(b0) first, conflicting with the pinned backend's counted draw setup.

Changed all four Studio/shaders/RenegadeGrid*/RenegadeImGui* HLSL files to reserve
one root-constant DWORD at slot 0 using native b999; retained matching VS/PS
signatures and b0 binding. Upstream source, submodule, arms/gameplay, accepted
owner project and exported game are unchanged. Corrected the earlier misleading
grid-shader comment and P1_HAND_GRIP_EDITOR assertion description.

Added Tests/StudioShaderDx12Proof.cpp and its manual Windows GPU target in
Tests/CMakeLists.txt. Updated docs/FEATURE_MATRIX.csv, docs/P1_HAND_GRIP_EDITOR.md
and docs/STUDIO_DX12_SHADER_ASSERT.md. The latter records exact diagnosis,
commands, logs and acceptance boundaries.

Validation (from repository root; CL=/MP4 for proof and Release Studio):
- cmake -S . -B BUILD/renegade: exit 0.
- MSBuild BUILD/renegade/Tests/RenegadeStudioShaderDx12Proof.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false: exit 0. Repeat Configuration=Release: exit 0.
- BUILD/renegade/Tests/Debug/RenegadeStudioShaderDx12Proof.exe Studio/shaders: exit 0. Repeat Tests/Release: exit 0. All four shaders compile and pass real Wicked CreateShader/command-signature creation; both pipeline descriptions accepted.
- Original shaders under CDB reproduce #743. Corrected grid plus original ImGui: grid passes; ImGui fails identically; direct negative process exit 2170.
- MSBuild BUILD/renegade/Studio/RenegadeStudio.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false: exit 0. Repeat Release with /p:TargetName=RenegadeStudioDx12Fix: exit 0.
- ctest --test-dir BUILD/renegade -C Debug -R "Phase6Gate1Player|PlayerViewRig|TestLevelSnapshot|ReusableAssetTests" --output-on-failure: 6/6. Repeat -C Release: 6/6.
- CDB -c g with debugdevice: Debug reaches Project Hub, loads isolated HandGripsProof, renders floor/landmarks/grid and opens native Hand Grips with saved primary Z=0.583. Zero DX12 errors/breaks; two nonfatal #680 depth-view pipeline warnings remain. Release reaches rendered welcome screen; no DX12 validation messages in captured startup.
- All four source/deployed shader SHA-256 pairs match per configuration; git diff --check passes.

Native captures: BUILD/renegade/p1-hand-grips-proof/dx12-debug-grid.png and
dx12-debug-hand-grips.png. Application CDB logs: dx12-studio-Debug.txt and
dx12-studio-Release.txt there. Build/proof/negative/CTest logs use dx12-* under
BUILD/renegade. Agent-owned debugger sessions stopped after capture; owner's
original Studio and earlier Release grip candidate remain open.

Debug Studio SHA-256:
928016EFCF63041419D0BCDB4B01850C67522CDD8C89222EC5D266B677556A92
Release RenegadeStudioDx12Fix SHA-256:
A225220EFEBDD487372E86B940E89D1CE5FC20BB52B296AF1823A5796CBCC7EE
Shader deployment is part of the repair; exe hash alone is insufficient. Binaries
were built on parent f737777 with this patch; displayed revision remains parent.
No exact-source-commit owner acceptance is inferred.

Risks/next: synthetic copied fixture emits unrelated Story Flow stable-identity
errors and is not a production project acceptance. Nonfatal #680 warnings remain.
This bounded startup fix does not close P1 or prove real skinned arms. Next is
independent exact-commit verification and owner acceptance of native grip editing
on an appropriate governed asset; withdrawn original/GGMAX arms remain withdrawn.

## P1 isolated textured shotgun assembly proof - 4 October 2026

Source/proof checkpoint: a858e87b73ff3d632b7391834f60e99d6781fd0b.
Branch feature/p1-first-person-arms-rig; parent e8df1b5. No push or merge.
User authorized inspection/preview using supplied SawedOffShotgun pack, rather
than the earlier full-body Shotgun Animset. Original withdrawn arms remain unused.

Changed Tests/FirstPersonAssemblyGraphicsProof.cpp, Tests/CMakeLists.txt and
docs/P1_SHOTGUN_ASSEMBLY_PROOF.md. Added a manual Windows DX12 native diagnostic
and optional interactive view; no Studio/Runtime/Player/production importer or
upstream source/pin changes. No feature-exposure change; FEATURE_MATRIX and
production P1 status remain unchanged.

Owner-supplied FBXs found locally; four idle/reload inputs match uploaded bytes.
Copied pack and separate Manny D/N textures into ignored input tree. Weapon and
shell textures were already supplied. FBXs contain stale original-author paths;
governed PrepareModel correctly refuses unresolved dependencies. Fixture reports
that refusal, uses raw native converter and explicitly relinks supplied D/N
textures plus weapon/shell ORM maps. It does NOT prove governed asset import,
material relink UI, retained-source reimport or packaged game integration.

Native conversion: arms161 bones/483 channels; weapon7 bones/21 channels.
Arms Idle7s, weapon Idle0.666667s; both Reload3s. Separate skeletons preserved.
Direct identity weapon_r attachment was visibly wrong. Fixture derives a
provisional offset from right-hand and weapon Handle reference pivots at time0,
then retains native hierarchy attachment. Not an authored socket/IK solution.
Fixed first-person camera inspects original import coordinates; production
axis normalization/camera-relative placement remains unresolved.

Evidence:
- cmake -S . -B BUILD/renegade: exit0.
- CL=/MP4; MSBuild BUILD/renegade/Tests/RenegadeFirstPersonAssemblyGraphicsProof.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false: exit0, existing MSB8029 warnings.
- From BUILD/renegade/Tests/Release:
  RenegadeFirstPersonAssemblyGraphicsProof.exe ../../p1-shotgun-proof/input ../../p1-shotgun-proof/captures: exit0 with explicit texture relink including ORM.
- Overview and FP captures at0,.6,1.2,2,2.9s; Idle/Reload WISCENE saves reopened and rendered at1.2s.
- Textured idle FP, reload FP and reopened overview inspected: arms beneath
  camera, gun forward, red shells and wood/arm details present, reload opens gun.
- --view starts paused; R restarts both reload tracks, Space pauses/resumes,
  Escape closes. Initial blurry64px canvas corrected by refreshing app window
  canvas and render-path size after resize. Corrected live window inspected
  sharply at larger size. NO keyboard/mouse automation; owner controls pending.

Power cut interrupted first viewer-check attempt. User restarted Desktop Commander
with established npx.cmd remote launcher; connection restored and resumed above.
Only earlier agent-owned preview closed for size fix. Final proof viewer remains
open for owner (PID18824 at checkpoint; do not assume PID after a restart).
No running owner Studio was detected at initial inspection; accepted proxy
project/export/Runtime were not modified.

Ignored evidence BUILD/renegade/p1-shotgun-proof: input copies, captures,
textured-proof.log, viewer.log and viewer-window.png. Build log
BUILD/renegade/p1-shotgun-build.log.
Final Release proof exe SHA-256:
8C92B649D9279B5853DBD76B2D3A01C6B1B0AC1AB6A639FC3BC488827DCF8282.
Built from pre-commit working changes; no independent exact-commit gate claimed.
No third-party assets committed.

P1 remains IN PROGRESS. Final grip fit, loose-shell attachment/visibility events
and ammo/action timing need inspection; Unreal montage text is not a complete
demo Blueprint setup. Next: owner inspect R/Space/Escape preview, resolve exact
authored attachment/timeline, then bounded governed assembly import/relink/save
workflow, followed by existing Player View Rig integration. Do not broaden into
inventory/pickups or replace working Player controls at this checkpoint.

## P1 proof lateral grip correction - 4 October 2026

Source checkpoint 9af049296ad681f11a18bbfa5df06e58b5a229ff; parent ce2a2af.
Owner confirms animations work very well; reports gun/shells right of hands.
Uploaded screenshot read locally successfully despite displayed missing-file error.
Changed Tests/FirstPersonAssemblyGraphicsProof.cpp and
docs/P1_SHOTGUN_ASSEMBLY_PROOF.md: +0.025m imported-X correction to entire weapon
root before local attachment derivation (camera-right is -X). Shells follow
existing native weapon skeleton; clips/hands/camera/gameplay unchanged.

Same Release MSBuild command from prior checkpoint exits0; full textured capture,
WISCENE save/reopen/render proof exits0. Logs alignment-proof.log and existing
p1-shotgun-build.log. Idle and reload1.2s/2.0s FP renders visually inspected:
lateral mismatch reduced; exact final fit remains owner-pending. Agent-owned
previous preview closed through CloseMainWindow, corrected --view reopened.
No mouse/keyboard automation, push, merge, upstream or production Player changes.

Corrected Release proof SHA-256:
B539693C7A1A25E7CB42D735FF715875D332550BE56F070858FFA19DEB96F953.
Pre-commit compiled source content; owner exact-commit verification not inferred.
Animations accepted by owner, corrected alignment not yet accepted; P1 still
IN PROGRESS. Next assess grip/shell contacts through full reload with owner.

## Authored shotgun attachment restored - 4 October 2026

Source checkpoint 7c4a9762c864029f09b9449452e8aeda562edb99; parent 56d5b93.
Owner explicitly rejects previous 25mm calibration: stock rests on back of hand,
reload shells hover below palm. Animation quality accepted; grip NOT accepted.
Changed Tests/FirstPersonAssemblyGraphicsProof.cpp and
docs/P1_SHOTGUN_ASSEMBLY_PROOF.md only. Read-only original Unreal package
inspection located BP_DemoCharacter SKM_Weapon_GEN_VARIABLE serial export
369438..370399: AttachToName ik_hand_gun; location
(-3.466970,-27.336276,4.505738)cm, Rotator(6.552304,-182.929938,-10.254553).
Proof replaces guessed weapon_r placement with this exact converted local
transform. Skeleton GripPoint is middle_01_r, not demo attachment.
No original asset, upstream, production Studio/Runtime/Player change.

Same bounded Release MSBuild command recorded above: exit0. Full paired
captures and WISCENE save/reopen/render: exit0, authored-proof.log.
Idle, reload1.2s/2s and new live viewer visually inspected: fit improved,
owner acceptance pending. No original Unreal/Wicked Editor parity claimed.
Current exe SHA256 A81A3421BEBD20CD4F52E74B8F46736153B7ECA85A84098E252868A431641E20.
Built pre-commit source content; no independent exact-commit gate verification.
git diff --check clean. Existing untracked Tools/__pycache__/ and log.txt left.
Only earlier agent diagnostic closed via CloseMainWindow. New preview left open,
PID6632 at checkpoint; use process name/title, not a stale PID. R reload,
Space pause/resume, Escape close. No keyboard/mouse automation, push or merge.

P1 remains IN PROGRESS. Next owner inspect actual grip and shell contacts through
reload; resolve any remaining authored event/visibility behavior before governed
assembly import/relink/save workflow and existing Player View Rig integration.

## Owner acceptance and assembly import foundation - 4 October 2026

Source checkpoint e329ac830be1c7d79cd0a76656b1b4cc88894f94; parent 1ffe86e.
Owner accepts authored shotgun diagnostic grip and reload ("almost perfect",
then confirms no remaining mismatch). Baseline source 7c4a976; diagnostic exe
A81A3421BEBD20CD4F52E74B8F46736153B7ECA85A84098E252868A431641E20.
This is visual diagnostic acceptance, not production P1 completion.

Changed shared import/recipe/commit boundaries for explicit material-slot texture
relinks and native matching-rig clip ingestion; added MatchingRigAnimationService,
headless rejection/recipe tests, workflow/cold proof modes and a separate manual
workflow target. Studio's existing ADD ANIMATION now automatically preserves
matching native rigs, with prior humanoid retarget fallback retained. Texture
relink bridge API has no creator UI yet. Full changed-file list: git show --stat
e329ac8. README, architecture, roadmap, feature matrix and P1 docs updated.
Canonical detailed limits/commands: docs/P1_ASSEMBLY_IMPORT_FOUNDATION.md.

Release configure and builds of Bridge, Studio, matching-rig tests, workflow
proof, importer graphics proof and PlayerViewRig tests: exit0. Same MSBuild
/m:2 /nologo /verbosity:quiet /p:Configuration=Release /p:Platform=x64
/p:BuildProjectReferences=false with CL=/MP4.
CTest selection recorded in p1-workflow-ctest.log: six of six pass (matching-rig,
PlayerViewRig tests/contract, GLB importer, animated Character and cold reopen).
Real-pack --workflow exits0: both rigs/Idle+Reload clips retained, committed,
stable-ID reopened, reconstructed from retained recipes and reassembled using
accepted authored parent/transform. Duplicate relinks and disposable changed-
texture commit reject without product. Maximum clone normalization 1.78814e-7
for arms, zero for weapon; strict indices/binds preserved, no retargeting.
--workflow-reopen exits0 in a separate process; all FP frames require lit model
pixels, all captures require visible pixels. Fresh-process reload1.2s inspected
via unique cold-verified-reload-1743.png after stale same-path image display.
Final --workflow rerun with final proof executable exits0.

Studio exe SHA256 7390E00685B8DE7DB509AA6ECF2B3989F8036C7486F11257C78F0DEE1E27D5FA.
Workflow exe SHA256 9CE51204BC79A3729E06C7875284A1ACB8FAC78830CF864A0C785E3C84F7E168.
Compiled before commit (final newline-only normalization); exact-commit
independent acceptance/CI and Debug not claimed. Existing warnings remain.
Existing ReusableAssetReimportRecipeTests build blocked by removed
CreatorModelMaterialPreparationService.h from prior importer rebuild; recorded,
not silently counted passing or repaired by this scoped change.
Deep initial proof project hit Windows staging-path limit; final ignored project
uses BUILD/p1wf short destination. No production path-length fix claimed.

Evidence: BUILD/p1wf plus BUILD/renegade/p1-shotgun-proof/workflow-proof.log,
workflow-cold-proof.log and p1-workflow-*.log. Original assets/Unreal projects
untouched; licensed files not committed. No mouse/keyboard automation, new
Runtime launch, upstream change, push or merge. Accepted diagnostic viewer stays
open (6632 at prior checkpoint; resolve by process/title). Studio built but not
restarted. Untracked Tools/__pycache__/ and log.txt preserved.

Next bounded outcome: native Studio first-person assembly controls over these
bridge APIs; choose retained arms/weapon parts, explicit parent bone and authored
transform, paired semantic clip preview, persist/reopen one assembly product.
Then connect that assembly to the existing Player Start and prove camera-relative
skin, Test Level and Build Game parity. Do not treat the current two Character-kind
rigged-part test products as a final Weapon Asset/NPC workflow, and do not broaden
into inventory/ammo/fire gameplay. P1 remains IN PROGRESS.

## Studio assembly authoring recovered - 4 October 2026

Base source checkpoint 6562c81; implementation checkpoint is the commit containing
this section (resolve with git log). Branch feature/p1-first-person-arms-rig.
Recovered uncommitted assembly candidate rather than rebuilding accepted imports.
Changed-file inventory: git show --stat at that implementation commit. New bridge
FirstPersonAssemblyService, Studio FirstPersonAssemblyEditor, recipe tests and
workflow proof additions expose retained part selection, explicit attachment,
paired native preview, transactional assembly save/reopen and command-backed
Player Start assignment. README, architecture, roadmap and feature ledger updated.

Recovered evidence: p1assembly-ctest.log has seven passing Release checks;
p1assembly-final-save.log and p1assembly-final-cold.log report exact recipe reopen,
both rigs and paired camera rendering. Source-unavailable proof is separately
recorded in p1assembly-source-unavailable.log. These are prior-session results,
not newly rerun or independent acceptance. Cold Reload-1.200000.png visually
inspected during recovery: textured hands, weapon and shells are visible.
Studio panel screenshot studio-assembly-12.png exposed dark image styling,
parent-label overlap and clipped status. Applied the existing importer theme
exception to assemblyImage_ (white sprites and disableBackground), shortened the
parent label and expanded panel height. Also restart completed paired previews
on Play and report assignment failure accurately after successful product save.
Final UI appearance still requires direct inspection; no visual success inferred.

Recovery build command from repo: set CL=/MP4; MSBuild executable from VS18
BuildTools Current/Bin on BUILD/renegade/Studio/RenegadeStudio.vcxproj with /m:2
/nologo /verbosity:quiet /p:Configuration=Release /p:Platform=x64
/p:BuildProjectReferences=false /p:TargetName=RenegadeStudioAssemblyRecovery.
Exit 0, p1assembly-recovery-studio-final.log. Alternate exe name leaves the
already-running Studio untouched. SHA256:
FC38B3B890EAEA0488391C086139EE510172C59C49C314BB34A4CA5C0EBE7815.
Existing MSB8029 temporary-directory warnings remain. No application launched,
closed or controlled during recovery. No upstream change, push or merge.
Initial ctest invocation failed because PATH lacks CTest; do not count stale
LASTEXITCODE from that invocation. Correct explicit VS18 CTest binary used for
--test-dir BUILD/renegade -C Release -R
'FirstPersonAssemblySettings|MatchingRigAnimation|RenegadePlayerViewRig'
--output-on-failure; result in p1assembly-recovery-ctest.log.

P1 remains IN PROGRESS. Next: inspect the rebuilt Studio panel with the owner,
then extend existing RuntimePlayerViewAnimation to evaluate the assembly arms
and weapon tracks together (currently treated as action variants), preserving
camera/controller ownership. Prove real assembly Test Level and Build Game parity.
Do not broaden into ammo, firing, inventory or pickups. General texture relink UI,
draft Undo/Redo and assembly update/rebuild remain outstanding. Existing
Tools/__pycache__/ and log.txt stay untracked.

Corrected recovery CTest invocation exits 0: four of four pass, 0.34 seconds
(PlayerViewRig tests and source contract, MatchingRigAnimation, assembly settings).
git diff --check passes; Git warns only about LF-to-CRLF normalization in appended
documentation. Recovery build is pre-commit source-identical code; independent
exact-commit verification and owner panel inspection remain open.

Exact recovered assembly implementation commit: 2be899e. The following checkpoint
commit changes documentation only. Release recovery executable and four passing
checks correspond to the implementation code; final Studio visual inspection and
Runtime paired-action integration remain pending.

## Owner assembly preview acceptance and name-field repair - 4 October 2026

Owner reports everything works as expected in recovery build after testing native
assembly preview. Then reports renamed asset name did not save. Read-only project
inspection confirms Shotgun Assembly Verified.rasset and its recipe/projection/
thumbnail exist; a later First Person Assembly product also exists. Root cause:
OpenAssemblyEditor always resets the name input to the default despite reopening
the assigned assembly recipe. Studio/src/FirstPersonAssemblyEditor.cpp now derives
the reopened name from the assigned product's registered path stem. Stable asset
identity, product files and owner level remain unchanged. Save still creates a new
product; in-place asset renaming is not introduced. Name-field visual acceptance
and level persistence after owner actions remain unverified.

Build: same VS18 Release MSBuild command recorded above, with
/p:TargetName=RenegadeStudioAssemblyNameFix; log p1assembly-name-fix.log. Alternate
exe avoids changing the owner's running recovery build. No automatic restart.

Name-field repair Release build exits 0; git diff --check passes. Existing MSB8029
warnings remain. Commit containing this section is the bounded repair checkpoint.

## Runtime paired assembly movement - 4 October 2026

Base e7be162; implementation is the commit containing this section. Owner confirms
native assembly controls work as expected, then confirms selecting the named asset
and saving the level. Read-only real-product proof resolves the saved named assembly
assignment. P1 remains IN PROGRESS; no gameplay reload or release gate closure.

Changed RuntimePlayerViewAnimation.h: recognize explicit assembly root marker and
track roles, require one arms/weapon pair per movement action and an Idle pair,
set both paused native timers from one clock before the existing Wicked scene
update, suppress root motion and unused action tracks, hold shorter clips until
whole-pair wrap, preserve clocks across movement fallbacks, reset both tracks.
Legacy single-rig variant/crossfade path is retained. Pair-to-pair transitions
currently switch immediately; paired crossfades remain deferred. Diagnostics adds
paired-assembly and active-track count without per-frame clock event spam.
Tests/PlayerViewRigTests.cpp covers native pose evaluation of both tracks with
unequal starts/durations, no double timer advance, pause, wrap, fallback, action
switch/reset, missing/duplicate partners and native serialized entity remapping.
Tests/FirstPersonAssemblyGraphicsProof.cpp adds --runtime-assembly and
--runtime-package proof modes. README, roadmap, architecture, feature matrix and
P1_ASSEMBLY_AUTHORING.md updated. Full changed-file inventory: git show --stat.

VS18 Release MSBuild (same executable and CL=/MP4 as prior checkpoint): targets
Tests/RenegadePlayerViewRigTests, Tests/RenegadeFirstPersonAssemblyWorkflowProof,
Runtime/RenegadeRuntime with /m:2 /nologo /verbosity:quiet /p:Configuration=Release
/p:Platform=x64 /p:BuildProjectReferences=false: all exit 0. Final logs:
BUILD/renegade/p1paired-final-<target>.log. Explicit VS18 CTest --test-dir
BUILD/renegade -C Release -R
'FirstPersonAssemblySettings|MatchingRigAnimation|RenegadePlayerViewRig'
--output-on-failure: 4/4 pass, 0.41 seconds, p1paired-ctest.log.

Manual real-product proof commands, from repo:
BUILD/renegade/Tests/Release/RenegadeFirstPersonAssemblyWorkflowProof.exe
BUILD/p1wf/project-ad198d5b/AssemblyProof.renegade BUILD/p1paired --runtime-assembly
exits 0 (p1paired-project-proof.log). Saves disposable Test Level snapshot and
verifies assigned stable ID survives snapshot loading; no owner level rewrite.
Same exe with BUILD/p1paired/isolated-package BUILD/p1paired/cold-final
--runtime-package exits 0 (p1paired-cold-final.log) in a separate process.
Isolated asset fixture has only product and manifest: no retained-source rigs,
recipes or textures. Both modes verify two armatures, no second physics/Character,
paired rendering through the native Runtime loader/controller, pause and cleanup.
Cold paired-1.100000.png visually inspected: textured arms and shotgun visible.
This tests packaged loading, not the actual Build Game UI/export acceptance.

Initial headless native-pose test used full Scene::Update without a graphics device
and crashed; corrected to the pinned native animation dependency scan/update and
job wait. Initial proof compile hit ambiguous Translate initializer; corrected to
explicit XMFLOAT3. Initial diagnostic build rejected float DiagnosticValue;
removed the continuous clock observation. Final builds/tests above pass. Existing
MSB8029 warnings remain. Debug, full suite, CI and independent exact-commit review
not claimed. git diff --check passes.

Studio had stale Release/Runtime/RenegadeRuntime.exe, preferred over sibling Runtime.
After confirming no Runtime process running, copied final executable to that
existing Studio launch path; both SHA256:
352B73620B68A6B9D4B7D383E46825D9556F0B8BAD1501CB15AF99BB0274026F.
Studio remains open; no live Runtime launched or user app closed by this work.
No upstream change, push, merge or licensed asset commit. Existing untracked
Tools/__pycache__/ and log.txt preserved. Next owner closes assembly panel and
uses Studio PLAY to verify real camera-relative arms/weapon rendering and look;
then actual Build Game parity. Read diagnostics for paired=true and active_tracks=2.
Reload/Attack/Equip remain preview-only. No inventory/ammo/fire scope expansion.
