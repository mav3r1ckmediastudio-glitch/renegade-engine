# River V2 G7B — isolated continuous flat confluence
Date: 2026-10-24 (Europe/London). Engineering proof; owner visual acceptance OPEN.
G6 remains owner accepted. No Studio/Runtime production adoption, no commits/push/merge.
Mountain streams, waterfalls, rapids and lake/ocean joins remain paused.

## Implemented scope

RiverJunctionSurfaceService builds one immutable CPU surface from G7A's explicit native network: two incoming tributaries, one outlet, one exact common flat plane. It replaces the overlapping water surfaces in the disposable proof; it is not another mesh drawn on top of native water and is never a foam overlay. Authoritative source footprints come from G1 NativeRibbon, whose endpoints and banks match actual Wicked-generated geometry.

A double-precision local X sweep partitions the union using source boundary vertices and inter-reach boundary intersections. Each slab merges native polygon intervals. Neighbouring slabs share noded vertical edges; boundary vertices are welded on a 10-micrometre local grid. Interior fan centres retain full precision, preventing thin slabs from collapsing onto their boundaries. Native float32 world coordinates add their normal rounding error. Triangles face +Y. Non-manifold edges or open boundary/T-junction patterns refuse the result. Original scene, splines, materials and graph are not mutated. Failed builds preserve the previous result.

All output vertices carry world-XZ velocity. Native reach queries blend by bank/end-cap distance; a smooth bounded neighbourhood blends toward the outlet velocity at the junction. The anchor is explicitly noded and constrained to outlet velocity. Shared mesh vertices and barycentric queries provide continuous piecewise-linear velocity; one float32 512x256 map drives the existing accepted G3 two-phase native WATER normal extension. This is a kinematic directional blend, not hydraulic discharge conservation or obstacle-adjusted foam velocity.

Limits: exactly3 reaches /1 two-tributary join; native plane and cap mismatch <0.1mm; <=1024 boundary edges; <=4096 sweep events; <=65536 vertices and triangles;16M counted sweep/intersection work operations. Remaining bounded output/noding/query loops have their own container limits, not a claimed16M total instruction cap. Undeclared boundary crossings outside the blend neighbourhood refuse; this is not a general network intersection solver. CPU Evaluate is a bounded linear triangle scan and is not a production spatial index.

## Changed files and ownership

New EngineBridge/include/renegade/bridge/RiverJunctionSurfaceService.h and EngineBridge/src/RiverJunctionSurfaceService.cpp.
New Tests/RiverJunctionFixture.h, RiverJunctionSurfaceTests.cpp and RiverJunctionGraphicsProof.cpp.
Narrow registrations in EngineBridge/CMakeLists.txt and Tests/CMakeLists.txt.
Documentation: this report, ARCHITECTURE.md, FEATURE_MATRIX.csv and HANDOFF.md; original river HANDOFF navigation only.
No changes to G1, G6 renderer, Studio/Runtime call sites, native Wicked source/pin or shaders. The accepted G3 RiverFlowNormalPS.cso is reused with its existing48-byte ABI and0x52464c33 tag. Source and staging provenance remain in the G3 exact-shader evidence; no Unreal content was copied.

## Exact evidence on DESIGN

Integration worktree renegade-river-v2-integration-20261023; HEAD11bbb90e4214776d5e51c8bc4cba2e7f1ccde296 plus preserved uncommitted source. Wicked3a800b7134aafe58461093c8abb2e274d4e64033.
Evidence: original river BUILD/river-v2-G7B-20261024-0030. Exact absolute commands, working directories and arguments are in these scripts:

