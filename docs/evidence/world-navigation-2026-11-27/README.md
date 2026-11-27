# Generic world navigation local verification, 27 November 2026

Scope: Windows x64 DX12 Release on DESIGN, RTX 4070 Ti, native Studio and standalone Runtime. Source checkpoint is recorded in HANDOFF.md after the implementation commit. Local branch feature/ui-loading-media; no push, hosted CI, merge or release.

## Implementation and builds

EngineBridge owns optional navigation_version=1 metadata on schema3 Image components, validation, atomic authoring/history, typed portable resources and the shared live navigation service. ScreenRenderer owns common drawing, clipping and live view presentation. Runtime supplies observers and indexed entity resolution through a dedicated UpdateWorldNavigation that also runs on HUDs without binding sources. Governed C++/Lua use that same service. Studio uses shared native controls and the same renderer for its labelled private preview.

Configured BUILD/creator with CMake. Built EngineBridge, RenegadeScreenRenderer/DXC, RenegadeRuntimeBootstrap, RenegadeStudio (temporary TargetName=RenegadeStudioNavigationReview), RenegadeRuntime and scoped test targets. Model-layout consumers were explicitly rebuilt: 18 Bridge, 10 Runtime, 2 Renderer, 45 Studio and 48 test translation units. Logs/recompile inventory remain under BUILD/navigation-*.log and BUILD/navigation-recompiled.txt. Later include-only changes explicitly touched their owning translation units.

VS18 MSBuild commands used /t:Build /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /p:CL_MPCount=3 /m:1 /v:minimal /nologo, with CL=/MP3. Native Release executable was copied from the review build after clean shutdown.

Scoped CTest command:

    ctest --test-dir BUILD/creator -C Release --output-on-failure -R "^(Renegade(WorldNavigation|ScreenHealthPrefab|ScreenLibrary|ScreenAuthoringSession|ScreenRenderer|RuntimeScreen|ProjectUiScaffold|ParticleLua|WeatherLuaRuntime)Tests|StudioUiPolicy)$"

Ten tests passed. WorldNavigationTests cover wrap/projection, camera/player/manual sources, filtering/range/declutter, missing/deleted entity following, atomic edits and Undo/Redo, save/reopen, independent live views, authored grouping, portable artwork ownership, strict Lua errors/roundtrips, target lifecycle and joint image/backing-colour configuration independent of Lua table iteration order. Existing Screen/health/prefab and governed scripting suites passed. Test output is ctest.txt. Whitespace and package bytes/hashes were validated.

## Native Studio

Used a separate disposable ProjectService-created project:
C:/RenegadeNavigationReview1127/HUD Components Review/HUD Components Review.renegade.
The original owner's project is unchanged.

- Inserted World navigation through Elements > Basic. Root is an ordinary editable Image at (320,24), 640x104.
- Native settings use shared dark Studio controls and orange outline/tinted buttons.
- Selected Fantasy example, saved, one-step Undo restored neutral, Redo restored fantasy. Restart/reopen retained it.
- Embedded same-renderer sample preview displayed neutral at sample heading 90 and fantasy artwork. Editor sample data is explicitly labelled and never saved as gameplay state.
- Changed appearance/sample heading and Cancelled, then saved the Screen: SHA256 remained exactly 3c972b781424478a1657029532820cea0c9fd6a38c4dd497e67396ca8adcb346.
- Added Watchtower, custom tower category, world (15,0,30), priority 10. Imported original castle.png through the actual native category-icon picker.
- Saved settings as one operation. One Undo plus Screen Save restored that exact baseline hash. Redo plus Save restored exactly 3f526eed84684636538566c3bc09f1b15ce3f2b269d8d28cef902b5409f58c44, retaining target identity and imported icon.
- Ordinary Screen Preview rendered the saved bar/target. Escape returned to the unchanged editor canvas. Story Flow return/reopen retained settings and target.
- Pause Hint was moved/resized only in this disposable review project to avoid overlapping the compass.

Screenshots: editor-neutral-east.jpg, editor-fantasy-target.jpg, editor-preview-return.jpg.

## Standalone gameplay

Launched normal RenegadeRuntime.exe --project with the disposable descriptor; no proof flags or gameplay shortcuts. Entered Title > Main Menu > New Game > Start. The Level contains a real Player Start, native floor and marker. Attached the shipped navigation_example.lua as a governed global Level script through the existing script document service.

The initial native attempt exposed a missing update on HUDs without bindings. Navigation was separated from UpdateUiBindings and rebuilt. Final native gameplay then showed:
- Camera-following fantasy bar with Lua Quest/Merchant plus authored Watchtower.
- Independent Lua-created neutral west-facing bar filtered to Town.
- Normal W movement changed Quest 8 m to 5 m and Watchtower/Merchant 34 m to 31 m.
- Native mouse turning moved directions and targets through north in both directions while the second bar stayed west-facing.
- Pause > Main Menu removed gameplay HUD presentation. New Game restarted at north with one fresh independent bar, initial 8 m quest distance and no duplicates.
- 75 FPS observed in this small review Level; this is not a populated-scene performance claim.
- Log reports one governed script instance; no navigation or Lua error diagnostics observed. runtime-receipt.txt records session facts.

Screenshots: runtime-north.jpg, runtime-distance.jpg, runtime-north-east.jpg, runtime-north-wrap.jpg, runtime-restart.jpg.
The final Lua colour-order correction was subsequently rebuilt and verified by the affected tests; it does not change the example's heading/target behaviour.

## Supplied content and limits

Neutral works without artwork. Fantasy uses 18 unchanged owner-supplied original PNGs: backing, bearing strip, fixed pointer and 15 POI icons. asset-provenance.csv maps package names to the supplied gui/1x/world_compass source paths and SHA256. Portable publication/adoption verifies bytes/hashes and destination ownership. Studio installs both packages, navigation_example.lua and NavigationReadme.md.

Only this available fantasy set was audited/packaged; four full launch sets remain pending. Baked strip labels change by replacing the strip; generated directions/marker labels use the editable font. Wizard/full tooltip audit, minimap, Debug/Vulkan, small-window/DPI, 2x density selection, exported games, large/populated-scene profiling, live Runtime resource hot-reload, independent acceptance and all-four-set parity remain unverified. Live entity movement/deletion is tested through the real scene/Lua service; the native gameplay example uses fixed targets. Do not promote this evidence to a broader release gate.
