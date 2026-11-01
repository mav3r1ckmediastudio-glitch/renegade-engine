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

## 2026-10-30 — G4 sheet lightning OWNER ACCEPTED
- Owner replied **"acceptable"** after reviewing 30%-wider, edge-feathered sheet lightning on DESIGN. Record that as definitive sign-off for the **sheet-lightning visual slice only**; avoid unnecessary further tweaking.
- Signoff document: `docs/WEATHER_V2_G4_SHEET_LIGHTNING_OWNER_SIGNOFF_2026-10-30.md`. Accepted source baseline is root `6920275`, pinned Wicked `c25c9828` with Release build PASS and focused regressions 4/4 PASS. Sample DX12 Runtime FPS 75 at 1920x1009 RTX 4070 Ti.
- Gate 4 overall still OPEN: integrated authoring controls and persistence, thunder, rain/wind and environmental response remain. Snow V2 deferred. No push, PR or merge authorized. Untracked precipitation/wind foundation files must remain untouched.

## 2026-10-30 — Owner requirement: full weather scriptability + optional lightning impacts
- See `docs/WEATHER_V2_SCRIPTABILITY_AND_LIGHTNING_IMPACT_CONTRACT_2026-10-30.md` (new design contract, **not implementation**).
- Owner wants every preset, cloud/weather subsystem and weather event controllable from **Lua and C++**, including lightning-impact callbacks for scripts such as striking a tree and starting a fire in a future gameplay system.
- Reuse existing authored surface classification and collision/event plumbing; do NOT reuse bullet-hole visuals or automatic bullet damage. Atmospheric lightning need not have collision unless cloud-ground impact mode is enabled. Fire propagation itself is future scope.
- Current G3 Lua weather API includes `renegade.weather.set_preset/get_state/cancel` and `on_weather_transition_complete` but does not expose complete cloud/weather/property/event parity; full scripted coverage needs an explicit audited gate.

## 2026-10-30 — Owner question: imported lightning artwork (FUTURE DEVELOPMENT, design only)

Owner wants artists to be able to replace the stock fork-lightning artwork with their own assets; stock procedural visuals must NOT be a forced aesthetic.

**Simple artist UI under Weather > Lightning > Appearance:**
- Visual Source: **Renegade Procedural** (default) / **Custom Texture** / **Custom Lightning Effect** (advanced).
- Custom Texture: "Choose Graphic / Replace Graphic" uses the existing creator asset/texture-import workflow and supports a transparent bolt texture or compatible animated/flipbook material. Continue to use Renegade's native 3D strike path, branch density, per-branch fade, frequency, cloud illumination and optional impacts; map the imported artwork along actual segment length with UVs and preserve its alpha/emission. Current fork mesh is generated with vertex positions/normals/vertex colours, **no generated UV coordinates yet**: textured bolt art is a NEW implementation task, not already working.
- Custom Lightning Effect (advanced): permit a project-owned, packaged visual effect/prefab/mesh or animated sprite sequence to replace the whole visible bolt. A single flat full-bolt image must be labelled **camera-facing 2D** and cannot pretend to have real 3D branching or occlusion. This override can choose an orientation strategy; source strike origin/endpoint/timing still come from the weather engine. The built-in procedural branching slider may be irrelevant/disabled with a wholly custom full-strike asset.
- Keep **Cloud Illumination** separately toggleable; default to the approved sheet-lightning cloud-scattering effect triggered at the visible strike origin, unless creators explicitly override it. Preserve independent thunder, impact queries, strike frequency and weather-script event delivery even for custom bolt art.
- Art creators should be able to tune visible colour, emission, transparency/halo, thickness/scale and animation speed without writing shaders. Advanced controls can target trunk/secondary/tertiary appearances individually; do not require separately setting every branch.
- Reuse existing `CreatorTextureWorkflowService`, `MaterialTextureAssetService`, `AssetBrowserService` and particle texture workflows as applicable. Save references via the project asset registry, copy/package dependencies correctly, and provide a harmless missing-asset fallback. Never use raw absolute local paths that break packaged games.
- Allow the same replacement and control through typed C++ and governed Lua APIs as part of the full weather scriptability contract. New visuals should fire the same world-space strike/impact events as stock visuals.
- Acceptance: import a creator-owned transparent texture and an animated strike effect, preview correct 3D/2D behaviour, save/reopen, Undo/Redo, restart Runtime, package/test standalone, vary branch density and frequency where relevant, confirm event parity and 75FPS budget. Baseline procedural fork and approved sheet lightning unchanged.
- **Status:** DEFERRED, NOT IMPLEMENTED. This is future development, NOT a current G4/five-gate visual signoff blocker. No merge or push authorized.

## 2026-10-30 - Owner weather scope and visual signoff correction
- Owner approved the CURRENT sheet and fork lightning visual appearance, including synced cloud glow. See docs/WEATHER_V2_G4_LIGHTNING_VISUAL_SIGNOFF_2026-10-30.md. G4 overall remains OPEN for controls, effects and integration.
- Owner deferred custom lightning artwork, physical impact gameplay, tree burning and complete Lua/C++ weather API coverage to FUTURE development. These are no longer blockers for this weather iteration, overriding earlier contradictory planning language.
- Owner requested continuing with WIND, RAIN and SNOW. Assess the native snow baseline after wind/rain; Snow V2 was previously deferred but this is now back on the visual-development agenda, with a bounded scope.
- Preserve all approved G1/G2/G3/Sheet/Fork visuals and the weather feature branch. Do not push, PR or merge.
