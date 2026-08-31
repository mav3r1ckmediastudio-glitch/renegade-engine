# WD01 — Grass & Vegetation

**Programme position:** immediately after Phase 5 and before Phase 6.

**Base main:** `350927ee2e065405ff9d4c0c59ff791fa4df7c84`

**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Creator outcome

WD01 exposes Wicked's native terrain grass/HairParticle capability through a Renegade-owned vegetation workflow at the bottom of the existing **TERRAIN** Inspector workspace.

The creator can:

1. use Wicked's bundled grass as the initial built-in vegetation asset;
2. choose **PAINT** or **DELETE**;
3. set brush size and creator-facing density;
4. paint a smooth continuous grass stroke directly over authored terrain;
5. delete grass with the same continuous brush;
6. tune the useful native Wicked HairParticle properties without opening Wicked's generic editor window;
7. author sprite-atlas rectangles and per-rect size;
8. Undo/Redo completed brush strokes, density changes and grass-setting changes;
9. save/reopen without losing vegetation;
10. observe the same native vegetation in Test Level and packaged Runtime.

WD01 still deliberately excludes biome rules, slope/height filters, multiple vegetation species and procedural biome generation. Those remain later authoring layers over the same native backend.

## Native Wicked boundary

Renegade owns the creator workflow, layout and history commands. It does **not** create a parallel grass renderer or simulation.

The pinned Wicked terrain stores per-chunk grass as `wi::HairParticleSystem`, including per-vertex `vertex_lengths`. Wicked's runtime supplies GPU strand generation/simulation, view-distance culling, wind sampling, force/collider response and sprite-atlas support.

The bundled starting content is:

- `WickedEngine/Content/terrain/grass.wiscene`
- `WickedEngine/Content/terrain/grassparticle.png`

Renegade packages those files beside Studio and Runtime as read-only built-in content.

## Stable manual-paint contract

When WD01 is first activated for a terrain, Renegade switches that terrain to explicit manual vegetation authoring:

- already-generated procedural masks are cleared;
- existing chunks become empty until the creator paints them;
- new chunks generated later are initialized empty;
- persisted metadata records manual-vegetation mode;
- native `Terrain::grass_properties` and `Terrain::grass_material` remain the runtime authority.

The first owner build recalculated `strandCount` from the number of painted vertices on every brush sample. That changed Wicked's strand index/distribution while the mouse moved and caused visible popping/flicker.

WD01 now keeps a **stable full-chunk emitter distribution** once a chunk contains grass. Inactive vertices use a tiny CPU-side positive mask (`0.001`) so Wicked's `CreateFromMesh()` keeps the emitter triangle list stable, while Wicked's R8_UNORM length upload truncates that value to zero and therefore renders no grass there. Paint changes the mask to `1`; Delete returns it to the zero-render sentinel. Density remains a multiplier over the stable distribution.

Long mouse movement is sub-stepped in world space at overlapping brush intervals so fast pointer motion cannot leave frame-rate-sized gaps.

## Renegade grass UI

WD01 remains at the bottom of the existing Terrain panel and uses the normal Inspector scroll range. The advanced groups are collapsible.

### Placement

- `GRASS // WICKED DEFAULT`
- Brush Size
- Density
- **PAINT**
- **DELETE**
- compact status/readout

### Appearance

Maps directly to native Wicked HairParticle properties:

- Length — default `0.35 m` for Renegade's metre-scale terrain
- Width
- Randomness
- Random Seed
- Uniformity

### Movement / Response

- Stiffness
- Drag
- Gravity Power
- Camera Bend

Wind is intentionally not a per-grass control: Wicked's native grass simulation samples the scene/environment wind.

### Geometry / Performance

- Segments
- Billboards
- View Distance

Raw **Strand Count** is intentionally not exposed. Renegade's Density control is the creator-facing equivalent because terrain vegetation spans many chunk emitters and must preserve a stable strand index space while painting.

The generic Wicked **Mesh** selector is also intentionally omitted because Renegade terrain chunks are the emitter meshes by construction.

### Sprite / Texture Atlas

- native grass texture preview
- previous/next sprite rectangle
- add/remove sprite rectangle
- rectangle width/height
- U/V offset
- per-sprite Size

An empty atlas means Wicked uses the whole texture, matching native behaviour.

## Runtime behaviour

The grass remains Wicked-native:

- wind comes from Wicked scene wind sampling;
- stiffness, drag and gravity feed Wicked's strand simulation;
- camera bend is Wicked's native card-hiding behaviour;
- player/NPC/vehicle interaction uses the same Wicked collider/force participation path so vegetation can bend around moving actors without separate player/NPC/vehicle grass systems;
- view distance, billboard count, segment count, randomness, seed, uniformity and atlas selection are consumed by the native HairParticle runtime.

## Persistence and history

Persistent creator mutations require shared Studio history and WISCENE parity:

- Paint/Delete: completed-stroke Undo/Redo;
- Density: Undo/Redo;
- HairParticle appearance/movement/geometry/atlas settings: Undo/Redo;
- WISCENE save/reopen;
- Test Level parity;
- packaged Runtime parity.

The actual mask and grass settings live in Wicked terrain/HairParticle state, not transient Studio-only data.

## Current owner acceptance sequence

Before final hardening, the Release owner test should prove:

1. the Terrain panel scrolls cleanly with no control overlap;
2. **PAINT** produces a smooth stable patch without the previous redistribution flicker;
3. **DELETE** removes it smoothly;
4. Density retains the accepted visual density behaviour;
5. default Length is approximately knee-height (`0.35 m`) and can be changed live;
6. Appearance controls visibly affect grass;
7. Stiffness/Drag/Gravity/Camera Bend visibly affect native response;
8. Segments/Billboards/View Distance update safely;
9. sprite rectangles can be added, selected, edited and removed;
10. Undo/Redo works for a stroke and representative settings.

After creator interaction is accepted, WD01 is hardened through save/reopen, Test Level, packaged Runtime and final CI.

## Deferred expansion

The architecture remains open for multiple species, custom vegetation assets/textures, brush falloff, masks, slope/height rules, biome presets, richer erase/filter modes and vegetation-library workflows without replacing the Wicked backend.
