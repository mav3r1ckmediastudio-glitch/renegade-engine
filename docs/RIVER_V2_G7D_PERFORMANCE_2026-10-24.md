## G7D flow arrows and spherical spline handles — owner visually accepted

Owner request: missing flow arrows; replace tiny crosses with sphere nodes. Owner response after live candidate check: "looks good, thanks for that".
Fixed DrawArrows connected routing and moved invocation out of Inspector layout into the per-frame native river overlay path. Connected arrows are cached during domain rebuild from actual projected fluid velocity; no per-frame graph/domain rebuild or GPU readback. Original single-river path remains supported. Toggle text updates immediately; debug lines remain legible over transparent WATER.
Replaced native river-node screen crosses with shaded editor-only spherical handles, constant9px radius (hover11/selected12), cyan normal/orange selected and14px picking radius. Uses recursive world transforms. No authored objects/materials, shader or Wicked edits.
Build exact command: powershell.exe -NoProfile -ExecutionPolicy Bypass -File B/build-arrow-sphere-fix.ps1; Studio+Runtime Release x64 build0,54.18s. Visual actual project reopen verified spherical handles and connected directional arrows;75FPS. Evidence B/connected-arrows-on-sphere-handles-v1.png and latest creator-ui-current.png.
Preserved owner six live edits by Save in private project before graceful replacement; previous saved scene owner-before-arrow-save.wiscene and newly saved owner-before-overlay-ui-retest.wiscene retained. No test edits made to authored geometry by this overlay task. Current private Studio PID32792, HWND4326830 on left monitor; creator-launch.json authoritative. Same G7D Drawing Candidate shortcut. Full export/release gate remains pending; owner acceptance applies to these visual controls only.
Changed: EngineBridge/src/RiverWaterwayRenderer.h/.cpp;RiverFlowRenderer.cpp; Studio/src/TerrainInspectorSections.cpp;RiverFlowStudioIntegration.cpp;StudioApplication.cpp. Source backup before-arrow-sphere-overlays.zip; binary backups creator-bin-v1/before-arrow-sphere-fix.
No commit/push/merge. Accepted packages and private Wicked diff must remain unchanged.

Package hashes:
\RenegadeStudio.exe: f817b65ce85c318d4ab0f1b33cfae8f6634cdeff403c23498841d0b12bb14f53
\Runtime\RenegadeRuntime.exe: 83cea1c0121c98ac56b246e0eaa8874ab73d856c938a9ee3d1c3b23099f5c277
\RenegadeRuntime.exe: 83cea1c0121c98ac56b246e0eaa8874ab73d856c938a9ee3d1c3b23099f5c277


## G7D drawing/performance candidate verified in actual editor and Test Level — latest checkpoint

Supersedes the pending retest wording below. Private Studio PID23880 / HWND2229548; creator-launch.json and open-cap-fixed-package.json are authoritative.
Actual UI drawing from River Water5 Point2 succeeded. Inspector reports Connected WATER/shared flow+foam, visible contact foam and native transparency/refraction;75FPS. Single Undo removed tributary and restored bed (66items,Undo0/Redo1); Redo recreated joined native river (69items,Undo1/Redo0),75FPS. Saved via UI, then REOPEN:69items,Models7,foam visible,75FPS.
PLAY launched private Runtime PID14668 with --project private Intermediate/TestLevelSnapshots/.../TestLevel.renegade and ready-event handshake. Runtime DX12 screenshot shows RTX4070Ti,1264x681,75FPS,river/foam visible. Closed only that Runtime via CloseMainWindow; Studio reports TEST LEVEL COMPLETED and75FPS.
Editor remains open with Models > River Water6 > Point1 selected and focused. Point1 is the free upstream control; Point2 attaches to RiverWater5 Point2. Shared foam settings are in FLOW Inspector. Only one flat two-inlet confluence is supported. No new owner visual acceptance or complete G7D release is claimed.
EvidenceB: active-connected-75fps-after-draw.png;active-draw-undo-baseline-75fps.png;active-draw-redo-75fps.png;saved-reopen-75fps.png;test-level-runtime-75fps.png;creator-ui-current.png.
Studio SHA1c3fb9ebd819bbf5206e7aea8125033e5d2cbc01063bcaab95e2ee56b500d9b4.
Runtime root/child SHA36c0f3f5e957655bf6b347d3ba4fdc36fdeb7e5b234922087dc94000a903f034.
Additional unconnected terrain regression: run-cap-terrain-regression.ps1 rebuild0; RenegadeTerrainTests1/1PASS0.21s.
Accepted G6/G7C EXEs/shaders, both repo HEADs and private Wicked diff preserved. Final audit cache-and-caps-preservation.json; allowed source list explicitly includes all known river integration edits since original baseline.
Full BuildGame export/relocation, complete StoryFlow TestGame, long-session and more complex waterways remain pending. Mountain work remains paused. No commit/push/merge.


