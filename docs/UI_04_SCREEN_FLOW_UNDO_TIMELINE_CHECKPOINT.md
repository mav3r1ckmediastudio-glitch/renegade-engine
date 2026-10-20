# UI-04 Screen Editor chronology and saved routing checkpoint

Date: 16 October 2026. Local branch `feature/p3-original-impact-vfx`, baseline HEAD `60634ce`. Shared worktree is dirty; this UI work is local, uncommitted and not pushed.

## Owner goal
Continue implementing the approved native UI Designer (visual direction Concept 03); do not hand off an incomplete interface for owner testing.

## Change
- `Studio/src/ScreenEditorUndoTimeline.h`: a **presentation-only chronological dispatcher**, not a new document/history authority. It tracks Screen history entry counts from `ScreenAuthoringSession` and records destination route actions from the existing `StoryFlowAuthoringSession`. A single Ctrl+Z, Ctrl+Y, Ctrl+Shift+Z or top-bar Undo/Redo dispatches to the most recent Screen or Flow-route action, even when interleaved.
- `Studio/src/RenegadeScreenEditorWorkspace.h/.cpp`: records new Screen edits as they enter the existing history (including coalesced direct canvas manipulation), records only actual new route changes (not idempotent destination picks), and delegates route history changes through an injected callback. Existing Screen editing, Inspector, Layers, and keyboard guards remain.
- `Studio/src/RenegadeScreenEditorRenderPath.h`: delegates the narrow history callback; no separate Flow authoring stack is instantiated.
- `Studio/src/StoryFlowStudioIntegration.h`: validates the expected StoryFlow session/Screen identity and Undo/Redo counts before applying route history changes, and refreshes Journey/ImNodes Graph from the one authoritative Flow session. If presentation refresh fails, reverses the attempted history operation and reports the failure. New route mutations record expected Undo/Redo counts only on successful edit. No transition to the Graph is required for simple native destination authoring.
- `Tests/ScreenAuthoringSessionTests.cpp`: new mixed Screen-Flow-Screen Undo/Redo order test, new mutation after Undo truncation and out-of-band history reset protections.
- `Tests/StoryFlowGate8EOutcomeParityTests.cpp`: new real `StoryFlowAuthoringSession` Add/Update history/Undo/Redo and Save/Reopen roundtrip assertions on an isolated Flow fixture. The project Screen action is not changed or saved by route editing.

## Verification
- Rebuilt Release `RenegadeScreenAuthoringSessionTests` and `RenegadeStoryFlowGate8EOutcomeParityTests` targets: **PASS**.
- Both rebuilt Release CTests **PASS 2/2** (0.94 s).
- Scoped `git diff --check`: **PASS**.
- Full linked Studio Release build with new native UI: **PASS** (exit 0, 319.85 s), log `BUILD/ui04-combined-history-studio-build.log`. Output: `BUILD/renegade/Studio/Release/RenegadeStudio.exe`.
- Final six focused Release Screen/Flow regression suites **PASS 6/6** (1.20 s), including newly rebuilt authored Screen and StoryFlow parity tests.
- Scoped `git diff --check`: **PASS** after all changes.

## Important limits, not yet accepted
- Screen actions and Flow routes remain in **separate authoritative documents and persisted histories**. Chronological Undo is a Studio gesture dispatcher, not a cross-document disk transaction and not a universal history spanning all StoryFlow authoring surfaces.
- `ScreenAuthoringSession::Save()` persists Screen only; Flow routes are saved through StoryFlow. UI-024's atomic *two-document save* gate remains open where both document contents need to change simultaneously.
- The coordinator fails closed on out-of-band Flow session history changes; a deliberately conservative recovery is return to StoryFlow, not silently rewrite someone else's graph.
- No mouse/keyboard live observation or linked Release verified yet at checkpoint creation. Only claim after build and isolated visual test.
- The wider approved UI-001..029 remains incomplete; UI widget kinds and gameplay bindings must be proven against real Runtime services, not made to appear functional via palette placeholders.
