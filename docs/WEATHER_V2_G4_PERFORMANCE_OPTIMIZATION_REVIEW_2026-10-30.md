# G4 performance optimization review candidate - 30 October 2026

Owner authorized a more aggressive measured pass while preserving approved appearance and work. Local Release candidate only, no commit/push/merge. Root HEAD f073233a61e8d8814cdc29a610e242689832c37f; Wicked HEAD 7e2326c0338e625f4405d8fe5150fde9547eaded. Source digest: 6a632022e6ec0f28553a7f1fe3f9ae02e6e06b56596d7b51afd774d9e34fb7d1. Exact source/binary identities: BUILD/g4-critical-source-manifest.json.

## Retained changes

Studio skips camera-preview resolution unless a Player Start is selected, and skips capsule selection work when there is no eligible click. Render/Compose share a frame-local Player Start result keyed by scene, graphics frame, scene revision, command revision and metadata count. Every read refreshes the live transform; missing transform/tag falls back to normal validation. Add Player Start keeps uncached validation. Private PlayerStartFrameCache.h has a headless regression target covering create/delete, duplicate markers, Undo/Redo, settings, direct edits, scene isolation and live drag pose.

Renegade formations1-5 use a128x128 hemispherical lighting/reflection cloud cache, integrating over half as many intervals. The physical interval length and Beer-Lambert integration remain consistent. Its four temporal phases, initialization and continuous updates remain native. Texture changes rebuild variance/reset capture. Main camera clouds keep the approved resolution, adaptive sampling, density field, geometry, illumination and motion-aware reconstruction, including the separate transient lightning buffer. Native Legacy capture stays at its requested resolution/full integration budget. Fork/sheet geometry, rain/snow and thunder implementations/assets are untouched.

This IS an explicit off-screen cache-quality tradeoff, not a claim of identical sampling or universal visual parity. Bare launch argument cloud-capture-reference restores original requested256 cache/full intervals while retaining CPU improvements. Wicked argument matching is literal; do not prepend -- to that reference argument.

## Measurements and visual evidence

Controlled DX12 Release Runtime, same project/camera/90% coverage, cloud base750m/thickness6500m, rain/lightning off, moving clouds,1080 samples after120 warmup:
| View | Cache128 full intervals FPS | Cache128 half intervals FPS | GPU median before/after ms | GPU P95 before/after ms |
|---|---:|---:|---:|---:|
| Horizon | 75.38 | 95.76 | 12.796 / 9.926 | 15.377 / 12.512 |
| Up35 degrees | 74.89 | 93.48 | 12.783 / 10.156 | 14.839 / 12.135 |

Cache pass median fell from5.735 to2.858ms (horizon) and5.592 to2.843ms (up35). Main raymarch cost was essentially unchanged. This isolates the interval-budget gain at the same128 cache size. The separate corrected256-versus128 comparison showed an additional roughly0.8ms cache saving; do not add FPS differences from independent runs or confuse the already-reduced128 reference with the original256 approved build.

The actual dense editor displayed63 FPS in BUILD/g4-opt-editor-live.png versus the earlier48-50 FPS screenshot/test. Treat this as an indicative on-screen result, not an independently complete matched quantitative editor A/B. The attempted optimized-editor CSV had Runtime-camera signatures and is excluded from CPU-before/after claims; parent/Test Game inherit diagnostic filenames and can overwrite each other's output. Controlled standalone timing CSVs are separate, validated, and retained. Final normal review has no weather/camera/freeze profiling override.

Frozen1920x1009 output comparison, original256/full versus final128/half: sky mean absolute channel difference0.075714 out of255;0.043018% of sky channel samples differ by more than2, maximum23. Full-image mean0.083409. Primary volumes remain visually close in this fixture. These numbers do not prove every formation, night/reflection condition or motion case is identical. Owner should recheck water/reflections, thin distant clouds, night/moon views and continuous movement.

