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
  four neutral automatic material regions.
- Safety bounds protect chunk radii, scale, height range, blend thresholds,
  and LOD bias.
- `RenegadeTerrainTests` covers capture, apply, presets, safety, no-op
  filtering, Undo, and Redo without requiring a graphics device.

## Remaining V1 slices

1. Terrain workspace and create/preset/generation controls.
2. Viewport sculpt transaction: raise/lower, smooth, flatten, adjustable
   radius, strength, and falloff.
3. Four-region material painting plus slope- and height-rule controls.
4. 16-bit PNG/RAW heightmap import and export with validation.
5. Painted chunk snapshot commands, save/reopen, and packaged Runtime check.

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
