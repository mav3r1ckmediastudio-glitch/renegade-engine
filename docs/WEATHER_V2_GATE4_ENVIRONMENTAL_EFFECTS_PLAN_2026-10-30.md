# Weather V2 Gate 4 — environmental effects (owner priority revised)
30 October 2026 | Branch `feature/weather-v2-all-gates`
G3 approved; G4 OPEN.

## Owner-requested first slice: volumetric SHEET lightning
The owner explicitly rejected starting G4 with branching/forked lightning geometry and requested **illumination within the clouds**. Do not create lightning bolts/rods, full-screen white flashes or a second weather rendering system.

### Architecture
The pinned Wicked cloud raymarch (`shaders/volumetricCloud_renderCS.hlsl::SampleLocalLights`) ALREADY integrates **volumetric-cloud-enabled native point lights** with cloud density. The C++ LightComponent has `SetVolumetricCloudsEnabled(true)`; this is separate from fog volumetrics. Start with a bounded group of native point lights positioned in the authored cloud volume, no visible mesh or gameplay hit logic. Native cloud scattering and self-occlusion should shape patches internally. The CPU produces deterministic, staggered irregular pulses, with a spatially moving glow rather than simultaneous full-sky flashing.

### First prototype (G4-S1)
- Opt-in Runtime preview, off in production unless explicitly enabled; review at night and twilight.
- Frequency/radius/pulse-strength defaults bounded, with deterministic seed and pause-aware game time.
- 2–3 independently pulsing invisible lights at locations inside cloud base and mid-volume. Do not make the entire sky uniformly luminous. The original G1/G2 visual baselines must remain unchanged when disabled.
- No shader fork or shader constant tweaks as a shortcut; first verify whether the native light path yields convincing internal scatter in live DX12.
- Tests for safe disabled mode, timing, no negative/invalid delta behaviour, deterministic event sequences and bounded flash duration. Test actual runtime FPS.
- Transient lights belong to Runtime preview, not persisted in authored scene, never impact collision, physics or UI hierarchy. Clean up on scene reload/exit.

### After owner visual approval
- Add a clear Weather > Lightning control (Off, In-cloud flashes, advanced frequency/intensity/coverage), command-backed authoring and scene metadata persistence; Studio Preview and Test Game parity. Thunder optional, independently controlled and distance-delayed.
- Only consider cloud shader tweaks if native-scatter proof demonstrably cannot meet visual quality.
- Once sheet lighting is owner-approved, return to rain/wind and environmental surface response. Existing native basic Snow visual remains, but **Snow V2 is deferred beyond all five Weather V2 gates**.

### Non-goals / release
No branching bolt renderer, cloud-to-ground strikes, gameplay damage, lightning targeting, whole-screen overlay, Snow V2 or forecast simulation in this prototype.
Continue single local feature branch, milestone commits, no push/PR/merge without owner authorization.

## 30 October 2026: sheet-lighting runtime defect and corrected vertical slice
- The first opt-in Runtime lit no visible clouds. A trace independently established FLASH_START events and plausible in-cloud positions. The owner reported stable **74 FPS**, so a single observed 39 FPS frame sample must not be treated as an established performance regression.
- Root issue: native local point lights in `lightingHF.hlsli::attenuation_pointlight` use `half` squared distances and inverse-square falloff. The event centres are 2–5 km from camera and sample distances easily exceed useful FP16 range; increasing point light intensity would not fix clipping/precision reliably.
- Replaced the experimental scene lights with four transient float4 fields in the existing `WeatherComponent` and `ShaderWeather` carrier, filled by `RuntimeSheetLightning`. Shader function `RenegadeSheetLightningRadiance` computes three smooth local in-cloud fields in float32 at existing raymarch samples, then sends the radiance through ordinary density scattering and ray transmittance. No extra shadow or cloud samples; no geometry or full-screen overlay. The fields default to zero and are **not serialized**.
- Runtime visual experiment remains opt-in only. Verify the pinned Wicked shader submodule build, G1/G2/G3 regression tests, editor/non-preview parity, and actual flash screenshots/FPS before sign-off. The 74 FPS claim applies to the previous (invisible) prototype, not the revised shader until tested.
- Evidence paths: `BUILD/g4-sheet-cloudfield-build.log`, `BUILD/g4-sheet-cloudfield-ctest.log`, `BUILD/g4-sheet-lightning-live-trace.log`, `BUILD/g4-sheet-before-flash.png`, `BUILD/g4-sheet-flash-*.png` once captured.

