# Story Flow Gate 5 — Owner Audit

Gate 5 gives Screen nodes a governed project-content lifecycle. It does **not**
include the visual Screen Editor; that remains Gate 8. The Gate 5 open action
therefore proves the stable Screen Editor handoff boundary rather than switching
to an editor that does not exist yet.

## Build to test

Use the exact **Release** artifact from the final Gate 5 integrated validation
checkpoint on PR #80. Do not use an older Gate 5A artifact.

## Owner path

1. Launch Renegade Studio and open a project with a valid startup Story Flow.
2. Confirm Story Flow opens normally and the existing Level lifecycle controls
   still work.
3. In the Screen lifecycle row, enter `Gate 5 Title`.
4. Select `TITLE / MAIN MENU` as the Screen template.
5. Click `+ SCREEN`.
6. Confirm a new **Screen** node named `Gate 5 Title` appears in Story Flow and
   is selected.
7. Save Story Flow normally.
8. Leave/reopen the project and confirm the same Screen node still exists.
9. Select `Gate 5 Title` and click `OPEN SCREEN`.
10. Expected Gate 5 behaviour: Story Flow remains active because the visual
    Screen Editor is Gate 8. The open action must resolve successfully and the
    backlog must report that the **Screen Editor handoff is ready** (or that it
    resolved by stable identity after a move). There must be no missing/invalid
    Screen error.
11. Optionally create one additional Screen with a different template such as
    `OPTIONS`, `LOADING`, `FAILURE / DEATH`, or `VICTORY` and confirm it creates
    as another normal Screen node rather than a separate document architecture.

## What CI proves so the owner does not need to

The automated Gate 5 tests are responsible for proving:

- Screen + Flow creation is atomic and rolls back on transaction failure;
- generated Screen documents have stable document identity;
- Title template actions are the real authored outcomes
  `new_game`, `load_game`, `options`, `credits`, `quit`;
- a moved Screen resolves by stable identity and reports a stale path hint;
- missing and duplicate/ambiguous Screen identities fail closed;
- the Screen Editor handoff carries the stable node ID, stable document ID,
  resolved project path, moved-hint status, and authored action IDs;
- Runtime Screen/Flow tests remain green.

## Pass / fail

**PASS** if the new Screen is created, persists across reopen, can be selected,
and `OPEN SCREEN` produces a successful Screen Editor handoff without disturbing
the existing Level lifecycle.

**FAIL** on any crash, lost Screen after reopen, wrong node type, failed open
resolution, unexpected switch to the Level Editor, or regression in the Gate 4
Level path.
