# Custom navigation icons follow-up, 27 November 2026

Problem: applying an appearance replaced the complete category icon map, silently discarding custom categories and user-selected images. The owner then raised the separate missing Level/world-target authoring and discovery workflow.

## Change

ApplyNavigationAppearanceDraft is the common validated, atomic appearance merge used by ScreenAuthoringSession, the Studio draft and WorldNavigationService. Existing category assignments win by default, including custom images for standard categories; previously missing preset categories are added. Explicit keepAssignedIcons=false replaces the icon set. Targets, identity, geometry and heading/filter behaviour are retained. No serialized format or model-layout change.

Studio defaults Keep assigned icons on. Selecting a target selects its icon category, a direct Choose selected target icon action imports for that category, and the draft shows an explicit fallback-dot message when no icon is assigned.

Governed renegade.navigation.apply_appearance(id, appearance_settings, keep_icons=true) uses the same service. It accepts appearance-only fields, rejects gameplay/geometry edits and invalid policy types/paths, and supports explicit replacement. Existing configure icons remains full-map replacement.

## Verification

Windows x64 DX12 Release configured build. Built RenegadeEngineBridge, RenegadeRuntimeBootstrap, RenegadeWorldNavigationTests, RenegadeStudio, RenegadeRuntime, RenegadeScreenAuthoringSessionTests and RenegadeScreenLibraryTests. Studio used TargetName=RenegadeStudioNavigationReview; canonical executable was copied after clean shutdown. Include-only Lua/editor changes explicitly touched RuntimeScriptRuntime.cpp and RenegadeScreenEditorWorkspace.cpp. No public model layout changed.

MSBuild flags: /t:Build /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /p:CL_MPCount=3 /m:1 /v:minimal /nologo, with CL=/MP3.

    ctest --test-dir BUILD/creator -C Release --output-on-failure -R "^(Renegade(WorldNavigation|ScreenAuthoringSession|ScreenLibrary)Tests|StudioUiPolicy)$"

Four tests passed in 6.31 seconds; see ctest.txt. Regressions cover custom categories, custom images assigned to existing categories, adding preset categories, explicit replacement, invalid merge caller preservation, one-step Undo, save/reopen, portable custom icons surviving source removal and governed Lua appearance merge/replacement/error handling. The initial portability assertion incorrectly expected a pointer after applying a pointer-free appearance; the fixture was corrected to include the pointer it intends to test. An editor const-getter compile error was corrected with a draft-state cache. Final builds/tests pass.

Native UI and standalone follow-up verification are pending. Studio was saved/restarted in preparation for inspection; the owner then intentionally closed it and explicitly requested it stay closed. No crash is inferred. The new controls have not received a passing native visual check. Earlier native compass evidence does not close this follow-up acceptance.

## Next task and scope

When the owner is ready, inspect the new checkbox/status/button, import a unique custom-category icon, switch appearances with preservation on/off, check Save/Cancel/Undo/reopen and standalone playback. Preserve current owner Screen content; a local baseline snapshot remains under BUILD.

The separate Level-authoring/discovery gap is recorded in UI_WORLD_NAVIGATION_REQUIREMENTS.md: select an entity or place a marker, explicitly define a point of interest and its visibility/discovery rules, then publish through the common registry. No recognition of town/castle/cave/boss geometry or discovery persistence/minimap implementation is claimed. Exact discovery defaults/UI remain design work.

Local checkpoint only. No push, CI, merge, release, Wicked source/gitlink changes or independent acceptance.
