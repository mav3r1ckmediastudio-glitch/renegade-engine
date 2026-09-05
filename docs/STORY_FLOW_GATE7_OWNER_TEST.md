# Story Flow Gate 7 — Packaged Release Owner Test

Use only the exact Release ZIP produced for the Gate 7 commit under review.
Record the artifact name, SHA-256 and commit before starting. A green CI run is
not owner acceptance.

## 1. Native new-project entry

1. Launch `Run-RenegadeStudio-DX12.cmd`.
2. Create a project named `Gate 7 Native` in an empty parent folder.
3. Confirm the Project Hub closes and the dedicated Story Flow Journey surface
   is the first project workspace shown.
4. Confirm only the permanent `Game Start` card exists initially. There must be
   no seeded `Main Level` card and no flash of a blank 3D Level Editor.
5. Confirm the project folder contains
   `Content/StoryFlow/Main.renegade-flow` and does not contain
   `Content/Scenes/Main.wiscene`.

## 2. Ordinary authoring remains functional

1. Create a Level named `Gate 7 Level`.
2. Connect Game Start to that Level using the existing Journey authoring
   controls.
3. Save Story Flow.
4. Double-click `Gate 7 Level`; confirm the governed Level Editor opens.
5. Return to Story Flow and confirm the same Journey is still present.

## 3. Close/reopen project home

1. Return to the Project Hub and reopen `Gate 7 Native` from Recent Projects.
2. Confirm Story Flow opens directly with the saved Game Start, Level and route.
3. Confirm the Project Hub details identify
   `Content/StoryFlow/Main.renegade-flow` as `PROJECT HOME`; they must not claim
   a placeholder startup Scene exists.

## 4. Regression boundary

1. Open a known scene-first project created before Gate 7.
2. Confirm it still migrates/opens through Story Flow and retains its existing
   startup Scene as a governed Level.
3. Reopen `Gate 7 Native` and confirm the scene-first project's Scene does not
   remain visible behind or inside the native project.

## Acceptance record

Record PASS/FAIL for each section and attach screenshots of:

- the first native Story Flow surface;
- the project folder showing Flow present and `Main.wiscene` absent;
- the reopened saved Journey;
- Project Hub `PROJECT HOME` details.

Stop on the first failure. Do not merge without the project owner's explicit
decision on the exact tested commit and Release artifact.
