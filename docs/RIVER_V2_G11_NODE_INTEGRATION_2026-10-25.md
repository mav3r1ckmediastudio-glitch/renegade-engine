# G11 ordinary-node integration — bounded engineering checks PASS, owner review active

2026-10-25, DESIGN. Authority and accepted appearance remain the River V2 implementation handoff and G10C/G10D owner decisions. No commit/push/merge; mountain streams paused.

The shared bridge derives rounded supports from ordinary authored river nodes in the native root-local frame. Native accumulated travel and arc-to-toe data replace the fixed +X proof. An indexed native-triangle classifier supplies local height to conservative measured-bed transport. Linked terrain uses the same derived geometry. Studio and Runtime share RiverFlowRenderer composition, owned course WATER shaders and scoped native mesh/material restoration. Saved scenes retain authored controls and UUIDs; derived supports, descriptors and render bindings remain transient.

Vertical node edits shift the downstream level; horizontal edits retain existing compact smoothing. Automatic drops have no style/angle/curvature controls. The existing river inspector supplies speed, foam, rapids amount, surface normal turbulence and width. Rapid amount updates change emission while preserving density history. Accepted impact-energy calibration is applied downstream of the actual native toe. Native regeneration arriving after preparation now invalidates and rebuilds the transient binding on the next preparation, rather than leaving a broken preview.

Evidence root: C:\Users\paulw\source\repos\renegade-spline-water-20261020\BUILD\river-v2-G11-20261025-node-integration.
FINAL_RESULT.json contains exact command arrays, exit results, source/binary hashes and limits. BATCH_COMMANDS.json retains all recorded attempts, including failures. Reused integration BUILD/g6 Release cache; outputs redirected to the separate G11 candidate.

Validation:
- Release Studio/Runtime and all three course shaders build and deploy.
- CTest: 19/19 rebuilt river regressions pass.
- Actual Scene::Update-generated native mesh parity at translated origin and yaw 0 / 90 degrees / 0.47 radians: max bank error 0.00000687704m. Native landing lookup/entrainment remains present in each case.
- Real native terrain, ordinary create/lower-node commands, existing-cascade edit/Undo/Redo, forced native regeneration between Prepare and Update, scoped restoration, GPU foam, zero speed and native save pass.
- Frozen foam amount and rapid amount edits preserve density and rebuild count exactly.
- Fresh-process native reopen passes with seven authored spline nodes and no custom shader serialized.
- Latest exact Runtime: 30-second Test Game, unsaved profile snapshot, return edits/Undo and temporary-session cleanup pass.
- Latest Build Game passes after adopting the fixture's owned terrain/normal maps into its project Content directory. Relocated package launched from unrelated cwd: active river, zero steady rebuilds, normal close exit 0; about 74.64FPS on this small DESIGN fixture.
- Shared-renderer fixture measured about 77FPS. Structural rebuild about 2.1 seconds; not a whole-game performance guarantee.
- Actual Studio node edit, ready status and menu Undo/Redo were observed and captured. Owner was also manipulating the same window; exact attribution of individual injected mouse/key events is not isolated. Automated UI input stopped on owner's question. Current owner edit remains unsaved and untouched; latest read-only capture shows River ready and 75FPS.

Candidate: desktop shortcut Renegade River V2 - Studio Node Integration. Copied project: C:\RenegadeCourseReview_20261025\RiverV2Review.renegade. Latest Studio PID/window and package commands are recorded in the evidence root. Accepted G10D reference remains separate and exact.

Resolved failed attempts: rotated world-frame bank precision (derive locally); local/world toe mismatch; existing profile guards refusing elevated rapids; CPU archive tests run beside deployed textures without a GPU (correct CTest working directory); copied registry identity/canonical newline mismatch; disposable light argument order/exposure; fixture external texture references; fixture helper GPU teardown; native gizmo regeneration binding recovery. Initial standalone attempt paused in background before river preparation; repeat uses documented dx12/alwaysactive launch flags.

Remaining limits:
- Synchronous structural rebuild is roughly 2.1 seconds. Existing Studio suspends transport while the gizmo is active.
- Fully validated case is one small drop; repeated-drop appearance/energy recovery requires further validation.
- Supported derived surface is finite, upright/uniform-scale and single-valued in XZ; width 0.5–12m, height range <=2.5m, continuous static bed within 4m. Linked-bed authoring retains unscaled/unrotated root guards.
- Elevated connected junctions and reverse elevated flow remain unsupported. Accepted flat tributaries retain their existing renderer.
- Native planar reflection is still an approximation. No free-surface, ballistic or volumetric spray claim.
- Turbulence controls surface normal strength rather than a solved wake. Breakup threshold is appearance attenuation; full signed-range breakup tuning remains a follow-up.
- Owner visual acceptance of G11 and independent exact-change release verification remain pending. This does not declare the entire river programme finished.
