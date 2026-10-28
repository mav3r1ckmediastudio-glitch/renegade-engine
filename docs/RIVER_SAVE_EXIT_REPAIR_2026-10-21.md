# Save-to-close loop: river project report — 21 October 2026

## Report and disk evidence

Owner reports repeated X -> Unsaved changes -> Yes/Save -> still open ->
X -> same confirmation. A clean document should exit without any dialog,
and successful confirmed Save must continue to exit. Do not equate existence
of a WISCENE file with full save confirmation: StudioSession::SaveScene()
writes the WISCENE and then saves governed .rscripts scripting data.
If the latter fails, it calls MarkUnsaved(), returns false, and the old
RequestSceneReplacement discarded failure without showing the error.

A separate project fixture at BUILD/river-layers-20261021-content-proof/
contains persistent-river-layers.wiscene timestamp 17:58:37 and rolling
backups. It does not prove subsequent user colour changes were saved.
Do not force-close Studio or discard current project until verified.

## Repair in isolated worktree

1. ScriptAuthoringService::SaveForScene does not write an unnecessary empty
   .rscripts or update the project build closure when an otherwise unused
   script inspector only loaded an empty document, current scene path is
   unchanged, and no prior companion existed. Existing script companion and
   Save As paths still execute full governed save.
2. StudioRenderPath::RequestSceneReplacement now displays a detailed save
   failure alert and status with Scenes().LastError(), does not falsely close
   after a partial save, and calls optional cancellation callback.
3. RequestExit tracks an in-progress save/exit so repeated clicks cannot
   open overlapping confirmation dialogs. On clean scene it still exits
   immediately; Cancel or save failure releases the pending request.
4. Tests/S4BScriptAuthoringTests.cpp covers read-only inspector load followed
   by Scene save, clean command history and no unwanted .rscripts sidecar.

No accepted alpha, main checkout, owner projects or owner's running Studio
executable are changed; no commit, push or merge. Candidate requires
native Release build and owner interaction acceptance.

## Verification

S4B Script Authoring, S4D Global Script, Terrain, River Spline, River
Underwater and Player Swimming CTest tests: 6/6 PASS Release (see
BUILD/river-exit-20261021/ctest.log). Build
RenegadeStudioSaveExitFixed.exe separately and verify exit by save,
no changes, Cancel and Save failure in a disposable project. Native
Studio launch smoke alone cannot prove Windows message-box interaction.

## Final isolated build checkpoint

Native VS18 Release Studio candidate compile/link PASS exit0 in 400.88s,
log BUILD/river-exit-20261021/studio-exit-build.log. Separate candidate:
BUILD/renegade/Studio/Release/RenegadeStudioSaveExitFixed.exe;
SHA256 A2C0A16AB353F4D0702844800DC6258CADD1B74F0248E8D5DC54E4EC99AB4FD6.
Win32 startup-smoke exit0. The existing candidate remained running PID26424.
Interactive owner exit tests still pending; a smoke test cannot simulate them.

Further disk check: same scene file updated again at 18:05:08 to 192515
bytes with contemporaneous automatic backup. This confirms a later archive
write, not correctness of all later slider values. Owner's active build still
running, and no project data was modified by this repair.

## Owner acceptance update

Owner explicitly confirmed: "THE SAVE FIX WORKS" (2026-10-21).
Mark the reported Save -> Close loop owner-verified as resolved in the
isolated candidate. This does not independently establish coverage of
failure injection, Cancel/Save As paths, or release readiness.
