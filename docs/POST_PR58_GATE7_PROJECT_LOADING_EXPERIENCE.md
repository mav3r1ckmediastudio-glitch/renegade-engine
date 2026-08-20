# Post-PR58 Gate 7 — Existing Project Loading Experience

## Baseline

Gate 7 starts from merged `main` commit `029a2d878acf7c9f8cd6fe88fb222c5d5c5fa114`, containing accepted Gate 5 transactional project switching and Gate 6 Save / Discard / Cancel protection.

The accepted Project Hub, startup sequence, Recent Projects cards, project persistence, reusable-asset behavior and save-safety semantics are locked for this gate.

## Purpose

Opening a populated existing project can take long enough that a static Hub makes Studio look hung. Gate 7 turns that wait into a Renegade-owned loading experience driven by the actual project-open pipeline rather than a timer.

New Project is intentionally excluded because its accepted path is effectively immediate and must not be slowed down for theatre.

## Real pipeline

Existing-project opening now follows these visible phases:

1. **READING THE FINE PRINT...** — stage and validate the `.renegade` project without changing the authoritative project.
2. **ASSEMBLING REALITY...** — deserialize the startup WISCENE into a detached prepared scene on Wicked's job system.
3. **PUTTING THE PAINT BACK ON... / ROUNDING UP YOUR ASSETS... / CONVINCING THE MATERIALS TO COOPERATE... / LOCATING THOSE TEXTURES YOU SWEAR YOU PACKAGED...** — restore governed material resources against that detached candidate. The progress callback reports real unique governed-resource preparation counts.
4. **ARGUING WITH THE LAST FEW BYTES...** — commit project identity and the already-prepared scene at the Gate 5 adoption boundary, then refresh project-scoped editor state.
5. **READY. GO BREAK SOMETHING.** — 100% state briefly confirms success before the overlay automatically reveals the editor.

The percentage is stage-weighted pipeline completion. It is not a prediction of elapsed time. Opaque Wicked operations hold at their real stage while an activity scanner remains animated; Renegade does not fabricate intermediate percentages.

## Responsiveness rule

Startup-scene preparation and governed resource restoration execute against the detached candidate scene on `wi::jobsystem`. The active editor scene is not mutated by that worker. UI rendering and Windows message pumping therefore remain on the normal Studio thread while expensive candidate work proceeds.

The overlay's worker-facing phase/count updates are atomic only. UI strings and failure presentation are changed on the Studio thread.

## Transaction rule

Gate 5 remains authoritative:

- a candidate project is staged first;
- the current project/scene remains authoritative during background preparation;
- only a prepared scene reaches `CommitPendingProjectScene()`;
- failure discards the pending candidate and leaves the previous project intact;
- governed-resource restoration warnings do not silently substitute a different project or bypass the project transaction.

## Failure UX

A failed descriptor or startup scene leaves the loading overlay in a deliberate failure state:

**WELL. THAT WASN'T SUPPOSED TO HAPPEN.**

The creator receives the real diagnostic and a **RETURN TO HUB** action. There is no automatic adoption of a partially prepared project.

## Owner Release acceptance

### A. Existing populated project

1. Start Studio normally and enter the Hub.
2. Open the populated project previously measured at roughly the mid-teens of seconds.
3. Observe the loader from first click until editor handoff.

Pass conditions:

- loader appears immediately after Save/Discard/Cancel handling, if required;
- loading messages correspond to actual pipeline stages;
- governed-resource phase shows real counts when governed bindings exist;
- activity continues while a stage is opaque;
- Studio does not present a frozen Hub as the loading state;
- success reaches **READY. GO BREAK SOMETHING.** and automatically enters the correct editor project;
- no extra Enter Project button is required.

### B. Project A → B → A

Switch between two valid existing projects through Recent Projects.

Pass conditions:

- each switch uses the same loading presentation;
- the old project remains authoritative until the new one is prepared;
- the correct project, scene and Asset Browser are visible after each automatic handoff;
- Gate 5 isolation remains intact.

### C. Gate 6 dirty-state preservation

Make Project A dirty, then attempt to open Project B.

Pass conditions:

- Save / Discard / Cancel appears before Gate 7 starts;
- Cancel means no loader and no switch;
- Save completes before loading begins;
- Discard proceeds into the loader without saving;
- no Gate 6 behavior is bypassed.

### D. Failure rollback

Attempt to open a disposable project with a missing or malformed startup scene while a valid project is active.

Pass conditions:

- loading presentation reports failure rather than revealing a half-loaded editor;
- **RETURN TO HUB** works;
- previous project/scene authority remains intact;
- failed project is not promoted as successfully opened.

### E. New Project regression

Create a new project.

Pass conditions:

- the accepted immediate New Project path remains immediate;
- Gate 7 does not insert a cosmetic loading delay.

## Exclusions

Gate 7 does not attempt further load-time optimisation beyond moving safe candidate work off the UI thread. It does not clean legacy governed texture filename probes; that remains the final cleanup gate requested for this section.

## Merge rule

Do not merge until:

- exact-head Renegade Studio Debug and Release are green;
- exact-head Windows baseline Debug and Release are green;
- owner Release acceptance passes A-E;
- the accepted Hub/startup/lifecycle/save-safety behavior has not regressed.
