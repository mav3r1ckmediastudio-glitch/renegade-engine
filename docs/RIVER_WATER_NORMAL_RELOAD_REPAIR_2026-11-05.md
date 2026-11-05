# River water normal-map reload repair — 5 November 2026

## Reproduced problem
The owner's saved five-river fixture loads each WATER spline with normal strength 1, UV animation V -0.001, frame rate30 and a missing normal resource. Both the original project and a disposable copy resolve the stored filename to Content/Scenes/waterNormalMap.jpg, where no file exists. The supplied map is deployed beside Studio and Runtime in Content/river.

The pinned Wicked MakePathRelative implementation falls back to the filename when an absolute resource path cannot be made relative across drives. Studio was running from E while the project was on C. Ordinary SceneDocumentService saving does not switch the resource manager into embedding mode. This explains how the persisted stock binding becomes unresolved; it is independent of the riverbed locality changes. No upstream source or pointer is changed by this repair.

## Bounded repair
RestoreBundledRiverWaterNormals scans native WATER splines at the GPU adoption safe point. Only a missing/invalid texture whose existing name identifies waterNormalMap.jpg is rebound to the installed stock asset. Valid bindings, custom filenames and intentionally empty slots are preserved. Tint/opacity, clarity, normal strength, UV tiling/offset, flow animation, refraction and roughness are not edited. Actual resource-manager keys and retained normal-map bytes support the existing embedded Test Game path.

Both SceneDocumentService commit (Studio) and SceneService startup load (Runtime) hydrate the prepared candidate before replacing the active scene. A missing/unreadable required bundled map refuses adoption and preserves the active document. Headless tools do not enter GPU resource loading. Known stock normals can be recovered on reopening legacy scenes without rewriting an owner's saved file.

## Verification
Targeted Release compilation: RiverSplineService, SceneService and SceneDocumentService; combined Studio preview uses the existing Preview3 Studio/TerrainService/RiverTerrainLayerService objects. Native build and link exit0. This is not a clean full build or CI claim.

Explicit Windows/DX12 proof Tests/RiverWaterNormalPersistenceProof.cpp is a fixture-driven executable, not an unattended headless CTest. It requires a marked disposable copy of the five-river fixture. Its CMake target deploys the stock map. Commands: WaterPersistence <disposable-project> <output> warm|cold|missing.

- Studio opens the broken fixture: five real normal textures restored.
- Custom missing filename, intentionally empty slot and valid stock resource retained.
- Broken stock reference repaired with authored water optics, UVs and animation unchanged.
- Studio SaveScene and reopening pass for all five rivers.
- A separate process uses Runtime SceneService to open the saved scene: five normals valid.
- A separate process with the proof-only bundled asset hidden refuses loading and preserves the sentinel active scene. Asset restored afterward.
- All three proof processes exit0.

Native StudioRenderPath before/after and RuntimeRenderPath after captures load the same disposable terrain/five-river fixture at 1920x1009 with Storm/rain. Before: all five normal resources invalid. After: all five valid; normal strength1 and V speed-.001 preserved. UV offsets advance over recorded frames in both render paths. GPU-frame medians before Studio11.170ms, repaired Studio10.500ms, repaired Runtime10.109ms; run variation and camera scope prevent attributing a speedup or claiming resolution of the historical58FPS issue. Cloud shaders are unchanged. Screenshots and raw material/motion/timing evidence retained externally.

Evidence folder: CloudDiagnosis20261105/WaterNormalRepair. Build/run helpers: build.py, prepare-proof.py, run-proof.py, prepare-visual.py, run-visual.py. Earlier read-only material inspection outputs identified the missing resource but their abbreviated harness shutdown returned1; they are not passing product tests and are superseded by the exit0 regression proof above.

## Delivery and limits
Separate owner preview RenegadeStudio.WaterNormalPreview.exe and TEST_RIVER_WATER_NORMALS.cmd reuse the complete existing owner-preview package and deployed assets. The original Studio/Preview3 executables are preserved. Runtime source loading is repaired; the bundled existing Runtime executable is not replaced by this Studio-only preview. Standalone full gameplay/package acceptance and owner sign-off remain open. No commit, CI, main merge or external distribution.

Full packaged Studio smoke check: staged preview SHA25681d22423ce5443980c872c23413741aa69553d1861deac70c1ae8af47bf03452 launched via its native Windows entry, completed reveal/identity, opened the marked disposable project and Starter Level through Story Flow. River Water5 selected and focused through ordinary hierarchy/F key. Surface specular/normal detail visible in native-screen.png, author controls unchanged (normal1, flowV-.001, clarity.874), responsive UI and75–78 displayed FPS in the clear-weather overview. This is not a renewed Storm FPS acceptance claim. Owner saved Starter.wiscene SHA256 remains51e3e8f19c26e648da6d75be2ce82edee8e20f35d0c772b624677e0eea95955b. Corrected native preview left open for owner review. The previous profiling app was not closed by the repair automation.

Owner visual acceptance: the owner confirmed the corrected combined preview works a treat and subsequently authorized the focused carving/water-normal fixes to merge to main. This does not accept any cloud changes or authorize an external alpha release.
