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

The published `11678cd` artifact passed launch, hierarchy, selection, render,
and gizmo-drag inspection but failed repeated-history inspection. Correction
`9804e63ab390ef748e2097b97e10c84646353662` is prepared and requires Windows
CI plus a new GPU visual check.

## Deliberately deferred

- Production docking and visual design.
- UI toolkit selection.
- HDR, Vulkan, and multi-monitor gates.
