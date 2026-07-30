# EngineBridge

UI-independent services and adapters around Wicked Engine. This layer protects
Studio and Runtime workflows from upstream implementation changes.

## Current services

The implementation provides:

- `SceneService`, which owns a Renegade scene, reads a WISCENE archive into a
  temporary scene with `Scene::Serialize`, validates that it contains entities,
  and only then replaces the active scene. It generates the Proving Ground from
  `ProvingGroundBlueprint()` and exposes the primary weather entity.
- `SelectionService`, which owns editor selection independently of any UI.
- `CommandService`, which owns undoable complete transforms, recursive entity
  duplication, recursive entity deletion, and curated weather edits.
- `ProjectService`, which creates and validates v1 `.renegade` descriptors,
  persists the ordered recent-project list, and stores editor preferences
  alongside it.
- `StudioSession`, which coordinates scene replacement and selection reset.

These are deliberately bounded service interfaces. Studio widgets must use
them instead of creating UI-owned project or scene state.

## Two rules that are easy to break

**No renderer-dependent calls.** `Scene::Update()` must not appear anywhere in
this layer; the render-capable Studio frame loop owns scene advancement. The
same applies transitively — `wi::scene::LoadModel` is an import path that calls
`Scene::Update()` internally, which is why `LoadScene` reads the archive
directly instead.

**Curated state, not whole components.** A command's state struct captures only
the fields Renegade exposes, and applying it must leave every other value on the
Wicked component untouched. `WeatherState` authors sky mode, exposure, ambient,
fog, and the primary cloud layer; the second cloud layer, wind, rain, ocean,
weather maps, and advanced scattering survive an edit unmodified. Follow this
shape for lights and materials.

## Generated scene entities

The generated Proving Ground is described as plain data by
`ProvingGroundBlueprint()` and instantiated separately. The split is not
stylistic: Wicked's primitive factories call
`MeshComponent::CreateRenderData()`, which requires a graphics device, so the
composition can only be asserted in headless tests through the blueprint.

Every generated entity has a unique, creator-facing name and a test enforces
it. The reserved `__renegade_internal_` prefix remains available for genuine
implementation entities; `SceneService` keeps those out of the creator-facing
hierarchy while retaining them in WISCENE serialization. The editor grid is not
one of these — it is drawn by Studio in its own render pass and never enters the
scene at all.
