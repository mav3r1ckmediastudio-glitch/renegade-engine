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
- Conversion and WISCENE round-trip work run on Wicked's job system, matching
  the upstream Editor importer. Only status and result presentation return to
  `EVENT_THREAD_SAFE_POINT`; the converter must never run inside that callback.

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

## First packaged-test correction

The first packaged proof route incorrectly invoked the complete converter from
`EVENT_THREAD_SAFE_POINT`. A valid 18 MB Sketchfab GLB containing 68 nodes,
27 meshes, two materials, seven embedded textures, one skin, and one animation
then exited Studio during import. The source GLB passed container and JSON
structure inspection and uses only optional `KHR_materials_specular` material
data supported by the pinned Wicked converter.

The corrected route starts conversion on a dedicated Wicked job-system context
and subscribes to the thread-safe point only after the import result exists.
Both renderer acceptance tests must use the same GLB again; the original crash
is evidence of a failed gate, not evidence of a bad source asset.

## Next gate after acceptance

Build the Renegade-owned **Asset Browser > Add Asset** importer workspace:
isolated preview, import settings, project asset registration, and reusable
browser entry. Model placement and scene-instance inspection follow that
registered-asset boundary.
