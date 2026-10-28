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


## G7D DRAWING native oracle PASS; real editor stock-optics failure FIXED, fresh candidate needs retry

Owner complained about long silence ("Have you stalled ... not spoken in nearly an hour"). Give clear user commentary at least every60s; do not disappear in lengthy analysis. Still MUST deliver working drawing, not another plan or premature candidate.

build-drawing-ui-v1.ps1 Studio+Runtime+proofs build0 244.81s; affected CTests3/3 PASS1.46s including disabled receiver profile Undo/rollback assertions.
Implemented/reran actual new drawing-native geometry oracle:
implement-draw-native-oracle.py EXECUTED; Tests/RiverWaterwayNativeProof.cpp now supports --draw (actual CreateRiverTributaryCommand). run-draw-native-v1.ps1 rebuilds native proof and stages B/native-bin-v2/NativeDrawProof-v1.exe next to actual normal-map Content. Actual Create0 and fresh Reopen0,24.43s. Both actual Wicked generated bank/span maxerror1.90735e-6m,44800 native-triangle footprint samples,3263wet, saved/fresh hash8556916249514424735. Native drawing Undo/Redo and connected edit Undo/Redo PASS. Original receiving native mesh bytes/controls unchanged.
run-drawing-shared-v1.ps1 composed shared GPU create0/fresh0 27.76s after union fix; mass34.4184/max.293085 create;34.3901/max.289215 fresh, domain204800cells/rebuild1. Frozen profile history/Undo unchanged. Pixel check via original check-composed-pixels.py substituted drawing-shared-v1: native_restore_max0/outside_water_max1. Actual engineering image already inspected before; not an owner visual pass.

Preserved owner live changes:
Old private package had genuine unsaved UI settings changes (Undo2/Redo1, intensity1.8,lifetime~.556,turbulence1.5) and camera navigation. Cancelled earlier Unsaved close dialog; then explicitly clicked SAVE in PRIVATE copied project, tab star cleared. owner-before-drawing.wiscene and owner-before-drawing-v2.wiscene snapshot SHA64e9fcd8652143b5437579a247b87431150027cb964596f1023f2b1647853eee. Source accepted G6 project never edited.
update-creator-drawing.py then gracefully closed old PID12700 and installed new drawing candidate; source/previous hashes in creator-update-result.json, backups creator-bin-v1/before-drawing. Owner or remembered state opened private project/river afterward. Our actual UI gesture was captured in draw-owned-tributary.py / drawing-gesture-v1.json, from node2 atcapture(873,524) through(835,495),(780,462),(720,432),(660,404),(600,380), correct upstream side under camera's X/Z gizmo. It FAILED safely: Models6/entities66/Undo0 and scene not dirty. Native log OBSERVED exact error "Tributary creation refused: Tributary optics initialization failed". Scene retained, no orphan. drawing-stock-failure-console.png preserves failure context. Custom Studio Console does NOT display wi::backlog posts; full error was in package log.txt. That log was overwritten on relaunch; retain this observed excerpt, do not claim full old log archived.

Cause: existing SetRiverWaterMaterialCommand::Execute returns false for an EXACT NO-OP. Native new stock river and real stock receiver match, while white-material test fixtures did not. FIX in RiverWaterwayEdit now executes existing copy command then validates resulting captured optics field-by-field; exact matching settings are success. New CPU test sets all four receiving fixtures to actual stock optics first, preserving native no-op coverage.
Also actual drawing revealed potential node/gizmo input capture and stale paused-flow status. Studio now skips scene-icon picking while anchored drawing and skips gizmo_.Update while drawing mode armed. Native Translator::IsInteracting is state!=IDLE (includes hover), so riverFlow_.SetSuspended now uses Studio gizmoDragActive_ rather than hover. No Wicked edits. Removed misleading "See Console" from default error text; developer full error remains wi::backlog/log.txt.
fix-stock-tributary.py backups before-stock-tributary-fix.zip.
build-drawing-ui-v2.ps1 build0 69.49s,3/3 CPU PASS1.50s (stock optics + all previous cases). build-draw-input-v3.ps1 Studio build0 28.48s, final actual-drag suspend and no-gizmo-in-draw guard. New editor source warning C4834 at old unrelated line11075 retained, no error.

CURRENT LIVE process AFTER second safe update:
update-creator-drawing-v2.py completed0; preserved same saved private scene SHA; old candidate closed gracefully, backups before-drawing-v2. New PID25284, must RE-ENUMERATE HWND (old3671364 invalid). Latest package Studio SHA b2b72438bf608329103c4ae9f593caeec76618c292aed8b276e239bdf2259d8a; Runtime root/child SHA1c81130650815ccd42fe091be119610654ab775f7e29366601655a9ebe8c17e7. Exact manifest creator-update-v2-result.json.
It is starting through normal intro/handshake (can take~30s). Do not alter startup product code. It previously opened the private River project after intro (possibly owner or remembered state). If Hub, open the one private recent project then level. User told no action required until finished. No working drawn tributary is saved yet. MUST retry actual UI now; new no-op fix not yet manually proven.
Current private source has no new branch, no authored changes from failed drawing. Main RiverWater5 flat Y-.15 width4, interior Point2 near rock. Root already LINKED TO TERRAIN (verified UI); other older mountain test rivers untouched. Owner root profile intensity1.8/lifetime.556/turbulence1.5 preserved. Shared renderer ignores turbulence/sourceWidth and disables knobs when connected; do not silently modify owner values.

Useful owned UI:
capture-creator-ui.py re-enumerates exact PID from creator-launch.json; captures if owned foreground; writes creator-windows.json. owned-ui-input.py verifies PID/currentrect, activates exact owned HWND, checks foreground then click. Never reuse modal-first or old bounds without enumeration.
Most recent maximized window rect[-8,-8,1928,1040], full capture1936x1048. Models row230; RiverWater5 root369; after expanding, Point2 row426. Selecting point auto selects SCENE workspace/right River Inspector. New Draw button at(1745,544) with bed section expanded. Main gizmo pivot~(873,524), +X arrow bottom-left, +Z arrow right-down; correct branch outward toward UPSTREAM is upper-left (e.g600,380), not downstream lower-right. Arm button -> orange selected node and "CANCEL TRIBUTARY DRAWING". draw-owned-tributary.py guards exact inspected dimensions/viewport and makes2sec drag. Re-capture before using; camera/bounds change on restart.
Native failure log is creator-bin-v1/log.txt. Console footer actual x491,y995; x560 is Output. Opening drawer changes viewport height; collapse via same tab before replay. Native app now newly launched, so no drawer/frame assumed.

Next urgent work: retry new UI gesture, inspect actual bed + union + foam and receiving shared status. If failure inspect log and correct, do not hand back broken workflow. Then actual GUI Undo/Redo (single transaction inclbed), SAVE and reopen source, TestGame/render/performance and export shader closure. Keep commentary frequent. No commit/push/merge. Latest free2.278GB after two small candidate backup sets. Accepted G6/G7C and dirty Wicked still protected; final audit pending.


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
