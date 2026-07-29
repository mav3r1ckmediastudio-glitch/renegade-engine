# Architecture

## Dependency direction

```text
Wicked Engine
      |
      v
EngineBridge services
   |            |
   v            v
Studio       Runtime
   \            /
    v          v
 Projects, assets, WISCENE
```

Dependencies flow downward. `WickedEngine` must not depend on Renegade layers.
Studio panels must depend on bridge interfaces rather than reaching arbitrarily
into Wicked internals.

## Layers

### WickedEngine

Pinned upstream dependency containing rendering, ECS/scene, physics, animation,
resource management, audio, input, jobs, importers, serialization, shaders, and
the original editor used as a parity oracle.

### EngineBridge

UI-independent adapters and services that translate product workflows into
Wicked operations. Initial service boundaries:

- `ProjectService`
- `SceneService`
- `SelectionService`
- `CommandService`
- `AssetService`
- `ImportService`
- `BuildService`
- `PlaySessionService`
- `SettingsService`
- `DiagnosticsService`

### Studio

Renegade's editor shell, panels, layout, project hub, content browser, hierarchy,
inspectors, viewport tooling, preferences, and visual language.

### Runtime

Standalone player without editor code. It loads project settings, startup scene,
assets, input configuration, and scripts, then runs the packaged game.

### Tools

Import, shader, packaging, migration, feature-inventory, and validation tools.

## Core rules

1. Prefer adapters over Wicked source changes.
2. Isolate unavoidable Wicked changes in a dedicated fork commit and core-patch
   ledger before changing the submodule pointer.
3. Do not change WISCENE semantics without an ADR and migration tests.
4. Commands own editor mutations that require undo/redo.
5. Creator-facing state must survive save, close, reopen, and standalone play.
6. Editor services must remain independent of the chosen UI toolkit.

## UI decision gate

The production UI toolkit is deliberately unresolved. Phase 2 compares:

- Dear ImGui Docking plus ImGuizmo.
- A custom editor built on Wicked's native `wiGUI`.

The decision requires evidence for HDR/SDR composition, DPI, input capture,
multi-monitor behaviour, viewport render-state safety, docking/layout quality,
and packaging. See `docs/adr/0002-ui-toolkit-decision-gate.md`.
