# Phase 2 Editor Interaction Proof

## Outcome

This bounded increment extends the green Studio shell without selecting the
production UI toolkit:

- `RenegadeRuntime` is a standalone player target with no Studio dependency.
- `SceneService::ListEntities()` exposes a UI-independent hierarchy projection.
- The diagnostic Studio UI lists the hierarchy and binds selection through
  `SelectionService`.
- Translation edits execute through `SetTranslationCommand`.
- The pinned Wicked translation gizmo is hosted by Studio, while completed
  drags are committed through the same Renegade command boundary.
- `CommandService` rejects microscopic no-op translations instead of adding
  them to editor history.
- `RenegadeBridgeTests` validates hierarchy order, selection, execute, ten
  ordered undo operations, ten ordered redo operations, history counts, and
  microscopic-change filtering.
- `SceneService` owns WISCENE Save As and reopen operations.

The temporary `wiGUI` controls remain proof UI. They do not close ADR 0002.

## Automated acceptance

- Windows x64 Debug and Release build `RenegadeStudio`, `RenegadeRuntime`, and
  `RenegadeBridgeTests`.
- CTest reports `RenegadeBridgeTests` passing.
- Studio and Runtime packages each contain the executable, `dxcompiler.dll`,
  and `Content/cube.wiscene`.
- Studio owns no duplicate scene model.
- Runtime links EngineBridge but no Studio source.
- Ten distinct transform commands undo and redo in exact order.
- A microscopic translation does not increase the Undo count.

## Human-observed acceptance

- Studio lists the fixture scene hierarchy.
- Clicking a hierarchy entry updates the selected transform inspector.
- Editing X, Y, or Z moves the selected entity.
- Dragging the viewport translation gizmo moves the selected entity.
- Ten visible edits can be undone and redone in exact order.
- Clicking a gizmo without moving it does not add an Undo entry.
- Clicking UI controls does not also manipulate the viewport gizmo.
- Save As writes a WISCENE; Reopen reloads it with the edited transform.
- Runtime opens and renders the fixture without editor controls.

The published `b4da74a` artifact passed launch, hierarchy, selection, render,
gizmo-drag, ten-edit, ten-Undo, and ten-Redo inspection. Save As crashed
because Renegade explicitly closed its scene archive before the `Archive`
destructor closed it again. Correction
`be5167b087789109a2bb1f98305b8ffa18b454ed` removes the explicit close and
matches the pinned Wicked Editor save lifecycle.

The published `db6b6ca` artifact then passed Save As, repeated Save As, Reopen,
and standalone Runtime inspection on the project owner's Windows GPU. The
submitted WISCENE decompressed successfully and retained a valid scene payload.
The interaction and persistence proof is accepted.

## Deliberately deferred

- Production docking and visual design.
- Renegade UI/UX design and production panels.
- Physical HDR, Vulkan, and mixed-DPI/multi-monitor gates.
