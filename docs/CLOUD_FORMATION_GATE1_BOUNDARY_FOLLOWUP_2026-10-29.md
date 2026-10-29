# Weather Gate 1: boundary and density follow-up, 29 October 2026

Gate 1 remains OPEN. This is a separate owner-review candidate, not visual acceptance or production performance sign-off.

## Isolation and source

Worktree: renegade-cloud-formations-g1-20261028; branch codex/cloud-formations-g1-20261028. Base HEAD 150eca4b761d3f62f122df3ab5e8536ec084f926. Accepted river Wicked gitlink and checkout ceb816b844b4ab2fa56a159ac60a4f65c931bdfc retained. All 45 original prototype authored files remain byte-identical to the initial snapshot. No commit, push, PR, merge, Weather Gates 2–5 or Snow V2 work.

Original and first-candidate owner windows were never closed, saved, reloaded or edited by this task. Earlier Studio processes ceased appearing during validation without a close/terminate operation from this task. The first candidate executable and project are retained. No claim is made to recover an unsaved camera or owner edits.

## Architecture and investigation

See CLOUD_FORMATION_GATE1_ARCHITECTURE.md for the bounded core-patch decisions. This pass changes Tools/night_sky/renegadeCloudFormations.hlsli, cmake/RenegadeCloudQualityOverlay.cmake and its parent overlay, fixture/proof tests, capture analysis, and the explicit separate-preview launch path in StudioApplication.cpp. The earlier metadata/Undo service and compact type selector remain the authoring boundary.

The native raymarch scales 96 samples over 15 km: roughly 156 m spacing even in shallow layers. It traces quarter-size images and reprojects half-size history. A high-sampling experiment removed some edge speckle but did not repair the broad macro-density feather. It also cost substantially more GPU time.

Formation geometry now uses approximate interior distances in metres, a 48 m condensation band and bounded 3D displacement. Internal condensate is a separate measured 3D field; uniformly dense bodies were rejected after visual inspection because their self-shadowing looked flat and hard. Geometry is calibrated to the former scalar body's median-noise isosurface, rather than its outer ellipsoid: the uncalibrated conversion inflated thin-layer 80% coverage to 91–92%. This was rejected.

New formations use a 48 m target march spacing with a 384-sample cap, conservative empty-space skips and bounded upper intersections. Native Legacy parameters and archive behaviour remain unchanged. Quarter/half spatial allocation stays the default. The local diagnostic cloud-reference-quality argument enables half-size rays/full-size history at greater cost; it is not an artistic default.

The owner's screenshots show Cumulus, about .8 coverage, Base 2280 and Depth 3190 at midnight. The disposable comparison fixture uses those weather values, fixed eye (0,2,0), pitch 35 and FOV 70. It is a comparable thin-layer replay, not an exact reconstruction of an unknown unsaved camera. The earlier saved owner scene had clouds disabled and no serialized camera, so it could not establish the rejected live state.

Studio and renderer default temporal AA are OFF. cloud-studio-taa is an explicit proof option; TAA is not established as the cause of the owner's screenshot. The confirmed spatial resolution loss exists in the cloud reconstruction itself.

## Validation

Studio and Runtime Debug/Release builds passed after the density change: BUILD/condensate-Release-build.log and condensate-Debug-build.log. RenegadeCloudFormationTests passed in both configurations, including real disk save/reopen, legacy/future schema, independent fields, and atomic Undo/Redo. Both Studio startup-smoke runs passed (construction smoke, not a rendered viewport test).

Release CTest: 16/16 focused checks and 58/58 SourceContract checks passed; these sets overlap and are not additive. Accepted river spline, underwater/swimming, flow/foam/transport, waterfall authoring/runtime and flight checks are included. Logs: BUILD/condensate-ctest.log and condensate-source-contracts.log.

All four final native GPU matrices passed clear/full/progressive opacity checks: 96 cases per 1080p backend and 32 per 1440p backend, 256 total, plus motion warm/settled image pairs. Backend identities are real HLSL6/DX12 format 2 and SPIR-V/Vulkan format 3 on RTX 4070 Ti. Moon new/quarter/full cloud-lighting ordering passed.

| Backend | Output | Cumulus .8 sky opacity >= .05 | Native .8 GPU frame | New .8 GPU frame |
|---|---:|---:|---:|---:|
| dx12 | 1920 | 83.46% | 1.52 ms | 11.24 ms |
| dx12 | 2560 | 83.37% | 1.82 ms | 13.85 ms |
| vulkan | 1920 | 83.42% | 1.50 ms | 11.11 ms |
| vulkan | 2560 | 83.34% | 1.79 ms | 14.48 ms |

