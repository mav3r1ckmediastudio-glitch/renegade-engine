# Phase 3 — Terrain polish build

## Outcome

This bounded follow-up turns the accepted terrain proof into a faster material
and sculpt iteration workflow. It does not add the remaining Terrain V1 paint,
heightmap, erosion, vegetation, river, road, biome, or procedural systems.

This document covers material-scale and sculpt polish only. The removed Flat
World, Island, Coastline, and Highlands terrain-shape controls were a separate
workflow and must not be confused with the material-scale presets below.

## Included

- New terrain defaults to the bundled grass at an apparent 8x scale rather
  than the original, overly dense 32x presentation.
- Texture Scale is a live 1x–32x material UV control. Wicked serializes the
  material `texMulAdd` value in WISCENE, so the setting survives save/reopen
  without changing the texture source files.
- Meadow (8x), Coarse Grass (12x), and Fine Ground Cover (16x) presets apply
  the same bundled PBR material at useful starting scales.
- Apply Default assigns the bundled grass to all four automatic terrain
  regions as one Undo step. Reload Files reloads changed bundled texture files
  and regenerates the native terrain material without recompiling Studio.
- Releasing a sculpt stroke no longer restores the entire pre-stroke terrain
  and executes the completed stroke a second time. The already-live result is
  registered directly in command history.
- Undo/Redo states retain only chunks whose heights changed. Mouse release
  refreshes physics only for those chunks.
- The terrain Inspector shows brush size/strength and the last stroke's
  affected tile count and finish time.
- Renegade's owned bottom status bar restores a lightly smoothed FPS readout,
  sampled over 250 ms and hidden with the workspace on Project Hub.

## Persistence and architecture

Material presentation is captured by `TerrainMaterialState` and changed by
`SetTerrainMaterialCommand`. Scene material paths, PBR values, primary AO and
UV scale are restored by Undo/Redo without exposing Wicked internals to Studio.
The original 32x build-time maps remain the single packaged texture set; live
scale is a serialized sampling multiplier, not another generated asset.

`CommandService::RecordExecuted` is intentionally limited to interactions whose
after-state is already visible. It preserves the normal Undo/Redo ownership
while avoiding redundant restore/re-execute work at transaction completion.

## Verification status

The project owner has verified the sculpted 169-chunk terrain Save/Open round
trip on packaged DX12, including survival of the first terrain update after
the material-dirty correction in `c7eea43`. Material-scale presets, Reload
Files, Runtime, and Vulkan still require the acceptance pass below.

## Remaining acceptance

On the packaged Windows Release build:

1. Fresh terrain opens with Meadow 8x and visibly larger grass detail.
2. Moving Texture Scale and applying all three presets updates the terrain.
3. Save, close and reopen preserves the selected scale.
4. Apply Default, Undo and Redo restore the expected material state.
5. Replacing a bundled TGA and pressing Reload Files updates the terrain
   without rebuilding the application.
6. Raise, Lower, Smooth and Flatten remain seamless across an edge and a
   four-tile corner; Undo/Redo retain the complete stroke.
7. Mouse release reports affected tiles and finish time without the prior
   obvious one-second double rebuild.
8. The bottom-right status bar displays live FPS in the workspace and no FPS
   text on Project Hub.
9. Save/reopen and Runtime keep the terrain material and sculpted height data.
10. Repeat the visual regression on Vulkan after DX12 passes.

Windows compilation, packaged visual behaviour, and GPU timing remain the
release authority. The Linux scratch environment cannot build the Windows-only
Studio target.
