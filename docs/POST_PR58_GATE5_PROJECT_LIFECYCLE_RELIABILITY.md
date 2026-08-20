# Post-PR58 Gate 5 — Create / Open / Recent / Switch Reliability

## Baseline

Gate 5 starts from merged `main` commit `647df608670dff42099373ab130cc2b3f33456d4`, which contains accepted Gate 4 visual project cards and the merged governed-texture restore optimisation.

The accepted Hub presentation is locked for this gate. Gate 5 changes lifecycle behaviour underneath it; it is not a Hub redesign.

## Problem being closed

Before Gate 5, Studio called `ProjectService::OpenProject()` / `CreateProject()` before proving that the target startup WISCENE could actually be adopted. Project identity and Recent Projects could therefore move to the candidate project while the old scene was still active if startup-scene loading failed.

That is a project/scene split-brain condition.

## Gate 5 transaction rule

Studio project adoption is now staged:

1. validate/create the candidate project in isolation;
2. retain the currently authoritative project and Recent Projects unchanged;
3. resolve the candidate startup scene path;
4. prepare/deserialize the candidate WISCENE without replacing the active scene;
5. only after the WISCENE is ready, commit the project identity and Recent Projects;
6. commit the already-prepared scene at the same adoption boundary.

A failed project descriptor, missing project folder, missing startup scene, malformed startup WISCENE, or cancelled file dialog must leave the current project and scene context untouched.

## Automated proof

`RenegadeProjectStructureTests` now also proves the Studio staging contract:

- Project A can be staged without becoming current or entering recents;
- committing Project A makes it authoritative;
- staging Project B leaves Project A and recents untouched;
- discarding Project B leaves Project A untouched;
- a Project B descriptor whose startup scene is missing fails without changing Project A or recents;
- repaired Project B can then stage and commit and becomes the newest recent project;
- Create Project follows the same staged-adoption rule.

## Owner Release acceptance

Use two real projects with visually distinguishable scenes/assets.

### A. Repeated switching

1. Open Project A from the Hub.
2. Return to Hub.
3. Open Project B from its Recent Project card.
4. Return to Hub.
5. Open Project A again.
6. Repeat A → B → A at least once more.

Pass conditions:

- the correct scene is visible after every switch;
- the Hub current-project identity matches the scene that is actually active;
- Recent Projects selection/order remains coherent;
- no stale scene from the previous project remains active.

### B. Project-scoped Asset Browser isolation

Project A and Project B should contain at least one clearly different asset/folder.

After each A/B switch, inspect the Asset Browser immediately.

Pass conditions:

- it shows the newly active project's Content tree/items;
- it does not continue presenting the previous project's assets;
- no manual refresh or unrelated action should be required to correct it.

This is an explicit Gate 5 acceptance boundary. If stale Asset Browser state is observed, Gate 5 is not accepted and the refresh boundary must be fixed before merge.

### C. Missing / moved project

Move or rename a disposable recent project folder (or its `.renegade` descriptor), then select it from Recent Projects.

Pass conditions:

- Studio remains responsive;
- the existing active project/scene remains authoritative;
- the unavailable recent entry is reported safely rather than crashing or silently changing context.

### D. Missing / malformed startup scene

Using a disposable project, temporarily remove its declared startup WISCENE or replace it with an invalid file, then attempt to open it while another valid project is active.

Pass conditions:

- the open fails;
- the old project identity remains current;
- the old scene remains active;
- the failed candidate is not promoted into Recent Projects as though it successfully opened.

### E. Cancel paths

Start Open Project and cancel the file picker. Start Create Project and cancel before creation.

Pass conditions:

- current project, scene and recents are unchanged;
- no editor work is discarded.

### F. First-open NEW PROJECT controls

Owner testing found that the first NEW PROJECT modal display could paint the authored Hub chrome over the native project-name, CREATE PROJECT and CANCEL controls. The controls existed and were clickable; interacting with them promoted their Wicked GUI priority and made them appear, so subsequent modal opens looked correct.

The repair changes only top-level widget registration order. Wicked renders these widgets back-to-front, so the Hub chrome is now registered after the three modal controls and therefore renders behind them from their first visible frame.

Pass conditions from a completely fresh Studio launch:

- the first click of NEW PROJECT immediately shows the project-name input;
- CREATE PROJECT is immediately visible;
- CANCEL is immediately visible;
- no hover, click in empty space, second modal open or other interaction is required to make those controls paint.

## Locked exclusions

This gate does not add the requested existing-project loading progress bar. That is a later dedicated loading/progress gate so progress can be driven by real lifecycle phases rather than a fake timer.

This gate also does not perform the final legacy governed-texture filename cleanup. That remains the additional cleanup gate requested at the end of this section.

## Merge rule

Do not merge until:

- exact-head Renegade Studio Debug and Release are green;
- exact-head Windows baseline Debug and Release are green;
- owner Release testing passes A–F above;
- the accepted Gate 2 startup flow, native Hub, Gate 4 project cards/artwork and PR #57 reusable-asset behaviour have not regressed.
