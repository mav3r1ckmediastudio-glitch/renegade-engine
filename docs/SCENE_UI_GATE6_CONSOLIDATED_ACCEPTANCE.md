# Scene UI Gate 6 — Consolidated Whole-Editor Acceptance

## Baseline

Gate 6 starts from merged Scene UI Gate 5 commit
`1e0470a9e530dd20c42ddf16662c3771aaede825` (PR #105).

Gate 5 exact-head CI was green and the project owner accepted the corrected
packaged Release after verifying Environment, realistic sky, stars, Sun,
Terrain creation and expansion, old-project open, Undo/Redo, dirty state and
save/reopen behaviour. The earlier truncated 205 MiB package and its rejected
heads are not valid baselines.

## Boundary

Gate 6 is one consolidated whole-editor acceptance and hardening pass. It is
not a new feature gate and does not rewrite accepted subsystems. A code change
is allowed only when the consolidated audit or owner Release pass exposes a
specific integration defect.

The source audit must retain all of these established paths:

- Project Hub to Story Flow project-home entry;
- Story Flow to Level Editor and explicit return;
- Story Flow to Screen Editor and guarded return;
- Hierarchy, Inspector, selection, transform gizmos and viewport navigation;
- Asset Browser, governed import, thumbnail reveal and surface-aware placement;
- Scene Undo/Redo, dirty state, Save, Save As and Reopen;
- independent Environment and Terrain workspaces;
- Test Level through an unsaved Runtime snapshot;
- Story Flow Preview through the governed project descriptor; and
- Build Windows Game followed by direct standalone launch.

`RenegadeSceneUiGate6SourceContract` locks the cross-workspace wiring and keeps
the accepted Scene UI Gate 2-5 contracts registered. It complements rather than
replaces behavioural and visual testing.

## Exact-head CI

Only one complete, unsuperseded Gate 6 head counts. It must pass all four
required checks:

1. `Renegade Studio Windows x64 Debug`;
2. `Renegade Studio Windows x64 Release`;
3. `Windows x64 Debug`; and
4. `Windows x64 Release`.

Superseded runs do not prove the final candidate. CI proves compilation,
automated tests and package construction; it does not prove appearance,
hitboxes, interaction, persistence or Runtime parity.

## One-Release owner sequence

Use one exact-head packaged Release. Do not request separate builds for the
three resolutions; resize the same running Studio instance.

1. Open a Story Flow-native project from Project Hub.
2. Open a governed Level, return to Story Flow and reopen the same Level.
3. Open a governed Screen, exercise selection and one reversible edit, then
   verify dirty-state protection, Save and Return to Story Flow.
4. In the Level Editor, exercise Hierarchy selection, translate/rotate/scale,
   Undo/Redo, camera navigation and viewport selection.
5. Import or select one governed model, verify its Asset Browser thumbnail,
   place it on an object or Terrain surface, then Undo and Redo placement.
6. Exercise Environment at midday and midnight, including realistic sky,
   clouds, stars and the named Sun. Exercise rain, snow and Ocean where present.
7. Create or open Terrain, sculpt across a chunk seam, expand by one ring,
   Undo/Redo the expansion and confirm the prior sculpt remains unchanged.
8. Save the Level, return to Story Flow, reopen it and confirm the placed asset,
   Environment and Terrain state persist. Open one pre-Gate5 terrain project as
   a regression check.
9. Run Test Level and confirm the separate Runtime reaches ready state from the
   unsaved snapshot without changing authoritative Scene dirty state.
10. Run Story Flow Preview and traverse a representative Screen/Level route.
11. Run **BUILD > BUILD WINDOWS GAME...**, then launch the promoted executable
    directly and confirm the same representative route reaches its Level.
12. Resize the same Studio build to `1280x720`, `1680x945` and `1920x1080`.
    Confirm that primary controls remain readable, reachable and non-overlapping.

Owner Release acceptance overrides green CI. Any crash, black/missing rendered
state, lost authored data, incorrect workspace transition, hidden control,
failed persistence or editor/Runtime disagreement rejects the candidate.

## Explicit exclusions

Gate 6 does not absorb a new product programme. The following remain later
editor/world polish unless they expose a regression in an already accepted path:

- Horizon visibility, terrain/grid datum alignment and +/- terrain-ring UI;
- terrain streaming/residency or terrain beyond the accepted finite limit;
- grid/snapping redesign;
- new materials, gameplay, physics, animation or multiplayer systems;
- broad Studio, Story Flow or Screen Editor redesign; and
- distribution/signing/installer work.

## Completion

Gate 6 completes only when:

- the source/integration audit is clean;
- exact-head four-way CI is green;
- the single packaged Release passes the full owner sequence at all three
  resolutions; and
- the project owner gives explicit final acceptance.

Until then the Gate 6 pull request remains a candidate and must not merge.
