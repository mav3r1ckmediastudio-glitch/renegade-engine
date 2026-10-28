# Persistent river terrain layers — implementation contract

Existing sculpted height vertices remain the base landscape. A versioned metadata
payload on the terrain root retains baseline and last composed heights per native
chunk coordinate, plus river layer settings linked by persistent entity UUID.
No raw ECS IDs are serialized in this payload. Native scene remapping therefore
cannot redirect a layer onto unrelated geometry. Missing river entities contribute
no cut; duplicate persistent river IDs reject recomposition.

For each vertex, evaluate each river cut independently against the baseline and
choose the minimum resulting elevation. This makes overlapping channels independent
of iteration order, prevents repeated bank falloff from deepening the bed, and
restores the former footprint when a river moves or is removed. No procedural
terrain generation or water material blending into terrain is permitted.

Before changing a layer, validate the whole resident finite terrain and compare
its current heights with the last composed result. An unknown external mutation
must fail closed instead of being overwritten. The ordinary sculpt brush will
read and edit the baseline, then recompose, so sculpting under a channel remains
available when that channel moves away. Sculpt command snapshots must include
the layer payload as well as heights, including a baseline-only edit hidden by
an existing cut. Undo/Redo restores both in the same scene-safe operation.

Build the complete new document and output heights in memory before touching GPU
resources. Reuse the accepted ApplyTerrainSculpt resource/collision rebuilding
path at EVENT_THREAD_SAFE_POINT. Never rebuild from the viewport's late descriptor
consumption phase. Layer creation/removal, transform edits, sculpting and document
serialization must share these lifecycle boundaries.

Initial engineering limits: fully resident finite terrain, at most 81 chunks,
unit vertical terrain axis, unscaled/unrotated river roots, bounded schema payload
and sample/layer counts. Refuse malformed payloads, unsupported transforms,
out-of-envelope channels, topology mismatch or ambiguous references before mutation.
No automatic adoption of old rivers or existing owner scenes. No UI exposure until
native persistence, overlapping cuts, later manual sculpting, removal, exact Undo/
Redo and GPU descriptor/collision tests pass. Performance and real Studio/Test Game
validation remain mandatory before any candidate promotion.

## Implemented checkpoint (continuation in progress)

RiverTerrainLayerService now owns a schema-v1 JSON metadata document on the
terrain root. Chunk coordinates and baseline/last float arrays plus transform
fingerprints survive scene remapping; layers reference persistent river UUIDs.
Composition is minimum-of-independent-baseline-cuts, bounded to 81 chunks,
32 layers, 32 MB payload and 32 million vertex/segment operations. Native root
scale/rotation and per-node width scaling are restricted explicitly. Unknown
height/topology/transform edits, malformed payloads, duplicate IDs and out-of-
range cuts fail before GPU mutation. ApplyTerrainSculpt preflights layered
snapshots; composition retains rollback state for native-apply failure.

TerrainSculptState now includes the layer document. Sculpting on a layered
terrain edits base heights, then recomposes once, retaining metadata-only edits
in Undo/Redo even when no visible height changes. Unlayered sculpt computation
is unchanged. Rebuild/collision uses the accepted native path. Regeneration,
ring expansion and heightmap replacement are blocked while layers exist.

WatercourseSceneCommand wraps native transform/delete/spline-state/node edits
and owns terrain before/after snapshots in the same Undo action. Studio Link /
Update Bed and Remove Bed controls are wired. Gizmo commit, numeric transforms,
delete, spline width/settings and add/remove-last-node operations queue layer
mutation at EVENT_THREAD_SAFE_POINT. New rivers receive persistent IDs before
the creation snapshot. Existing saved rivers are not silently linked/migrated.
Channels currently recompose on edit commit, not every gizmo preview frame.

Native DESIGN VS18 Release proof passes: move/restoration, hidden base sculpt
history, combined move Undo/Redo, width and node history, deletion Undo/Redo,
overlap insertion-order independence, mask preservation, actual Jolt ray parity,
native WISCENE stable-ID/coordinate reload, failed-move rollback and malformed/
duplicate/external-edit refusal. See BUILD/river-layers-20261021-content-proof.log.
This proof now requires actual native terrain textures; Tests/CMakeLists copies
and packs the same grass assets as Studio. Earlier gray-terrain proof runs had
missing test-executable content and do not establish textured visual acceptance.

