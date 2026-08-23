# Story Flow Gate 8E — Packaged Owner Audit

This Release is the owner-acceptance candidate for Gate 8E Screen action / Story
Flow outcome parity. Green CI is necessary but is not owner acceptance.

## What Gate 8E proves

- Screen Editor owns symbolic Button action IDs only.
- Story Flow owns destination nodes, entries, conditions and priority.
- Screen-origin routes must use a currently authored Screen action.
- Renaming/deleting a Screen action never silently rewrites Story Flow.
- Story Flow diagnoses stale/unrouted Screen actions after Screen Editor return,
  Flow edits and reopen.
- Runtime and packaged standalone fail closed on a reachable broken mapping and
  execute the same repaired mapping when valid.

## Owner test

1. Launch `Run-RenegadeStudio-DX12.cmd`.
2. Create/open a project and enter Story Flow.
3. Use a Screen with at least two Button actions. Give those Button actions
   valid Story Flow destinations and save the Flow.
4. Open the Screen Editor. Rename one Button-used symbolic action, for example
   `options` -> `settings`, and Save the Screen.
5. Return to Story Flow.
   - The affected Screen must be selected/focused.
   - The status must report the old route outcome as no longer authored.
   - The renamed Button action must be reported as having no Story Flow
     destination.
6. Select the stale route and try to apply the old action ID. Story Flow must
   reject it because it is not an authored Screen action.
7. Change that route outcome to the renamed action ID and apply it. When every
   Button-used action is routed, the status must report synchronized Screen
   outcomes / Story Flow routing.
8. Save. Return to the Hub or close Studio normally, reopen the same project,
   and confirm the repaired route remains synchronized.
9. Run the project using Renegade's existing Runtime/test workflow. Activate the
   repaired Button and confirm it reaches the destination assigned in Story
   Flow.
10. Build the project using Renegade's existing Windows Game build workflow.
    Launch the packaged standalone, activate the same Button and confirm it
    reaches the same destination.
11. Confirm the Screen Editor never asks for or stores a Flow destination/path;
    only the symbolic action ID is authored there.
12. Confirm no stock Wicked Editor window appears.

## Rejection conditions

Gate 8E fails if any of these occur:

- a Screen route accepts an arbitrary action that the Screen does not author;
- a Screen action rename silently changes a Story Flow destination;
- return/reopen fails to diagnose a stale or unrouted Button action;
- a reachable broken Screen/Flow mapping starts successfully in Runtime;
- the repaired action reaches a different destination in packaged standalone;
- Screen Editor begins owning Story Flow destinations.

Gate 8E is accepted only when the exact packaged Release passes this audit.
