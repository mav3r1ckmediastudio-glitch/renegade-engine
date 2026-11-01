# Weather V2 G4: Save As repair and final engineering verification

2026-10-31, DESIGN, Windows x64 DX12 Release. Save As blocker FIXED; live workflow PASS. G4 is owner signed off and closed for the agreed scope; see WEATHER_V2_GATE4_OWNER_SIGNOFF_2026-10-31.md. G5 packaging and configuration checks remain separate.

## Owner decisions superseding earlier open items

The owner reports personally testing preset transitions and that they work as intended, and explicitly approves grass motion. Do not implement additional automatic rain/wind preset coupling to satisfy an earlier speculative open item. Current preset behaviour is accepted. Renegade has grass only; trees are a future system and are not a gate requirement. Prior clouds, moonlight, sheet/fork lightning, rain/snow, thunder and current performance acceptances stand.

## Confirmed cause and bounded correction

SceneDocumentService::Save wrote/validated the native archive and adopted its new path but never created the Renegade level identity sidecar. ScriptAuthoringService::EnsureCurrent requires that .rmeta even when no scripts are attached, so Studio Play failed before launching Runtime after Save As. The previous scripting test manufactured its own sidecar and therefore missed this creator workflow.

- EngineBridge/src/SceneDocumentService.cpp: validate project containment and existing destination identity before writing the archive. A new project scene receives a fresh scene envelope with the correct project ID and relative path. Commit the validated archive and new identity together using the existing journaled ProjectDocumentTransaction, with staging validation and rollback. Existing valid destination identities are preserved; malformed or foreign identity is rejected without adopting another path or clearing dirty work. Existing raw WISCENE saves without an active project remain supported. Normal archive and rolling backups remain.
- EngineBridge/src/ScriptAuthoringService.cpp: an unscripted legacy/imported source with no companion does not require its missing old identity merely to Save As. Do not mutate the original source to manufacture one. Existing scripted copies still clone the companion under the new level identity and preserve script-instance/source identity.
- Tests/S4BScriptAuthoringTests.cpp: stop supplying the missing sidecar manually. Cover legacy unscripted Save As, first save identity creation, scripted Save As into a new subfolder, fresh level/script document identities, preserved attachment IDs, unchanged original archive/identity/scripts, stable overwrite identity, reopen, real project-aware Test Level snapshot, and malformed-destination failure with dirty work/path preserved.

No weather/render/audio/vegetation code or shader changed in this repair. No speculative visual adjustment or build-only defect closure.

## Exact candidate

Root HEAD f073233a61e8d8814cdc29a610e242689832c37f; Wicked HEAD 7e2326c0338e625f4405d8fe5150fde9547eaded, with existing dirty work retained. No new commit, push or merge.

BUILD/g4-saveas-source-manifest.json records the three changed source files, existing approved weather sources and new Studio/Runtime binaries. Extended source digest:

    d2ee86910414d986ba42729231f993670a6c7233ace725c52b379f10350167cc

All source hashes from the approved weather manifest still match its digest 6a632022e6ec0f28553a7f1fe3f9ae02e6e06b56596d7b51afd774d9e34fb7d1. Original saved Main, the QA source level and companions, and all nine original thunder assets in both copies remain unchanged. Approved binaries are retained as .saveas-baseline.exe; pre-repair copies of the three source files and evidence are in the existing external critical-repair audit, saveas-fix/baseline and saveas-fix/evidence.

## Build and regression evidence

Using the existing BUILD/g3 configuration and CMake 4.4.4, CL=/MP8, parallel 4:

    cmake --build BUILD/g3 --config Release --target RenegadeStudio RenegadeRuntime RenegadeS4BScriptAuthoringTests --parallel 4

PASS, BUILD/g4-saveas-final-build.log. Additional affected identity/transaction and weather test executables relinked against the final bridge: BUILD/g4-saveas-regression-build.log PASS.

    ctest --test-dir BUILD/g3 -C Release -R "Renegade(S4BScriptAuthoring|IdentityTests|ProjectDocumentTransactionTests|WeatherTransition|WeatherEffectsTransition|SheetLightning|ForkLightning|Precipitation|Moon|CloudFormation|StudioPlayerStartCache)" --output-on-failure

13/13 PASS, BUILD/g4-saveas-final-tests.log. This is focused Release verification, not full four-configuration CI. Whitespace check passes for all three source files.

## Actual Studio -> Save As -> Reopen -> Test Game -> Studio

Used an isolated copy of the registered grass validation project. The original scene and owner work were not overwritten. In the actual Studio UI, opened its TestLevel, used File > Save As to create GrassWeatherCopy2.wiscene, and verified creation of a fresh sidecar. Its scene ID is 601d0184-4a27-4d79-b967-e7100c7fb9c2, distinct from original 8fcb05c1-bba6-4a87-9035-4dd60662d341; project ID and Content/Scenes/GrassWeatherCopy2.wiscene path hint are correct.

Used File > Reopen. The saved copy and grass load, with a clean document/empty post-load history. Clicked the normal Studio Play button. Real Runtime child PID29340, parent Studio3984, launched with the descriptor under this QA project's Intermediate/TestLevelSnapshots and Studio's ready-event argument. This was not a manually substituted standalone level. Runtime rendered the saved grass/cloud scene; the maximized screenshot reports 1920x1009, RTX4070Ti and 75 FPS. Recorded 24 frames. This sampled dry-grass view is not a new combined-Storm performance benchmark.

Closed only that diagnostic Test Game child. Studio resumed the same saved copy and displayed TEST LEVEL // COMPLETED, with no unsaved marker and about69 FPS sampled. Its disposable snapshot directory was cleaned. Studio3984/HWND201150 remains open for final owner review. Earlier Studio35136 and standalone Runtime8360 remain minimized and preserved.

Evidence: BUILD/g4-saveas-final-saveas.png, -final-reopen.png, -testgame.png/.gif, -returned-studio.png, source manifest, final test/build logs and external final-studio-ui.log. Unscripted snapshots intentionally omit scripting companions after the live empty document is bound; lack of .rmeta inside that disposable snapshot is expected. The authored saved level's identity is required and now present.

## Limits and risks

The first UI session PID44632 closed after a successful Save As, before its reopen/Play test. No exit status was retained and no matching Windows Application Error/WER event was found; the reason is unconfirmed. A fresh monitored session completed the full workflow and stayed open afterward. Do not claim the first closure was diagnosed or attribute it to the user or the repair.

Native archive validation still logs unresolved scene-relative names for bundled default terrain/grass resources before the existing bridge rebinding step; the live reopened scene and Test Game render grass correctly. No general custom-asset relocation or dependency portability claim is added. A project-owned save outside the active project is rejected before payload replacement; cross-project export is not added here. Existing script-companion save remains its separate, explicit dirty-on-failure phase; this repair groups archive + newly created identity, not every project document.

The owner independently confirmed the repair and signed off on 2026-10-31: "Fix confirmed and signed off". G4 is closed for the agreed scope; see WEATHER_V2_GATE4_OWNER_SIGNOFF_2026-10-31.md. G5 packaging/final integration and configuration validation remain open. No further transition or grass-motion work is required unless the owner reports a failure. Future trees, custom lightning artwork, physical strike impacts and complete weather API parity remain deferred.
