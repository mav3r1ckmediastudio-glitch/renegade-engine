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


## Animated Character fixture

Run Blender in background with --python generate_character_fixture.py. The
script creates a two-metre textured cube weighted to MovingBone under a two-bone
skeleton, with Wave (X rotation) and Turn (Z rotation) actions. It writes the
embedded-texture animated_character.fbx and rig_checker.png, removes exporter
machine paths without changing binary offsets, and uses FBX_SCALE_ALL for the
pinned converter's skin/unit compatibility. Default FBX_SCALE_NONE produced a
100-times rendered skin mismatch and is not accepted as this fixture. Everything
is generated here; no owner model, third-party character or animation is committed.
