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

The Phase 3 shell uses the accepted native `wiGUI` integration with a
Renegade-owned theme and layout. `ProjectService` owns `.renegade` descriptor
validation and recent-project state. Studio calls that service rather than
parsing project files in widgets.

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

## UI foundation

ADR 0002 accepts Wicked's native `wiGUI` as Renegade's production integration
and rendering foundation after the Phase 2 HDR-source, DPI, input, persistence,
DX12, and Vulkan gate.

This does not select Wicked's stock Editor or styling. Renegade owns its
information architecture, visual language, components, docking/layout layer,
project hub, and workflows. EngineBridge remains UI-toolkit independent.

## Project metadata

ADR 0003 defines the v1 `.renegade` descriptor. It is a versioned,
project-relative identity above WISCENE. Project metadata does not replace or
alter Wicked scene serialization.
