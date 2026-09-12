# Phase 7 Integrated Repair Audit

**Status:** post-CI repair candidate — v3 compiled green, then a deeper ownership audit found and corrected native hierarchy gaps; new exact-head Windows CI and owner acceptance required before merge  
**Repair branch:** `repair/phase7-integrated-audit`  
**Repair baseline:** `main` at `3d305be84fedf73f5b3cfbb0522be2a732c1adca` (merged Phase 7F)  
**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Why Phase 7 was reopened

Phase 7A–7F reached `main`, but an integrated audit found three cases where the creator-facing surface overstated or under-proved the underlying contract. The repair is intentionally bounded to those defects. It does not replace Wicked animation/video systems, reopen accepted Phase 6 work, or redesign the Phase 7F mesh-blend slice.

The blocking findings were:

1. **7B retarget Undo/Redo was not deterministic.** Redo could depend on reopening the creator's original animation source instead of restoring the exact baked native result created by the first execution. Baked `AnimationDataComponent` entities also needed explicit command ownership.
2. **7D timeline authoring could violate native event/key semantics.** Recording appended keys rather than guaranteeing chronological ordering, Close Loop could duplicate event channels, and creator-facing SCRIPT PLAY/STOP implied integration with Renegade scripting even though Wicked `ScriptComponent` is not the `.rscripts` runtime.
3. **7E Video authoring used a machine-local file path.** That was incompatible with Renegade's governed project asset/package model and could not provide reliable Save/Reopen, Test Level or standalone parity.

Compilation alone is not the repair exit gate. Exact-head CI and owner behavioural proof are still required.

## 7B — deterministic humanoid retarget repair

`RetargetHumanoidAnimationsCommand` still uses Wicked's native `Scene::RetargetAnimation(..., bake_data=true, ...)` for the initial conversion. The repair changes command ownership after that first successful conversion:

- each created native `AnimationComponent` is snapshotted;
- every referenced baked `AnimationDataComponent` is captured once by stable entity ID;
- Undo removes both the retargeted animation entities and their command-owned baked data;
- Redo restores those captured native components at their original entity IDs;
- Redo does **not** reopen FBX, GLTF, GLB, VRM, VRMA or WISCENE input;
- entity-ID reuse fails closed rather than silently binding restored samplers to unrelated data; and
- pose reset remains Wicked-native before/after the operation.

Changing a humanoid bone map invalidates the native cached ragdoll bodies/joints because they correspond to the previous mapping. This is transient runtime cache invalidation, not deletion of an independent Renegade ragdoll authoring document; the existing ragdoll path remains responsible for rebuilding native bodies/joints from current authored state.

### 7B proof boundary

Automated mapping/classification tests and the strengthened source contract prove the deterministic command structure. The exact retargeted motion still requires owner proof with a real compatible character and source animation because a synthetic unit fixture would not prove creator/importer compatibility.

Owner acceptance must prove:

- auto-map/manual correction still work;
- `IMPORT + RETARGET` creates playable native clips;
- Undo removes the created clips;
- the original source file can then be moved/renamed;
- Redo restores the clips without consulting that source file; and
- save/reopen retains the restored clips.

## 7D — native timeline/event repair

The Renegade timeline remains an authoring surface over Wicked `AnimationComponent`, channels, samplers and `AnimationDataComponent` state. The repair makes its mutation rules match that native authority.

### Ordered insert-or-replace

Recording first normalises existing key order while preserving each key's payload. A new key is inserted chronologically; a key at the same timestamp replaces its value rather than creating a duplicate. Event channels have zero-width payloads, so recording the same event at the same time is a no-op.

Move/Delete/record operations reset Wicked's per-channel `next_event` cursor for event channels so playback does not retain a traversal index from the pre-edit ordering.

### Close Loop is value continuity only

`CloseTimelineLoopCommand` copies the first value to the requested seam time for value channels. Native event channels are deliberately skipped. Closing a loop must never manufacture another SOUND PLAY/STOP event at the seam.

### Script boundary

Renegade ACTION/SCRIPT/GLOBAL SCRIPT authoring is governed by `.rscripts` and its established runtime. Wicked `ScriptComponent` is a different native system. Phase 7 therefore does not expose SCRIPT PLAY/STOP as working creator timeline presets. The enum values remain reserved for a future explicit adapter, but the creator picker does not advertise them and recording rejects them.

