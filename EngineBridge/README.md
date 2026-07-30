# EngineBridge

UI-independent services and adapters around Wicked Engine. This layer protects
Studio and Runtime workflows from upstream implementation changes.

## Current services

The implementation provides:

- `SceneService`, which owns a Renegade scene, loads a known WISCENE into a
  temporary scene, validates that it contains entities, and only then replaces
  the active scene. It also generates the first Proving Ground fixture.
- `SelectionService`, which owns editor selection independently of any UI.
- `CommandService`, which owns undoable editor mutations.
- `ProjectService`, which creates and validates v1 `.renegade` descriptors and
  persists the ordered recent-project list.
- `StudioSession`, which coordinates scene replacement and selection reset.

These are deliberately bounded service interfaces. Studio widgets must use
them instead of creating UI-owned project or scene state.
