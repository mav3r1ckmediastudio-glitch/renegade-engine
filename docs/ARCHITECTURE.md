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
- `SceneDocumentService`
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

The first functional slice uses `RenegadeTextInputField`, `RenegadeButton`,
`RenegadeCheckBox`, and `RenegadeComboBox`. They inherit wiGUI's proven focus,
keyboard and pointer mechanics but override rendering completely. This is the
standard migration seam: reuse low-level interaction machinery when it is
useful, while Renegade owns all visible pixels and all workflow composition.

`ProjectService` owns `.renegade` descriptor validation and recent-project
state. Studio calls that service rather than parsing project files in widgets.

`SceneDocumentService` is the UI-free scene-document operation boundary.
WISCENE preparation uses Wicked's archive and scene serialization on Wicked's
job system without mutating the active document. Studio commits the prepared
scene, document path, selection reset and command-history reset together at a
Wicked thread-safe point. `SceneService::LoadScene` is reserved for Runtime
startup and shares the same archive preparation operation; Studio must not use
it as a second editor lifecycle.

Save and Save As cross the same boundary. A save serializes the active Wicked
scene into a same-directory temporary WISCENE, deserializes it as validation,
protects the existing destination, replaces the destination atomically and
validates the final file before marking the command history saved. The previous
valid version remains as an openable `*.bak.wiscene`, while successful saves
also maintain the newest ten WISCENE files under
`Saved/Backups/Scenes/<scene-name>`. A backup warning never disguises a
successful primary save, and a primary failure never changes the document path
or clears dirty state.

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

Creator-facing entity creation follows the same command boundary. The first
implementation, `CreateLightCommand`, calls Wicked's native
`Scene::Entity_CreateLight` and snapshots the complete resulting entity through
Wicked's `Entity_Serialize`. Undo removes it and Redo restores the same entity
identity without reconstructing a parallel Renegade light. Studio owns the Add
menu, surface-raycast placement, selection, and Inspector transition; the
bridge owns native creation and history. Point, Spot, and Rectangle use a
modal click-to-place tool; Directional creates camera-relative because its
position is only an editor anchor. Directional discoverability is a
screen-space Studio icon rendered and hit-tested from the native transform.
It is not a scene entity, component, mesh, serialized light visualizer, or
Runtime feature.

Dedicated state services extend that curated model without widening
`WeatherState`: `PrecipitationState`, `SunState` and `OceanState` own their
respective native fields and commands. `OceanState` is intentionally complete
for the pinned `OceanParameters`, including FFT resolution and spectral wind.
`ApplyOcean` invalidates Wicked's lazy FFT runtime only when an authored change
requires recreation, while always synchronizing the serialized primary Weather
component into `Scene::weather`.

Terrain sculpting treats Wicked's streamed chunks as views into one canonical
integer height grid. Neighbouring 67x67 chunk meshes deliberately duplicate
their shared edge and corner vertices; a sculpt edit is therefore calculated
once per global grid coordinate, copied to every duplicate, and followed by a
cross-chunk normal/tangent rebuild. Smooth samples the same global grid. The
native per-chunk 16-bit height data remains the serialized authority and a
whole multi-chunk stroke remains one `SculptTerrainCommand`.

Until material importing exists, new terrain uses a bundled grass PBR default.
Source AO and roughness TGAs are packed at build time into Wicked's surface-map
layout (R=AO, G=roughness, B=metalness, A=reflectance), while base colour and
normal are pre-tiled consistently for the fixed native chunk UVs. Studio and
Runtime ship the generated maps beside their executables. Scene load rebinds
only filenames identifying this bundled default, so later creator-assigned
terrain materials are never overwritten.

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

The accepted Studio visual proof is the workspace UX authority. The supplied
Renegade brand guideline is narrower: it governs only the official mark,
wordmark lockup and logo type treatment. Its palette, layouts and general
typography do not override the accepted editor direction. The official lockup
is packaged as a Studio-owned UI asset rather than reconstructed with Wicked
widgets.

Owned continuous controls use a preview/commit transaction: capture component
state at drag start, apply temporary direct preview while moving, restore the
captured state on release, then execute one before/after command. This keeps
the viewport responsive without turning every rendered frame into an Undo
entry. Workspace splitters are shell state, persisted through ProjectService
editor preferences rather than scene serialization.

The Environment workspace resolves `SceneService::WeatherEntity()` directly;
the serialized Weather entity is deliberately omitted from hierarchy rows.
Native precipitation is isolated behind `PrecipitationService`: rain maps to
Wicked's GPU rain emitter and snow is a distinct Renegade-authored visual
profile over that emitter. This preserves an upgrade path to a snow-specific
renderer without coupling Studio widgets to Wicked's raw rain fields.

Sun authoring is isolated behind `SunService`. One command updates the
serialized Weather sun direction and the primary directional-light transform
together, keeping atmosphere, illumination, shadows and Undo/Redo in lockstep.
Its deterministic editor clock is also the contract for the future Lua runtime
day/night system.

## Project metadata

ADR 0003 defines the v1 `.renegade` descriptor. It is a versioned,
project-relative identity above WISCENE. Project metadata does not replace or
alter Wicked scene serialization.
