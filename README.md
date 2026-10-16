# Renegade Engine

[![Ask DeepWiki](https://img.shields.io/badge/Ask-DeepWiki-blue)](https://deepwiki.com/mav3r1ckmediastudio-glitch/renegade-engine)

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
- Selected Player Start camera inset previews the paused world and equipped arms.
- Versioned project action maps with governed gameplay input plus Pause/Resume
  and deterministic Reset lifecycle behaviour.
- Native Wicked audio authoring for global/2D and movable positional 3D sources,
  preview playback, buses/mixing/reverb and Runtime Pause/Reset integration.
- Audio-specific trigger zones remain intentionally deferred; a future shared
  ZoneService should serve audio, objectives, weather and other gameplay systems
  rather than creating separate incompatible zone implementations.

### Player Arms & Combat — active programme

The next Alpha Playability programme extends the existing Player rather than
replacing it. A camera-mounted first-person View Rig provides first-class
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
PR #178 is merged into main and owner-accepted on 5 October 2026. The supplied
shotgun setup provides 14 paired actions, movement/aim/equip/jump presentation,
discrete two-shot firing, dry fire and partial/full reload. Native assembly editing,
player prefab save/assignment/reset, local overrides and selectable always-visible
player capsules are integrated. Named prefabs can be dragged from `Content/Player`;
new levels remain player-free until the creator places a start.

The owner verified the shotgun in the existing v2 project. The P2 branch now
adds equipment/loadout assets, staged actions, independent sword/shield layers,
directional charge/chaining, native blending, collision-aware pose correction,
and saved hand assembly editing with weapon mesh replacement. Imported parts
have explicit roles and general Content destinations; stable-ID-preserving model
moves and optional tags are supported. Player Start includes solid camera/facing
guides and a frozen equipped camera inset.

On 6 October 2026 the owner confirmed movement and Build Game behave as expected.
PR #180 remains subject to Windows CI and independent exact-head review before
merge. Authoring UX and complete dependency-aware cross-project transfer remain
follow-ups. Reserve ammunition, hits/damage, recoil and wider Alpha combat scope
remain open; pose correction does not establish weapon hit detection.
See [P2 implementation](docs/P2_EQUIPMENT_ACTION_IMPLEMENTATION.md),
[player authoring UI/UX follow-up](docs/PLAYER_AUTHORING_UX_FOLLOWUP.md) and
[P3 continuation handoff](docs/P3_CONTINUATION_HANDOFF.md).

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


### Runtime fire and reload animation controls

Left mouse triggers the assigned Attack pair once; R triggers the assigned Reload
pair once. F8 resets the play session. Reload wins simultaneous presses; an active
action completes before another action can start, then idle/walk/run resumes.
Pause freezes the paired clock. Missing action assignments remain harmless.
These controls currently drive animation only; ammunition, damage, sound and
recoil gameplay remain later combat work. Legacy version-1 input maps gain fire
and reload defaults; the old default R reset moves to F8 while custom bindings
remain authored. Full-library Runtime must be rebuilt alongside the bridge.


### Two-shell shotgun Runtime prototype

The current paired shotgun starts with two loaded shells. Each accepted fire
press consumes one shell; empty fire presses do not play Attack. R after one
shot selects ReloadPartial, and R after two selects Reload. Reload at capacity
is ignored. The shell count returns to two only when the paired reload finishes,
including its weapon track. Busy actions and pause do not consume/refill shells.
An absent partial pair uses an explicitly assigned full reload if available.
Reset/reinitialization restores two shells. This is a bounded two-barrel prototype;
creator-configurable weapon definitions, reserve ammunition, HUD, damage and
reload interruption remain later work.


### Right-mouse aiming

Hold right mouse to play the assigned AimIn pair once and hold its final sight
pose; release plays AimOut then returns to movement. Left mouse while aimed uses
AimAttack and consumes the same two-shell ammunition. Reload lowers the sights,
uses partial/full reload as appropriate, and resumes aim-in if right mouse is
still held. Transitions finish before queued hold/release changes are reconciled;
pause freezes them. This uses authored native animation only, with no zoom/FOV
change or new camera/controller.


### Equipment and jump animation routing

Q toggles holster/equip using the assigned Unequip/Equip pairs. Holstered arms
hold the final out-of-view pose; fire, aim and reload are blocked, with shells
preserved. Existing Space jump physics drives grounded-to-airborne JumpStart,
airborne JumpLoop and grounded-contact JumpLand. Busy actions finish before
pending jump transitions. Jump animation ownership blocks fire/aim/reload until
landing; movement physics remains authoritative. Pause freezes action clocks.
Initial airborne spawn does not pretend a jump occurred. A collidable floor is
required to exercise takeoff and landing; the empty preview fixture is not a
complete gameplay level. No new controller or root-motion locomotion is added.

### Editable firearm settings

ASSEMBLY > WEAPON SETTINGS exposes loaded capacity, minimum seconds between shots
and permission for partial reload. Assembly draft Undo/Redo and governed SAVE
CHANGES / SAVE AS NEW persist these fields in the version-1 recipe and native
payload. Player Start's existing assembly reference carries settings through
Test Level snapshots and package loading. Old recipes retain the accepted two-shot
defaults. Runtime reload fills configured capacity only at animation completion;
shot cooldown advances only during gameplay. These are discrete shots, with no
reserve ammunition or automatic firing yet. Gameplay capacity does not change the
number of shells visible in authored clips. Independent equipment definitions
remain a next stage; player prefabs are implemented.
See [player authoring continuation](docs/PLAYER_AUTHORING_CONTINUATION.md).

### Reusable player prefabs

Player Start's native Inspector can save controller/camera defaults and the
assigned arms assembly as a project player prefab. Select that prefab on another
level's start capsule to reuse it while retaining the level's spawn position and
facing. Local edits are marked as overrides; RESET TO PREFAB restores the assigned
defaults with Undo/Redo. Prefab saves create new assets; global propagation and
starting inventories remain later work. See PLAYER_AUTHORING_CONTINUATION.md.


### Player placement from Content/Player
New levels have no automatic Player Start. Project browsing ensures a registered Basic
Player Start preset under Content/Player without creating a scene entity. Saved player
prefabs appear by authored name in the Asset Browser and support drag-and-drop surface
placement (ground-plane fallback) and the existing Place control. One command creates
the governed start and assigns resolved prefab defaults; Undo/Redo and WISCENE preserve
identity, transform and baseline. The always-visible selectable capsule represents the
placed player. Add no longer exposes Player Start. A second placement is refused: use
the existing Inspector to change prefab, or delete the old start before placing another.
Immutable saves refresh the browser. PR #178 functionality is owner-accepted; the UI/UX
follow-up remains open.


### Authored held equipment actions

Equipment definitions can make PrimaryUse wait until left mouse (or the authored
Fire binding) is released. C cancels eligible actions before activation; the
cancel_equipment input binding is remappable. Immediate shotgun definitions still
fire on a press. Held primary/cancellation are implemented; separate Charge/Release
presentation, charge strength and independent off-hand presentation remain pending.


### Charge and Release pairs

ASSEMBLY > More actions includes Charge and Release arms/weapon mappings.
Leave Attack unassigned when using the primary Charge/Release path, then create
equipment from that saved assembly. Runtime holds the Charge pose until Fire is
released, plays Release once, and preserves hand ownership through recovery.
C cancels eligible charging actions. Charge strength, projectile effects and
independent off-hand presentation remain later work.

### Independent hand assembly authoring

ASSEMBLY > HAND / MELEE SETUP exposes sword/shield mesh attachments, explicit hand roots,
directional Charge/Hold/Release groups, block clips, attack variants, scales,
charge/chaining timing and collision proxies. Existing arms bindings survive a
weapon-only swap. Draft Undo/Redo, automatic masked preview and governed save
are shared with paired firearm assemblies. See [hand assembly authoring](docs/P2_ASSEMBLY_HAND_AUTHORING.md).
Local validation is recorded in HANDOFF; owner UI acceptance and independent
verification remain open. This does not complete NPC combat or the wider P2 gate.

### Import destinations and model organisation

Native model import now exposes a general Content destination, optional tags and
explicit player arms/weapon roles independent of folder. Registered model MOVE
preserves stable IDs and source provenance through journaled rollback/recovery.
See [import destinations](docs/MODEL_IMPORT_DESTINATIONS.md).
Owner UX and independent exact-commit verification remain pending.
