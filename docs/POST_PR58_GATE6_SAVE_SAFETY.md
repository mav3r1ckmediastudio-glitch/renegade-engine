# Post-PR58 Gate 6 — Save Safety / Dirty-State Reliability

## Baseline

Gate 6 starts from merged `main` commit `4448b6f1325118a36a635818b761b7524edd69c8`, which contains the owner-accepted Gate 5 transactional project lifecycle.

The accepted startup sequence, native Hub presentation, project cards/artwork/recents, Gate 5 project adoption semantics, and PR #57 reusable-asset behavior are locked for this gate.

## Problem being closed

Studio already had a Save / Discard / Cancel guard around Open Scene and Reopen Scene. However, several equally destructive transitions bypassed it:

- creating a new project while the current scene was dirty;
- opening another project through the project file picker;
- opening another project from Recent Projects;
- EXIT RENEGADE from the Hub;
- native Windows close via the title-bar X or Alt+F4.

Those paths could replace the active scene context or terminate the process without first resolving unsaved scene work.

## Gate 6 rule

Every operation that can destroy the current scene context must cross the same existing dirty-state boundary.

When the current command state is clean, the requested operation continues immediately.

When the current scene is dirty, Studio asks whether to save changes and supports three outcomes:

- **Save / Yes** — save successfully, then continue;
- **Discard / No** — continue without saving;
- **Cancel** — abort the requested destructive operation and leave the current scene/project authoritative.

If Save As is required and its file picker is cancelled, the destructive operation remains cancelled.

If a save fails, the destructive operation must not continue.

## Implementation boundary

Gate 6 reuses `StudioRenderPath::RequestSceneReplacement()` rather than introducing a second unsaved-work system.

The guard now covers:

- Open Scene;
- Reopen Scene;
- Create Project after a destination folder has actually been selected;
- Open Project after a concrete `.renegade` descriptor has actually been selected;
- Open Selected Recent Project;
- Hub EXIT RENEGADE;
- native Windows `WM_CLOSE` from X / Alt+F4.

The guard's Save path now routes through `SaveSceneAfterTransientCleanup()`, matching normal Studio save behavior instead of directly serializing the scene. This preserves transient-preview cleanup and only runs the destructive continuation after a successful save.

`WM_CLOSE` is now treated as an exit request while the editor is active. The approved continuation destroys the native window directly, preventing recursive `WM_CLOSE` handling.

## Non-destructive Hub transition

Returning from the editor to the Project Hub does **not** itself discard or replace the active scene, so it intentionally does not prompt. Dirty state remains live while the Hub is visible.

From the Hub:

- BACK TO EDITOR returns to the same dirty scene unchanged;
- opening/creating another project crosses the Save / Discard / Cancel guard;
- EXIT RENEGADE crosses the Save / Discard / Cancel guard.

## Owner Release acceptance

Use a disposable copy of a real project and make an unmistakable scene edit before each dirty-state test.

### A. Project switch — Cancel

1. Open Project A and make an edit without saving.
2. Return to Hub.
3. Attempt to open Project B from Recent Projects.
4. Choose **Cancel** at the unsaved-changes prompt.

Pass conditions:

- Project B does not open;
- Project A remains authoritative;
- returning to the editor shows the unsaved edit still present;
- Project A remains dirty.

### B. Project switch — Save

1. Make another unsaved edit in Project A.
2. Open Project B.
3. Choose **Yes / Save**.
4. Allow Project B to open.
5. Reopen Project A.

Pass conditions:

- Project B opens only after the save succeeds;
- the edit made to Project A is present after reopening A;
- no project/scene split-brain occurs.

### C. Project switch — Discard

1. Make a distinct unsaved edit in Project A.
2. Open Project B.
3. Choose **No / Discard**.
4. Reopen Project A.

Pass conditions:

- Project B opens;
- the discarded edit is not present when A is reopened;
- the last successfully saved A state remains intact.

### D. Create Project

1. Make Project A dirty.
2. Enter NEW PROJECT, choose a real destination folder and submit creation.
3. Test Cancel once, then repeat and test Save.

Pass conditions:

- Cancel leaves A and its dirty edit untouched;
- Save completes before the new project becomes authoritative;
- cancelling the folder chooser itself produces no unnecessary unsaved-changes prompt.

### E. Hub Exit

1. Make the current scene dirty and return to Hub.
2. Choose EXIT RENEGADE.
3. Test Cancel.
4. Repeat and test Save.

Pass conditions:

- Cancel leaves Studio running and the dirty scene recoverable through BACK TO EDITOR;
- Save exits only after the save succeeds.

### F. Native Windows close

1. Make the current scene dirty.
2. Click the title-bar X (or use Alt+F4).
3. Test Cancel.
4. Repeat and test Save or Discard.

Pass conditions:

- Cancel prevents the native window from closing;
- Save closes only after a successful save;
- Discard closes without modifying the saved scene;
- no recursive close prompt or double-close behavior occurs.

### G. Hub round-trip preservation

1. Make the scene dirty.
2. Open the Hub.
3. Do not switch project or exit.
4. Use BACK TO EDITOR.

Pass conditions:

- no save prompt is shown merely for viewing the Hub;
- the scene edit is still present;
- dirty state is still active.

## Locked exclusions

This gate does not add the requested existing-project loading/progress UI. That remains a later dedicated loading/progress gate.

This gate does not normalize legacy governed-texture filename strings. That remains the final cleanup gate requested for the end of this section.

## Merge rule

Do not merge until:

- exact-head Renegade Studio Debug and Release are green;
- exact-head Windows baseline Debug and Release are green;
- owner Release acceptance passes A-G above;
- the accepted Gate 2 startup flow, native Hub, Gate 4 project cards/artwork, Gate 5 lifecycle behavior, and PR #57 reusable-asset behavior have not regressed.
