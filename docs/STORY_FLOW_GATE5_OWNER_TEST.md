# Story Flow Gate 5 — Owner Audit

Gate 5 gives Screen nodes a governed project-content lifecycle. It does **not**
include the visual Screen Editor; that remains Gate 8. The Gate 5 open action
therefore proves the stable Screen Editor handoff boundary rather than switching
to an editor that does not exist yet.

## Build to test

Use the exact **Release** artifact from the final PR #84 recovery head. Confirm
this file exists inside that artifact under `Docs/`; a package without it is not
an owner-test candidate.

Do not use the failed `d184f5478fecb4f1bf6a041fed858d88a522d5cd`
artifact or an older Gate 5A build.

## Test 1 — New Project opens on Story Flow

1. Launch Renegade Studio.
2. Create a new project named `Gate 5 Recovery` in a disposable parent folder.
3. **Expected:** after creation, the dedicated Story Flow workspace opens.
   The 3D Level Editor must not be the active project home.
4. Confirm the Flow contains exactly one permanent `Game Start` and one adopted
   `Main Level` representing the generated startup WISCENE.
5. Confirm `Game Start` is routed to `Main Level`.
6. Confirm both lifecycle rows are visibly present across the Story Flow header:
   `LEVEL` with `+ NEW LEVEL` / `+ EXISTING...` / `OPEN LEVEL`, and `SCREEN`
   with its template chooser / `+ SCREEN` / `OPEN SCREEN`. Missing or obscured
   lifecycle controls are an immediate failure; stop the audit.

## Test 2 — Recent Project returns to Story Flow

1. From Story Flow, open `PROJECTS`.
2. Select `Gate 5 Recovery` from Recent Projects and open it.
3. **Expected:** the same Story Flow reopens. It must not land directly in the
   3D Level Editor and must not create duplicate Game Start or Main Level nodes.
4. Close Studio, relaunch it, and open the same Recent Project once more.
5. Confirm the same Story Flow opens without duplicated nodes or a migration
   error in the backlog.
6. Open a disposable copy of the existing project used against the rejected
   build. Whether that copy is still scene-first or was partly migrated by the
   rejected build, confirm it now opens on Story Flow with no duplicate nodes.

## Test 3 — Screen lifecycle

1. In the Screen lifecycle row, enter `Gate 5 Title`.
2. Select `TITLE / MAIN MENU` as the Screen template.
3. Click `+ SCREEN`.
4. Confirm a new **Screen** node named `Gate 5 Title` appears in Story Flow.
5. Save Story Flow normally.
6. Leave/reopen the project and confirm the same Screen node still exists.
7. Select the `Gate 5 Title` Screen node and click `OPEN SCREEN`.
8. Expected Gate 5 behaviour: Story Flow remains active because the visual
   Screen Editor is Gate 8. The open action must resolve successfully and the
   backlog must report that the **Screen Editor handoff is ready** (or that it
   resolved by stable identity after a move). There must be no missing/invalid
   Screen error.
9. Optionally create one additional Screen with a different template such as
   `OPTIONS`, `LOADING`, `FAILURE / DEATH`, or `VICTORY` and confirm it creates
   as another normal Screen node rather than a separate document architecture.

## Test 4 — Gate 4 Level lifecycle regression

1. From Story Flow, enter `Gate 4 Regression` in the Level field.
2. Click `+ NEW LEVEL`, select the created Level node and click `OPEN LEVEL`.
3. Confirm Renegade switches to the normal 3D Level Editor only after that
   explicit open action.
4. Click `< STORY FLOW` and confirm the same Story Flow returns.
5. Confirm the Level node and the Screen node both remain present.

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
- New Project, Open Project and Recent Project project-home migration is
  idempotent and refreshes active Studio metadata rather than leaving it staged;
- Runtime Screen/Flow tests remain green.

## Pass / fail

**PASS** only if New Project and Recent Project both open on Story Flow, the new
Screen persists and opens its handoff, and the Gate 4 Level open/return path
still works.

**FAIL** on any crash, lost Screen after reopen, wrong node type, failed open
resolution, unexpected switch to the Level Editor, or regression in the Gate 4
Level path.