### Blank-scene timeline creation

`CreateTimelineAnimationCommand` provides an undoable `NEW CLIP` path that creates a real native `AnimationComponent`. Timeline authoring no longer requires a model import to have supplied the first clip.

### 7D proof boundary

`RenegadePhase7Gate7DNativeTimelineTests` checks:

- native blank-clip creation and Undo/Redo;
- chronological key insertion;
- value/time pairing after sorting and moving;
- same-time event de-duplication;
- event payload remains empty;
- script-preset rejection;
- Close Loop adds value continuity keys without adding event keys; and
- post-audit native ownership: newly-created `AnimationDataComponent` entities are children of their Animation owner, that relationship survives Undo/Redo recreation, and recursive clip deletion removes those children.

Owner acceptance must additionally confirm the actual Studio controls, playback/scrub behaviour, save/reopen and audible event behaviour.

## 7E — governed video ownership and package parity

Video now follows the same project-ownership principles as other LP08 governed resources.

### Creator adoption

`CreatorVideoWorkflowService` accepts a creator-selected supported video, retains the source under `SourceAssets/Video`, and imports the authoritative product under `Content/Video/*.rasset` through `ResourceAssetService`. The scene does not persist the creator machine's absolute path.

`SetVideoAssetCommand` binds the native `VideoComponent` to an in-memory Wicked resource and persists:

- `renegade.video_asset_binding_version`; and
- `renegade.video.asset_id`.

`VideoComponent::filename` is deliberately empty for governed video. The stable LP08 product ID is scene truth.

Loop edits remain ordinary native `VideoComponent` authored state and do not modify the governed StableId. Regression coverage explicitly protects that separation.

### Save/Reopen and Runtime

`StudioSession` restores governed video bindings after a committed scene open/project adoption and after Reload. The restoration resolves the persisted StableId through the active project's LC01/LP08 product state and recreates the live Wicked video resource.

Authoring/Test Level Runtime performs the same project-root restoration before gameplay. Standalone Runtime resolves the StableId through `GameData/content-manifest.json` and the packaged LP08 payload; it does not require the original external video or LC01 at runtime.

### Dependency closure

`ResourceAssetDependencyProvider` inspects both governed material-texture and governed video metadata in WISCENE. A referenced Video StableId emits a required `DependencyClass::Video` LP08 product into the build closure. A scene containing only video bindings is not incorrectly treated as having no governed resource dependencies.

### 7E proof boundary

Source contracts protect the adoption, StableId, dependency and packaged-runtime seams, while the specialist executable tests ensure ordinary loop Undo/Redo cannot erase governed identity. Actual MP4 decode/playback remains an owner/GPU integration check rather than a fake unit claim.

Owner acceptance must prove:

- add/select a native Video component;
- `ADOPT MP4` imports and plays an H264 MP4 in Studio;
- Loop, Play, Pause, Stop and Seek work after adoption;
- Loop Undo/Redo does not detach the video;
- save, close and reopen restores playback without reselecting the original file;
- moving/renaming the original external MP4 after adoption does not break the reopened scene;
- Test Level plays the governed video; and
- Build Game/standalone Runtime plays it from the packaged project with no access to the original source path.

Wicked upstream still does not provide video audio-track playback through this component; this repair does not claim otherwise.

## Post-CI audit — four-green v3 was not sufficient

The v3 head `8278d0a531f701296a9127a3ade69219c38ad56e` completed both Windows baseline and Renegade Studio workflows successfully in Debug and Release. A deliberate post-CI logic audit then found defects that compilation/source-contract success did not expose. That v3 result is therefore superseded as merge evidence.

### Finding A — 7B native hierarchy ownership

Pinned Wicked does not create a baked retarget as a flat set of components. It attaches the retargeted animation entity beneath the destination humanoid and attaches every baked animation-data entity beneath that animation. V3 restored component bytes and sampler IDs but did not recreate those hierarchy relationships.

The correction now:

