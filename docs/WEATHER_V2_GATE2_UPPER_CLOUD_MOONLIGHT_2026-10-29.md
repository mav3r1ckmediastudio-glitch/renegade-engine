# Weather V2 Gate 2 — Upper-cloud Moonlight / View-angle Correction
Date: 2026-10-29
Status: Engineering candidate validated; owner visual acceptance **PENDING**.

## Owner feedback and in-editor diagnosis
The owner provided night-cloud references where thin clouds near the Moon are brighter, dense clouds farther away remain dark, and asked about an image taken **above the cloud layer** that looked too dark. They explicitly authorised Desktop Commander GUI control of Renegade Studio on DESIGN.

Via remote GUI, inspected running original Gate 2 locality V2 editor (PID 7040; kept open); Environment showed Custom 00:00, Sun Elevation -75°, Moon enabled with Full Moon and automatic "Follow Time of Day"; Moon brightness about 0.9. With automatic orbit, Moon direction is opposite Sun, so should be high (+75°) at midnight. Captured actual GUI at `BUILD/g2-above-clouds-live-current.png` and lunar settings in `BUILD/g2-above-cloud-moon-settings.png`. Temporary Moon-off/on toggle (not saved) from exactly same elevated camera showed cloud-region pixel means of approx RGB 8.5,10.1,11.8 with Moon on versus 1.6,1.5,1.2 off. The Moon was present and lighting worked, but the tops were badly under-illuminated.

**Root issue:** previous Gate 2 locality shader attenuated *direct moonlight* by `0.58+0.42*vicinity` based on camera view ray versus Moon direction. That is physically wrong: light arriving at a cloud top does not depend on where the observer points; only apparent forward-scattering, halo and phase-function contrast are view-angle dependent.

## Precisely scoped correction
`Tools/night_sky/renegadeMoonCloudLighting.hlsli`: keep Moon-proximity angular computation for thin-cloud fill; return `float2(1.0f, 0.045f + 0.32f * vicinity)` so the first component (direct incident lunar irradiance) stays constant across viewing directions. No geometry, cloud coverage, Moon orbit/phase, scene, animation, UV map, shadow algorithm, ray-march count or new texture lookup changed. Tightened `Tests/MoonlitCloudSourceContract.cmake` to assert this invariant.

`Tests/NightSkyGraphicsProof.h` gains `g2-above-cloud-lunar-study`: captures all four cloud formations (IDs 1,2,4,5) at ground height 2 m and **6200 m altitude** above authored 2280–5470 m cloud layer, pitch +35° / -25°, Moon on/off, midnight Full Moon. All 16 DX12 and 16 Vulkan GPU images passed; cross-API max mean difference 0.2782/255 (`BUILD/g2-upper-direct-parity.json`).

| Cloud type | Above cloud Moon-on before (mean RGB) | After |
| --- | ---: | ---: |
| Cumulus (1) | 14.95 | 22.56 |
| Towering (2) | 11.32 | 17.21 |
| Overcast (4) | 10.55 | 15.61 |
| Cirrus (5) | 11.35 | 16.93 |

These are deterministic GPU test-image measurements, **not physical lux**, and do not guarantee owner photo-realistic acceptance. Moon-off test images retain an approximately 0.24 sky/cloud average; midday shader code path unchanged. Full comparative contact sheet: `BUILD/g2-above-lunar-direct-before-after.jpg`; numerical `BUILD/g2-above-lunar-direct-before-after.json`. Original v2 shader backed up `BUILD/g2-moonlit-locality-v2-before-top-fix.hlsli`.

## Real GUI and performance verification
- Release Studio and Runtime built successfully: `BUILD/g2-above-cloud-light-studio-runtime.log`, `BUILD/g2-upper-qa-studio-release-build.log`.
- New non-overwriting executable `BUILD/g2/Studio/Release/RenegadeStudioGate2MoonTopLightV3Oct29.exe` SHA256 `703AB88022413F326AA03D4F55CE81256AEC057DEAF3281B1789EA6D8E4FC0E4`.
- Separate isolated QA project `BUILD/g2-upper-moonlight-review/Gate2UpperMoonlightReview.renegade` with unique project ID `3d8c144a-7897-48a2-a306-112e74d8fd54`. Cloned and verified all seven converted ID-bearing documents and preserved Main.wiscene hash `a3e48b0887435f5dde764a361e6bed90b067fbda6a6e90d2f10bdd54cb99a960`. Original user-edited project not modified or saved.
- **Actual GUI path verified**: project opened, Story Flow Main Level selected, Open Editor clicked (hold mouse button >100 ms to register), clouds and stars render, Studio responsive at **75 FPS** in that *initial* editor view. Evidence screenshot `BUILD/g2-upper-v3-level-open-success.png`. The 75 FPS figure is an on-screen VSync-capped reading, and is **not** a measured elevated-camera runtime/gameplay FPS.
- Current new reviewer PID 22172 (at launch), caption `Renegade Studio - GATE 2 UPPER MOONLIGHT V3 [DX12]`. Launcher `BUILD/Run-Weather-G2-Upper-Moonlight.cmd`. The previous G2 reviewer PID 7040 is preserved, with user's elevated camera state untouched and no save.
- 7/7 focused Release CTests green: `BUILD/g2-upper-direct-final-ctest.log`; manual source contract passed. `git diff --check` root and Wicked both exit0. Both G1 and G2 Wicked Git commits remain on accepted river `ceb816b844b4ab2fa56a159ac60a4f65c931bdfc`, G1 signed-off cloud executable SHA `87025817AC89DFF9AF2B5CC3883402C08AF07653A8F430AC51482B1DC98936DC` intact.

## Pending
Owner inspect new V3 reviewer in a comparable above-cloud Moon-enabled view. If it still looks too dark, investigate the scattering-to-ambient and tone mapping rather than incorrectly rescaling direct illumination by view angle or boosting blanket rim lighting. Retain the phase-aware, optically thick/self-shadowed response and 75 FPS guardrail. Do not claim Gate 2 signoff, four-build CI, push, PR, merge, Gates 3–5 or Snow V2.
