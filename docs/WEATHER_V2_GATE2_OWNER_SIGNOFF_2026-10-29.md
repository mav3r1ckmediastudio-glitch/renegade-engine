# Weather V2 Gate 2 — owner visual sign-off
Date: 2026-10-29
Status: **ACCEPTED by owner**, after final Moonlight V4 Studio visual inspection.

## Accepted build and preservation
- G2 Studio accepted exact file: `BUILD/g2/Studio/Release/RenegadeStudioGate2FinalMoonlightV4Oct29.exe`
- SHA256: `34171E94B4656BF5962CE929398B6178FBBBAE96C0EEB975483967B17CAA9581`
- Final review project: `BUILD/g2-final-moonlight-review/Gate2FinalMoonlightReview.renegade`, new independent registry/project ID `5af7c70c-5c88-4e8d-905c-d59a4b225b90`; all 7 identity-bearing source files migrated; Main.wiscene unchanged SHA256 `a3e48b0887435f5dde764a361e6bed90b067fbda6a6e90d2f10bdd54cb99a960`.
- Actual UI handover verified: Project Hub > final project > Story Flow > Main Level > Open Editor > Environment. Editor responsive and on-screen **75 FPS** at default view. Owner inspected V4 and explicitly signed off.
- Focused Release ctest: **7/7 green** in `BUILD/g2-final-v4-focused-ctest.log`.
- 16 DX12 + 16 Vulkan GPU test captures for angular proximity and above-cloud modes; Moon-disabled captures unchanged and above-cloud Moon-on illumination retained relative to V3. `BUILD/g2-final-v4-analysis.json` and matched before-after sheet `BUILD/g2-final-v4-before-after-contact.jpg`.
- DX12/Vulkan return codes 0, shader source contract 0 and root+Wicked `git diff --check` 0.
- Final V4 code: `Tools/night_sky/renegadeMoonCloudLighting.hlsli` preserves direct lunar illumination (view angle independent), enhances localized angular forward scatter; `Tools/night_sky/renegadeMoon.hlsli` adds gentle Moon halo. Existing volumetric ray marching and V3's density/visibility are unchanged.
- Wicked accepted river baseline is `ceb816b844b4ab2fa56a159ac60a4f65c931bdfc` (cloud changes layered on top). No published changes; no PR or merge.

## Next
Gate 3 weather presets & transitions. See `docs/WEATHER_V2_GATE3_MODULAR_TRANSITIONS_PLAN_2026-10-29.md`. Isolate work from this accepted V4 source checkpoint; protect 75 FPS. Owner authorized proceeding to G3, not G4/G5 or Snow V2.