Exact full build: BUILD/river-layers-20261021-exact-build.log, exit 0.
Focused tests after building the correctly named RenegadePhase7Gate7ETests target:
BUILD/river-layers-20261021-ctest-final.log, 8/8 pass. An earlier attempt named
the CTest registration as a build target and failed; its logs remain for trace.

Actual Studio validation is ongoing. A first disposable scene copy broke relative
asset paths; it is only an abandoned fixture under BUILD/river-layers-gui-20261021.
The correct fixture stays beside its destination-aware scene archive at
BUILD/river-layers-20261021-content-proof/River-Layer-GUI.renegade. It has a real
Player Start and native terrain content. Studio opened its Level with textures.
After selecting/focusing River Water 5 the terrain became black. This is a REAL
visual failure and overrides harness success for integration acceptance.
FocusSelection had an extra late scene.Update(0); that redundant update has been
removed, and a new native build is being compared against the same fixture.
The removal is NOT YET a verified rendering fix. Selection-outline and camera-
dependent terrain rendering remain alternative causes to distinguish.

Owned isolated Studio PID 4284 was closed normally before rebuilding. Accepted
alpha PID 26696 was left minimized and untouched. Do not query port 38741 and
assume it belongs to the candidate: the alpha owns that endpoint. No owner
project was opened; only generated BUILD fixtures were changed. No push/merge,
Wicked changes, release distribution or finished-waterfall claim.


## 2026-10-21: Persistent terrain layers and native Studio defects checkpoint — NOT ACCEPTED

Supersedes the older "layers not implemented" statements below. HEAD remains
fc54fd4dbd6f2d2194c2cf0ceb53804086504681; all changes uncommitted. No push,
merge, alpha/main edits or owner project changes. Read
docs/RIVER_TERRAIN_LAYERS_DESIGN_2026-10-21.md in full before continuing.

Implemented persistent UUID-based channel layers over an exact sculpted baseline,
overlap composition, link/update/remove UI, combined transform/delete/spline
history, hidden baseline sculpt history, strict external-edit refusal and bounded
finite terrain support. Geometry previews live; bed updates on drag COMMIT.
R16 import, terrain regeneration and expansion refuse while layers are linked.
New river creation assigns persistent identity before its creation snapshot.

Native VS18 builds passed. Terrain GPU proof validates baseline restoration,
overlap order, hidden sculpt, move/width/node/delete Undo/Redo, WISCENE fresh-scene
reload, unchanged paint/grass and Jolt ray contact parity. Focused CTest was 8/8
before the final transform publication repair; see final logs for latest outcome.
Evidence and limitations are in the design checkpoint.

Actual Studio exposed black terrain after Focus Selection. Removed its redundant
late Scene::Update(0); same scene then stayed textured before/after focusing
(BUILD/river-layers-gui-selected-before-focus.png and
river-layers-gui-focusfix-after-focus.png). Native water still looks flat: NOT
visually accepted. No waterfall rendering, foam, spray, mist or positional sound.

Actual point drag changed the native ribbon. UI Undo exposed stale mesh + local
gizmo publication: SetTransformCommand clears dirty before native spline mesh
generation sees it. CommandService.cpp now reconstructs immediate parent-world
pose and invalidates affected native prev_mesh_generation_nodes on Execute/Undo/
Redo. RiverSplineTests adds parented-point publication/cache regression; native
build and test PASS. Full UI revalidation of this final repair is PENDING:
later desktop inputs/pointer/controls were inconsistent, and some captures named
commandfix-drag/undo are StoryFlow or wrong selections, NOT acceptance evidence.

Candidate PID42068 was left open in the disposable River Layer Native Content
Proof project only. Do not drive or close unknown processes. Accepted alpha
PID26696 owns diagnostics port38741; candidate requests to that port are NOT
candidate diagnostics. Candidate fixture:
BUILD/river-layers-20261021-content-proof/River-Layer-GUI.renegade.
This fixture may contain later exploratory UI edits; regenerate a new proof
directory for a clean baseline. Do not overwrite existing proof directories.
The initial BUILD/river-layers-gui-20261021 copied scene had broken relative
resource paths; do not use it for texture validation. Proof CMake now deploys
the actual packed default grass resources and asserts material maps are valid.

