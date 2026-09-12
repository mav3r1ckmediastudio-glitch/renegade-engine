# Phase 7 Integrated Repair — Owner Test Checklist

Use the exact successful PR artifact. Do not merge the repair PR until every required item below passes.

## 7B — humanoid retarget deterministic Undo/Redo

- Open a project containing a real humanoid character that can be mapped successfully.
- In the Phase 7 humanoid/retarget surface use AUTO-MAP or manually correct required bones until the mapping is valid.
- Use `IMPORT + RETARGET` with a compatible FBX/GLTF/GLB/VRM/VRMA/WISCENE animation source.
- Confirm one or more new native clips appear and can be played from the Phase 7 animation controls.
- Undo the retarget and confirm the created clips disappear.
- Move or rename the original animation source so Renegade can no longer open it at the original path.
- Redo. The retargeted clips must return without an import/source-file error.
- Play the restored clip and confirm motion is coherent.
- Save the Level, close/reopen it, and play the restored clip again.
- After a separate retarget/Redo check, delete a restored retargeted clip and confirm its baked animation data does not remain as orphan scene data.
- In a disposable copy of the Level, delete the retargeted humanoid/character recursively and confirm its retargeted clips disappear with it rather than remaining as detached clips.

## 7D — native timeline/key/event repair

- In a blank Level select an entity and open `TIMELINE / KEYFRAMES`.
- Use `NEW CLIP` and confirm a native clip appears without importing a model animation first.
- Record a transform key at a later time and then record another at an earlier time.
- Scrub/play and confirm the keys behave chronologically rather than in recording order.
- Add a SoundComponent target and record a `SOUND // PLAY` event.
- Record the same SOUND PLAY event at the same time again; it must not create a duplicate event.
- Use `CLOSE LOOP`. It may close value channels but must not add another SOUND event at the seam.
- Play through the loop and confirm the sound fires only for the deliberately authored event time.
- Exercise Undo/Redo for key creation/move/delete and Close Loop.
- After Undo then Redo of newly-created channels, confirm the clip still behaves normally; this specifically proves recreated AnimationData remains owned by the native Animation entity.
- Save, close/reopen and confirm the clip/channels/keys remain correct.
- In a disposable Level, delete the authored timeline clip and confirm its created channel/key data disappears with the clip rather than surviving as orphan scene data.
- Confirm SCRIPT PLAY/STOP is not presented as a working Renegade `.rscripts` timeline integration.

## 7E — governed Video project/package parity

- Add/select a native Video component and choose `ADOPT MP4` with a known-good H264 MP4.
- Confirm the video loads in Studio and Play/Pause/Stop/Seek work.
- Toggle Loop, Undo and Redo the Loop edit; the loaded governed video must remain attached throughout.
- Save the Level.
- Move or rename the original external MP4 so its original machine path is unavailable.
- Close/reopen the project/Level. The video must restore without selecting the original file again.
- Re-test Play/Pause/Stop/Seek/Loop after reopen.
- Confirm an H265/HEVC or corrupt MP4 is rejected before it is adopted into the project.
- Run Test Level and confirm the governed video is available there.
- Build the Windows game.
- Run the standalone build from its package with no access to the original external MP4 and confirm the video is available.
- Do not expect audio from the VideoComponent path; Wicked upstream video-audio playback is not claimed by this gate.

## Phase 7 regression smoke

- Phase 7A animation playback section opens and its accepted transport controls respond.
- Phase 7C IK/look-at/expression section opens and responds.
- Phase 7E Hair/Fur controls open and respond.
- Phase 7E Force Field controls open and respond.
- Phase 7E Spline controls open and respond.
- Phase 7E Gaussian Splat surface remains available.
- Phase 7E Terrain specialist controls remain available.
- Phase 7F mesh-blend material/global controls remain available.

## Acceptance

Record any owner-visible failure against the exact PR head and keep the PR open. Green CI alone is not Phase 7 acceptance.
