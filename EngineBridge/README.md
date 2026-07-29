# EngineBridge

UI-independent services and adapters around Wicked Engine. This layer protects
Studio and Runtime workflows from upstream implementation changes.

## Phase 2 increment

The first implementation provides:

- `SceneService`, which owns a Renegade scene, loads a known WISCENE into a
  temporary scene, validates that it contains entities, and only then replaces
  the active scene.
- `SelectionService`, which owns editor selection independently of any UI.
- `StudioSession`, which coordinates scene replacement and selection reset.

These are deliberately small service boundaries. Studio code must use them
instead of creating a second, UI-owned scene state.
