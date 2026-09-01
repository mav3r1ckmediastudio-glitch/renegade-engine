# Phase 6 Gate 2 — Gameplay Input and Play-Session Lifecycle

## Outcome

Gate 2 promotes the temporary Gate 1 control defaults into one governed
project action-map document and gives Runtime explicit Pause/Resume and Reset
semantics without moving gameplay simulation into Studio.

The project remains authoritative. Studio authors/migrates project data,
EngineBridge owns stable input-map semantics, Runtime owns execution, and
Wicked owns raw device state and the one physics world.

## Project action map

Every project opened or created by this Gate receives:

`Content/Data/GameplayInput.renegade-input`

The version-1 document is transactionally persisted and registered as a
required `data:` Always Include dependency, so Test Level and independently
packaged Runtime consume the same project file.

The initial bindings preserve Gate 1 behaviour:

| Action | Keyboard / mouse | Gamepad |
|---|---|---|
| Move forward | W | Left stick up |
| Move backward | S | Left stick down |
| Move left | A | Left stick left |
| Move right | D | Left stick right |
| Look yaw | Mouse X | Right stick X |
| Look pitch | Mouse Y | Right stick Y |
| Jump | Space | Button 2 / primary lower face button |
| Sprint | Left Shift | Button 7 / left-stick press |
| Pause / resume | Escape | Unbound in Gate 2 |
| Reset play session | R | Unbound in Gate 2 |

Bindings are represented by stable named actions. Runtime no longer contains
the Gate 1 W/A/S/D, mouse, Space and Shift polling block; raw gameplay-device
polling is isolated in `GameplayInputService`, which produces the existing
action-shaped `PlayerInputFrame` for `PlayerService`.

The document format is deliberately bounded to known version-1 binding tokens.
Malformed, incomplete or unsupported bindings fail closed instead of silently
falling back to different controls.

## Pause / resume

Escape toggles the Runtime play session between running and paused states.
While paused:

- Runtime keeps Wicked input/window processing alive so Escape can resume;
- the active 3D path receives zero simulation delta;
- Wicked physics simulation is explicitly disabled and its previous simulation
  state is restored on resume;
- player movement is not submitted to the character controller;
- the mouse pointer is released; and
- Runtime draws a simple `PAUSED` overlay with the resume/reset shortcuts.

This is Runtime-owned pause. Studio remains a separate process and never hosts
a second simulation loop.

## Deterministic reset

R resets the active play session to the resolved startup authority used for the
current Runtime launch:

- a Test Level launch reloads its exact unsaved WISCENE snapshot;
- a scene-first packaged/project launch reloads its resolved startup Scene;
- a Story Flow launch recreates the Flow controller and re-enters the authored
  startup path; and
- a startup Screen launch recreates its Screen controller/presenter state.

The runtime-only player capsule is despawned before replacement and recreated
from the reloaded Player Start. Pending actions, pause state and transient Flow
or Screen execution state are cleared. Authored project or Scene files are not
mutated by Reset.

## Packaging boundary

`StudioProjectService` ensures the gameplay input document exists before a
project is staged for creator use and registers exactly:

`data:Content/Data/GameplayInput.renegade-input`

through the existing project Always Include transaction. The accepted LP05 /
LP06 dependency graph, LC01 identity refresh and Build Windows Game pipeline
therefore package it as normal governed project data; Gate 2 does not create a
parallel packaging path.

Packaged Runtime fails closed if this required document is missing or invalid.
Source/Test Level launches can create the default document only for migration
of a project that predates Gate 2.

## Acceptance

Automated evidence must prove:

- the default map is created under `Content/Data`;
- Gate 1 keyboard, mouse and controller bindings are preserved;
- creator-edited bindings round-trip without being replaced by defaults;
- malformed/unsupported binding tokens are rejected;
- the project migration registers the input map for the normal dependency
  closure;
- Runtime input is captured through `GameplayInputService`, not the old
  `RuntimeApplication::CapturePlayerInput` block;
- pause sends zero simulation delta and disables/restores Wicked physics;
- reset reuses the accepted Scene/Story Flow/Screen loaders rather than
  constructing a second reset model; and
- Studio Debug/Release and baseline Debug/Release remain green.

Owner acceptance should verify with keyboard/mouse:

1. existing Gate 1 movement, look, sprint, jump and collision still behave the
   same;
2. Escape pauses visibly and prevents player/world simulation;
3. Escape resumes without changing the player's authored controls;
4. R after moving/jumping returns the player and world to the exact startup
   state;
5. Save/reopen retains the project input document;
6. Build Windows Game packages and runs with the same controls, pause and reset;
   and
7. the accepted #122 editor interaction/performance baseline is unchanged.

Controller bindings remain automated/source-covered where owner hardware is
unavailable; this is recorded as an evidence limitation rather than a product
failure.

## Gate boundary

Gate 2 does not add audio, general Lua gameplay lifecycle, objectives,
navigation/AI, player arms/body, animation, weapons or combat. A richer
creator-facing Project Settings editor for arbitrary binding capture can grow
on this stable document later; Gate 2 establishes the governed map, persistence,
packaging and Runtime consumption contract first.
