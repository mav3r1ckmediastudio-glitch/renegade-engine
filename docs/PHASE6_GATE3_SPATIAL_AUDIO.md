# Phase 6 Gate 3 — Spatial Audio and Mixing

## Outcome

Gate 3 makes Wicked's native audio system creator-facing through Renegade Studio
and proves the same authored audio survives Scene save/reopen, Test Level and
Build Windows Game.

This gate does **not** introduce a second audio engine, middleware mixer or
parallel asset/package path. Wicked remains the low-level audio owner;
EngineBridge owns stable Renegade semantics; Studio owns authoring; Runtime owns
playback lifecycle.

## Pinned Wicked capability

The Phase 6 Wicked pin already provides:

- native `wi::scene::SoundComponent` entities;
- WAV/OGG sound resources and playback instances;
- native 3D listener/emitter processing through `wi::audio::Update3D`;
- per-source looping, 2D/3D mode, volume and reverb routing;
- four serialized submix types: Sound Effect, Music, USER0 and USER1;
- global per-submix volume controls; and
- Wicked's native global reverb preset library.

Renegade names USER0 **Ambience** and USER1 **Voice**. The numeric mapping is
unchanged so native serialization remains authoritative.

## Renegade authoring contract

### Sound Source

`ADD > SOUND SOURCE` creates one command-backed native Wicked Sound entity at
the current editor view, selects it and opens:

`SOUND SOURCE // NATIVE WICKED AUDIO`

The dedicated Inspector exposes:

- audio asset;
- Preview Play / Stop;
- Play On Start;
- Loop;
- Spatial 3D / 2D;
- source Volume;
- Reverb send enabled; and
- Bus: SFX, Music, Ambience or Voice.

Transform/gizmo position is the native emitter position. Renegade does not draw
or serialize a fake Runtime speaker mesh; any editor icon/guide is editor-only.

Audio file selection must resolve to project-owned audio content so the existing
LP05/LC01/LP06 dependency and package chain remains authoritative.

### Scene Mix

The Sound Source Inspector also exposes one Scene Mix block:

- Master;
- SFX;
- Music;
- Ambience;
- Voice; and
- Reverb preset.

Default values require no extra Scene entity. The first non-default edit creates
one command-backed WISCENE metadata authority named `Audio Mix`. Undoing that
first edit removes it again. Duplicate mix authorities are not created.

Mix state is stored in native WISCENE metadata. Runtime maps it directly to
`wi::audio::SetVolume`, `SetSubmixVolume` and `SetReverb` after Scene adoption.

## Runtime contract

On every accepted Runtime Scene replacement:

1. apply the authored Scene Mix to Wicked;
2. preserve normal native SoundComponents;
3. start Renegade Sound Sources marked Play On Start;
4. update spatial sound through Wicked's normal Scene/audio update path; and
5. use the Runtime camera/player position as the listener through the existing
   Wicked Scene update.

Gate 2 Pause/Resume also pauses/resumes active Renegade sound instances without
destroying their playback position. Gate 2 Reset reloads the authored Scene,
reapplies the mix and restarts Play On Start sources from the reset state.

## Persistence and packaging

Sound Source native state and Renegade source/mix metadata live in WISCENE.
Audio resource paths therefore enter the already accepted WISCENE dependency
extractor. Gate 3 must not add a second package manifest or copy loop.

Legacy WISCENE SoundComponents remain valid. Renegade-specific Play On Start and
mix semantics apply only to entities carrying the Gate 3 metadata markers.

## Automated acceptance

- source-state sanitization and native SoundComponent round-trip;
- bus mapping remains exactly Wicked's four serialized submix values;
- source and mix metadata markers are deterministic;
- command-backed create/edit/mix Undo/Redo contract;
- Runtime scene adoption calls the Gate 3 activation boundary;
- Gate 2 Pause/Resume controls audio playback state;
- Reset reuses the accepted Scene/Story Flow/Screen loaders;
- project audio remains in existing dependency/package closure; and
- historical Gate 1/2 and WD01 contracts remain green.

## Owner acceptance

1. Add a Sound Source and choose a short project WAV/OGG.
2. Confirm Preview Play/Stop works in Studio.
3. Move the source with the gizmo and enable Play On Start + Spatial 3D.
4. Test Level and walk toward/away from and around it; direction/distance must be
   clearly spatial rather than fixed stereo playback.
5. Change source volume, loop, bus and reverb; Save/Reopen and confirm values.
6. Change Scene Mix volumes and reverb preset; Save/Reopen and confirm values.
7. Pause during playback: audio and gameplay pause together; resume continues.
8. Reset: audio returns to the authored startup state with the rest of the Level.
9. Build Windows Game and repeat spatial playback, Pause and Reset in the
   independent packaged executable.
10. Recheck the accepted WD01 editor-navigation/performance baseline.

## Deferred

- general gameplay/Lua audio calls belong to Gate 4;
- objective-specific cues belong to Gate 5;
- occlusion/obstruction simulation, DSP graphs, convolution reverb, streaming
  music systems and third-party audio middleware are outside this bounded gate;
- voice chat/network audio is not part of Phase 6.
