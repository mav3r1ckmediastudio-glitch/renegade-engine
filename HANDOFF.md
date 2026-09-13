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
