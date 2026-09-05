# Phase 3 Editor Usability Milestone

## Outcome

Implementation commits `cb04cf4` and `547906a` turn the initial interactive
viewport into a practical scene-editing surface. The milestone adds:

- creator-facing hierarchy filtering for generated grid internals;
- position, rotation-in-degrees, and scale fields in the Inspector;
- Move, Rotate, and Scale gizmo modes;
- focus-on-selection;
- duplicate and delete;
- undo and redo for complete transforms, duplication, and deletion;
- direct Save alongside Save As and Reopen;
- Open Scene from both the Project Hub and Studio File menu, with a native
  `.wiscene` picker, Save/Discard/Cancel protection, and the opened filename
  shown in the scene tab;
- keyboard shortcuts for the common editing actions; and
- a compact FPS readout confined to the viewport instead of diagnostic text
  overlapping the command bar.

Wicked source and the pinned submodule remain unchanged.

## Architecture

Studio still owns presentation and input routing, while `EngineBridge` owns
scene mutations:

- `TransformState` captures local position, quaternion rotation, and scale.
- `SetTransformCommand` applies and restores a complete local transform.
- `DuplicateEntityCommand` snapshots a recursive Wicked entity archive so the
  same entity IDs can be removed and restored through Undo/Redo.
- `DeleteEntityCommand` snapshots the entity recursively before removal and
  restores it without remapping on Undo.
- `SceneService` owns the convention that generated entities whose names begin
  with `__renegade_internal_` do not appear in the creator hierarchy.

The UI does not retain a second scene model. Persistent scene edits are
committed through `CommandService`; the gizmo's live preview is restored to
its before-state before the completed transform command is executed.
Bridge commands do not call renderer-dependent `Scene::Update()`. The
render-capable Studio frame loop owns scene advancement, while headless bridge
tests use lightweight ECS fixtures that require no graphics device.

## Controls

| Input | Result |
|---|---|
| W | Move tool |
| E | Rotate tool |
| R | Scale tool |
| F | Frame selected entity |
| Ctrl+D | Duplicate selected entity |
| Delete | Delete selected entity |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+S | Save current scene |
| Ctrl+Shift+S | Save scene as |

W/A/S/D continue to control the fly camera while the right mouse button is
held. Tool switching is suppressed during fly navigation and text entry.

## Windows acceptance

1. Build `RenegadeStudio` and `RenegadeBridgeTests` in Windows x64 Debug and
   Release.
2. Run `RenegadeBridgeTests` and confirm the full-transform,
   duplicate/delete Undo/Redo, hierarchy-filtering, and project tests pass.
3. Launch the packaged DX12 Studio and create or open a project.
4. Confirm generated `Grid X` and `Grid Z` entries no longer flood the
   hierarchy.
5. Confirm the command bar is unobstructed and only a compact FPS readout
   appears inside the viewport.
6. Select a visible mesh and edit all nine Position, Rotation, and Scale
   fields. Confirm the object, gizmo, and fields remain synchronised.
7. Use W, E, and R and confirm the visible gizmo changes between Move, Rotate,
   and Scale.
8. Use F on a small prop, the core, and the ground. Confirm each is framed
   without changing its transform.
9. Use Ctrl+D and the Duplicate button. Confirm the copy appears in the world
   and hierarchy, is selected, and is named with a `Copy` suffix.
10. Undo and redo duplication and confirm the hierarchy and rendered object
    match the history state.
11. Delete a copied entity, then undo and redo deletion.
12. Undo and redo a position, rotation, and scale edit.
13. Use Ctrl+S, close Studio, reopen the project, and confirm the saved
    transform persists.
14. Confirm shortcuts do not trigger while typing in an Inspector or project
    name field.
15. Repeat the visual transform, focus, duplicate, delete, and history checks
    through the Vulkan launcher.

Required report:

```text
DX12 EDITING PASS / HIERARCHY PASS / TRANSFORM PASS / FOCUS PASS /
DUPLICATE-DELETE PASS / HISTORY PASS / SAVE PASS / SHORTCUTS PASS /
VULKAN EDITING PASS
```

Visual or behavioural failure overrides green CI.
