# G10A bounded native spline grade proof — 2026-10-25

Status: bounded G10A engineering checks PASS. Owner visual acceptance pending. Not adopted by Studio/Runtime.

## Scope

The owner requested short lowered river sections of a few feet, producing a corresponding speed/whitewater change. The accepted G8H appearance and G9 Studio/Runtime path remain intact. Full waterfalls and mountain streams remain paused.

New owned implementation: RiverGradedFlow.h, RiverGradedClassification.h, RiverGradedTransport.h; native fixture and CPU tests; isolated generated graphics harness/shaders. Tests/CMakeLists.txt is append-only. No production flat-path replacement, owner terrain editing, submodule change, commit, push or merge.

## Model and practical limits

Height, width and direction come from RiverFlowField native ribbon samples and triangle queries. The graphics fixture checks those queries against the actual Wicked-generated ribbon mesh before refinement, including after real node edits and Undo/Redo.

A bounded excess-energy model accelerates authored baseline flow down a grade and dissipates excess energy with a roughness response. Fixed section discharge determines target depth; flat speed remains authored. This is a calibrated model over authored geometry, not a free-surface hydrodynamics solver or automatic catchment/discharge simulation. The isolated fixture builds its own bed to the target depth, then measures actual transformed bed/rock triangles for depth and projects depth-weighted flux conservatively. Arbitrary existing bed conveyance is not yet solved or integrated.

Supported proof bounds: height range 1.25 m; absolute sampled grade 0.25; width 0.5–12 m; model speed up to 8 m/s; section depth 0.075–2 m. These section bounds do not imply every projected obstacle-cell velocity equals section speed. Catmull-Rom interpolation is native. Large drops and reversed net grade are refused. A single connected reach is proved here; graded junctions are future work.

Local height is used for waterline/bed classification; triangle clipping approximates the varying water surface linearly across each obstacle triangle. This needs further error characterization for large obstacle triangles. Whitewater sources respond to grade/Froude and actual obstacles; existing accepted transported foam artwork and detail are retained.

Native WATER lighting, refraction, transparency and normal sampling remain. Owned proof PS uses the local geometric tangent plane for water depth optics. Surface-wave normal adjustment preserves the authored base slope. The original native planar reflection pass remains a single plane: reflections across multiple elevations are an approximation, not exact per-reach reflection captures. Visual review must consider that limit.

## Supplied Unreal reference

Inspected the supplied SHADERSOURCE Branching River Tool in a private UE 5.5 reference project, using native Python commandlets and native ObjectExporterT3D exports. Original Downloads ZIP was read only. Proprietary graph exports remain outside Git and are not packaged.

Verified Blueprint calls include UpdateRiverMeshesScaleLocationTangent, GetLocationAndTangentAtSplinePoint, GetScaleAtSplinePoint, SetLocationAtSplinePoint and SetTangentAtSplinePoint. Native exposed defaults include spline direction/point diagnostics and river attachments. This verifies 3D spline mesh authoring and attachment concepts; it does not establish an automatic gravity-flow solver.

Native material/function inspection verifies separate foam/normal/noise flow controls, vertex/world geometric inputs, height blending, distance/depth inputs and displacement controls. No proprietary graph, formula, texture, mesh or material was ported. Renegade's implementation is independently authored using existing owned river/native geometry services.

Reference evidence is private under C:\Users\paulw\Documents\RenegadeReferenceReview_20261025. Neutral audit scripts and execution logs are in the G10A BUILD evidence directory.

## Commands and evidence

DESIGN; Windows x64 Release; DX12. Evidence root:
C:\Users\paulw\source\repos\renegade-spline-water-20261020\BUILD\river-v2-G10A-20261025-graded

Build wrappers build-cpu.py / build-proof.py record full cmake command arrays, output and exit status. stage.py compiles the owned PS, detail CS, main VS and shadow VS and stages the original normal map plus accepted foam artwork/bulk CS. run.py records exact proof command arrays, exit code, duration and output location. audit.py records hash comparisons in preservation.json.

CPU regression command:
`ctest.exe --test-dir C:\Users\paulw\source\repos\renegade-river-v2-integration-20261023\BUILD\g6 -C Release -R ^RenegadeRiver.*Tests$ --output-on-failure`
Result: all 16 river tests passed in 12.11 seconds; river-regression.log.

