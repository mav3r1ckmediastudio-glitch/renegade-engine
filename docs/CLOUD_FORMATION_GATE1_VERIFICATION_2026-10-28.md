# Weather Gate 1: isolated owner preview verification
28 October 2026. CANDIDATE; owner visual approval and release gate remain OPEN.

## Source and preservation
- Branch: codex/cloud-formations-g1-20261028. HEAD/base: 150eca4b761d3f62f122df3ab5e8536ec084f926.
- Fetched origin/main was 150eca4; the root Wicked gitlink and working HEAD remain
  ceb816b844b4ab2fa56a159ac60a4f65c931bdfc, the accepted river-enabled fork.
- No commit, push, PR or merge. The source is an uncommitted isolated worktree.
- All 45 snapshotted authored files in the original Moon/Clouds worktree match
  their preflight SHA-256 hashes. Other worktrees and their dirty Wicked sources
  were inventoried and never edited. Source manifest: BUILD/gate1-evidence.json.
- The earlier Studio processes were neither closed nor overwritten. The new
  separately named executable opens a separate generated project.
- No Weather Gates 2-5 or Snow V2 implementation was undertaken.

## Implementation
CloudFormationService owns version-1 Environment metadata and one atomic command.
Legacy/missing/unknown schemas retain the native formation path. The existing
Clouds disclosure adds one selector: Legacy plus Cumulus, Towering Cumulus,
Stratocumulus, Overcast and Cirrus. Coverage, Base, Depth and shadows remain
independent. The authored second layer is preserved.

The per-sample model combines independently placed, irregular connected 3D bodies
and height profiles, the native 3D Perlin-Worley body signal, and subtractive fine
erosion. The field is evaluated by camera and shadow rays. The native GPU shape
texture was actually generated/read back on both APIs: body P10 was about .6024,
P95 .8582. Density normalization uses this measured range rather than a cutoff
below the entire input signal.

High-only Cirrus intersects its actual upper altitude band. Its render-distance
limit is derived from the planetary-shell tangent distance. The artistic horizon
fade is normalized by the layer altitude; actual-distance atmospheric scattering
remains intact. This fixed the measured 100% Cirrus failure without lowering the
coverage-test threshold. Legacy and low-cloud intersection/fade paths stay native.

The existing padding0 float in VolumetricCloudLayer is a transient formation ID.
Both layers initialize it to zero. No GPU layout or native archive version change;
ShaderWeather remains 1040 bytes and sky_exposure remains byte 136. Consistent
C++ and embedded HLSL6/SPIR-V builds were completed.

Main authored changes:
- EngineBridge/include/renegade/bridge/CloudFormationService.h
- EngineBridge/include/renegade/bridge/NightSkyService.h (shared hydration)
- Tools/night_sky/renegadeCloudFormations.hlsli
- cmake/RenegadeNightSkyOverlay.cmake (bounded generated-core overlay)
- Studio/src/StudioApplication.cpp and .h; NightSkyStudioIntegration.cpp
- Tests/CloudFormationTests.cpp; CloudFormationGraphicsProof.h;
  NightSkyStateTests.cpp; CloudsV2VolumetricContract.cmake
- CMakeLists.txt, Tests/CMakeLists.txt and Build-Studio-Windows.ps1 register the
  new native test in the normal CI aggregate and manifest.
The bounded Moon/meteor dependencies from the original uncommitted prototype
were ported alongside all existing river resources and registrations.

## Reproduction and mismatch diagnosis
The old fixture authored an extra .16 second layer, used different heights in
many views, advanced wind/time sequentially, and disabled TAA. Three distinct
Studio candidates were running. The saved owner file additionally has clouds
OFF, coverage 1, exposure 1.7 and no saved camera; it cannot represent the rejected
unsaved view. No attempt was made to overwrite/save/read unsaved owner memory.

The new capture fixture explicitly uses Base 1500, Depth 5000, layerSecond zero,
a fixed 70-degree FOV, recorded positions/pitches, exposure one and frozen wind.
History resets after discrete parameter/camera changes. Above-volume views,
rotated/translated viewpoints, twilight, new/quarter/full moon and continuous
wind/history convergence are captured. The separate Studio preview opens this
same explicit formation/base/depth state; the actual native viewport was inspected
in BUILD/studio-before-enter.png. The owner's original unsaved camera state is
still not reconstructed and should not be claimed as an exact historical replay.

