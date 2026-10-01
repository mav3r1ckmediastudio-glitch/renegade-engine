# Static FBX importer fixtures

These fixtures were generated from a new two-metre Blender cube with one UV
checker material. The 64x64 red/blue image is generated from numeric pixel
values. No third-party authored model is included.

- `static_textured_cube.fbx` references `textures/checker.png` relatively.
- `static_embedded_cube.fbx` embeds the same PNG.
- Neither fixture has animation, skinning, or bones.

Regenerate with Blender 5.1.1 (other exporter versions may change binary bytes):
`blender --background --factory-startup --python generate_fbx_fixtures.py`
Run from this directory or pass the script's full path. The generator strips
exporter-written absolute texture paths without changing FBX binary offsets.
The committed fixtures make running the proofs independent of Blender.

The proof copies input into its own disposable source folder, verifies missing
and changed external-texture rejection, imports and persists textures, checks
preview rotation and placement Undo/Redo/save/reopen, then deletes that input
copy. A separate process verifies textured placement and retained-source
reconversion without the original disposable source directory.
