# UI-07 — automatic Screen saves with Undo/Redo preservation

**Owner correction:** All Screen documents should autosave. Users should never need to remember to press Save when moving to another Screen or returning to StoryFlow. Undo/Redo is the supported way to reverse an edit.

**Scope:** Existing local Renegade Studio UI Designer in branch \`feature/p3-original-impact-vfx\`. No new Runtime widget types, game assets, second Flow graph, CI push or changes to the WickedEngine submodule.

## Implementation

- \`Studio/src/ScreenEditorAutosavePolicy.h\` owns editor-only 1.5-second quiet-period timing, never document contents. It defers writes during manipulation, attempts once after settling, never repeatedly hammers the disk after a failure, re-arms on another edit, and detects dirty StoryFlow-only changes.
- \`Studio/src/RenegadeScreenEditorWorkspace.cpp/.h\`: native Screen Editor autosaves active Screen + StoryFlow via existing \`ScreenFlowAtomicSaveService\`. Status says AUTOSAVE ON / AUTOSAVED. The optional Save button remains Save Now. Both returning to StoryFlow and choosing another Screen invoke immediate coordinated Save; failure retains changes and blocks navigation with an error, plus backlog log.
- \`Studio/src/RenegadeScreenEditorRenderPath.h\`: calls TickAutosave after Basic and Advanced Inspector GUI updates, so edits made in either editing surface are seen. Direct drag/resize stays coalesced as one Undo mutation and is not saved mid-gesture.
- \`EngineBridge\` Screen and StoryFlow authoring sessions now track the last persisted document snapshot **independently from the bounded Undo history index**. Previous implementation refused joint Save when Undo + new edit removed a saved revision from history. The atomic save transaction now validates against the preserved persisted snapshot, and advances it only after both file writes and validation have completed. Direct single-document Save and reload also update their snapshots.
- Existing transaction journal checks external edits; failure does not mark either document saved and does not silently discard dirty authoring data.

## Tests and checks

- Rebuilt \`RenegadeScreenAuthoringSessionTests\`: PASS. Covers dirty idle autosave, no save during drag, quiet timer, one failed write/no hot-loop, edit retry, Flow-only change and reset on Screen switch.
- Rebuilt \`RenegadeStoryFlowGate8EOutcomeParityTests\`: PASS. Covers journaled paired Save, second-file injected write failure / rollback, persistence and Undo/Redo, external file edit protection, and **Undo -> new branch -> coordinated Save** even after the old saved history entry has been pruned.
- Studio Release compile/link and six Screen/Flow CTests: complete verification on this exact source revision before declaring this usable. A linked Release build from previous UI-06 remains available; it predates autosave and must not be mistaken for UI-07.

## Known limitations

- Autosave applies to document edits committed to the Screen authoring session; text still being actively typed into an Inspector field may need that field to commit its value (e.g. Apply/Enter) before it reaches the ScreenDocument. Native GUI field commit is a manual acceptance check.
- An abrupt power loss or hard process termination before the quiet period has elapsed cannot be promised recoverable without a separate crash journal. Immediate navigation via the Screen list or Return forces a save. Stop/close of the entire host application requires a separate lifecycle integration to guarantee flushing every pending edit.
- A failed disk transaction keeps documents dirty and blocks Screen-to-Screen/StoryFlow navigation. Users should resolve the disk issue before terminating Studio.
- Screen EngineBridge and StoryFlow histories remain authoritative; autosave updates saved snapshots without discarding Undo/Redo.

## Manual test

1. In the isolated UI Designer sample project, edit a Text or Button and wait ~2 seconds. Reopen and verify it persisted without pressing Save.
2. Edit, immediately choose a different Screen, and return; verify data persisted.
3. Edit, immediately return to StoryFlow, then reopen; verify data persisted.
4. Use Ctrl+Z to undo a previously autosaved edit; make a different edit and wait. Reopen and confirm the branched version persisted; older Undo operations remain available in the current session.
5. Change a Button's destination only, leave and reopen; verify the StoryFlow Graph route.
6. Simulate a save failure in an isolated disposable project: navigation should not discard unsaved work.

## Final UI-07 verification (16 October 2026)

- Studio linked Release target **PASS**, exit 0 (206.56 seconds), `BUILD/ui07-autosave-final-studio-release.log`. Built executable `BUILD/renegade/Studio/Release/RenegadeStudio.exe` includes UI-07 autosave and persisted snapshot branch repair.
- Six targeted Release Screen/StoryFlow CTest suites **PASS 6/6** (1.95s) with the newly rebuilt autosave policy and divergent saved-history branches.
- Scoped `git diff --check` **PASS**.
- User's Downloads launcher `C:\Users\paulw\Downloads\Renegade_UI_Designer_TEST_LOCAL.cmd` already points to this local Release executable; no new launcher required.
- The live older downloaded CI Studio process was not terminated. Native GUI mouse/keyboard validation remains user acceptance testing rather than a claim of verified GUI behaviour.
