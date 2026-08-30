# WD01 — Grass & Vegetation

**Programme position:** immediately after Phase 5 and before Phase 6.

**Base main:** `350927ee2e065405ff9d4c0c59ff791fa4df7c84`

**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Creator outcome

WD01 adds the smallest useful vegetation-authoring loop to the bottom of the existing **TERRAIN** Inspector workspace:

1. use Wicked's bundled grass as the initial built-in vegetation asset;
2. choose **PAINT** or **DELETE**;
3. set brush size and overall density;
4. paint grass directly onto authored Renegade terrain;
5. delete grass with the same viewport brush;
6. Undo/Redo the completed brush stroke;
7. save/reopen without losing the painted vegetation;
8. observe the same vegetation in Test Level and packaged Runtime.

The first version deliberately does **not** add biome rules, slope/height filters, multiple vegetation species, random scale controls, procedural biome generation, per-type erase rules or a stock Wicked Editor window.

## Native Wicked boundary

Renegade owns the creator workflow and persistence commands, but it does not create a parallel grass renderer or simulation.

The pinned Wicked terrain already stores per-chunk grass as `wi::HairParticleSystem`, including per-vertex `vertex_lengths`, and serializes that data with the terrain chunks. The hair-particle runtime already supplies GPU simulation, culling, wind sampling and collider response.

WD01 therefore edits the native terrain chunk grass masks and lets Wicked continue to render and simulate them.

The bundled starting preset is the pinned Wicked content:

- `WickedEngine/Content/terrain/grass.wiscene`
- `WickedEngine/Content/terrain/grassparticle.dds`

Renegade packages those files beside Studio and Runtime as read-only built-in content.

The viewport brush uses the existing `StudioRenderPath::camera` member and the same Wicked `GetPickRay`/terrain-pick path already used by Renegade terrain authoring; WD01 does not introduce a second editor camera.

## Manual-paint contract

When WD01 is first activated for a terrain, Renegade switches that terrain to explicit manual vegetation authoring:

- already-generated native procedural grass masks are cleared;
- existing chunks become empty until the creator paints them;
- new chunks generated later are initialized empty as they appear;
- a small persisted metadata marker records that the terrain and chunk are in Renegade manual-vegetation mode;
- the native `Terrain::grass_properties` and `Terrain::grass_material` remain the runtime authority for how grass looks and moves.

Painting sets native per-vertex grass mask values under the brush to present; deleting sets them to zero. A completed stroke records only the affected chunks for Undo/Redo rather than snapshotting the entire terrain.

## Initial UI

Append below the existing terrain sculpt controls:

### VEGETATION // GRASS

- readout: `GRASS // WICKED DEFAULT`
- `Brush Size`
- `Density`
- `PAINT`
- `DELETE`
- compact status/readout

No separate workspace is introduced.

Selecting a vegetation tool disables the terrain sculpt brush for that interaction. Selecting a sculpt mode disables the vegetation brush.

## Runtime behaviour

The grass must use Wicked's native simulation rather than a Renegade approximation:

- wind comes from Wicked's existing scene wind sampling;
- player/NPC/vehicle interaction is expected to use Wicked collider/force participation so grass bends when an actor moves through it and settles naturally afterwards;
- this interaction boundary is retained for Phase 6 player/NPC/vehicle work rather than hard-coding a player-only grass deformation path in WD01.

WD01 must not invent separate player-grass, NPC-grass or vehicle-grass systems.

## Persistence and history

Paint/delete strokes are persistent creator mutations and therefore require:

- completed-stroke Undo/Redo;
- WISCENE save/reopen proof;
- no dependency on transient Studio-only state for the actual grass mask;
- Test Level and packaged Runtime consuming the same native WISCENE terrain/grass data.

Changing overall vegetation density is also persistent and Undo/Redo capable.

## First owner acceptance

The first usable Release is accepted when the owner can:

1. open TERRAIN;
2. select **PAINT**;
3. paint a visible patch of the bundled Wicked grass;
4. select **DELETE** and remove part of that patch;
5. Undo and Redo the completed vegetation stroke;
6. save and reopen the scene with the patch intact;
7. launch Test Level and see the same grass;
8. build/run the packaged game and see the same grass.

Wind/collider simulation remains native Wicked. WD01 verifies wind response with the current scene environment; player/NPC/vehicle bending is wired to the same native collider path as those actor systems become creator-facing.

## Deferred expansion

The architecture must leave room for later additions without replacing the backend, including multiple species, custom vegetation assets, brush falloff, masks, random variation, slope/height rules, per-species draw distance, biome presets and richer erase/filter modes.
