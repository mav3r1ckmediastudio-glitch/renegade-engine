# Renegade Engine — Current Handoff

**Date:** 2026-09-01

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Authoritative main:**
`cda9e26138f7144da9e0bc72f6d2ea1e1dc88a77`
(`Phase 6 Gate 1: Player Start and runtime possession (#123)`).

**Wicked pin:**
`3a800b7134aafe58461093c8abb2e274d4e64033`.

## Accepted baseline

The accepted production baseline on `main` includes:

- Story Flow through Gate 10 / PR #101;
- Scene UI recovery through PR #106;
- JP01 full Wicked/Jolt physics foundation through PR #107;
- Phase 5 Scene/render exposure through PR #118;
- WD01 native Wicked vegetation plus the 75 FPS/editor-interaction recovery
  through PR #122; and
- Phase 6 Gate 1 Player Start, first-person Runtime possession and Wicked/Jolt
  player movement through PR #123.

## Gate 1 owner acceptance

The project owner confirmed the Gate 1 candidate in a real test Level:

- Player Start placement/selection and its dedicated Inspector work;
- the flat direction arrow and selected capsule visualizer work;
- position, heading and controller settings persist through save/reopen;
- duplicate Player Start rejection works;
- Test Level spawns at the authored marker;
- W/A/S/D + mouse look work;
- sprint and jump work;
- the player collides with the Level through the accepted Wicked/Jolt character
  controller; and
- packaged behaviour was accepted before merge.

The owner does not own controller hardware. Controller bindings remain
source/automated covered but were not physically owner-tested; this is an
explicit evidence limitation, not a Gate 1 failure.

## Active work — Phase 6 Gate 2

Branch: `phase6/gate2-input-lifecycle`.

Gate contract: `docs/PHASE6_GATE2_INPUT_LIFECYCLE.md`.

Gate 2 is bounded to two outcomes:

1. promote Gate 1's hardcoded Runtime bindings into a persistent project action
   map; and
2. add Runtime-owned Pause/Resume and deterministic Reset without embedding
   gameplay simulation inside Studio.

### Current candidate architecture

`GameplayInputService` owns the version-1 project document:

`Content/Data/GameplayInput.renegade-input`

It contains stable named actions for movement, look, jump, sprint, pause and
reset, with the accepted Gate 1 keyboard/mouse/gamepad defaults. The document
is transactionally written and fails closed on malformed or unsupported tokens.

`StudioProjectService` migrates projects on create/open by ensuring the document
exists and registering:

`data:Content/Data/GameplayInput.renegade-input`

in the existing Always Include dependency declaration. This deliberately sends
the file through the accepted project dependency graph, LC01 identity refresh
and Build Windows Game package path rather than creating a Gate-specific
packager.

Runtime now consumes `GameplayInputService::CaptureGameplayInput()` rather than
owning the old W/A/S/D/mouse/Space/Shift polling block. `PlayerService` remains
action-shaped and still receives only `PlayerInputFrame` values.

### Play-session lifecycle

Default Gate 2 session controls are:

- `Escape` — Pause / Resume;
- `R` — deterministic Reset.

Pause keeps window/input processing alive, passes zero simulation delta to the
active Runtime path, disables Wicked physics simulation, releases the pointer
and displays a simple paused overlay. Resume restores the physics simulation
state that existed before pause.

Reset despawns the runtime-only player, clears transient session state, then
reuses the already accepted startup authority:

- Test Level reloads its resolved unsaved WISCENE snapshot;
- scene-first Runtime reloads the resolved startup Scene;
- Story Flow recreates/re-enters the startup Flow path; and
- startup Screen recreates its Screen state.

The Player Start is then resolved again and the runtime-only character capsule
is recreated from authored state.

## Gate 2 automated coverage

New Gate 2 tests cover:

- default input-map creation under `Content/Data`;
- Gate 1 keyboard/mouse/gamepad default preservation;
- persisted binding round-trip without default overwrite;
- malformed binding rejection;
- stable Pause/Reset action IDs; and
- source contracts for project migration, package dependency registration,
  Runtime input ownership, pause physics/zero-delta behaviour and reset through
  existing loaders.

## Required next evidence

1. Open the Gate 2 PR only after the implementation/docs candidate is assembled.
2. Require all four Windows checks: Studio Debug/Release and baseline
   Debug/Release.
3. Owner-test existing Gate 1 movement/look/sprint/jump/collision for regression.
4. Press Escape while moving/falling and verify the session visibly freezes;
   press Escape again and verify clean resume.
5. Move away from spawn/change dynamic world state, press R and verify the exact
   authored startup state is restored.
6. Save/reopen the project and confirm the input document remains present.
7. Build Windows Game and verify the same controls, Pause and Reset in the
   independently packaged executable.
8. Recheck the accepted #122 editor interaction/performance baseline.

Do not add audio, general gameplay Lua, objectives, navigation/AI, player arms,
animation, weapons or combat to Gate 2. Those remain later Phase 6 gates.
