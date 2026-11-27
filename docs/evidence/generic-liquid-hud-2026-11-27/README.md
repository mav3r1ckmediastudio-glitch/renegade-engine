# Generic liquid authoring and gameplay HUD evidence

Windows x64 DX12 Release on DESIGN, 27 November 2026. This is local scoped verification; owner and independent acceptance remain pending.

A disposable native project was created using the health suite's opt-in fixture helper:

    RenegadeScreenHealthPrefabTests.exe --hud-review C:/RenegadeHUDReview1127

Use a new short parent path. The helper creates a real Starter Level/Player Start/static floor/marker and cleans the stock HUD. The indicators themselves were added/configured through actual Studio palette, Inspector, mask picker and shared modal. No owner project was edited.

- studio-hud-reopened.jpg: saved HUD reopened through Story Flow, with independently selectable vial/fill/artwork, manual custom-mask gauge and bound Player Health text.
- glass-layer-selected.jpg: native Image selection exposes Change Graphic, while the fill is a separate Progress layer.
- custom-mask-full-preview.jpg: native mask import with fitted 6.25/18.75/93.75/81.25% bounds and 100% transient fill, showing the full battery silhouette.
- four-directions.jpg: crops from four native Studio captures at 65%, with animation disabled; labels describe the chosen direction. No pixels of the fill were synthesised.
- runtime-health75.jpg, runtime-health50.jpg, runtime-health25.jpg: actual Player HUD in the native Level. The red liquid decreases, the artwork stays unchanged and the manual gauge stays 65%. Overlay reports 75 FPS.
- combat-values.txt: relevant native Runtime backlog entries only.

Runtime launch used the existing --renegade-inventory-health-proof diagnostic. H applies 25 actual combat damage in that opt-in mode. Started at 100, then 75,50,25. This change adds no gameplay damage shortcut. Healing and audible SFX were not verified.

Native mask-change Cancel plus Screen Save retained exact prior Screen bytes. The custom effect was saved, undone/redone, saved/reopened; disabling it and saving then one Undo restored the effect. Bounds, manual binding, colour, flat motion settings and direction were retained.

Final checks:

    cmake -S . -B BUILD/creator
    ctest --test-dir BUILD/creator -C Release --output-on-failure -R "^Renegade(ScreenHealthPrefab|ScreenAuthoringSession|ScreenRenderer|ScreenLibrary|RuntimeScreen)Tests$"
    BUILD/creator/Studio/Release/RenegadeStudioUiPolicyTests.exe
    git diff --check

All five scoped suites passed in 6.35 seconds; Studio UI policy passed. Affected bridge/renderer (including DXC shader)/Runtime/Studio built successfully with VS18 BuildTools MSBuild Release x64, BuildProjectReferences=false, CL_MPCount=3, /m:1 and CL=/MP3. Final Studio build copied Content/UI/Examples/BatteryMask.png and Readme.md successfully.

Debug, Vulkan, exported-game packaging, small-window/DPI usability, audible mixing, healing, wizard and damage-threshold overlays are outside these receipts. No push, hosted CI, merge or release.
