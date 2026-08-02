# Renegade Engine — Current Handoff

**Date:** 2026-08-02

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Active branch:** `phase3/terrain-authoring`

**Cleanup base:** `0a1d0b6`

**Pull request:** #11 into `main`

**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Current truth

Renegade's native terrain can be created, sculpted across chunk seams, saved,
closed, and opened through the Renegade UI. The project owner verified in the
packaged DX12 editor that all 169 sculpted chunks remain present and visually
unchanged after Open and the first terrain generation update.

The standard terrain is 13 x 13 chunks. Each chunk is 132 m square at the
standard configuration, giving 1,716 x 1,716 m (about 2.945 km2).

Windows Debug and Release CI are green at `0a1d0b6`. The Windows baseline is
also green. Packaged Vulkan, Runtime terrain loading, material-preset behaviour,
and the complete save-backup/failure checklist remain pending.

PR #11 is not yet merged. Obsolete PR #5 has been closed.

## Terrain persistence root cause and fix

The WISCENE deserialize was correct. The failure occurred immediately after
load:

1. `CommitPreparedOpen()` called `RebindDefaultTerrainMaterials()`.
2. The rebind used material setters, which marked the bundled grass material
   dirty.
3. Wicked's first `Generation_Update()` interpreted the dirty material as a
   regeneration request.
4. `Generation_Restart()` cleared the 169 correctly loaded sculpted chunks and
   replaced them with procedural terrain.

Commit `c7eea43` preserves the material's incoming dirty state during the
default-texture rebind. A clean deserialized material remains clean; genuine
pre-existing dirtiness remains dirty. Other callers already request an
explicit restart. Commit `0a1d0b6` removed the temporary diagnostic logging
after the packaged DX12 test remained at 169 chunks.

## Cleanup prepared after `0a1d0b6`

The four terrain-shape controls—Flat World, Island, Coastline, and
Highlands—were not functioning presets in the packaged editor. Their tests
only proved that parameter values differed, not that different terrain was
generated.

The cleanup therefore:

- removes the terrain-shape enum and `MakeTerrainPreset()` bridge API;
- removes the Studio selector, pending action, and handler;
- creates terrain directly from the standard `TerrainState`;
- replaces preset-oriented tests with the known standard configuration;
- retains the separate Meadow, Coarse Grass, and Fine Ground Cover
  material-scale presets; and
- updates project documentation to reflect verified behaviour and remaining
  work.

No Wicked source or submodule pointer is changed. The working terrain
persistence fix is untouched.

## Scene document workflow

PR #11 also contains the protected WISCENE document workflow:

- Open and Reopen prepare a candidate scene before replacing the active one.
- Save and Save As write and validate a same-directory temporary WISCENE.
- A successful overwrite preserves the previous file as `*.bak.wiscene`.
- Successful saves retain the newest ten rolling WISCENE backups under
  `Saved/Backups/Scenes/<scene-name>`.
- Failed operations must retain the active scene, path, and dirty state.
- All interaction remains in the Renegade UI; no stock Wicked Editor windows
  are used.

The packaged DX12 terrain round trip proves the reported terrain-loss path is
fixed. It does not prove every document scenario. The remaining authoritative
checklist is in `docs/PHASE3_WICKED_OPEN_SCENE_INTEGRATION.md`.

## Required verification before merge

1. Apply the cleanup to `phase3/terrain-authoring` and push it.
2. Require Renegade Studio Debug and Release CI to pass.
3. Package Release and launch DX12.
4. Create the standard terrain, sculpt across an edge and four-chunk corner,
   Save, close, Open, wait for the first update, and confirm the shape remains.
5. Confirm terrain Inspector contains no Flat World, Island, Coastline, or
   Highlands control.
6. Check the remaining material-scale controls and their Undo/Redo.
7. Repeat the terrain Save/Open test with the `vulkan` argument.
8. Test Runtime loading and the `.bak` plus rolling-backup paths before
   promoting the whole document workflow to passed.
9. Squash-merge PR #11 only after the applicable checks pass.

## Known remaining terrain work

- Packaged acceptance of material-scale presets and Reload Files.
- Runtime and Vulkan parity.
- Four-region material painting and automatic slope/height rules.
- Validated 16-bit PNG/RAW heightmap import and export.
- Real terrain-shape presets only when backed by distinct generation logic and
  packaged visual regression tests.

Light and Material Authoring follows the accepted Terrain V1 foundation.

## Repository rules

- Do not edit Wicked or move its pin without an explicit core-patch/upstream
  task.
- Do not claim behavioural success from compilation alone.
- A visible failure overrides green CI.
- Persistent scene mutations belong in EngineBridge commands and require
  Undo/Redo plus save/reopen evidence.
- `main` remains untouched until PR #11 is accepted.
