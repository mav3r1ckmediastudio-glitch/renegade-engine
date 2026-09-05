# Post-PR58 Gate 4 — Project Identity, Artwork & Recent Projects

## Baseline

This gate starts from `main` after merged PR #59 (`a0395f784747b458247a445ce37584bde8a96c7f`). The native Project Hub presentation accepted in PR #58 is locked: this gate must not reintroduce concept-plate UI, startup changes, or editor bleed.

## Objective

Make Recent Projects authoritative visual project identities rather than a text/file-system list.

## Implemented scope

- The central Recent Projects area renders up to three real selectable project cards at once.
- Cards use each project's persisted Hub screenshot when one exists.
- Cards without artwork use the Renegade project fallback treatment rather than fake project imagery.
- The selected card is visually explicit and continues to drive the approved large preview and right-side Project Details panel.
- Recent projects beyond the visible three remain reachable through the existing previous/next controls; the card window follows the selected recent project.
- Clicking a card selects that project without opening it immediately.
- The large selected-project preview remains the explicit click target for choosing/replacing project artwork from local storage.
- Custom artwork remains project-local under `Saved/ProjectHub/project-preview.<ext>` and is rediscovered on Studio restart; it is not WISCENE scene content.
- A recent entry whose descriptor is missing/moved remains selectable for diagnosis, is visibly marked `MISSING` / `PROJECT UNAVAILABLE`, and cannot invoke the selected-project open action or artwork write path.
- Existing descriptor inspection continues to populate project name, root, startup scene and project format where available.
- Existing New Project and direct Open Project actions remain unchanged.

## Artwork precedence in this gate

1. User-provided/custom project screenshot under `Saved/ProjectHub/`.
2. Future automatic project capture (not invented in this gate; no accepted automatic capture source exists yet).
3. Renegade branded fallback rendering.

The UI presents artwork inside 16:9 card/preview surfaces without manufacturing fake screenshots.

## Acceptance checklist

Owner Release test must verify:

1. Hub appearance remains the approved native PR #58 design.
2. Existing recents appear as visual cards and the card selection state is obvious.
3. Selecting different cards updates the large preview and Project Details accurately.
4. Clicking the large preview for a valid project opens the local image picker, and the chosen artwork persists after complete Studio restart.
5. Projects without artwork show a clean branded fallback, never fake landscape content.
6. A deliberately missing/moved descriptor is shown safely as unavailable and does not crash or open stale context.
7. Previous/next navigation can reach all recent entries beyond the visible three.
8. New Project and direct Open Project still work.
9. Gate 2 startup media/identity/iris behavior and PR #59 texture-restore performance are not regressed.
10. Exact-head Renegade Studio Debug/Release and Windows baseline Debug/Release are green.

## Deferred items

- Automatic project screenshot capture is deferred until there is an explicit accepted capture moment/policy; custom artwork already has precedence.
- Create/Open/Continue/Switch transaction reliability remains the next lifecycle gate.
- Legacy governed Wicked texture filename normalization remains an additional cleanup gate at the end of this section, per owner decision; it is not folded into this visual project-card gate.
