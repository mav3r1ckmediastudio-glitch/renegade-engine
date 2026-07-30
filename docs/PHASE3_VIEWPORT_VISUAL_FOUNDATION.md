# Phase 3 Viewport and Proving Ground Visual Foundation

## Outcome

The editor viewport stops relying on serialized helper geometry, and the
generated Proving Ground stops reading as a primitive test diorama.

- Wicked's renderer-owned grid helper replaces the 22 serialized
  `__renegade_internal_grid_*` cube and line entities.
- The generated scene is composed around the world origin as a designed
  proving ground with ground relief, PBR materials, enclosure and background
  depth.
- Sky, lighting, shadows, exposure and layered atmosphere are authored on real
  scene data so they survive save and reopen.

## Editor grid

`wi::renderer::SetToDrawGridHelper(true)` enables a renderer-owned helper that
allocates temporary GPU line vertices inside `DrawDebugWorld` each frame. It is
not built from scene entities, so it is automatically:

- editor-only;
- not selectable by `wi::scene::Pick`;
- absent from `SceneService::ListEntities`; and
- absent from every serialized WISCENE.

At the pinned Wicked commit `3a800b7`, the 3D helper draws a fixed 20x20 unit
grid centred on the world origin at `y = 0.01`, with the centre lines tinted as
axis indicators. Renegade tints the remaining lines ice-blue through
`wi::renderer::SetGridHelperColor`. `RenegadeRuntime` never enables the helper,
and Studio disables it whenever the Project Hub is visible.

A Renegade-owned infinite, fading, holographic grid remains a later Renegade
layer. This milestone deliberately uses the stock helper and does not modify
Wicked source.

## Generated Proving Ground

`ProvingGroundBlueprint()` in `EngineBridge` describes every generated entity as
plain data. `SceneService::CreateProvingGround()` instantiates that list
verbatim. The split exists because Wicked's primitive factories call
`MeshComponent::CreateRenderData()`, which dereferences the global graphics
device: the instantiation needs a renderer, but the composition can be asserted
without one.

Composition, all centred on the origin so the grid helper reads as the deck's
own measurement grid:

| Element | Purpose |
|---|---|
| Proving Ground Terrain | Generated 180x180 unit relief mesh that flattens under the deck |
| Proving Ground Deck | Smoked near-black composite slab, 22x22 units |
| Deck Edge (x4) | Ice-blue projected deck boundary |
| Alignment Pedestal, Renegade Hologram Core | Centre stage |
| Gateway Left/Right/Crown | Framed entry with a volumetric beam |
| Range Marker (x2) | Amber scale reference |
| Perimeter Pylon (x8) | Enclosure and long shadow casters |
| Retaining Terrace (x3) | Stepped ground transition |
| Equipment Crate (x4) | Human-scale reference and shadow catchers |
| Distant Structure (x5) | Background masses for fog and aerial perspective |
| Environment Probe | Local reflections |
| Environment | WeatherComponent carrier |

The ground relief is a generated treatment, not a terrain-authoring system.
There is no editing surface, no chunking, no virtual texturing and no
streaming. Its height function is deterministic, so every generated project
produces identical geometry.

## Atmosphere

Weather is authored on a real entity named `Environment`. This matters:
`Scene::weather` is only a runtime copy of `weathers[0]` that
`RunWeatherUpdateSystem` overwrites each frame, and it is not serialized on its
own. Only a `WeatherComponent` on an entity survives save and reopen.

Three layers combine:

1. Distance fog (`fogStart`, `fogDensity`).
2. Low-lying mist (`HEIGHT_FOG` with `fogHeightStart` and `fogHeightEnd`).
3. Volumetric light scattering: `RenderPath3D::setVolumeLightsEnabled(true)`
   plus per-light `SetVolumetricsEnabled` and `volumetric_boost` on the sun,
   the hologram core, the gateway beam and both range markers.

Layer 3 is what makes the atmosphere react to lighting rather than read as a
flat overlay. Realistic sky with aerial perspective supplies the environment
and the background falloff.

Arbitrary local fog volumes are still not a Renegade system.

## Scene advancement and save

`Scene::Update()` is renderer-dependent. It is now absent from every
`EngineBridge` path:

- `CreateProvingGround()` no longer calls it.
- `SaveScene()` no longer calls it. Serialization writes local transforms only,
  and `TransformComponent` recomputes its world matrix on read.
- `LoadScene()` deserializes the archive directly with `Scene::Serialize`
  instead of `wi::scene::LoadModel`. `LoadModel` is an import path: it
  reparents every unparented transform under a temporary root, calls
  `Scene::Update()`, then detaches. Reading the archive directly is the exact
  inverse of `SaveScene` and preserves the authored hierarchy.

The render-capable Studio frame loop remains the only owner of scene
advancement, through `RenderPath3D::Update`.

## Windows acceptance

Test the exact packaged Release artifact through both
`Run-RenegadeStudio-DX12.cmd` and `Run-RenegadeStudio-Vulkan.cmd`.

1. Project Hub create, open and recents still work, with no grid and no
   frame-rate readout drawn over the Hub.
2. A new project contains no serialized line or cube grid helpers. Confirm in
   the hierarchy and by reopening the saved scene.
3. The viewport grid renders, cannot be clicked, is not listed in the
   hierarchy, and does not reappear after save and reopen.
4. Viewport selection and fly-camera navigation still work.
5. Inspector transforms, W/E/R gizmos, focus, duplicate, delete, Undo, Redo,
   Save, Save As and reopen still work, including saved transform persistence.
6. The Proving Ground shows materially improved ground, PBR materials,
   composition, sky, lighting, shadows, exposure and atmosphere.
7. Volumetric scattering visibly reacts to the sun and the gateway beam rather
   than reading as a flat fog overlay.
8. Fog, mist and sky are still present after closing and reopening the project.
9. Performance remains acceptable on the RTX 4070 Ti with no unexplained
   regression from the prior 75 FPS VSync-limited result.

Required report:

```text
DX12 GRID PASS / GENERATED SCENE PASS / ATMOSPHERE PASS /
EDITING REGRESSION PASS / SAVE REOPEN PASS / PERFORMANCE PASS /
VULKAN PASS
```

Visual comparison must use the project owner's approved reference imagery.
Green CI alone cannot pass this milestone.