- validates the original Wicked parent relationships during first-execution capture;
- stores parent entity identity, not raw `HierarchyComponent` bytes;
- recreates the animation/data components at their deterministic IDs;
- calls Wicked `Scene::Component_Attach()` to rebuild ownership exactly through the same native API used by Wicked's original retarget path;
- fails closed on any entity-ID reuse before restoration; and
- uses recursive entity removal for fallback baked-data cleanup.

This matters because Wicked recursive deletion follows hierarchy ownership. A restored clip that merely plays is not sufficient proof if deleting its owner can leave orphan clips or baked data.

### Finding B — 7D native AnimationData ownership

Creator-authored timeline channels in v3 created native `AnimationDataComponent` entities but did not attach those entities beneath the owning Animation. Wicked's FBX/GLTF importers do attach sampler data to the Animation entity.

The correction now:

- calls `Component_Attach(dataEntity, animationEntity)` when a creator channel first creates its sampler data;
- removes the complete data entity on Undo, not only the `AnimationDataComponent` manager entry;
- when Redo must recreate removed sampler data, attaches the recreated entity back beneath the Animation owner; and
- extends the executable 7D test to prove parent ownership after initial record, cleanup on Undo, ownership after Redo, event-data ownership, and recursive removal when the Animation entity is deleted.

Existing imported/legacy sampler data is not reparented merely because a key is edited; the correction only creates ownership when Renegade creates or recreates the data entity.

### Finding C — cheap Video rejection must precede full decode

V3 correctly used Wicked's real H264 decoder before committing an LP08 project asset, but the existing 4 GiB creator-import ceiling was checked later inside the import workflow. An oversized MP4 could therefore enter the expensive decode path before being rejected.

The correction now performs `std::filesystem::file_size()` and applies `MaximumCreatorVideoBytes` before `wi::video::CreateVideo()`. The 7E source contract explicitly asserts that source ordering. In-range files still receive the full Wicked decode proof before any governed import, preserving the corrupt/H265 rejection guarantee.

## Systems intentionally unchanged

The audit found no repair justification for rewriting:

- 7A native animation playback/clip controls;
- 7C character/IK/expression controls outside their existing native ownership;
- 7E Hair/Force Field/Spline/Gaussian/Terrain specialist controls except the Video ownership path; or
- 7F native mesh blending.

Those systems remain part of the integrated owner smoke test because the repair branch must not regress already-merged Phase 7 surfaces.

## CI acceptance contract

The exact corrected PR head must:

- configure and compile the Windows baseline and Renegade Studio workflows;
- run the complete CTest suite including the strengthened Phase 7B, 7D and 7E contracts/tests;
- build the same Studio/Runtime artifact family used for owner acceptance; and
- show no regression in existing Phase 6/earlier suites.

The earlier four-green v3 result does not satisfy this requirement because the post-CI ownership corrections changed source and tests after that run.

If CI fails, fix the actual compile/test defect and rerun the failed integration proof. Do not weaken a test merely to make the repair green.

## Owner acceptance contract

Use the exact successful corrected PR artifact. Minimum owner pass:

1. **7B Retarget:** map a real humanoid, import+retarget a compatible animation, Undo, make the original source unavailable, Redo, play the restored clip, save/reopen and play again. In a disposable copy, verify deleting restored retarget ownership does not leave orphan clips/baked data.
2. **7D Timeline:** create a NEW CLIP in a blank scene; record keys out of chronological order; verify scrub/play is coherent; add a SOUND PLAY event; Close Loop and prove the sound fires once per authored event rather than gaining a seam duplicate; Undo/Redo; save/reopen. In a disposable Level, delete the authored clip and confirm its created data is removed with it.
3. **7E Video:** adopt an H264 MP4; exercise transport/seek/loop; Undo/Redo Loop; save/reopen after making the external original unavailable; Test Level; Build Game; run standalone from the package. H265/corrupt input must be rejected before project adoption.
4. **Regression smoke:** open the 7A–7F Inspector sections and confirm existing animation playback, character controls, Hair/Force/Spline/Gaussian/Terrain and mesh-blend surfaces still render and respond.

Any owner-visible failure keeps the repair PR open even if CI is green.

## Exit decision

Phase 7 can return to accepted/closed status only after both exact-head Windows CI and the owner contract above pass. Until then Phase 8 work must not use the repaired branch as an accepted production baseline.