- `powershell -NoProfile -ExecutionPolicy Bypass -File <evidence>/build.ps1`: VS18 CMake configure; `cmake --build BUILD/g6 --config Release --target RenegadeRiverJunctionSurfaceTests RenegadeRiverJunctionGraphicsProof --parallel 2`; final build EXIT0, CPU executable EXIT0.
- CPU test:39000 coverage samples versus native ribbon queries,7119 wet; no sampled output triangle overlap; every generated vertex checked for consistent velocity; reversal inherited from native fixture; three translated/yaw-rotated/nonuniform-parent cases; transformed union area, anchor direction, deterministic repeat and gap refusal. Final830 vertices/1099 triangles,342.156m2.
- `powershell -NoProfile -ExecutionPolicy Bypass -File <evidence>/run-proof-v3.ps1`: actual native Scene::Update generates three independent curved native meshes, including reversed tributary.44800 world sample points tested against actual native transformed mesh triangles:236 original overlap samples,0 output overlaps, identical sampled union coverage. Then render only the single union with native WATER. EXIT0, native shader errors0; captures-v3 and graphics-v3.log.
- Final graphics run:69.9915FPS at1264x761 after using the owner-supplied Rocky Rugged PBR bed and corrected native rock scale. Earlier v1/v2 runs74.66/74.82FPS had inadequate scene presentation and are not final visual evidence. These capped fixture measurements are not uncapped GPU timings or a performance guarantee.
- `python <evidence>/check-captures.py`: native-before versus native-restored RGB mean0/max0; flow-versus-native mean0.46314 and stepped normal motion mean0.79388, proving the shader actually changes WATER and time changes normal motion. Native material fields remain the same between F modes. Pixel tests use paused same-phase native frames and TAA disabled.
- `powershell -NoProfile -ExecutionPolicy Bypass -File <evidence>/run-ctest.ps1`: focused bridge/river/importer/snapshot/runtime/source-contract regression selection86/86, EXIT0,34.16s.
- Fresh-process archive reopen, preservation and candidate details are appended after final validation. Archive strips transient shader/userdata and carries native mesh/material/resource data; no network/derived-cache schema is introduced.

## Visual inspection and discarded iterations

v1 exposed an imported-rock units/centering error and tiled a non-seamless rock atlas over the ground. It was rejected as presentation evidence despite geometry and native runtime checks passing. v2 corrected scale/atlas but was too dark and used an unsuitable standalone terrain material. Final v3 uses the already owner-supplied seamless Rocky Rugged PBR maps, physical1.8m rock bounding extent, an actual shallow sloped opaque bed, native daylight and the native WATER material. Inspected overhead, velocity-debug and eye-level captures. It is a sparse confluence diagnostic, not a finished landscaped level.

Initial unit checks caught a thin-slab fan-centre weld error and a missing explicit anchor velocity constraint; both were fixed without weakening the checks. Initial proof compile failures were native DrawLine/DrawPoint signatures and an unavailable SetWireRender symbol. The final proof uses native temporary line/point queues for mesh-edge diagnostics, with no upstream changes. All final builds/tests above pass.

## Acceptance boundary

No foam transfer, editor snapping, automatic middle-of-spline splitting, joined-bed authoring, Undo/Redo, metadata migration, production caching, connected-network Runtime integration or general multi-junction geometry is implemented here. No per-rock flow/foam physics claim. Proof native WATER toggling preserves reflections, refraction, transparency, lighting and normal-layer controls; original upstream WATER is untouched. Device loss and arbitrary precision/adversarial geometry remain uncertified.

Owner reviews the isolated junction surface and velocity/normal transition first. Then foam transfer can consume the shared domain, followed by explicit creator connection commands/snapping and persistence. The accepted G6 desktop candidate remains intact.


Final lifecycle/candidate: fresh-reopen-v3 rendered but exited0xc0000005 and is NOT a pass. Final native helper waits for IsPipelineCreationActive to clear before release; run-fresh-final.ps1 EXIT0, fresh native material/normal resource and render checks PASS. Final build also enables native TAA for interactive viewing only (automatic pixel checks retain deterministic TAA-off mode). Opened final interactive candidate through launch-junction.ps1; screenshot interactive-native.png inspected; close-owned-proof.py verified exact owned image path, sent Escape, waited and recorded exit0 in interactive-exit.json. No owner process was closed. Desktop shortcut Renegade River V2 G7B - Junction Proof creates a unique output folder per launch and refuses a second copy of its own running executable.

Final source hashes, exact allowed/new files, unchanged original worktrees/native patch and accepted G6 binary hashes are recorded in final-audit.json. Existing ledger quoting/row contents are preserved; only the G7B capability row is appended. No source/owner-project changes outside the documented scope.
