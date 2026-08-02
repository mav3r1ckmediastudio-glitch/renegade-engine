# Phase 3 Terrain Authoring

## Current result

Terrain remains Wicked's native streamed WISCENE component and is authored
through Renegade-owned controls. The standard configuration creates 13 x 13
chunks at 132 m per chunk: 1,716 x 1,716 m, or about 2.945 km2.

The project owner has verified in the packaged DX12 editor that a sculpted
169-chunk terrain survives Save, close, Open, and the first terrain generation
update without changing shape. Windows Debug and Release CI are green at the
cleanup base commit `0a1d0b6`.

## Delivered

- `TerrainState`, `SetTerrainCommand`, and `CreateTerrainCommand` provide
  bounded native creation, no-op filtering, and Undo/Redo.
- The permanent Terrain workspace exposes generation values directly.
- Raise, Lower, Smooth, and Flatten operate across shared chunk edges and
  corners as one stroke and one Undo/Redo item.
- Native per-chunk height and blend data remain the serialized authority.
- New terrain uses the bundled default grass PBR material.
- Texture scale, Apply Default, Reload Files, and the Meadow, Coarse Grass,
  and Fine Ground Cover material-scale presets remain available.
- Save/Open uses the protected scene-document transaction described in
  `docs/PHASE3_WICKED_OPEN_SCENE_INTEGRATION.md`.

## Removed misleading controls

Flat World, Island, Coastline, and Highlands were only different parameter
envelopes. They did not produce functioning, distinct landforms in the
packaged editor. Their Studio selector, bridge API, pending action, and tests
have therefore been removed.

New terrain now uses one explicit standard `TerrainState`. Shape presets are
deferred until each preset has a real generation implementation and packaged
visual tests. Material-scale presets are separate and are not removed by this
decision.

## Persistence correction

Commit `c7eea43` fixed the observed post-load terrain wipe. After a correct
WISCENE deserialize, `RebindDefaultTerrainMaterials()` changed the grass
material through setters that marked it dirty. Wicked interpreted that dirty
material during the first `Generation_Update()` as a request for
`Generation_Restart()`, clearing the 169 loaded sculpted chunks and replacing
them procedurally.

The rebind now preserves a clean material's clean state while retaining any
genuine pre-existing dirty state. Other callers already request an explicit
generation restart. Commit `0a1d0b6` removed the temporary diagnostic logging
after the packaged DX12 proof held at 169 chunks.

## Remaining V1 work

1. Packaged Vulkan repetition of sculpt, Save/Open, and the first update.
2. Packaged verification of material-scale presets, Apply Default, Reload
   Files, material Undo/Redo, and Runtime loading.
3. Four-region material painting and automatic slope/height rules.
4. Validated 16-bit PNG/RAW heightmap import and export.
5. Real terrain-shape presets, only if each has distinct generated output and
   visual regression evidence.

Procedural worlds, runtime deformation, rivers, roads, landscape splines,
foliage scattering, and biome generation remain out of this slice.

## Merge gate

Run Windows Debug and Release CI after this cleanup. Package Release and prove
the standard terrain still creates, sculpts, saves, closes, opens, and remains
unchanged after the first update on DX12. Repeat the round trip on Vulkan.
Automated success alone is not visual acceptance.
