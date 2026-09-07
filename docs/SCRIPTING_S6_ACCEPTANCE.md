# S6 Integrated Owner Acceptance

This checklist is deliberately one acceptance run for S6A-S6F.

## CI boundary

Do not owner-test an artifact unless all four protected checks for this PR are green:

- Windows x64 Debug
- Windows x64 Release
- Renegade Studio Windows x64 Debug
- Renegade Studio Windows x64 Release

## Creator Library discovery

1. Start Renegade Studio from the S6 artifact and open a saved project/Level.
2. Supply an installed script package under `Library/Scripts` beside the Studio working directory, or set `RENEGADE_SCRIPT_LIBRARY` to a package-library root before starting Studio.
3. Select an entity and expand ACTION or SCRIPT, or open GLOBAL SCRIPT for the Level.
4. Press **REFRESH**.
5. Confirm compatible installed entries appear in the existing source chooser under a `LIBRARY / ...` category. No second scripting window should appear.

## First-use adoption

1. Select a Library row and press **ADD**.
2. Confirm the attachment appears through the normal S4 authoring UI.
3. Confirm the project now contains the selected entry and its declared transitive Lua modules under `Content/Scripts/Library/<package-id>/...`.
4. Confirm `Content/Scripts/.renegade/library-lock.json` exists.
5. Save the Level and confirm normal Studio dirty/Undo/Redo behaviour remains intact.

## Test Level parity

1. Run Test Level with the adopted script attached.
2. Exercise behaviour that calls a transitive module through governed `require()`.
3. Confirm the script runs from the Test Level project snapshot without the installed Library being consulted.
4. Confirm normal S3 restrictions remain: arbitrary `package`, `io`, `os`, `loadfile` and unrestricted filesystem module lookup are not exposed.

## Build Game parity

1. Save the Level after adoption.
2. Build Game normally.
3. Confirm the build completes and launches.
4. Confirm the same adopted script/module behaviour works in the packaged Runtime.
5. The project descriptor should carry required `.rscripts`, root script and module paths through its normal typed Always Include dependency declarations; no installed-Library path should be required by the packaged game.

## Update and conflict protection

1. With a clean adopted project copy, replace the installed package with a newer `package_version` and updated matching hashes.
2. Refresh the scripting source chooser; the row should identify an available Library update.
3. Select the update and press **ADD**. Confirm the project copy updates and the source identity remains attached through the normal script document.
4. Edit an adopted project Lua file manually.
5. Install another newer package version and refresh.
6. Confirm Renegade reports the local conflict and does **not** overwrite the edited project bytes.
7. The locally edited project file remains the authoritative source until the creator resolves the conflict.

S6 passes owner acceptance only when discovery, adoption, Test Level, Build Game, clean update and local-edit protection all work from the same final CI artifact.
