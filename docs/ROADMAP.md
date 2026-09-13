# Renegade Engine Roadmap

**Current programme:** Phase 7 — integrated acceptance repair  
**Merged implementation baseline:** PR #156 — Phase 7F native mesh blending parity  
**Merged commit:** `3d305be84fedf73f5b3cfbb0522be2a732c1adca`  
**Repair branch:** `repair/phase7-integrated-audit`  
**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Current state

**Phase 6 — Playable Core is complete and remains closed.** PR #148 passed the Windows workflows and owner acceptance, including repaired terrain/rigid-body contact and the packaged mini-game exit proof.

**Phase 7A–7F are implemented and merged**, but Phase 7 is not yet accepted as a complete programme. The deferred integrated audit found three creator-facing contract defects in the merged sequence. They are being repaired together on `repair/phase7-integrated-audit` so the owner does not have to validate known-broken semantics across multiple long builds.

Do not begin Phase 8 from the Phase 7 branch until the integrated repair has both exact-head Windows CI and owner acceptance.

## Merged production baseline

| Programme | Merged result |
|---|---|
| Story Flow Gates 1-10 | Project-home Journey/Graph authoring, Screen lifecycle, Runtime traversal, Build Game and standalone parity through PR #101. |
| Scene UI recovery | Shell/workspace isolation, Hierarchy and Inspector, Asset Browser placement, Environment/Terrain and consolidated whole-editor recovery through PRs #102-#106. |
| JP01 physics foundation | Wicked/Jolt physics authoring, Physics Lab, serialization, Runtime and packaged parity through PR #107, with grounded primitive/terrain-contact repair completed in PR #148. |
| Phase 5 Gates 1-9 | Scene components, cameras, decals/probes, materials/shaders, post-processing, AO/GI/reflections, ray/path-tracing exposure, lightmap/baking and render diagnostics through PRs #109-#118. |
| WD01 vegetation/frame-loop recovery | Native Wicked grass painting across Terrain chunks, restored editor interaction and accepted capped frame-loop baseline through PR #122. |
| Phase 6 Gates 1-3 | Player Start/possession, gameplay input/lifecycle and native Wicked spatial audio through PRs #123-#125. |
| Scripting S1-S7 | Inspector-provider foundation, governed Lua lifecycle, typed properties/references, gameplay API, diagnostics/IPC, Creator Library adoption and six stock Actions through PRs #127-#144. |
| Asset Browser + creator recovery | Governed catalogue/tombstone recovery and restored creator placement path through PR #145. |
| Markers + particles | Editor marker overlays and native Wicked GPU-particle authoring through PR #146. |
| Objective vertical slice | Reusable Objective Counter composed with accepted stock Actions through PR #147. |
| Native navigation + Phase 6 closure | Wicked VoxelGrid/PathQuery/CharacterComponent authoring/runtime proof plus terrain/primitive collision repair through PR #148. |
| Phase 7A | Native AnimationComponent playback/scrub/range/loop/root-motion creator controls through PR #151. |
| Phase 7B | Humanoid mapping, manual correction and Wicked-native baked retargeting through PR #152. |
| Phase 7C | Native IK, humanoid look-at and expression controls through PR #153. |
| Phase 7D | Native AnimationComponent/AnimationData timeline and keyframe authoring through PR #154. |
| Phase 7E | Hair, Force Field, Video, Spline, Gaussian Splat and remaining Terrain specialist exposure through PR #155. |
| Phase 7F | Native mesh-blend material control and global render-path exposure through PR #156. |

## Phase 7 integrated repair

The authoritative repair/acceptance document is [`PHASE7_INTEGRATED_REPAIR_AUDIT.md`](PHASE7_INTEGRATED_REPAIR_AUDIT.md).

### 7B — deterministic retarget history

The first successful retarget still uses Wicked `Scene::RetargetAnimation(..., bake_data=true, ...)`. The command now captures the resulting native `AnimationComponent` and referenced baked `AnimationDataComponent` state. Undo removes that command-owned result and Redo restores the exact snapshots without reopening the creator's FBX/GLTF/GLB/VRM/VRMA/WISCENE source.

This closes the external-source dependency in Undo/Redo and prevents orphan baked animation-data ownership. Humanoid bone-map edits also invalidate stale native ragdoll body/joint caches so the current mapping is rebuilt rather than using bodies created for a previous skeleton map.

