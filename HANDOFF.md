# Renegade Engine — Current Handoff

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
