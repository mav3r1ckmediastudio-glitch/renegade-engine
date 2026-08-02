# Model Import V1

## Gate 1 purpose

Gate 1 proves the technical conversion boundary before Renegade builds a
creator-facing importer. Wicked's native GLB/GLTF converter must import a real
model into an isolated scene, save it as WISCENE, reload it, and preserve the
recorded scene structure.

The temporary proof command is **BUILD > VALIDATE GLB/GLTF IMPORT...**. It is
not the future **Add Asset** workflow and does not place, register, or expose
the selected model in the Asset Browser.

## Safety boundary

- The active Studio scene, selection, hierarchy, Undo history, and dirty state
  are not changed.
- Proof output is written beneath
  `Saved/Validation/ModelImport/<source-name>.wiscene`.
- Nothing is written to project `Content` and no asset record is created.
- The converter runs only after Studio has initialized its DX12 or Vulkan
  graphics device.

## Packaged acceptance

Use one representative textured GLB. A model with multiple nodes and material
slots gives stronger evidence than a single untextured primitive.

1. Open a Renegade project in the packaged DX12 Studio.
2. Choose **BUILD > VALIDATE GLB/GLTF IMPORT...** and select the model.
3. Require a `PASS // MODEL IMPORT V1 GATE 1` dialog.
4. Record the object, mesh, material, texture-reference, transform, hierarchy,
   armature, and animation counts plus the output WISCENE path.
5. Confirm the open scene and its dirty state did not change.
6. Repeat with the packaged Vulkan Studio and the same source model.
7. Require both renderers to report a pass and matching component counts.

A crash, failure dialog, changed active scene, or differing structural counts
stops the gate. A pass does not yet authorize the visible importer workspace;
the owner must report both packaged renderer results first.

## Next gate after acceptance

Build the Renegade-owned **Asset Browser > Add Asset** importer workspace:
isolated preview, import settings, project asset registration, and reusable
browser entry. Model placement and scene-instance inspection follow that
registered-asset boundary.
