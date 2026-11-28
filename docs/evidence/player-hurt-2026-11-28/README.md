# DESIGN player hurt feedback verification, 28 November 2026

Local Windows x64 Release / DX12 on RTX 4070 Ti. Based on accepted local minimap checkpoint 36901594b949edbf2840747183a97398fd83ccb6, branch feature/ui-loading-media. Implementation checkpoint b4a1cfaea02eeac18e44a42ba4427585805a54a6. No push/hosted CI, vendor source or gitlink change.

## Automated checks

EngineBridge, ScreenRenderer, RuntimeBootstrap, Runtime and Studio built in Release. After changing shared headers, dependent compilation units were explicitly touched and consumers relinked to avoid stale incremental executables. The final camera fix rebuilt Runtime; the final modal layout rebuilt Studio; the fixture correction rebuilt HurtFeedbackTests.

Ten focused Release suites passed: HurtFeedbackTests, CharacterAiCombatTests, ScreenAmmoPrefabTests, ScreenHealthPrefabTests, MinimapTests, WorldLocationTests, WorldNavigationTests, ScreenRendererTests, RuntimeUiGameplaySnapshotTests and ScreenLibraryTests (each target has the Renegade prefix). StudioUiPolicyTests additionally passed. Six CMake source contracts passed: CharacterAiCombat, MarkerIcons, EditorPointLookup, Phase5Gate3, Phase6Gate3 and CharacterWorkflow. AssistantWorkflowSuite is recorded separately after the final documentation/hash refresh.

HurtFeedbackTests executes real accepted/rejected combat damage, direction against camera heading, unattributed damage, low health/healing/death/expiry, reduced motion, paused decay, load/respawn baseline, hit bounds, malformed settings/paths, legacy insertion migration, Screen history/save/reopen, portable image and WAV dependency remapping and actual governed Lua creation/configuration/geometry/error/hit/clear/removal.

Reproducible commands from a configured Windows checkout:

```powershell
MSBuild BUILD/creator/Runtime/RenegadeRuntime.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false
MSBuild BUILD/creator/Studio/RenegadeStudio.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false
MSBuild BUILD/creator/Tests/RenegadeHurtFeedbackTests.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false
BUILD/creator/Tests/Release/RenegadeHurtFeedbackTests.exe
cmake -DRENEGADE_SOURCE_DIR=. -P Tests/CharacterAiCombatSourceContract.cmake
python Tests/AssistantWorkflowSuite.py --repo . --output BUILD/hurt-assistant-suite
```

Other named test targets/contracts use the same commands. Libraries must be rebuilt first when changed. The temporary Review Studio used TargetName=RenegadeStudioNavigationReview; the final installed executable uses the normal RenegadeStudio target name.

## Native checks

A disposable review project contains Player Start, ground/buildings, one passive enemy Character and Location Zones. It includes an E sample script that calls hurt.hit without modifying health. Existing opt-in --renegade-inventory-health-proof supplied H native accepted damage for verification; H is not an ordinary game binding.

- native-hit.png / hit-faded.png: actual H accepted damage produces a smooth vignette/flash, then expires. native-state.json records 100% to 75% and advancing sequence/age.
- direction.png: explicit directional pulse produces an indicator without changing health. Legitimate NPC bearing and rejection paths are covered in the native C++ combat suite; this screenshot alone is not an NPC attack proof.
- low-health.png / low-health-pulse.png: actual damage reaches 1%; persistent low-health appearance changes with game time after transient hits expire.
- restored-no-hit.png: F9 restores the saved 100% health and resets hit sequence/age without a false flash. The save-result receipt records the preceding native save.
- shake-baseline.png / shake-active.png / shake-active-later.png / shake-ended.png plus shake-state.json: exaggerated test strength 0.12 metres / two seconds visibly shifts scene geometry and foreground arms, then settles. HP stays 100%. The test settings were restored to shake OFF with stock 0.025 / 0.25 values afterwards.
- ui-history.json: actual modal Save, one toolbar Undo, Redo and Cancel compared exact saved HUD bytes after the normal idle autosave.
- palette.json / editor.jpg: Basic insertion creates an ordinary fourth layer; Cancel closes its draft; one Undo restores the original exact HUD. Reopened settings retain the saved choices and the overall intensity field fits.

The custom direction PNG and two WAV pickers imported owned resources. A fresh Runtime rendered the custom marker, and the owner heard the diagnostic wood-impact WAV. This verifies playback wiring, not production pain/heartbeat quality. Both diagnostic clips and the test marker were cleared before final handoff; stock clips are silent. No human pain media is bundled.

A native check caught shake being overwritten by the post-physics player camera. The corrected implementation applies it at PreRender before visibility; the final native captures were repeated after rebuilding. Earlier failed startup/focus/save attempts and the pre-fix zero-motion captures are excluded from acceptance. R was removed from the fixture because the existing governed input API does not expose Reload.

Debug, exported distribution, Vulkan, large-scene performance and independent release-gate verification remain pending. No universal render/performance parity is claimed.
