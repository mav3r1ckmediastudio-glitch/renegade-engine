# Hurt overlay viewport coverage correction

Owner review rejected the original hurt overlay: the fitted 1280x720 HUD canvas left uncovered strips at a 1264x681 game-window client size. Earlier general visual acceptance did not establish edge coverage.

ScreenRenderer now maps hurt component rectangles proportionally across the actual viewport, independent of the HUD Fit/Fill/Stretch choice. A full-design rectangle covers every viewport edge; custom partial rectangles remain editable. Ordinary HUD elements retain their existing canvas transform. This applies equally to generic/custom artwork, low-health overlays, script-created views and the embedded Studio sample.

ScreenRendererTests now checks full coverage at 1264x681, 1920x1009, 2560x1080, 1024x768 and 720x1280, plus an embedded viewport with nonzero origin. It covers all three canvas scale modes, unchanged ordinary-HUD layout and proportional custom hurt rectangles. ScreenRenderer and HurtFeedback Release suites passed; ScreenRenderer and Runtime rebuilt.

Native DX12 proof uses a separate disposable copy of the review project. It alternates an exaggerated solid flash and generic vignette with shake/sound disabled, then samples differences against the baseline at all four edges and four corners. Actual client sizes are 1264x681, 1604x711 and 1024x891. All 48 edge/corner comparisons changed visibly; coverage.json contains the values and real gameplay state. Original, wide and tall screenshots were inspected. This diagnostic strength is not the stock prefab opacity.

Normal Studio is relinked against the corrected renderer. The owner's review project/settings are preserved. Windows x64 Release only; independent release gates remain separate.

Source-evidence hashes refreshed; all 84 AssistantWorkflowSuite checks passed after the bounds correction.
