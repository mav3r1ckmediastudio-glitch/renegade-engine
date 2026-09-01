# Phase 6 — Playable-Core Capability Audit

**Baseline:** `90ebea4b9a9ec41cd7c92d86224d1484ec55d70b`

**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

**Programme outcome:** a packaged test project containing a controllable
character, collisions, audio and a scripted objective.

## Architectural decision

Phase 6 builds gameplay on the accepted Wicked/Jolt, Scene, project, Runtime
and packaging foundations. It does not create a second physics world, a second
scene format or a Studio-owned gameplay loop. Studio owns authoring; stable
EngineBridge services own product semantics; Runtime owns execution; Wicked
owns the low-level systems.

The phase proceeds as complete vertical slices. A UI control is not complete
until its state survives Save/Open, executes in Test Level and survives the
packaged standalone path where applicable.

## Current capability and gap matrix

| Area | Accepted capability on `main` | Phase 6 gap |
|---|---|---|
| Physics | JP01 exposes Wicked rigid bodies, character physics, vehicles, ragdolls, constraints, queries, debug controls and physics Lua. | No player possession or gameplay orchestration should be inferred from those low-level capabilities. |
| Player | Wicked metadata includes a generic `Player` preset and project folders include `Content/Player`. | No authoritative Player Start, runtime-spawned player, possession rule or player-camera owner exists. |
| Character control | `MovePhysicsCharacter`, ground queries and live capsule resizing wrap Wicked's real character controller. | Runtime never resolves or moves a character controller. |
| Runtime camera | Runtime owns one render-path camera and currently initializes it at a fixed spectator transform. | No first-person/third-person policy, player follow or look input exists. |
| Input | Wicked raw keyboard, mouse and gamepad input is available; Screen Runtime and Studio use it directly for their bounded UI/editor roles. | No gameplay action map, binding persistence or Runtime/Lua gameplay-input service exists. |
| Play lifecycle | Test Level launches an unsaved WISCENE snapshot in the separate Runtime process; Story Flow Preview launches the governed project; Stop terminates the supervised process. | Pause/resume and deterministic gameplay reset are not implemented. |
| Lua | Runtime binds the accepted `renegade.physics` namespace after Wicked initializes Lua. Dependency extraction recognizes declared Lua assets. | No project gameplay lifecycle, entity-script attachment or general input/objective API exists. |
| Audio | Project/resource/asset/dependency/package layers recognize Audio; Wicked supplies the low-level audio system; an LP06 packaging probe exists. | No Renegade audio authoring service, spatial-source workflow, bus/submix/reverb model or gameplay API exists. |
| Navigation | Wicked exposes voxel-grid navigation and path queries. | No Renegade service, Studio authoring or Runtime consumer exists. |
| AI/gameplay | Stable entity IDs, physics queries, Story Flow and Screen actions provide useful foundations. | No reusable actor, objective, combat or AI runtime framework exists. |
| Packaging | LP06 and Story Flow Gate 10 provide safe Build Windows Game, dependency closure and standalone Runtime. | Phase 6 assets and gameplay documents must enter those existing boundaries rather than create a new packager. |

## Bounded gate sequence

### Gate 1 — Player Start, possession and movement

Author one governed Player Start in the Scene, spawn one runtime-only Wicked
character capsule, drive it through an action-shaped input frame and make the
Runtime camera follow it. Prove Save/Open, Test Level and packaged parity.

### Gate 2 — Gameplay input and play-session lifecycle

Promote Gate 1 defaults into a project action-map document with keyboard,
mouse and controller bindings. Add pause/resume and deterministic reset without
embedding Runtime simulation inside Studio.

### Gate 3 — Spatial audio and mixing

Expose native Wicked sound sources through Renegade-owned authoring, then add
the minimum bus/submix and reverb model required by the playable slice. Prove
distance, save/reopen and packaged playback.

### Gate 4 — Lua gameplay lifecycle

Add governed project scripts and deterministic lifecycle callbacks. Expose the
minimum stable entity, input, player, audio and physics operations needed by the
slice; do not expose raw engine pointers.

### Gate 5 — Objective and interaction slice

Build one generic reusable objective loop using the accepted Lua and Screen /
Story Flow action boundaries. The proof must not hardcode Heathen-specific
content into engine services.

### Gate 6 — Navigation and actor path queries

Expose the pinned Wicked voxel-grid/path-query capability through a stable
service and prove one Runtime actor can navigate the reference Level. Broader
enemy behaviour remains a later playable-core programme.

### Gate 7 — Phase 6 integrated acceptance

Package and owner-test one project with a Player Start, controllable character,
collisions, spatial audio and scripted objective. Reopen the authored project,
run Test Level and run the independently packaged executable before acceptance.

## Explicit deferrals

- JP01 vehicle, ragdoll and broad physics exposure are not reopened without a
  concrete failing Phase 6 use case.
- Player arms, weapon sockets, animation graphs, melee/projectile/magic combat
  and production enemy AI are not silently absorbed into Gate 1.
- UDP networking remains a documented programmer-tier audit item unless the
  owner explicitly promotes it into a creator-facing gate.
- Advanced animation, specialised simulation and full export/template work
  remain governed by later master-plan phases.

