# Renegade Engine

Renegade is a Windows-first game engine and creator environment built on
[Wicked Engine](https://github.com/turanszkij/WickedEngine). Wicked supplies the
renderer, ECS, Jolt physics integration and other low-level engine systems;
Renegade owns its Studio editor, project and asset workflows, Runtime/player,
build lifecycle, creator UX, diagnostics and higher-level gameplay framework.

> **Status: active development — Phase 6 / Playable Core, entering Alpha
> Playability.** Renegade has moved beyond scene-authoring foundations into an
> integrated gameplay stack with player control, physics, audio, governed Lua
> scripting, a rebuilt native asset and Character importer, Character Prefabs,
> animation assignment/crossfading, voxel/path-query navigation and profile-driven
> NPC behaviour. The current programme is the Player Arms & Combat Framework:
> turning the accepted Player and Character systems into a complete small-game
> combat loop with first-person arms, equipment, firearms, directional melee,
> bows, crossbows, magic, HUD state and packaged Runtime parity. Renegade is not
> yet a distribution-ready v1 engine.

## What Renegade already does

### Studio and project authoring

- Custom Renegade Studio chrome and workspaces over Wicked subsystems rather
  than embedded stock Wicked Editor windows.
- Project Hub, Story Flow/Journey authoring, Scene hierarchy, selection,
  transform gizmos, Inspector workflows and command-backed Undo/Redo.
- Asset Browser placement and creator-owned project/scene lifecycle.
- Environment, Terrain, Render, Physics, Audio, Character and Diagnostics
  authoring surfaces.
- Stable Inspector section/provider architecture used by both existing and new
  creator-facing systems.
- Test Level launches the real Renegade Runtime while Studio yields 3D ownership,
  avoiding a second competing simulation/render world.

### World and rendering

- Finite one-metre Terrain creation, non-destructive ring expansion and
  cross-chunk sculpting.
- Native Wicked grass/vegetation painting across Terrain chunks with normal
  viewport navigation preserved while the brush is armed.
- Environment, realistic sky, sun/time-of-day, precipitation and native FFT
  ocean foundations.
- Lights, materials, decals, probes, post-processing, AO/GI/reflections,
  ray/path-tracing exposure, lightmap/baking workflows and render diagnostics.
- Packaged Studio/Runtime parity checks for accepted world and rendering paths.

### Assets, importer and standalone builds

- Rebuilt native importer workflow rather than the former painted/non-interactive
  importer prototype.
- Static GLB/GLTF and FBX import with native preview, rotation controls, retained
  source provenance, texture handling and save/reopen verification.
- Character import workflow with native preview, Character creation and reusable
  Character Prefabs.
- External animation clip import and per-character animation assignment for
  semantic categories such as Locomotion, Idle, Attack, Hit and Death.
- Stable asset identity, source provenance and deterministic moved/missing
  source recovery.
- Deterministic dependency extraction and standalone Windows build staging,
  validation, rollback/promotion and isolated Runtime launch.

See [MODEL_IMPORTER_REBUILD](docs/MODEL_IMPORTER_REBUILD.md) and
[CHARACTER_WORKFLOW_PROGRESS](docs/CHARACTER_WORKFLOW_PROGRESS.md).

### Character animation and AI

Renegade now has a functioning Character gameplay pipeline rather than treating
animated models as generic scene assets.

- Native animation playback with crossfades between assigned character clips,
  including differing bone-track coverage.
- Continuous outgoing action tails during attack transitions to avoid the
  stop/start appearance of hard clip switching.
- Idle base-loop selection plus occasional idle variation playback.
- Character profiles and creator-facing AI settings.
- Normal roles including Idle, Guard, Patrol and Wander.
- Wander uses the existing Wicked CharacterComponent/PathQuery movement pipeline,
  with a configurable terrain-chunk extent around the spawn position.
- Hostile perception, Chase/Attack behaviour, search memory, death handling and
  return to the configured normal role.
- Authored pursuit radius/leash behaviour so an NPC can disengage and return to
  Guard, Patrol, Idle or Wander rather than pursuing indefinitely.
- Runtime diagnostics expose decision intent, navigation goals, pursuit state and
  Wander state for debugging.

Current character movement is built on Wicked navigation/path-query facilities.
The system is intentionally still evolving: gait phase synchronisation, animation
layers, additive animation, blend spaces, authored contact markers and
production-scale crowd behaviour remain future work.

See
[RENEGADE_CHARACTER_WORKFLOW_IMPLEMENTATION_SPEC](docs/RENEGADE_CHARACTER_WORKFLOW_IMPLEMENTATION_SPEC.md),
[RENEGADE_CHARACTER_AI_SYSTEM_IMPLEMENTATION_AUTHORITY](docs/RENEGADE_CHARACTER_AI_SYSTEM_IMPLEMENTATION_AUTHORITY.md)
and [AI_IMPLEMENTATION_HANDOFF](docs/AI_IMPLEMENTATION_HANDOFF.md).

### Physics, player, input and audio

- Renegade-owned authoring over Wicked/Jolt physics, including the JP01 physics
  foundation and Physics Lab workflow.
- One governed Player Start, Runtime first-person possession and a Wicked/Jolt
  character capsule with movement, mouse look, sprint and jump.
- Versioned project action maps with governed gameplay input plus Pause/Resume
  and deterministic Reset lifecycle behaviour.
- Native Wicked audio authoring for global/2D and movable positional 3D sources,
  preview playback, buses/mixing/reverb and Runtime Pause/Reset integration.
- Audio-specific trigger zones remain intentionally deferred; a future shared
  ZoneService should serve audio, objectives, weather and other gameplay systems
  rather than creating separate incompatible zone implementations.

### Player Arms & Combat — active programme

The next Alpha Playability programme extends the existing Player rather than
replacing it. A camera-mounted first-person View Rig will provide first-class
primary/off-hand/two-hand presentation while world movement/collision remains
owned by the accepted Player controller.

The shared combat framework is required to support firearms, physical
projectiles, **directional melee and directional defence**, shields, bows,
crossbows and spellcasting through common equipment/action/projectile/effect
boundaries. Directional melee includes swept weapon collision, attack/guard
directions, block/parry/feint seams and surface-specific impact feedback rather
than a generic centre-camera melee ray.

Combat acceptance explicitly includes animation quality, recoil/weapon impulse,
camera response, muzzle flash, particles, SFX/mechanical audio, projectiles,
surface impacts and target reactions. A dedicated owner gameplay session is a
required gate because passing damage/state tests cannot prove that combat feels
good.

See [PLAYER_ARMS_COMBAT_FRAMEWORK](docs/PLAYER_ARMS_COMBAT_FRAMEWORK.md).
P1's rig, asset binding and movement-animation foundation is implemented on the
active P1 branch. The owner accepts the authored shotgun diagnostic grip/reload;
native Studio assembly authoring now supports retained parts, explicit attachment,
paired preview and governed save/reopen with Player Start assignment. Runtime
paired-action playback and direct owner UI acceptance remain outstanding. See
[P1 assembly authoring](docs/P1_ASSEMBLY_AUTHORING.md). The
[P1 status and recovery checkpoint](docs/P1_STATUS_AND_RECOVERY.md) distinguishes
implemented code, passing automated checks and the unresolved gameplay result.

## Governed Lua scripting

Renegade has a creator-facing scripting stack rather than relying on ad-hoc raw
Lua execution:

- **S1A/S1B — Inspector foundation:** extensible Inspector sections/providers and
  migration of existing Inspector ownership.
- **S2 — Script document/source model:** durable project .rscripts companions,
  transactional edits, source identity and validation.
- **S3 — Governed Lua Runtime:** Runtime-owned Lua lifecycle, live EntityRef
  validation and a deliberately restricted standard-library surface.
- **S4A-D — Creator authoring:** restricted metadata evaluation, ACTION/SCRIPT
  attachments, typed generated properties, governed entity references and
  GLOBAL SCRIPT authoring through normal Undo/Redo and dirty-state ownership.
- **S5A-C — Gameplay APIs and events:** generation-safe entity/transform access,
  governed gameplay lifecycle and bounded cross-script event dispatch.
- **S5D — Structured diagnostics/IPC:** Studio-to-Test-Level handshake and
  structured diagnostics across the creator/runtime boundary.
- **S6 — Creator Library packages:** immutable installed package manifests,
  deterministic transitive dependency closure, transactional first-use adoption
  into project-owned Content/Scripts/Library/..., clean update handling,
  creator-edit conflict protection and structured package diagnostics.
- **S7 — Stock Actions:** installed Lua Actions for doors, switches, trigger
  zones, pickups, relays and sound playback, with generic Runtime Interact
  prompts and creator-facing Action selection.

After Library adoption the **project copy is authoritative**. Runtime does not
search or execute scripts directly from the installed Library. Test Level and
Build Game consume the same project-owned script closure through the project
snapshot and dependency-graph paths.

See
[SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE](docs/SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE.md).

## Diagnostics

Renegade has a built-in Diagnostics surface for inspecting the running editor and
Test Level/Runtime rather than relying only on build logs. The current diagnostic
stack includes structured editor/runtime state, script and audio diagnostics,
Runtime heartbeat/liveness semantics, local live diagnostic transport, the
Studio/Test Level IPC handshake and Character AI/navigation state.

See [LIVE_DIAGNOSTIC_ACCESS](docs/LIVE_DIAGNOSTIC_ACCESS.md).

## Current programme

Phase 6 exits when Renegade can author, reopen, run and independently package a
small interactive game using the same creator-facing workflows that built it.

The current bounded work is centred on:

1. **First-person Arms Rig** — primary/off-hand/two-hand presentation on top of
   the existing authoritative Player controller.
2. **Generic equipment/actions and projectiles** — shared foundations for
   firearms, melee, bows, crossbows, throwables and magic.
3. **Reference combat families** — pistol, directional sword + shield, bow,
   crossbow and spellcasting must all work without Player special cases.
4. **Combat feel and HUD loop** — health/ammo/resource state, death/restart,
   recoil, animation, SFX, particles, impacts and owner-led feel tuning.
5. **Packaged parity and performance** — preserve clean CI, Test Level/Build Game
   equivalence and investigate the open rigged-character FPS issue before
   scaling to large populations.

The detailed programme state lives in [ROADMAP](docs/ROADMAP.md).
Implementation checkpoints and owner-test evidence live in [HANDOFF](HANDOFF.md)
and the focused subsystem documents rather than in fast-aging branch notes here.

## Build baseline

- Wicked upstream: https://github.com/turanszkij/WickedEngine.git
- Pinned branch: master
- Pinned Wicked commit: 3a800b7134aafe58461093c8abb2e274d4e64033
- Primary target: Windows x64 / DirectX 12
- Development cross-check: Vulkan on Windows

Wicked Engine is included as a pinned Git submodule at /WickedEngine.

Clone with:

~~~bash
git clone --recurse-submodules \
  https://github.com/mav3r1ckmediastudio-glitch/renegade-engine.git
~~~

For an existing clone:

~~~bash
git submodule update --init --recursive
~~~

The Windows reference build and evidence workflow is documented in
[BUILD_WINDOWS](docs/BUILD_WINDOWS.md).

## Product layers

| Path | Responsibility |
|---|---|
| /WickedEngine | Pinned upstream engine foundation |
| /Studio | Renegade editor application and owned creator UX |
| /EngineBridge | Stable Renegade services/adapters around Wicked APIs |
| /Runtime | Standalone game/player executable |
| /Tools | Import, shader, packaging and validation tools |
| /Templates | Starter projects and examples |
| /Tests | Automated, integration, packaged and acceptance tests |
| /docs | Canonical architecture, roadmap, gate contracts and verification records |
| /assets | Renegade-owned editor assets |

The original Wicked Editor remains available inside the submodule as a parity
reference. It is not the Renegade editor and is not embedded as Renegade UI.

## Start here

1. Read [PROJECT_CHARTER](docs/PROJECT_CHARTER.md).
2. Read [MASTER_PLAN](docs/MASTER_PLAN.md).
3. Check [ROADMAP](docs/ROADMAP.md) for the current programme state.
4. Check [HANDOFF](HANDOFF.md) for the latest implementation checkpoint.
5. For the rebuilt importer, read
   [MODEL_IMPORTER_REBUILD](docs/MODEL_IMPORTER_REBUILD.md).
6. For Character workflows and AI, read
   [RENEGADE_CHARACTER_WORKFLOW_IMPLEMENTATION_SPEC](docs/RENEGADE_CHARACTER_WORKFLOW_IMPLEMENTATION_SPEC.md)
   and
   [RENEGADE_CHARACTER_AI_SYSTEM_IMPLEMENTATION_AUTHORITY](docs/RENEGADE_CHARACTER_AI_SYSTEM_IMPLEMENTATION_AUTHORITY.md).
7. For the active Player/combat programme, read
   [PLAYER_ARMS_COMBAT_FRAMEWORK](docs/PLAYER_ARMS_COMBAT_FRAMEWORK.md).
8. For live diagnostics, read
   [LIVE_DIAGNOSTIC_ACCESS](docs/LIVE_DIAGNOSTIC_ACCESS.md).
9. Follow [AI_WORKFLOW](docs/AI_WORKFLOW.md) for Codex, ChatGPT,
   Claude or human handovers.
10. Treat [FEATURE_MATRIX.csv](docs/FEATURE_MATRIX.csv) as the capability
   evidence ledger; compilation alone is never proof of creator-facing parity.

## Verification policy

Green compilation is necessary but not sufficient. Creator-facing work must be
behaviourally verified in the editor, save/reopen must be exercised where state
is persisted, and gameplay-facing work must be checked in Test Level and/or an
independently packaged Runtime as appropriate. Visual or behavioural owner
failure overrides nominal automated success.

## Licensing and distribution

Wicked Engine is MIT licensed and retains its original copyright and licence.
Renegade's own project-wide licence has not yet been selected. Current standalone
outputs are engineering/acceptance builds rather than commercial redistribution
clearance. See [LICENSING](docs/LICENSING.md) before redistributing any build.