Graded CPU fixture: nativeRows=168; section peak 2.63384 m/s from baseline 1 m/s; downstream recovery 1.05101 m/s; projected Q=2.59859; maximum divergence 1.24332e-7; positive rapid emission. Flat baseline and overheight refusal pass.

Preservation: all recorded G9 source/candidate hashes unchanged; previous core classifier/transport unchanged; Tests CMake original prefix preserved; integration HEAD and exact private Wicked binary patch unchanged.

## Engineering issues found and corrected

The new isolated binary initially lacked Content/river/waterNormalMap.jpg; native river creation correctly refused. Staging now includes it. The fixture also needed a Scene update before accessing generated ObjectComponent.

The node editing test caught a stale native ribbon: calling UpdateTransform clears the transform dirty flag before Wicked's spline system consumes it. Structural refresh explicitly marks native node transforms dirty before Scene::Update. The strict actual-mesh parity check is retained. Intermediate failure logs remain in BUILD; they are not acceptance passes.

## Remaining acceptance

Complete GPU regression, fresh native-scene reopen, images/performance review and candidate resource closure. Record results below when complete. Owner visual acceptance is still pending. Studio/Runtime graded integration follows that bounded proof; accepted flat G9 stays unchanged.

## Final consolidated result

Final Release river regression: 16/16 PASS in 12.47 s. Fresh saved-native-scene full GPU run: exit 0 in 114.05 s. Commands and exact identities: BUILD evidence FINAL_RESULT.json, river-regression-final-exit.json and verified-final-exit.json.

Native geometry edit/Undo/Redo hash parity PASS; actual mesh height error 0 in tested refreshes. GPU/CPU bulk parity ['RAPIDS_GPU_CPU_PARITY=6.34193e-05']. Surface and appearance history preservation, zero-flow, emission expiry, native restoration and captures PASS. Original artwork and normal map load; shader errors 0. Moving/flat surface captures differ.

The final fresh scene applies the fixture-only visibility filter again: native serialization did not retain temporary suppression of reference objects. Earlier fresh run showed an overlapping old flat surface; corrected reload images were inspected. B is labelled global/local depth diagnostic; it is not a foam-art comparison in this proof. Default foam amount remains accepted 0.3.

Actual owned-window UI input test: G cycles native drops [1.15, 0.0, 0.6, 0.9]; exit 0; 63.39 s including startup and explicit structural rebuilds/captures. No global keyboard input used. Normal close PASS. Node edits deliberately reset simulation history; appearance-only tuning retains history.

Small isolated fixture wall timings: ['STRAND_BENCH enabled=0 frameMS=1.02584 fps=974.809', 'STRAND_BENCH enabled=1 frameMS=1.01931 fps=981.054', 'STRAND_BENCH enabled=0 frameMS=0.980811 fps=1019.56', 'STRAND_BENCH enabled=1 frameMS=1.02477 fps=975.833', 'RAPIDS_FPS=1070.42']. These are not GPU timestamps or a whole-game FPS claim; G9 integrated scene performance is unchanged. Structural CPU domain rebuild is synchronous and noticeably slower than steady rendering; acceleration/asynchronous rebuild is required before general Studio graded editing.

Candidate: separate desktop shortcut Renegade River V2 - Graded Drop. G cycles drop; U Undo; Y Redo; N node spheres; 1–5 views; Space pause; foam amount slider. It edits actual native node heights in a controlled fixture, not arbitrary individually draggable Studio nodes. Final live launch is recorded in live-launch.json.

All recorded preservation comparisons PASS. No commit/push/merge. Remaining: owner visual acceptance of this isolated grade; arbitrary-bed conveyance/depth treatment; local-height clipping accuracy for large obstacles; graded junctions; reflection limitations; efficient Studio node editing and Runtime/package integration. No claim of full waterfall/hydraulic-jump simulation.

Final live native 3D and HUD captures inspected at view 4: control spheres visible; local graded depth; amount 0.3; no overlapping reference river. Candidate PID 29784. git diff --check for modified existing task files exits 0; only pre-existing line-ending conversion notices.