## Completed validation
- Studio Debug, Studio Release, Runtime Debug and Runtime Release: build exit 0.
  Final renderer build logs: BUILD/gate1-strata-Release.log and -Debug.log.
- Studio startup-smoke: exit 0 in Debug and Release.
- Native CloudFormationTests: pass in Debug and Release, including every profile,
  independent authored fields, Undo/Redo, actual disk WISCENE save/reopen, shared
  hydration, future schemas and unrelated-metadata preservation.
- Focused CTest: 16/16 pass, including sky/Moon/meteors, cloud contracts and native
  river, foam, underwater, swimming and waterfall tests.
- Broader source contracts: 58/58 pass. These counts overlap; do not sum them into
  a unique-test total. This is not a full unfiltered repository CI claim.
- Runtime launched the disposable project on DX12 and Vulkan in both Debug and
  Release. Each matched its own live diagnostics PID and closed normally with
  exit 0 after application-frame state became available.
- Complete embedded HLSL6 and SPIR-V compilation passed. API identities are read
  from the real device: RTX 4070 Ti, shader format 2 (DX12), 3 (Vulkan).
- git diff --check passes for root staged/unstaged changes and the generated
  Wicked overlay. No assertions/tests were suppressed to obtain a pass.

## Real GPU results
256 parameter/view cases across both APIs/resolutions, plus motion pairs. Each
case includes a cloud-only opacity target. Sky coverage counts opacity >=.05
on positive-elevation sky rays, excluding ground, objects and the lunar disc.
Cloud components/edge transitions are recorded separately, not inferred from RGB.

| API | Width | Cases | Cumulus at 80% | Minimum of all five at 100% | Native 80% GPU frame ms | New 80% GPU frame ms |
|---|---:|---:|---:|---:|---:|---:|
| dx12 | 1920 | 96 | 78.77% | 99.96% | 4.40 | 6.37 |
| dx12 | 2560 | 32 | 78.64% | 99.98% | 6.46 | 8.08 |
| vulkan | 1920 | 96 | 78.82% | 99.95% | 4.86 | 7.27 |
| vulkan | 2560 | 32 | 78.66% | 99.98% | 5.33 | 9.98 |

Every matrix passes the unchanged clear/full/progressive-coverage checks.
Mean same-time wind/history opacity difference is approximately 1%; the 95th
percentile is approximately 5.5-5.9%. New/quarter/full moon masked-cloud brightness
ordering passes on both APIs at both resolutions.
Evidence: BUILD/gate1-strata-dx12-1920, -dx12-2560, -vulkan-1920 and -vulkan-2560.
Each contains backend.txt, captures.csv, opacity-metrics.csv, validation.json,
PNGs and a formation contact sheet.

GPU timestamps above cover the graphics frame, not an isolated cloud pass.
The native baseline has much less occupied sky at its nominal 80%; topology and
workload differ. Three older Studio sessions also remained open. These are
inspectable reference timings, not a production game budget or an FPS promise.

## Owner preview and remaining checks
Desktop shortcut: Renegade Cloud Formations G1 Preview.
Executable: BUILD/gate1/Studio/Release/RenegadeStudioCloudFormationsG1.exe.
Launcher: Tools/night_sky/LaunchCloudFormationsG1Preview.cmd.
Project: BUILD/cloud-formations-preview/CloudFormationPreview.renegade.

Environment -> Clouds contains the type selector and existing controls.
The comparison scene deliberately freezes animation; normal authored weather
movement is preserved and separately tested. Full Moon is enabled for switching
Time of Day to midnight. The preview project is disposable; original scenes stay
in their original applications.

Still required: owner aesthetic/motion acceptance, native manual selector/slider
Undo/Redo and Save/Reopen, representative complex-level cloud/terrain-shadow,
fog/water/reflection stress checks, game-package acceptance and independent review.
Native quarter-resolution cloud reconstruction can still show grain at some
boundaries; numerical success does not close the visual-quality gate.
Advanced authored second-layer + high-Cirrus combinations retain native broad
ray bounds and need a separate multi-band visual check before release.

Rollback: leave the original applications/worktrees as they are; close only this
separate candidate when its own edits are no longer needed. Nothing was merged.
