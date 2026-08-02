# EngineBridge

UI-independent services and adapters around Wicked Engine. This layer protects
Studio and Runtime workflows from upstream implementation changes.

## Current services

The implementation provides:

- `SceneService`, which owns Renegade's active Wicked scene, provides the
  Runtime startup load, generates the Proving Ground from
  `ProvingGroundBlueprint()`, and exposes the primary weather entity.
- `SceneDocumentService`, which owns WISCENE Save, Save As, Open and Reopen.
  Open prepares without touching the active scene and commits scene, path,
  selection and history together at Studio's Wicked thread-safe point. Save
  writes and validates a temporary archive, protects the previous version,
  atomically replaces the destination and maintains ten rolling backups.
- `SelectionService`, which owns editor selection independently of any UI.
- `CommandService`, which owns undoable complete transforms, recursive entity
  duplication, recursive entity deletion, and curated weather edits.
- `ProjectService`, which creates and validates v1 `.renegade` descriptors,
  persists the ordered recent-project list, and stores editor preferences
  alongside it.
- `StudioSession`, which exposes the document, project and editor-state
  services to Studio without duplicating scene replacement logic.

These are deliberately bounded service interfaces. Studio widgets must use
them instead of creating UI-owned project or scene state.

## Two rules that are easy to break

**No renderer-dependent calls.** `Scene::Update()` must not appear anywhere in
this layer; the render-capable Studio frame loop owns scene advancement. The
same applies transitively — `wi::scene::LoadModel` is an import path that calls
`Scene::Update()` internally, which is why `SceneDocumentService` prepares a
native archive read directly through Wicked's `Archive` and `Scene::Serialize`.

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
