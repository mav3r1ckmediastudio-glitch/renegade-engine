# Renegade Engine Roadmap

**Current authoritative main:**
`861c4d9b0f8acbb57f49db0b84b004d925b51136`
(`Phase 6 Gate 2: gameplay input and play-session lifecycle (#124)`).

**Wicked pin:**
`3a800b7134aafe58461093c8abb2e274d4e64033`

**Current state:** Phase 6 is active. Gates 1-2 are merged and owner-accepted.
Gate 3 is a draft repair candidate on `phase6/gate3-spatial-audio`. The latest
owner-tested head passed all four Windows jobs and proved global audio, Preview
Play/Stop and ordinary positional 3D playback in Test Level. Its audio-specific
zone experiment had no working viewport placement and is being removed rather
than represented as delivered.

## Accepted production baseline

The following programmes are complete on `main`:

| Programme | Accepted result |
|---|---|
| Story Flow Gates 1-10 | Project-home Journey/Graph authoring, Screen lifecycle, Runtime traversal, Build Game and standalone parity through PR #101. |
| Scene UI recovery | Shell/workspace isolation, Hierarchy and Inspector, Asset Browser placement, Environment/Terrain and consolidated whole-editor acceptance through PRs #102-#106. |
| JP01 physics foundation | Wicked/Jolt physics authoring, Physics Lab, serialization, Runtime and packaged parity through PR #107. |
| Phase 5 Gates 1-9 | Scene components, cameras, decals/probes, materials/shaders, post-processing, AO/GI/reflections, ray/path tracing, lightmap/baking and render diagnostics through PRs #109-#118. |
| WD01 vegetation and frame-loop recovery | Native Wicked grass painting across Terrain chunks, editor interaction recovery and the 75 Hz performance baseline through PR #122. |
| Phase 6 Gate 1 | One governed Player Start, first-person Runtime possession, Wicked/Jolt character capsule, movement/look/sprint/jump, save/reopen and packaged parity through PR #123. |

The original Wicked Editor remains the parity oracle. Accepted Renegade
features continue to use Renegade-owned UI and stable EngineBridge boundaries;
they do not embed Wicked's stock Editor windows.

## Phase 6 / Heathen playable core

The Phase 6 exit gate from [`MASTER_PLAN.md`](MASTER_PLAN.md) is to author and
package a small interactive game with a controllable character, collisions,
audio and a scripted objective.

The capability audit and bounded sequence are recorded in
[`PHASE6_CAPABILITY_AUDIT.md`](PHASE6_CAPABILITY_AUDIT.md).

### Gate 1 — accepted

PR #123 established the player foundation:

- `ADD > PLAYER START` creates one command-backed WISCENE marker;
- the flat editor arrow and capsule guide expose spawn heading/body scale without
  becoming Runtime render objects;
- the dedicated Inspector persists capsule, eye-height and movement/controller
  settings in native Metadata;
- Runtime creates one non-serialized Wicked/Jolt character capsule and owns the
  first-person camera;
- W/A/S/D + mouse, sprint and jump were owner-tested in a real Level;
- duplicate markers are rejected and zero-marker legacy Levels retain spectator
  Runtime; and
- CI passed after the stale Phase 5 ADD-menu source contract was corrected.

Owner hardware did not include a game controller. Controller support therefore
remains automated/source-covered rather than physically owner-tested; it was not
a Gate 1 blocker.

### Gate 2 — accepted

Branch: `phase6/gate2-input-lifecycle`.

Contract: [`PHASE6_GATE2_INPUT_LIFECYCLE.md`](PHASE6_GATE2_INPUT_LIFECYCLE.md).

Gate 2 promotes Gate 1's temporary Runtime bindings into a governed project
action-map document and adds Runtime-owned Pause/Resume and deterministic Reset.
The candidate uses:

- `Content/Data/GameplayInput.renegade-input` as the versioned project input map;
- stable named actions for movement, look, jump, sprint, pause and reset;
- `GameplayInputService` as the sole raw gameplay-device polling boundary;
- the existing project Always Include / LP05 / LC01 / LP06 chain for packaged
  input-map parity;
- Escape for Pause/Resume, including zero simulation delta and Wicked physics
  suspension/restoration; and
- R for deterministic reset through the already accepted Scene, Story Flow and
  Screen loaders.

Gate 2 does not embed gameplay simulation inside Studio and does not reopen the
JP01 physics architecture.

### Gate 3 — active repair

PR #125 exposes native Wicked sound sources, scene submixes/reverb and Runtime
audio lifecycle. The repaired core uses an independent top-level Audio surface,
a transient 2D Studio audition instance, validated audio containers, one native
instance creation path, explicit global 2D and movable 3D source creation, and
Runtime Scene activation plus Pause/Reset. Audio-specific zones are removed;
one reusable ZoneService for audio, weather, objectives and later systems is a
separate future slice. Exact-head owner acceptance and packaged proof remain
required.

### Remaining bounded sequence

3. **Spatial audio and mixing** — native Wicked sound-source authoring plus the
   minimum bus/submix and reverb model for the playable slice.
4. **Lua gameplay lifecycle** — governed project scripts and stable lifecycle /
   player/input/audio/physics operations.
5. **Objective and interaction slice** — one reusable scripted objective loop
   using the accepted Screen/Story Flow action boundaries.
6. **Navigation and actor path queries** — stable Wicked voxel/path-query
   service and one Runtime actor navigation proof.
7. **Integrated acceptance** — reopen, Test Level and independently packaged
   playable-core acceptance.

## Verification policy

- Green compilation is necessary but never sufficient for creator-facing work.
- Visual or behavioural owner failure overrides nominal automated success.
- Each implementation PR runs Studio Debug/Release and baseline Debug/Release.
- Save/reopen and packaged standalone behaviour are required wherever authored
  or gameplay-facing state is involved.
- Controller physical-device evidence is explicitly marked unavailable when no
  owner hardware exists; automated coverage remains required.

## Deferred boundaries

- Shared viewport-placeable trigger/volume zones remain deferred until one
  ZoneService can serve audio, weather, objectives and later systems.
- Renegade still has no user-facing VSync control; the accepted 75 FPS result is
  the current capped baseline, not proof of uncapped maximum throughput.
- Hardware-specific ray/path-tracing, HDR and light-baking limitations remain
  recorded in their Phase 5 gate documents.
- Player arms, body mesh, weapon sockets, animation graphs, combat and production
  enemy AI remain outside Gates 1-2.
- Advanced animation, specialised simulation and full export/template work
  remain governed by later master-plan phases unless the playable-core audit
  identifies a strictly necessary vertical-slice dependency.

Historical gate contracts and evidence remain under `docs/`; this file records
the current programme rather than reproducing every completed gate narrative.
