# Phase 3 Terrain Authoring

## Product decision

Terrain Authoring supersedes Light and Material Authoring as the immediate next
gate by product-owner decision on 2026-07-31. Light and Material Authoring
remains required and follows this bounded terrain sequence.

## V1 outcome

Renegade exposes Wicked's native streamed terrain through Renegade-owned bridge
services and editor workflows. Terrain remains a native WISCENE component; no
parallel heightfield format is introduced.

## Delivered foundation

- `TerrainState` captures the creator-facing generation envelope without
  overwriting materials, modifiers, props, painted blend maps, or chunk data.
- `SetTerrainCommand` provides no-op filtering and Undo/Redo.
- Flat World, Island, Coastline, and Highlands presets use restrained height
  envelopes rather than exaggerated showcase values.
- `CreateTerrain` creates the native component, transform, hierarchy name, and
  four automatic material regions using the bundled grass PBR default.
- Safety bounds protect chunk radii, scale, height range, blend thresholds,
  and LOD bias.
- `RenegadeTerrainTests` covers capture, apply, presets, safety, no-op
  filtering, Undo, and Redo without requiring a graphics device.

## Implemented Studio slice — pending Windows verification

- Empty-selection Inspector action creates and selects one native terrain.
- `CreateTerrainCommand` snapshots the native entity tree for Undo/Redo while
  preserving the terrain entity identity across redo.
- The selected terrain exposes Flat World, Island, Coastline, and Highlands.
- Visible chunk radius, chunk scale, minimum/maximum height, low/base/slope
  automatic material thresholds, and LOD bias use preview/commit sliders.
- Each completed slider drag and preset application contributes at most one
  `CommandService` history entry; terrain generation restarts on commit.
- Terrain duplication is disabled until recursive native terrain cloning has a
  dedicated validated workflow.

Implementation commit: `dd43851`. This slice has not compiled or run in the
current Linux workspace because its Windows CMake toolchain is unavailable.

## Seam and default-material correction — pending Windows verification

Implementation commit: `762c828` (code-only; apply after the separate texture
asset commit `87ad2d2`).

- Sculpting now builds a bounded canonical grid across every chunk touched by
  the brush. Shared edge and corner vertices receive one identical height.
- Smooth samples a 3x3 neighbourhood in that global grid, including adjacent
  chunks, rather than averaging each tile independently.
- Changed vertices and their normal-dependent neighbours rebuild matching
  normals, tangents, render data, BVHs, bounds, native 16-bit height data, and
  chunk region textures as one stroke. The completed command also refreshes
  native heightfield collision; live drag preview does not rebuild physics on
  every mouse sample.
- The existing complete-stroke snapshot continues to make all affected chunks
  one Undo/Redo operation.
- New terrain automatically assigns the supplied grass base colour, normal,
  AO and roughness maps. `RenegadeTerrainSurfacePacker` produces Wicked's packed
  surface texture and 32x pre-tiled base/normal maps during the Studio and
  Runtime builds; source TGAs remain unchanged.
- WISCENE load rebinds only the bundled default filenames to the current
  package, preserving the default through save/reopen without overriding later
  custom terrain materials.

The height source is retained for the later displacement/parallax decision and
is not applied to the sculpted large-scale geometry. Windows CI, packaged DX12
seam/material inspection, Undo/Redo, save/reopen, Runtime, and Vulkan remain
the acceptance gate.

## Remaining V1 slices

1. Windows compile and packaged visual verification of create/preset/generation
   controls at `dd43851`.
2. Packaged verification of the viewport sculpt transaction, shared seams,
   bundled default PBR material, and save/reopen.
3. Four-region material painting plus slope- and height-rule controls.
4. 16-bit PNG/RAW heightmap import and export with validation.
5. Painted chunk snapshot commands and packaged Runtime acceptance.

Procedural world generation, runtime deformation, rivers, roads, landscape
splines, foliage scattering, and biome generation remain out of V1.

## Windows verification gate

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\Build-Studio-Windows.ps1 -Configuration Debug
powershell -ExecutionPolicy Bypass -File .\Tools\Build-Studio-Windows.ps1 -Configuration Release
ctest --test-dir .build\windows -C Release --output-on-failure
```

Packaged DX12 must then prove create, preset, Undo/Redo, save/reopen, terrain
generation around a moving camera, ocean coastline interaction, cloud shadows,
fog, and no regression to transform/environment authoring. Vulkan repeats
after DX12 acceptance. Automated success is not visual acceptance.
