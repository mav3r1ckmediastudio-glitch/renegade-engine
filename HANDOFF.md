# Renegade Engine — Current Handoff

**Date:** 2026-09-02

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**PR:** #125 — `Phase6/gate3 spatial audio`

**Branch:** `phase6/gate3-spatial-audio`

**Main/base:** `861c4d9b0f8acbb57f49db0b84b004d925b51136`
(`Phase 6 Gate 2: gameplay input and play-session lifecycle (#124)`).

**Last owner-tested head:**
`75218f6cdec4cacdba4c380b488895bdad452f54`.

**Zone-removal implementation commit:**
`d3be47df0fd845bb65323f120f866513f3682082`
(`Defer zones and preserve Gate 3 core audio`).

**State:** DRAFT — DO NOT MERGE. Core audio has owner evidence at `75218f6`,
but the exact zone-removal head still requires Windows CI and a short owner
regression before acceptance.

## Accepted evidence before this repair

`75218f6` passed all four required Windows jobs:

- Windows baseline run 1588: Debug and Release green;
- Renegade Studio run 1002: Debug and Release green.

The owner then confirmed in Studio that global sound worked, a short clip could
be assigned and Previewed/Stopped, and positional 3D sound played correctly in
Test Level. This is behavioural evidence for the core audio path, not acceptance
of the abandoned zone path.

## Why audio-specific zones were removed

The Sound Zone controls and Runtime metadata existed, but the promised creator
workflow did not. Owner testing found no cursor ghost, no click placement and no
usable zone volume. The placement state had been implemented inside the Audio
Inspector widget rather than as a shared viewport placement/volume service, and
the authored transform scale did not govern the Runtime radius. Automated token
checks proved that code was present, not that the viewport interaction worked.

The durable product decision is to keep Gate 3 bounded to working global and
ordinary positional audio. Reusable trigger volumes will be designed once as a
shared ZoneService for audio, weather, objectives and later systems.

## Repair implemented at `d3be47d`

- Removed Sound Zone state, metadata reads/writes, Runtime entry tracking and
  per-frame zone activation.
- Removed the nonfunctional ghost/click-placement state and zone-only Inspector
  controls, radius guide and trigger/duration UI.
- Replaced the misleading zone action with `ADD 3D SOUND`, which creates a
  native spatial sound five metres in front of the editor camera, selects it and
  leaves movement to the ordinary transform gizmo.
- Preserved `ADD GLOBAL SOUND`, WAV/OGG selection, transient Preview Play/Stop,
  Play On Start, Loop, Spatial 3D, Volume, Reverb, buses, Scene Mix, source
  glyphs, Save/Reload metadata and Runtime activation.
- Changed Pause from Stop to Wicked's native `wi::audio::Pause` while clearing
  `SoundComponent::PLAYING`; Resume therefore continues from the existing audio
  cursor instead of restarting the clip.
- Added command coverage for global transform-free audio and movable positional
  3D audio, and changed the source contract to reject reintroduction of the
  deferred zone surface.
- Updated README, architecture, roadmap, feature matrix and the Gate 3 contract
  to state the actual supported boundary.

## Files changed by the implementation

```text
EngineBridge/include/renegade/bridge/AudioService.h
EngineBridge/src/AudioService.cpp
README.md
Runtime/src/RuntimeApplication.cpp
Runtime/src/RuntimeApplication.h
Studio/src/RenegadeAudioWorkspace.cpp
Studio/src/StudioApplication.cpp
Tests/Phase6Gate3AudioTests.cpp
Tests/Phase6Gate3SourceContract.cmake
docs/ARCHITECTURE.md
docs/FEATURE_MATRIX.csv
docs/PHASE6_GATE3_SPATIAL_AUDIO.md
docs/ROADMAP.md
```

## Local evidence

This Linux workspace has no CMake installation and the Wicked submodule is not
initialized, so no local Windows/XAudio build was claimed. The following checks
passed against the exact implementation diff:

```text
git diff --check
manual exact-token equivalent of Phase6Gate3SourceContract.cmake
repository-wide stale zone-symbol search
FEATURE_MATRIX.csv parsed as 38 rows with a consistent 16-column schema
```

Pinned Wicked commit `3a800b7134aafe58461093c8abb2e274d4e64033`
was inspected directly: `SoundComponent::_flags` and `PLAYING` are public,
`wi::audio::Pause(SoundInstance*)` exists, Pause preserves the source cursor,
and `SoundComponent::Play()` restores playback intent and resumes the instance.

## Required exact-head owner check

After the Windows jobs pass:

1. Open Audio and confirm there is no Sound Zone/trigger/radius/duration surface.
2. Add Global Sound, assign a short WAV/OGG, Preview Play/Stop, and Test Level.
3. Add 3D Sound and confirm it appears selected in front of the camera with a
   source glyph and transform gizmo; move it and verify spatial playback.
4. Confirm Pause/Resume continues rather than restarting, then Reset.
5. Save/Reopen and verify source asset, position, loop, bus, volume and reverb.
6. Recheck Scene, Environment, Terrain, Render, Physics and WD01 navigation.
7. Run one packaged Windows Game check for global/3D playback, Pause and Reset.

Do not spend another build cycle trying to validate zones on PR #125. Their next
implementation begins with the shared ZoneService contract and viewport owner.
