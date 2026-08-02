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
2. Before the prepared scene is handed over, `ImportService::
   ResolveScaleFactor(ModelScaleMode::Automatic, ...)` computes a uniform
   scale correction from the union of every mesh's local vertex-position
   bounds, normalizing the largest extent to a 2 m target. This is the fix
   for arbitrary downloaded/authored models with an unknown source unit
   (glTF mandates metres, but an asset authored or exported assuming a
   different convention can still land absurdly large or small). See
   "Import scale correction" below.
3. At `EVENT_THREAD_SAFE_POINT`, `PlaceImportedModelCommand` merges the
   converted scene directly into the active Studio scene via Wicked's
   `Scene::Merge()`, positions the new root five metres in front of the
   editor camera (matching stock Wicked Editor's own "place in front of
   camera" convention and its "imported models always have a root transform
   entity" rule for locating that root), applies the resolved scale factor
   as the root's uniform `Scale`, and snapshots the merged entity via
   `Scene::Entity_Serialize(..., RECURSIVE)` for Undo/Redo.
4. The new entity is selected and revealed in the hierarchy, matching the
   Add Light UX convention. The status bar reports the applied scale factor
   (`AUTO SCALE x0.XXX`) so packaged evidence can confirm what was applied.
5. Undo removes the whole imported hierarchy (`Entity_Remove(..., recursive
   = true)`); Redo restores it — including the applied scale — under the
   original entity IDs from the snapshot (`EntitySerializer::allow_remap =
   false`), the same pattern already used by
   `DuplicateEntityCommand`/`DeleteEntityCommand`.

## Import scale correction

`ImportService::ModelScaleMode` offers `Original`, `Meters`, `Centimeters`,
`Inches`, and `Automatic`, modeled after GameGuru MAX's importer scaling
modes. Two differences from GameGuru MAX's implementation, both deliberate:

- The chosen factor is applied as a **non-destructive uniform `Scale`** on
  the import root transform, never baked into vertex positions, bone
  matrices, or animation keyframes. It can be freely edited or reset
  afterward like any other transform value, and Undo/Redo covers it for
  free as part of the existing entity snapshot.
- `Automatic` normalizes the **union of every mesh's own local
  vertex-position bounds** — not a world-space, hierarchy/bone-aware
  bounding box like GameGuru MAX's `DBOAssImp.cpp` computes. This is exactly
  right for the common single-node or flat-hierarchy model, and an honest
  approximation — not a precise world-space bound — for a model whose nodes
  carry large relative offsets or per-node scale of their own.

`ADD > IMPORT MODEL...` always applies `Automatic` today; there is no UI
choice among the other four modes yet. `Original` and `Meters` always
resolve to the same 1.0 multiplier for a GLB/GLTF source specifically,
since glTF 2.0 mandates metres — both exist in the enum for a mode list
that reads the same as a familiar FBX/OBJ importer's, and so `Centimeters`/
`Inches` are ready to expose later without further service changes. A
manual override control (a mode picker applied to the currently selected
entity, independent of import) is deliberately deferred: it would need to
either recompute a bounding box scoped to just the imported entity's
descendant subtree inside the live active scene, or forgo `Automatic`
entirely for the manual case, and it would touch the Inspector's
hand-rolled, hardcoded absolute-pixel layout across many lines with no way
to verify the result without a packaged build. Both are real, scoped
follow-on work, not abandoned scope.

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
5. Confirm the model lands at a plausible real-world size next to existing
   scene content (not "VERY large" or vanishingly small), the status bar
   reports `AUTO SCALE x0.XXX`, and the imported root's Scale X/Y/Z in the
   Inspector all read that same value uniformly.
6. Confirm Undo removes the entire imported hierarchy in one step (not
   node-by-node), Redo restores it complete — including the same applied
   Scale — and the restored entity is still selectable and inspectable.
7. Edit a transform on the imported root, then Save, close, and Reopen.
   Confirm the imported model, its applied scale, and the edit all survive.
8. Launch Runtime with the saved scene and confirm the model renders there
   the same size and place as in Studio.
9. Repeat the complete check with the `vulkan` argument.
10. Attempt an import while a `BUILD > VALIDATE GLB/GLTF IMPORT...` proof or
    another `ADD > IMPORT MODEL...` is already running; confirm the busy
    message appears and no partial/duplicate entity is created.
11. Attempt an import of a malformed or unsupported file; confirm a
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