Rejected experiments: coverage-admission hoisting gave no useful measured gain (up35 GPU12.803 vs12.812ms) and was reverted. First reference runs used an incorrect -- prefix, so both rendered128; discard those as reference comparisons. One later admission-baseline run had a1.5s disturbance; no FPS gain is claimed from it. The retained capture-budget comparison above is clean and independent.

Evidence: external optimization-r1/capture2-ab/{timing,visual}, summary.json, image-comparison.json and PNGs. Local logs BUILD/g4-opt-capture2-timings.log, g4-opt-capture2-visual.log. These are native DX12 timestamps, not PIX captures.

## Verification and owner review

BUILD/g4-optimization-final-build.log: normal Studio/Runtime plus intended regression targets rebuilt successfully in Release. BUILD/g4-optimization-final-tests.log:11/11 passed (seven focused weather tests, Phase6 player, player prefab, player view source contract and new frame-cache tests). No full Debug/Release four-build CI claim. Saved original Main.wiscene hash unchanged and all9 original WAV hashes match. The full approved baseline binaries/source are retained in external optimization-r1 backups; no wholesale reset is safe because pre-existing owner work is dirty in both repos.

Final files owned by this optimization: Studio/src/StudioApplication.cpp, Studio/src/PlayerStartFrameCache.h, Tests/StudioPlayerStartCacheTests.cpp, Tests/CMakeLists.txt; WickedEngine/WickedEngine/wiScene.cpp, wiRenderer.cpp and shaders/volumetricCloud_renderCS.hlsl. Tools/night_sky/renegadeCloudFormations.hlsli and Wicked formation/HF files were restored after the rejected hoist. RuntimeApplication/RuntimeWeatherDiagnostic are restored to the preceding approved normal implementations. No new creator UI or serialized feature exposure.

The normal owner review opens an isolated copy of the original unsaved snapshot under external optimization-r1/owner-review-project. Earlier diagnostic Studio PID15216 and any work in it are preserved/minimized, not discarded. Normal-review launcher BUILD/Run-Weather-G4-Performance-Review.cmd; original capture quality comparison BUILD/Run-Weather-G4-Performance-Reference.cmd. Use one foreground renderer for comparable timings. Normal review uses original authored rain/lightning/thunder and has no forced Storm/coverage/camera/freeze settings.

No75FPS guarantee across all settings/views, and no new performance/visual signoff is claimed. Owner approval of the preceding cloud/lightning/thunder repair remains recorded; this optimization is a separate review candidate.

## Final normal application smoke check

Final normal Studio PID42896, titled Weather Performance Review, opened the fresh owner review copy with authored rain/lightning/thunder and no profiling overrides. BUILD/g4-normal-review-now.png shows65 FPS with rain visible. Its actual Play child Runtime PID5596 launched successfully, on the75-Hz primary display at1920x1009, showing75 FPS in BUILD/g4-performance-normal-testgame.png. These are observed on-screen values, not a long-duration all-effects median/P95 claim. The Runtime and parent editor are left open for owner review; closing Test Game returns to the new editor. Previous diagnostic PID15216 remains preserved/minimized. Final source digest6a632022e6ec0f28553a7f1fe3f9ae02e6e06b56596d7b51afd774d9e34fb7d1; both Release binary SHA256 values are in the manifest. Performance is improved, but the editor is still below75FPS in this view and owner acceptance remains pending.


## Owner acceptance - 30 October 2026

Owner replied "im happy with that" after the update reporting normal Test Game observed75 FPS, editor around65 FPS (previous48-50), controlled cloud-only94-96 FPS uncapped,11/11 regression checks and the lighting/reflection cache tradeoff. Record acceptance of the presented optimization, current visual quality and current performance for this scoped review. This supersedes preceding statements that owner approval of this optimization is pending. Preserve the accepted candidate identified by source digest6a632022e6ec0f28553a7f1fe3f9ae02e6e06b56596d7b51afd774d9e34fb7d1 and BUILD/g4-critical-source-manifest.json. This is not a claim of75 FPS in every editor view, full four-configuration CI, completion of all Weather V2 gates, or authorization to commit/push/merge. No implementation or build change accompanies this acceptance record.
