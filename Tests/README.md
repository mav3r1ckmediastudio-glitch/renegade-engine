# Tests

`RenegadeBridgeTests` is the automated suite: a single executable run by CTest
in both Debug and Release from `.github/workflows/studio.yml`. See
`docs/TEST_STRATEGY.md` for the full strategy.

## It runs without a graphics device

This constraint shapes everything here. `wi::graphics::GetDevice()` returns
null, so anything that dereferences it crashes the test process. In practice
that rules out:

- Wicked's primitive factories — `Entity_CreateCube`, `Entity_CreateSphere`,
  `Entity_CreatePlane`, `Entity_CreateMeshFromData` — because they all call
  `MeshComponent::CreateRenderData()`.
- `Scene::Update()`.
- Reloading any archive containing meshes or materials, because their
  deserializers call `CreateRenderData()` too.

An earlier CI failure came from ignoring this: the suite called
`SceneService::CreateProvingGround()` directly and the executable crashed.

Two patterns work around it. Scene *composition* is asserted through the
renderer-independent `ProvingGroundBlueprint()` rather than by instantiating
the scene. Save and reload round trips use a mesh-free fixture, leaving full
generated-scene reload to packaged-Release acceptance. Component authoring
needs no device, so transform, weather, and future light and material commands
are fully covered here.

## What is covered

- Hierarchy listing, parent/child ordering, and internal-entity filtering.
- Selection state.
- Complete transform commands with Undo/Redo, repeated history, and no-op
  filtering.
- Entity duplication and deletion with Undo/Redo.
- Curated weather commands: apply, undo, redo, preset content, and preservation
  of Wicked weather values outside the curated state.
- Proving Ground blueprint structure: no internal helper entities, no grid
  geometry, no duplicate names, ground relief wider than the grid, four deck
  edges, a weather entity, a shadow-casting sun, a volumetric light.
- Headless WISCENE save and reload including a saved transform and serialized
  weather.
- Project lifecycle, recent-project persistence, and editor preferences
  surviving a restart.

## Adding a test

Assert against `EngineBridge` services and commands, never against Studio
widgets. If a test needs a mesh, a material, or a rendered frame, it does not
belong here — add it to the packaged Windows acceptance checklist for the
milestone instead.
