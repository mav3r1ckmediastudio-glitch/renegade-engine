# Phase 3 Terrain Authoring

## Current result

Terrain remains Wicked's native WISCENE component and is authored through
Renegade-owned controls. Scene UI Gate 5 changes the standard configuration to
19 x 19 chunks at 66 m per chunk: 1,254 x 1,254 m (about 1.573 km2) with one
metre between sculptable samples.

The project owner has verified in the packaged DX12 editor that a sculpted
169-chunk terrain survives Save, close, Open, and the first terrain generation
update without changing shape. Windows Debug and Release CI are green at the
cleanup base commit `0a1d0b6`.

## Delivered

- `TerrainState`, `SetTerrainCommand`, and `CreateTerrainCommand` provide
  bounded native creation, no-op filtering, and Undo/Redo.
- The permanent Terrain workspace presents resolution and honest current
  dimensions rather than exposing Wicked's raw generation radius.
- `EXPAND TERRAIN // +1 RING` adds one 66 m chunk on every side without
  restarting or altering any existing chunk; expansion is one Undo/Redo item.
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
6. A separate authored-chunk residency store before camera-following removal is
   enabled. Wicked's pinned removal path erases `terrain.chunks`, including the
   serialized sculpt/blend authority, so it cannot safely unload edited chunks.

Procedural worlds, runtime deformation, rivers, roads, landscape splines,
foliage scattering, and biome generation remain out of this slice.

## Merge gate

Run Windows Debug and Release CI after this cleanup. Package Release and prove
the standard terrain still creates, sculpts, saves, closes, opens, and remains
unchanged after the first update on DX12. Repeat the round trip on Vulkan.
Automated success alone is not visual acceptance.

## New-terrain initialization recovery (2 November 2026 candidate)

Standard new terrain now has an identity root and a surface at world Y=0 inside
the existing -20 to +120 metre encoding range. A serialized native constant
HeightmapModifier supplies that same surface reference to initial generation,
later expansion rings and reopened terrain. Existing levels keep their authored
transforms, height data, modifiers, masks and river layers.

Ordinary shallow river beds can be linked on untouched new terrain without
Allow Deeper Beds. The explicit range-extension command remains available for
legacy terrain or cuts beyond the current minimum. The maximum-terrain
continuation below replaces its earlier expansion restriction. Sculpting clamps mesh edits before encoding so visual,
collision and saved heights stop at the same valid limits.

New terrain starts without procedural grass; explicit manual painting enables
the existing native vegetation path. Chunk synchronization acknowledges completion
only after generated meshes are merged, preserving bare expansion rings.

A separate reproduced Player Start preview defect shared generated terrain
texture-resource wrappers with the editor scene. Native preview generation then
rebound the editor to the preview atlas, producing dark chunks until reopening.
Preview chunk resources are now private; authored material textures retain the
existing sharing path. This changes no saved terrain format or Wicked pin.

Native regression evidence and outstanding actual-Studio gates are tracked in
TERRAIN_INITIALIZATION_RECOVERY_2026-11-02.md. This candidate is not release
acceptance and does not authorize another alpha.


## Maximum-size continuation (3 November 2026 candidate)

The supported maximum is radius 16: 33 x 33 chunks, 2,178 x 2,178 metres.
River layer coverage now accepts the complete supported resident extent.
Expansion before linking and expansion with existing linked rivers are both
required workflows. Linked expansion validates all old coverage, generates
one new native ring and composes that ring before recording the command.
Undo removes that ring; Redo restores its entity identities so later sculpt
and river history retain their targets. Existing chunk heights and paint
must remain unchanged.

Schema v3 stores changed sample indices and baseline/result values rather
than full grids for every touched chunk. The changed-sample budget remains
equivalent to 81 full grids; the 32 MiB document, 32-layer and 32M composition
work limits remain enforced. v1/v2 scenes remain readable. Rocky bed painting
uses local segment selection and retains its original worst-case work bound.
Unsupported or pathological edits still fail before changing the terrain.

An optional explicit floor extension appends a serialized native affine
generator adjustment, preserving its original absolute height function on
new rings. Ordinary fresh terrain still starts at world Y=0 inside -20..120 m;
no floor extension is needed for shallow linked channels.

Implementation is under validation. Maximum-size automated, actual Studio,
Player Start preview, Test Game, Runtime and cold-process reopening evidence
is tracked in MAXIMUM_TERRAIN_RIVER_RECOVERY_2026-11-03.md. Earlier separate
default-river and unlinked-expansion passes do not establish their combination.