All five maximum-coverage profiles occupy at least 99.996% of eligible sky in the canonical capture. Cumulus tiny-component area is below .03% of cloudy pixels. Motion versus same-time settled opacity deltas average .78–.92%; p95 is 5.49–6.27%. These metrics do not constitute aesthetic approval.

Timings cover the graphics frame, not an isolated cloud pass or a production game budget. Native nominal .8 occupies only about 17% of sky versus about 83% for new formations; workloads differ. Other Studio visibility/process conditions changed between exploratory runs, so earlier cross-run timings cannot establish a clean speed ratio. Performance remains an open Gate 1 concern.

Final GPU artifacts: BUILD/condensate-{dx12|vulkan}-{1920|2560}/. They contain PNGs, cloud-only opacity, backend.txt, captures.csv, opacity metrics, validation.json and contact sheets. BUILD/gate1-condensate-evidence.json records source, generated overlay and binary SHA256 manifests and verifies original preservation.

Runtime startup uses the new disposable project on Debug/Release with DX12/Vulkan. See BUILD/boundary-runtime-startup-results.json and the matching snapshots. The initial copied project failed with exit 25 because its scene/flow sidecars retained the previous project ID. Only the new project's text metadata was remapped; all four subsequent startups succeeded. Native bootstrap logs confirm requested/actual backend and Main Level entry. Recheck final results before claiming success.

## Separate owner preview

Desktop shortcut: Renegade Cloud Boundary G1 Preview. Executable: BUILD/gate1/Studio/Release/RenegadeStudioCloudBoundaryG1.exe. Project: BUILD/cloud-formations-boundary-preview/CloudFormationBoundaryPreview.renegade. Launcher: Tools/night_sky/LaunchCloudBoundaryG1Preview.cmd. The new project has its own identity; the first preview project is not overwritten.

The scene starts at midnight with Full Moon and the screenshot's thin-layer weather. Wind is deliberately paused for comparison; continuous motion is separately exercised by the GPU proof. Formation, Coverage, Base and Depth remain independently editable in Environment / Clouds.

Still open: owner aesthetic approval (including residual grain, close edges and high-coverage appearance), manual native persistence/Undo review, representative complex-level fog/water/terrain-shadow/reflection checks, game-package acceptance, a production performance budget and independent exact-source review. The optional second stratum plus high Cirrus needs its own multi-band visual review.

Safe rollback: leave other worktrees and preview files intact. Close only the new candidate when the owner no longer needs its own edits. Rebuild/relaunch from this isolated worktree; do not overwrite an executable while it is open.

Only this task's disposable linker .ilk files and selected superseded auxiliary GPU PNGs were pruned for disk space. Source, owner projects, binaries, canonical before/after profile examples, CSV/JSON metrics, logs and the final matrices were retained. BUILD/owned-disposable-proof-pruning.json records removed file names and hashes.

## Final native preview observation

The separate DX12 Studio PID 33404 reached the actual Main.WISCENE Environment viewport with the correct Cloud Boundary Gate 1 Preview project label, 3 scene items and Undo/Redo 0/0. Image: BUILD/studio-boundary-before-level.png. The startup moved from Story Flow into the level asynchronously. A guarded attempted Main Level card click detected that the screen had already changed and sent no input. No owner edits were made or saved.

Observed: coherent three-dimensional bodies and larger gaps, with remaining soft edges and some grain. This is not photorealism or owner acceptance. The actual viewport review is material evidence in addition to synthetic images.

All final thin-layer standard, optional TAA and optional full-history reference proofs completed on DX12/Vulkan. TAA comparison measurements are in BUILD/condensate-taa-comparison.json. Studio and renderer default AA are off; the optional mode has not been established as the cause of the owner's original screenshot. The proof's manual lunar elevation is 40 degrees while this preview follows the automatic lunar orbit. Do not treat those RGB lighting views or the different viewport aspect ratios as exact camera/lighting parity. Cloud-opacity coverage metrics are independent of that RGB brightness difference.

Final Runtime rechecks after the density change passed in all four configuration/backend combinations with scene-synced diagnostics and normal exit 0. BUILD/condensate-runtime-{Release|Debug}-bootstrap.log records actual Vulkan startup; per-run diagnostics and requested DX12/Vulkan combinations are in boundary-runtime-startup-results.json. The previous numerical gate checks remain scoped evidence, not full package/complex-scene approval.

## Owner rejection — 29 October 2026

Owner: “they look worse”. The actual Studio screenshot shows solid, sculpted blob shapes and excessive distant fragments. The boundary/density candidate is REJECTED. Numerical coverage/build passes do not offset this visual failure. Preserve this executable/project and its evidence; do not promote or ask for approval of this appearance again. Next investigation replaces explicit ellipsoid bodies with continuous 3D density formations and studies the supplied weather-pack architecture without importing proprietary assets.