Remaining required: stable actual point drag/Undo/Redo and save/reopen checks on
the FINAL build; real Studio-to-Test Level Runtime and return; post-return sculpt;
interactive link/remove/move restoration visual inspection; rendering root cause
and realistic water; connected waterfall geometry and complete effects/audio.
A Play click was attempted but no Runtime process was observed; do not claim
Studio-to-Runtime success. Performance and independent release gate remain open.



### Final validation and blockers (later checkpoint)

- Headless regression reproduced twice as 0xc0000005 after real default-grass
  assets were deployed beside test executables. ConfigureDefaultGrassMaterial
  unconditionally tried GPU uploads in a no-device physics test. It now retains
  authored texture names but skips upload with no graphics device. Rebuilt
  focused CTest is 8/8 PASS: BUILD/river-layers-20261021-headless-ctest.log.
  Earlier final-ctest and terrain-retry failures remain preserved.
- Command world-publication repair and GPU terrain proof PASS:
  BUILD/river-transform-proof-20261021-105734.log. A first final-proof invocation
  refused a nonempty output directory (exit2); no evidence was overwritten.
- MatrixValues now traverses nested matrix rows instead of pointer arithmetic
  across row-array boundaries. Final native build/proof logs follow below.
- Actual Test Level requests WERE received. Candidate log explicitly says:
  "Could not release the assistant GPU model before Test Game. The game was not
  started; inspect the assistant lifecycle service." No Runtime process started.
  Do not bypass this guard or stop accepted alpha/assistant owner services.
- GUI Save wrote the primary scene but reported:
  "Scene saved but scripting companion failed: Script document scene path must
  identify a project Content/*.wiscene." This disposable project was legacy
  adopted with its scene at project root, so it is unsuitable for complete
  Studio save/Test Game acceptance. Regenerate a NEW governed Content/Scenes
  fixture using native serialization at its final destination, never a raw
  scene-file copy that breaks relative resources. This is fixture repair scope.
- Owned candidate PID42068 was closed normally through its unsaved dialog after
  preserving its saved primary scene as
  BUILD/river-layers-20261021-gui-saved-checkpoint.wiscene.
  It is NO LONGER OPEN. The final GUI session log was copied to
  BUILD/river-layers-20261021-gui-session.log.
  Accepted alpha PID26696 remains untouched.
- Some later UI captures/actions are unreliable because observed pointer and
  selected controls changed between calls. Do not infer successful final gizmo
  Undo/Redo or full save/reopen from them. Automated scene reload and history
  proofs do pass, but actual final UI certification remains OPEN.



### Final exact working-state evidence
Native Studio final build PASS: BUILD/river-layers-20261021-studio-final.log.
Native Runtime/bridge/tests final build PASS: BUILD/river-layers-20261021-headless-build.log.
Focused CTest 8/8 PASS: BUILD/river-layers-20261021-headless-ctest.log.
Final textured DX12 terrain/material/collision/history/reload proof PASS:
BUILD/river-final-proof-20261021-110502.log and its image/scene directory.
Earlier intermediate failure logs retained. git diff --check has no whitespace errors;
feature matrix parses as 50 rows of 16 columns. Wicked diff SHA256 unchanged:
18DF9DB7AA80F6A2221ABB02B27A9D26A076EE4427739CF7BF31B8FAEC338A89.
This is an uncommitted implementation checkpoint, NOT a visually accepted test candidate.

## Linked riverbed per-selection inspector readback (continuation)

Before this change the native per-river channel metadata persisted correctly
but the Studio Riverbed Depth/Bank Width widgets did not restore those saved
values on selection. Selecting a different river could carry the previous
river's widget values into a new LINK / UPDATE BED command. The new read-only
ReadRiverTerrainLayerSettings bridge seam validates the selected native WATER
spline and UUID against bounded schema-v1 terrain layer metadata; never
recomposes terrain, touches graphics or calls Generation_Cancel. Linked values
are restored on SELECT/Terrain river switch and Undo/Redo history transitions.
Unlinked rivers use the owner-approved 1.35m / 2m defaults; Remove Bed is
disabled until linked. Malformed/duplicate metadata is reported and blocks
unsafe edits. This is display/readback correction, not the future terrain
material/shoreline blend-mask composition: those still require a separately
versioned authored baseline, mask layering and full GPU/Undo/save acceptance.
Tests/RiverSplineTests exercises readback and metadata refusal. Studio GUI
acceptance remains pending native build and actual owner interaction.
