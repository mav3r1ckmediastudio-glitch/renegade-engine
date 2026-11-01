# Renegade Weather V2 Gate 2 — Atmospheric Lighting
**29 October 2026 — authorised by owner immediately after formal visual sign-off of Gate 1.**

## Accepted source and isolation
- Sole development tree for Gate 2: `C:\Users\paulw\source\repos\renegade-weather-g2-20261029`, branch `codex/weather-v2-g2-atmospheric-20261029`.
- Based on exact Gate 1 root HEAD `150eca4b761d3f62f122df3ab5e8536ec084f926` PLUS all dirty and untracked accepted G1 working files copied byte-for-byte. `docs/WEATHER_G2_G1_SNAPSHOT_MANIFEST_2026-10-29.json` records SHA256 for all 114 transferred files.
- Local Wicked clone at accepted river-enabled commit `ceb816b844b4ab2fa56a159ac60a4f65c931bdfc` with its 22 dirty/untracked Gate 1 source changes copied and verified. ThirdParty/imnodes at `eb36902c892548ef94f88f51ad7e7c9c7058a71c`.
- Original Gate 1 worktree, Studio reviews, exact accepted 75 FPS DX12 preview binary and clone scene remain untouched. Gate 1 visual sign-off is authoritative; it is not a GitHub CI or merge approval.
- No commits, pushes, PR, merges, Weather Gates 3–5 or Snow V2 without new permission.

## Gate 2 visual contract
1. Distinct solar/cloud interactions at midday, golden hour, sunrise/sunset, civil twilight and night. Realistic atmospheric attenuation and scattering rather than a uniform tint or comic-book glow.
2. Moonlight tied to the *authored Moon*: enabled state, fixed/automatic direction, above-horizon placement, 8 phases, brightness. Full Moon may softly highlight illuminated surfaces and thin backlit edges; new or below-horizon Moon must not illuminate clouds. Cloud self-shadow and scene-night atmosphere remain consistent.
3. No discontinuities when transitioning between Sun- and Moon-dominant scattering. Do not fake direct-light direction by interpolating opposing sun/moon directions. Limit additional raymarch/shadow work.
4. Realistically black/dark undersides when unlit, subtle twilight ambient still visible; preserve stars, Milky Way, aurora, cloud shadows, fog and ocean.
5. Preserve Gate 1 cloud shapes, coverage, movement-speed slider, Undo/Redo and saved scenes, and owner-accepted isolated 75 FPS at Movement Speed 8 on 75 Hz/RTX4070Ti. Additional effects must not invalidate baseline.

## Initial diagnosis
The current `Tools/night_sky/renegadeMoonCloudLighting.hlsli` uses single dominant directional source, with an abrupt conditional switch exactly when solar direction Y drops below -0.09. Its night fade is zero at that crossing and then grows over 0.10 solar Y. The source direction changes between Sun/Moon in one branch, and it assigns a phase-scaled artistic lunar irradiance `float3(.50,.60,.74)`. It preserves native solar lighting during daylight but does not yet have a tested continuous solar/twilight/moon handover. The Sun is retained separately for aerial perspective via the cloud shader (good).
Gate 2 must first capture a matched baseline at several nearby solar elevations, day/night and Moon phase/orbit states. Avoid jumping directly to brightness constants.
Existing `Tests/NightSkyGraphicsProof.h` has Moonlit Clouds V1 day/night but lacks closely-spaced twilight passage and fixed-camera real-GPU cloud luma continuity checks.

## First engineering slice
- Record authoritative GPU baseline for daylight, sunset, twilight, moonlit/mainless night at fixed camera/coverage with clouds moving at a known controlled speed.
- Introduce a coherent **single-light twilight handover** that fades outgoing solar intensity *before* selecting lunar direction and fades incoming lunar intensity afterwards, without blending two physically different light directions or adding a second full shadow-ray march. Do not change broad day baseline.
- Add focused CMake source/contract test and CPU analytical transition checks before GPU, then DX12/Vulkan captures and image metrics for continuity. Report if a single-light architecture cannot meet quality; escalate to bounded dual-light solution only if required and performant.
- Produce isolated real Studio G2 preview only once the images are materially improved. Gate 2 stays OPEN until owner visually accepts it.

## Gate 2 gates for acceptance
- Day vs previous Gate 1 preview equivalent with Moon off.
- Gradual solar fade and nocturnal moon illumination; no visible twilight pop or false bright Moon when under horizon.
- Phases full > quarter > new; Moon disabled matches new baseline; no Moon in editor if actually below horizon or outside camera (explain in UI).
- DX12 and Vulkan shader proof, focused native tests, 1080p/1440p performance on single active editor; packaged Studio/Runtime and saving persistence tests.
- Explicit owner sign-off; never infer from synthetic GPU test alone.
