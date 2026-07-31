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

The Phase 3 shell uses the accepted native `wiGUI` integration as a canvas,
update scheduler, and low-level input host. It does not use Wicked's stock
widget rendering as Renegade's visual language. `RenegadeStudioChrome`
overrides `Widget::Render` and draws the Studio shell from Renegade-owned
primitives and design tokens. Interactive components will migrate onto that
visual foundation one accepted vertical slice at a time.

`ProjectService` owns `.renegade` descriptor validation and recent-project
state. Studio calls that service rather than parsing project files in widgets.

`CommandService` owns persistent Studio scene mutations. Full local transforms
use a toolkit-independent `TransformState`; duplicate and delete commands use
recursive in-memory Wicked entity archives so Undo/Redo can restore the same
entity IDs. Studio may preview a live gizmo transform, but it restores the
before-state before committing the completed command.

The generated Proving Ground is described by a renderer-independent blueprint
and instantiated as uniquely named, creator-facing scene entities. The reserved
`__renegade_internal_` prefix remains available for legacy/internal scene
helpers and `SceneService` excludes such entities from creator-facing lists.
The editor grid is not a scene helper: Studio draws it in an explicit
renderer-owned colour/depth pass, so it is never selectable or serialized.

Component authoring follows one shape, established by `SetTransformCommand` and
extended by `SetWeatherCommand`. A toolkit-independent state struct captures the
curated subset of a Wicked component that Renegade exposes; the command stores
a before and an after state; applying a state writes only the fields the struct
covers and leaves every other value on the component untouched. `WeatherState`
therefore authors sky mode, exposure, ambient, fog, and the primary cloud layer
while the second cloud layer, wind, rain, ocean, weather maps, and advanced
scattering parameters survive an edit unmodified. Light and material authoring
must follow the same pattern.

### Renegade-owned shaders

Renegade may own shaders without forking Wicked. `Studio/shaders` holds
standalone HLSL with an inline `[RootSignature]` and its own constant buffer,
deliberately free of `globals.hlsli` and `ShaderInterop` so that Wicked's shader
source tree does not have to ship beside the executable. The build copies them
to `Content/shaders/`, which the Windows packaging script already collects, and
Studio compiles them at load by pointing `wi::renderer::SetShaderSourcePath` at
that directory just long enough to resolve them. Wicked's own `Example_ImGui`
sample uses the same approach. A shader that fails to load must degrade to a
missing feature and a logged warning, never a failed startup.

### Editor preferences

`ProjectService` owns editor preferences, stored in an `editor` section of the
same `wi::config` state file that holds the recent-project registry. Preferences
describe how the creator likes Studio to behave rather than what a project
contains, so they must never reach a WISCENE or a `.renegade` descriptor. A
read-only settings location degrades to a session-only preference rather than
an error. Grid visibility is the first; camera speed and editor layout are still
session-only and should adopt this route rather than inventing a second one.

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
7. Headless bridge commands mutate scene data but do not advance the rendered
   scene. The render-capable application loop owns `Scene::Update()`.
8. Studio-owned renderer extensions must begin and end their own valid render
   passes unless they are invoked from inside a documented active Wicked pass.
   Do not assume a Wicked virtual render hook returns with attachments bound.

## UI foundation

ADR 0002 accepts Wicked's native `wiGUI` as Renegade's production integration
and rendering foundation after the Phase 2 HDR-source, DPI, input, persistence,
DX12, and Vulkan gate.

This does not select Wicked's stock Editor, widget presentation, or styling.
The accepted boundary allows Renegade widgets to inherit `wi::gui::Widget` for
canvas scheduling and input while completely overriding their rendering.
Renegade owns every visible shell primitive, information architecture, visual
language, component, docking/layout layer, project hub, and workflow.
EngineBridge remains UI-toolkit independent.

## Project metadata

ADR 0003 defines the v1 `.renegade` descriptor. It is a versioned,
project-relative identity above WISCENE. Project metadata does not replace or
alter Wicked scene serialization.
