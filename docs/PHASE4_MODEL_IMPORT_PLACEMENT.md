# Model Import — Scene Placement

## Purpose

Model Import V1 Gate 1 (`docs/PHASE4_MODEL_IMPORT_V1.md`) proved the
technical conversion boundary: Wicked's native GLB/GLTF converter can import
a real model, save it as WISCENE, reload it, and preserve the recorded scene
structure, without ever touching the active Studio scene. That gate is
closed.

This slice answers the actual creator need behind it: **import a model and
have it appear in the open scene**, undo-able like any other native edit.

It is deliberately smaller than the "Next gate after acceptance" milestone
named at the bottom of `docs/PHASE4_MODEL_IMPORT_V1.md` (a full
Asset Browser > Add Asset workspace with isolated preview, import settings,
project asset registration, and a reusable browser entry). That remains
future work. This slice does not:

- register the imported model as a reusable project asset;
- write any asset file of its own (no `Content/...` copy is created);
- add an Asset Browser entry or thumbnail;
- support re-instancing the same source file without re-running the GLTF
  conversion.

The imported model becomes native scene content the moment it lands: it
persists exactly the way any other entity does, through the existing
Save/Save As/Reopen path, and nothing else.

## What it does

`ADD > IMPORT MODEL...` opens a GLB/GLTF file picker. On selection:

1. `ImportService::PrepareGltfAsset` runs the same proven conversion used by
   Gate 1, on Wicked's job system, producing a heap-backed `Scene`. A
   `.wiscene`-shaped staging path under `Saved/Validation/ModelImport/` is
   still passed in (required by the service's extension check and
   stage-breadcrumb log) but nothing is ever written there for this flow —
   unlike Gate 1, this path calls `CompleteGltfAsset` never.
2. At `EVENT_THREAD_SAFE_POINT`, `PlaceImportedModelCommand` merges the
   converted scene directly into the active Studio scene via Wicked's
   `Scene::Merge()`, positions the new root five metres in front of the
   editor camera (matching stock Wicked Editor's own "place in front of
   camera" convention and its "imported models always have a root transform
   entity" rule for locating that root), and snapshots the merged entity via
   `Scene::Entity_Serialize(..., RECURSIVE)` for Undo/Redo.
3. The new entity is selected and revealed in the hierarchy, matching the
   Add Light UX convention.
4. Undo removes the whole imported hierarchy (`Entity_Remove(..., recursive
   = true)`); Redo restores it under the original entity IDs from the
   snapshot (`EntitySerializer::allow_remap = false`), the same pattern
   already used by `DuplicateEntityCommand`/`DeleteEntityCommand`.

No stock Wicked Editor window is embedded. No Wicked file or submodule
pointer is changed. The `BUILD > VALIDATE GLB/GLTF IMPORT...` diagnostic
proof route is untouched and remains the Gate 1 evidence path; it is not
repurposed into this creator-facing flow.

## Packaged acceptance

1. Package Release and launch DX12 Studio in a project with an existing
   scene (some pre-existing geometry helps confirm merge behaviour, not just
   creation-into-empty-scene).
2. Open `ADD > IMPORT MODEL...` and pick a textured, multi-node GLB (the
   same `windmill_in_soviet_village.glb` used for Gate 1 is good coverage:
   multi-object, multi-material, one armature, one animation).
3. Confirm the model appears in front of the camera, is automatically
   selected, and is revealed under the **Models** (or appropriate)
   collapsible hierarchy header.
4. Confirm the pre-existing scene content is untouched — no other entity
   moved, was renamed, or lost its selection state.
5. Confirm Undo removes the entire imported hierarchy in one step (not
   node-by-node), Redo restores it complete, and the restored entity is
   still selectable and inspectable.
6. Edit a transform on the imported root, then Save, close, and Reopen.
   Confirm the imported model and the edit both survive.
7. Launch Runtime with the saved scene and confirm the model renders there
   the same as any other native entity.
8. Repeat the complete check with the `vulkan` argument.
9. Attempt an import while a `BUILD > VALIDATE GLB/GLTF IMPORT...` proof or
   another `ADD > IMPORT MODEL...` is already running; confirm the busy
   message appears and no partial/duplicate entity is created.
10. Attempt an import of a malformed or unsupported file; confirm a
    controlled failure dialog, never a desktop exit, and confirm nothing
    was added to the scene.

A visible or behavioural failure stops this gate even if CI is green,
consistent with every other acceptance checklist in this repository.

## Next gate after acceptance

Unchanged from `docs/PHASE4_MODEL_IMPORT_V1.md`: build the Renegade-owned
**Asset Browser > Add Asset** importer workspace — isolated preview, import
settings, project asset registration, and a reusable browser entry — so an
imported model can be registered once and instanced many times without
re-running conversion each time.
