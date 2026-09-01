# Renegade Engine Roadmap

**Current authoritative main:**
`90ebea4b9a9ec41cd7c92d86224d1484ec55d70b`
(`Perf/editor frame loop recovery (#122)`).

**Wicked pin:**
`3a800b7134aafe58461093c8abb2e274d4e64033`

**Current state:** Phase 5 and the post-Phase-5 vegetation/performance recovery
are accepted and merged. There is no active Scene Editor recovery PR.

## Accepted production baseline

The following programmes are complete on `main`:

| Programme | Accepted result |
|---|---|
| Story Flow Gates 1-10 | Project-home Journey/Graph authoring, Screen lifecycle, Runtime traversal, Build Game and standalone parity through PR #101. |
| Scene UI recovery | Shell/workspace isolation, Hierarchy and Inspector, Asset Browser placement, Environment/Terrain and consolidated whole-editor acceptance through PRs #102-#106. |
| JP01 physics foundation | Wicked/Jolt physics authoring, Physics Lab, serialization, Runtime and packaged parity through PR #107. |
| Phase 5 Gates 1-9 | Scene components, cameras, decals/probes, materials/shaders, post-processing, AO/GI/reflections, ray/path tracing, lightmap/baking and render diagnostics through PRs #109-#118. |
| WD01 vegetation and frame-loop recovery | Native Wicked grass painting across Terrain chunks, editor interaction recovery and the 75 Hz performance baseline through PR #122. |

The original Wicked Editor remains the parity oracle. Accepted Renegade
features continue to use Renegade-owned UI and stable EngineBridge boundaries;
they do not embed Wicked's stock Editor windows.

## PR #122 closeout

PR #122 combined the clean WD01 vegetation implementation with the bounded
editor frame-loop and interaction recovery. Its exact owner-tested source head
was `ad97b0f631c7cb565fb3c2c209815abd073448e0`; GitHub squash-merged it as
`90ebea4b9a9ec41cd7c92d86224d1484ec55d70b`.

Accepted evidence:

- all four required Windows checks passed;
- an empty Level and a populated Level both held the 75 FPS VSync cap;
- grass Paint/Delete remained active across multiple Terrain chunks;
- top and bottom Studio chrome responded correctly;
- Hierarchy selection and collapse/expand responded correctly; and
- RMB look, RMB+WASD traversal and editor navigation remained available while
  the vegetation tool was armed.

The recovery removed governed-resource repair, persistent logging, duplicate
debug ownership and whole-terrain synchronization from ordinary frame work. It
also removed duplicate top-level Studio chrome registration and limited
vegetation input consumption to actual LMB painting/deleting or an active
stroke. These are architectural lifecycle corrections, not reduced rendering
or vegetation quality.

PRs #119, #120 and #121 were superseded by the accepted #122 tree and have
been closed. Their source branches are not product baselines.

## Next programme — Phase 6 / Heathen playable core

The next product outcome is the Phase 6 exit gate from
[`MASTER_PLAN.md`](MASTER_PLAN.md): author and package a small interactive game
with a controllable character, collisions, audio and a scripted objective.

JP01 means this programme must not create a second physics world or repeat the
accepted physics-authoring work. The first task is a bounded capability and gap
audit against the current product tree, covering:

- player start/possession, character controller and camera ownership;
- keyboard, mouse and controller action mapping;
- Play/Pause/Stop and deterministic Scene reset;
- 3D audio, submix/reverb requirements and Runtime persistence;
- Lua gameplay bindings and a simple objective lifecycle;
- navigation/path-query requirements needed by the first playable slice; and
- Studio, Test Level, save/reopen and packaged standalone parity.

That audit must produce a gate plan and acceptance matrix before implementation
starts. The first implementation PR should deliver one vertical slice rather
than expose unrelated systems in parallel.

The audit is now recorded in
[`PHASE6_CAPABILITY_AUDIT.md`](PHASE6_CAPABILITY_AUDIT.md). **Gate 1 — Player
Start, Possession and Movement** is active on
`phase6/gate1-player-foundation`. Its bounded contract is
[`PHASE6_GATE1_PLAYER_FOUNDATION.md`](PHASE6_GATE1_PLAYER_FOUNDATION.md).
Gate 1 adds one command-backed WISCENE Player Start, an action-shaped gameplay
input seam, one Runtime-only Wicked character capsule and explicit first-person
camera ownership. Audio, gameplay Lua, objectives and navigation remain later
gates rather than widening the first candidate.

## Verification policy

- Green compilation is necessary but never sufficient for creator-facing work.
- Visual or behavioural owner failure overrides nominal automated success.
- Each implementation PR runs Studio Debug/Release and baseline Debug/Release.
- Save/reopen and packaged standalone behaviour are required wherever authored
  or gameplay-facing state is involved.
- Documentation-only closeout changes following #122 are carried into the next
  implementation PR so they do not consume a separate four-build CI cycle.

## Deferred boundaries

- Renegade still has no user-facing VSync control; the accepted 75 FPS result is
  the current capped baseline, not proof of uncapped maximum throughput.
- Hardware-specific ray/path-tracing, HDR and light-baking limitations remain
  recorded in their Phase 5 gate documents.
- Animation, advanced Terrain/simulation exposure and broader scripting/export
  remain later master-plan phases unless the playable-core audit identifies a
  strictly necessary vertical-slice dependency.

Historical gate contracts and evidence remain under `docs/`; this file records
the current programme rather than reproducing every completed gate narrative.