### 7D — chronological native timeline semantics

Timeline recording is now sorted insert-or-replace rather than append-only. Key payloads stay paired with their timestamps, same-time event recording is a no-op, and event edits reset Wicked's `next_event` traversal cursor.

`CLOSE LOOP` is value continuity only and deliberately skips Event channels, so it cannot create an additional SOUND PLAY/STOP action at the loop seam.

SCRIPT PLAY/STOP is no longer exposed as a creator feature. Renegade script authority remains the governed `.rscripts` system; Wicked `ScriptComponent` timeline events will not be presented as equivalent until a deliberate adapter exists.

A blank scene can now create a real native `AnimationComponent` through **NEW CLIP**, with shared Undo/Redo.

### 7E — governed video parity

Video adoption now enters Renegade's LP08 governed-resource path rather than persisting an arbitrary machine-local MP4 path. A creator-selected video is retained under `SourceAssets/Video`, imported to `Content/Video/*.rasset`, and referenced from WISCENE by StableId metadata.

Studio scene adoption/reload restores the native Video resource from the active project. Test Level uses the same governed project product. Build dependency extraction includes referenced Video `.rasset` products, and standalone Runtime resolves them through the packaged content manifest rather than the original source path.

The existing native Video transport/Loop controls remain separate from asset identity. Regression coverage protects the StableId through Loop Undo/Redo.

## Phase 7 acceptance gate

Phase 7 is accepted only when all of the following are true:

- the integrated repair PR exact head passes the required Windows baseline and Renegade Studio workflows;
- the full CTest set, including repaired 7B/7D/7E contracts, passes;
- the owner proves real retarget Undo/Redo after making the original source unavailable;
- the owner proves NEW CLIP, out-of-order timeline recording, event-safe Close Loop, Undo/Redo and Save/Reopen;
- the owner proves governed MP4 adoption, transport, Loop Undo/Redo, Save/Reopen, Test Level and Build Game/standalone playback without access to the original external file; and
- a quick 7A–7F regression smoke finds no owner-visible regression in the other merged Phase 7 surfaces.

A green build is necessary but not sufficient. Any owner-visible failure keeps Phase 7 open.

## Phase 7 delivered scope after acceptance

When the integrated repair passes, Phase 7 will provide:

- native animation clip playback and authored settings;
- humanoid mapping and baked native retargeting;
- IK, look-at and expression controls;
- native timeline/keyframe authoring over transform, morph, light, sound, emitter, camera and material channels;
- finite terrain sculpt plus specialist material-paint/heightmap/virtual-texture controls;
- vegetation/grass and native GPU-particle authoring already delivered earlier;
- HairParticle and ForceField authoring;
- native Spline authoring;
- governed native VideoComponent media ownership and package parity;
- Gaussian Splat inspection/import exposure; and
- native material/global mesh blending.

Wicked remains the runtime/component authority. Renegade owns creator workflow, persistence contracts, governed project identity and package closure; it does not introduce a second animation/video runtime.

## Deliberate deferrals

- **Shared zones:** one future ZoneService must serve audio/gameplay systems; do not recreate feature-specific zones.
- **Flying/swimming AI volumes/navigation:** Phase 6 ground navigation is accepted; 3D movement/navigation needs its own later design.
- **Player arms, weapons, combat and production enemy AI:** not part of the Phase 7 repair.
- **Creator-facing VSync control:** remains absent.
- **Video audio track:** Wicked's VideoComponent path does not currently provide it; do not claim audio parity.
- **Timeline scripting events:** `.rscripts` needs an explicit adapter before the native timeline may expose creator script events.
- **Commercial distribution:** Build Game remains an engineering acceptance path until licensing/release packaging is separately cleared.

## Verification policy

- Green compilation is necessary but never sufficient for creator-facing work.
- Visual or behavioural owner failure overrides nominal automated success.
- Save/Reopen is required wherever authored state is persisted.
- Gameplay-facing state must be exercised in the real Runtime process.
- Packaged parity is required when a feature affects standalone gameplay.
- The original Wicked Editor remains the parity oracle for native Wicked systems.
- Exact repair evidence belongs in `docs/PHASE7_INTEGRATED_REPAIR_AUDIT.md`; this roadmap records programme state.
