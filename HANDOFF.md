# Renegade Engine — Current Handoff

**Date:** 2026-09-02

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**PR:** #125 — `Phase6/gate3 spatial audio`

**Branch:** `phase6/gate3-spatial-audio`

**Main/base:** `861c4d9b0f8acbb57f49db0b84b004d925b51136`
(`Phase 6 Gate 2: gameplay input and play-session lifecycle (#124)`).

**Rejected owner-tested head:**
`40282b781b757b5c02262d66fed53f044d30b363`.

**Repair implementation commit:**
`318b188` (`Repair Gate 3 audio ownership and runtime lifecycle`).

**State:** DRAFT — DO NOT MERGE. The repair is locally source-validated but has
not yet run Windows CI or owner Studio/package acceptance.

## Why the CI-green candidate failed

The earlier head passed Studio run #992 and Windows baseline #1578, but owner
testing found three hard failures: Terrain/Environment Inspector corruption,
silent Preview and a desktop crash with a longer WAV. Green CI did not override
those behavioural failures.

The repair audit established:

1. `SyncAudioInspectorPresentation()` called
   `inspectorPanel_.SetVisible(false/true)`. Pinned Wicked
   `Window::SetVisible(true)` explicitly sets every child visible, destroying
   the Inspector's per-section state and producing the overlapping controls.
2. Preview played the authored `SoundComponent::soundinstance` without setting
   authored `PLAYING`. Wicked's next Scene update therefore stopped it.
3. Pinned Wicked copies a WAV's declared `fmt` chunk into a fixed
   `WAVEFORMATEX` without bounding the copy. Extended WAV headers can corrupt
   memory. Gate 3 also created/recreated the same instance multiple times.
4. Runtime audio was only declared in `RuntimeApplication.h`; no `.cpp`
   activation, Scene-transition, Pause/Resume or Reset wiring existed.
5. Native audio pause cannot merely call `wi::audio::Pause` while leaving the
   component `PLAYING` flag set: Wicked's Scene update calls `Play` again every
   frame, including zero-delta paused frames.

## Repair implemented at `318b188`

- Removed `SyncAudioInspectorPresentation()` and its derived frame-loop hook.
- Promoted `RenegadeAudioWorkspace` to an independent top-level Wicked GUI
  widget registered above the accepted Inspector. Audio never toggles the
  Inspector parent Window.
- Propagated Audio surface priority into Wicked input scheduling so controls
  underneath it cannot react through the overlay.
- Prevented automatic Sound-source Inspector routing from reopening Audio over
  Environment/Terrain/Render workspace transitions.
- Added `ValidateAudioAssetForWicked()`:
  - validates RIFF/WAVE structure and chunk bounds;
  - accepts standard PCM WAV without an arbitrary duration cap;
  - rejects extended/malformed/over-limit WAV before the unsafe pinned loader;
  - validates OGG signature and the pinned decoder's signed-size boundary.
- Changed source creation to start with an inert native Sound entity, then load
  and create exactly one validated replacement instance.
- Made instance mutation transactional: the existing source is changed only
  after the replacement resource and instance succeed.
- Added a dedicated transient Studio-only 2D Preview instance. It never mutates
  authored Spatial 3D, Loop or Play On Start and stops on asset, selection,
  Scene, project, workspace and Studio lifetime changes.
- Implemented Runtime Scene-revision activation, authored Scene Mix application,
  Play On Start, Scene-transition replay, Pause/Resume and Reset wiring.
- Added transient `SceneAudioPauseState`; pause clears native component
  `PLAYING` intent and resume restores only the sources that had been playing.
- Strengthened unit/source contracts for standard-vs-extended WAV safety,
  independent Audio ownership, transient Preview and real Runtime wiring.
- Updated README, architecture, roadmap, feature matrix and Gate 3 contract.

## Files changed by the repair

```text
EngineBridge/include/renegade/bridge/AudioService.h
EngineBridge/src/AudioService.cpp
README.md
Runtime/src/RuntimeApplication.cpp
Runtime/src/RuntimeApplication.h
Studio/src/RenegadeAudioWorkspace.cpp
Studio/src/RenegadeAudioWorkspace.h
Studio/src/RenegadePhysicsLabStudioChrome.cpp
Studio/src/RenegadePhysicsLabStudioChrome.h
Studio/src/RenegadeStudioChrome.h
Studio/src/StudioApplication.cpp
Studio/src/StudioApplication.h
Tests/Phase6Gate3AudioTests.cpp
Tests/Phase6Gate3SourceContract.cmake
docs/ARCHITECTURE.md
docs/FEATURE_MATRIX.csv
docs/PHASE6_GATE3_SPATIAL_AUDIO.md
docs/ROADMAP.md
```

## Local evidence

The Linux host has no CMake installation and cannot execute the Windows/XAudio
backend, so Windows CI remains authoritative. The following checks passed:

```text
g++ -std=c++17 -fsyntax-only [Wicked/bridge includes] \
  EngineBridge/src/AudioService.cpp Tests/Phase6Gate3AudioTests.cpp

g++ -std=c++17 -fsyntax-only [Wicked/bridge/Studio includes] \
  Studio/src/RenegadeAudioWorkspace.cpp \
  Studio/src/RenegadePhysicsLabStudioChrome.cpp

g++ -std=c++17 -fsyntax-only [complete Studio/Runtime includes] \
  Studio/src/StudioApplication.cpp Runtime/src/RuntimeApplication.cpp

git diff --check
manual exact-token equivalent of Phase6Gate3SourceContract.cmake
```

All syntax checks passed. The complete Studio check emitted only the existing
unrelated `TestLevelSnapshotService::Cleanup` ignored-result warning.

## Residual risk requiring the next exact-head build

The exact owner WAV was not available in this workspace, so the desktop crash
could not be reproduced under the Windows debugger. The pinned source provides
a concrete corruption path for extended WAV headers, now blocked before load.

Pinned Wicked also assigns `SoundInstance::internal_state` before XAudio voice
creation has succeeded, while its Windows instance destructor assumes the voice
pointer is non-null. The new validation prevents known bad container/size paths
from reaching that failure and the bridge avoids redundant creation. If the
owner's exact file still makes `CreateSoundInstance` return false after passing
validation, stop and approve a documented Wicked core patch; do not hide it
with a file-size rule or add another audio backend.

## Next action

1. Run the normal four Windows checks once for the new exact PR head.
2. If green, owner-test one Studio build:
   - Scene, Environment, Terrain, Render, Physics and WD01 remain normal;
   - Audio opens/closes without changing those workspaces;
   - short standard PCM WAV, the original longer WAV and OGG do not crash;
   - Preview Play is audible and independent of emitter distance;
   - Preview Stop and selection/Audio-close cleanup work;
   - authored 3D distance/direction, Loop, Volume, buses, mix and reverb work;
   - Save/Reopen preserves authored values;
   - Escape pauses and resumes sound; R restarts authored startup sound.
3. Build Windows Game once and repeat authored spatial playback, Pause and Reset.
4. Do not merge until those owner checks pass. One Studio build and one packaged
   build are sufficient unless a specific remaining defect needs a targeted fix.

## Deferred non-blocker

`ADD SOUND SOURCE...` still inherits a selected transform or origin rather than
placing at the current editor view. Correct placement should reuse an existing
Renegade camera/raycast placement boundary after the regression/crash repair is
accepted; it must not be used to delay the blocker validation above.