## 2026-10-30 final flash-only reprojection result (owner review pending)
- Full-precision cloud-radiance implementation in paired Wicked fork has a visible in-cloud, spatially bounded illumination flash. G4-internal emission is only enabled by `RENEGADE_G4_SHEET_LIGHTNING_PREVIEW=1`. No generic point lights or bolt geometry are spawned; cloud formation/motion and G2 moonlight calculations are untouched.
- Debugging revealed 4-phase checkerboard cloud reconstruction mixes new bright samples with older dark history. The reproject shader now resolves the CURRENT cloud raymarch via bilinear sampling for all pixels during sheet lightning and clears the additional history response. Latest threshold >0.0001f holds the special resolve through the decaying pulse.
- Final Release build `BUILD/g4-sheet-final-threshold-build.log`: PASS. `BUILD/g4-sheet-final-tests.log`: 4/4 PASS (SheetLightning, WeatherTransition, Moon, CloudFormation).
- Warmed-up DX12 screenshot evidence `BUILD/g4-sheet-before-flash.png` and `BUILD/g4-sheet-flash-1.png`, `-2.png`, `-3.png`: visible cloud-localised glow with smooth fade, checkerboard no longer obvious; 75 FPS on RTX 4070 Ti at 1264x681. Earlier invisible opt-in prototype ran ~74 FPS per owner.
- Visual still only a prototype: owner has NOT approved appearance, timing, cloud-region selection, or final creator UX. No integrated Studio control, persistence, Runtime Lua exposure or thunder yet. Snow V2 deferred. No push or merge.

## 2026-10-30 G4-S1 sheet-lightning realism pass — VISUAL PREVIEW, NOT OWNER SIGNED OFF
- Owner reviewed the first visible result and said **"it's a start"**; do not mislabel that as G4 acceptance.
- Corrected visible checkerboard artefact in `volumetricCloud_reprojectCS.hlsl`: while lightning energy is present or within a short recovery interval, spatially filter the newly marched clouds from 4 quarter-res neighbours and mix only a small amount of recent history (24% during light / 12% during recovery). Outside this event, original volumetric temporal reconstruction remains unchanged.
- Added `SheetLightningFrame::temporalRecovery` with an approximately 1.12s bounded lifetime after an event, passed in `sheet_light_power.w`; the flash itself remains short. Regression tests cover recovery after the light fades and returning to normal.
- Revised the cloud raymarch light fields from spherical discs to overlapping shallow/oblique ellipsoids, retaining existing cloud scattering and transmittance.
- Moved opt-in preview cells farther away (horizontal range about 7.8–10.6km) and near the lower part of the cloud layer; reduced preview pulse intensity to 0.15 and increased spread/radius to 720m/1800m respectively.
- **Release build PASS**: `BUILD/g4-sheet-deeper-cloud-build.log`. **Focused CTest 4/4 PASS**: `BUILD/g4-sheet-deeper-cloud-tests.log`: SheetLightning, WeatherTransition, Moon, CloudFormation.
- Automated DX12 screenshots `BUILD/g4-sheet-before-flash.png` and `BUILD/g4-sheet-flash-1.png` / `-2.png` show a localized cloud-mass flash with no obvious checkerboard. Full-size 1920x1009 overlay shows 74–75 FPS in sampled frames (RTX 4070 Ti). This is a sampled test, not a long-run controlled benchmark.
- Runtime opt-in preview remains enabled only with `RENEGADE_G4_SHEET_LIGHTNING_PREVIEW=1` via `BUILD/Run-Weather-G4-Sheet-Lightning-TestGame.cmd`. Owner still to evaluate live visual and decide next iteration.
- No new bolt geometry, no gameplay lightning, no thunder audio, no Studio authoring UI, no weather G4 owner signoff, no merge/push.

## 2026-10-30 — G4 in-cloud sheet lightning: owner-requested 30% wider feather pass
- Owner requested roughly 30% greater spread with a gradual fade toward each light radius, and slight visible variation. Implemented directly on DESIGN, not handed to Codex.
- Opt-in preview cell radius `cloudLightRangeMetres` increased from 1800m to 2340m (+30%). Kept existing 720m cell spacing and event-randomised per-cell size/placement, preserving visible variation across strikes. Reduced preview intensity from 0.15 to 0.14 to avoid a hotter overlapping core.
- `volumetricCloud_renderCS.hlsl::RenegadeSheetLightningRadiance`: independent `smoothstep` edge feather widths 0.42 / 0.49 / 0.36 multiply the existing squared attenuations of the three oblique lobes, preserving a bounded centre and fading softly to zero at the expanded extent. No additional cloud raymarch/shadow samples or visible bolt geometry. Existing G4 flash-aware reconstruction unchanged.
- Added `RenegadeSheetLightningTests` case comparing identical-seed 1800m and 2340m storms, verifying exact 1.30x cell radius, unchanged timing/position and no larger core power.
- Release Runtime + test target build: PASS in `BUILD/g4-sheet-wide-feather-build.log`. Focused regression CTest **4/4 PASS** in `BUILD/g4-sheet-wide-feather-ctest.log` (SheetLightning, WeatherTransition, Moon, CloudFormation).
- Direct DX12 Runtime capture at 1920x1009, RTX 4070 Ti, shows **75 FPS** on sampled frames and visible internal cloud flashes with softer spread. Preserved prior reference captures as `BUILD/g4-baseline-g4-sheet-*.png`; new evidence `BUILD/g4-wide-pass2-flash-2.png`, `BUILD/g4-wide-pass3-flash-2.png`, other `BUILD/g4-wide-pass*-*.png`. 75 FPS is sampled, not a prolonged benchmark.
- This remains an opt-in visual preview (`RENEGADE_G4_SHEET_LIGHTNING_PREVIEW=1`). G4 is **NOT** owner signed off. Keep the Runtime preview for owner judgement. No push/PR/merge to main. Separate untracked WeatherEffectsTransition files remain untouched.
