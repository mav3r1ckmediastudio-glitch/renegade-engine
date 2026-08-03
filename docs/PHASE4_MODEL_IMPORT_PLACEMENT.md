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
   as the root's uniform `Scale`, calls `Play()` on every animation the
   import added (see "Import animation autoplay" below), and snapshots the
   merged entity via `Scene::Entity_Serialize(..., RECURSIVE)` for
   Undo/Redo.
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

`ADD > IMPORT MODEL...` always applies `Automatic` first. `Original` and
`Meters` always resolve to the same 1.0 multiplier for a GLB/GLTF source
specifically, since glTF 2.0 mandates metres — both exist in the enum for a
mode list that reads the same as a familiar FBX/OBJ importer's.

## Manual override: the Import Scale panel

Immediately after a model is placed, a small popup (`importScalePanel_`)
appears reporting the Automatic factor that was just applied and offering a
combo box to reinterpret the source unit: **Original/Meters (x1.0)**,
**Centimeters (x0.01)**, or **Inches (x0.0254)**. Choosing one and pressing
**APPLY** resets the imported root's Scale to that literal multiplier
through the same `SetTransformCommand`/Undo-Redo path as any manual
Transform edit (mirroring `ApplySelectedTransformValue`), and updates the
readout. **CLOSE** dismisses the panel without changing anything.

Two things this panel deliberately does not do:

- **It never re-offers `Automatic`.** Recomputing it correctly would need a
  bounding box scoped to just the imported entity's descendant subtree
  inside the live, already-merged active scene (the original computation
  ran against the isolated, pre-merge scene, where every mesh present
  belonged to the one imported model — that assumption no longer holds
  after `Scene::Merge()`). That subtree-scoped computation is separate,
  not-yet-built work; today the only way back to something like Automatic
  is Undo and re-import.
- **It is not wired into the Inspector.** Rather than insert a new row into
  the Transform section's hand-rolled, hardcoded absolute-pixel layout
  (`ResizeLayout`) — which would mean renumbering every row below Scale
  with no way to verify the result short of a packaged build — it is a
  fully independent `wi::gui::Window`, positioned and sized on its own in
  `ResizeLayout`, that only appears right after an import and can be
  dismissed. It does not persist as a docked panel and does not apply to
  any entity other than the one just imported.

**Known-fixed bug:** `wi::gui::Window::Render` scissor-clips every child to
the window's own rectangle, including a `ComboBox`'s dropdown list once
open, and `ComboBox::GetDropOffset`'s auto-flip logic only checks against
the full screen height, never the parent window's bounds. The panel's
original 168px height left the dropdown almost nowhere to render before
the panel's own edge clipped it away — the combo looked unresponsive but
was actually rendering its options into a clipped-away region. Fixed by
growing the panel to 288px and moving the Apply/Close buttons down to
y=234, so the full three-item dropdown has room inside the panel's own
scissor rect. Any future addition to this panel that needs more dropdown
headroom (more items, another combo) needs to grow the panel further for
the same reason.

No stock Wicked Editor window is embedded. No Wicked file or submodule
pointer is changed. The `BUILD > VALIDATE GLB/GLTF IMPORT...` diagnostic
proof route is untouched and remains the Gate 1 evidence path; it is not
repurposed into this creator-facing flow.

## Import animation autoplay

`wi::scene::AnimationComponent` is created `LOOPED` but not `PLAYING` by
default, and Wicked's own `ModelImporter_GLTF.cpp` never calls `Play()` on
the animations it creates — so an imported model's armature and keyframe
data can be complete and correct (as Gate 1's structural round-trip already
proved for `windmill_in_soviet_village.glb`'s "1 armature, 1 animation")
while sitting frozen at frame zero, because nothing ever started it. This
is not a missing rendering or skinning capability; it is one uncalled
method.

`PlaceImportedModelCommand::Execute()` now calls `.Play()` on every
animation the import adds, before the Undo/Redo snapshot is taken.
`ModelImporter_GLTF.cpp` attaches every animation entity under the import
root (`Component_Attach(entity, state.rootEntity)`), so this needs no new
Undo/Redo machinery: it is already inside the same subtree the existing
`Entity_Serialize(..., RECURSIVE)` snapshot and `Entity_Remove(...,
recursive = true)` capture and restore.

This only starts playback; there is no pause, seek, scrub, or blend control
anywhere in Studio yet, and it does not touch animations authored any other
way.

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
6. Confirm the Import Scale panel appears automatically right after
   placement, its readout matches the status bar's `AUTO SCALE` value, and
   it is positioned fully on-screen over the viewport (not clipped or
   overlapping the toolbar/Inspector) at a few different window sizes.
7. Pick each of Original/Meters, Centimeters, and Inches in turn and press
   APPLY; confirm the imported root's Scale X/Y/Z updates to the expected
   literal value each time (x1.0, x0.01, x0.0254) and the panel's readout
   updates to match. Confirm CLOSE dismisses the panel with no change.
8. Confirm each manual APPLY is its own Undo step (undoing it returns to
   the previously applied scale, not removing the whole imported model),
   and Redo restores the manually applied scale.
9. Undo the import itself while the Import Scale panel is still open (or
   open a different scene); confirm APPLY no-ops safely and does not throw,
   crash, or resurrect the removed entity.
10. Confirm Undo removes the entire imported hierarchy in one step (not
    node-by-node), Redo restores it complete — including the same applied
    Scale — and the restored entity is still selectable and inspectable.
11. Confirm the windmill's blades (or the equivalent moving part on
    whichever animated GLB is used) visibly rotate/animate immediately on
    placement, with no extra click needed. Confirm Undo stops the animation
    along with the rest of the removed hierarchy, and Redo resumes it.
12. Edit a transform on the imported root, then Save, close, and Reopen.
    Confirm the imported model, its applied scale, its playing animation,
    and the edit all survive.
13. Launch Runtime with the saved scene and confirm the model renders,
    scales, and animates there the same as in Studio.
14. Repeat the complete check with the `vulkan` argument.
15. Attempt an import while a `BUILD > VALIDATE GLB/GLTF IMPORT...` proof or
    another `ADD > IMPORT MODEL...` is already running; confirm the busy
    message appears and no partial/duplicate entity is created.
16. Attempt an import of a malformed or unsupported file; confirm a
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
