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
- `CommandService` proves one undo and redo cycle.
- `RenegadeBridgeTests` validates hierarchy order, selection, execute, undo,
  redo, and an on-disk component archive roundtrip without a graphics device.
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
- A compressed Wicked archive roundtrip preserves the edited name and
  transform.

## Human-observed acceptance

- Studio lists the fixture scene hierarchy.
- Clicking a hierarchy entry updates the selected transform inspector.
- Editing X, Y, or Z moves the selected entity.
- Dragging the viewport translation gizmo moves the selected entity.
- Undo restores the previous translation and Redo reapplies it.
- Save As writes a WISCENE; Reopen reloads it with the edited transform.
- Runtime opens and renders the fixture without editor controls.

## Deliberately deferred

- Production docking and visual design.
- UI toolkit selection.
- HDR, Vulkan, and multi-monitor gates.