## G7D measured animated-cache regression and connected terrain caps — 2026-10-24

The owner-reported 4 FPS regression has a measured cache cause: WATER texMulAdd.z/w animate every native frame. Connected Fingerprint and SurfaceBinding Shape included these transient offsets. Hash/guard now retain UV scale x/y only, consistent with the original single-river renderer. Native UV animation remains enabled. No Wicked changes or shader replacements.

Commands (PowerShell -NoProfile -ExecutionPolicy Bypass -File under B):
- run-animated-before.ps1: deliberate reproducer FAIL1; 15 rebuilds/30 positive-time frames, Prepare mean668.79ms/max1348.7ms.
- run-animated-after.ps1: PASS0,32.80s; 0 rebuilds/30frames, Prepare mean.0689333ms/max.108ms; native Z/W advanced .35/-.49.
- build-cache-fix.ps1: Studio+Runtime+3 CPU targets0,44.71s; CTest3/3 PASS1.64s.
- build-open-cap-fix.ps1: Studio+Runtime+3 CPU targets0,32.22s; CTest3/3 PASS1.62s.
- inspect-terrain-scene.ps1: actual privately saved editor scene, fresh SceneService load and actual native Update. Initial no-obstacle transport PASS; terrain alone blocked outlet at(20,70.5), rock alone PASS. Existing linked bed intentionally conceals unconnected endcaps with dry terrain.

Connected linked beds now use their ordinary radial channel cut through their terminal caps, instead of the dry concealed-end taper. This provides physically wet open computational boundaries without bypassing solid exclusions or cap-connectivity checks. Unconnected bed taper remains unchanged. Connection/disconnection and drawing run through the existing terrain transaction, so this derived change is undoable. Terminal finite WATER mesh ends are open boundaries and need visual review; no end-cap overlay or fabricated flow-through-solid workaround.

Final real-scene inspection PASS0,19.87s: terrain672 triangles, rock2506; combined85936 wetcells. Exact terrain Undo heights/document and Redo transport topology PASS. SceneInspection proof recomposes only its in-memory loaded copy; it does not overwrite the saved project.
Tests/RiverWaterwayNativeProof.cpp --inspect <actual-scene> is the bounded actual-scene regression path.

Owner settings snapshot owner-before-drawing.wiscene retained. Private UI draw succeeds, single Undo66items/Redo69items observed. Initial cache-fixed editor75FPS but shared renderer refused closed terrain caps, so that observation is NOT proof of active connected-flow performance.
Current private candidate is open-cap-fixed Studio PID23880; creator-launch.json is authoritative. Owner UI drawing must be repeated with active connected WATER status before handover. Previous saved drawn failure retained drawn-before-open-caps.wiscene; saved baseline after our Undo retained before-open-cap-ui-retest.wiscene.
Protected G6/G7C EXEs/shaders, both repo HEADs and private Wicked diff audit PASS. No commit/push/merge. Build/export relocation and complete G7D lifecycle remain open.
