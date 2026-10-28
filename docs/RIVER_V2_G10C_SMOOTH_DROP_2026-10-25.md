# G10C automatic smooth drop — engineering PASS; owner visual pending

Owner requirement: one river tool. Draw the river and lower a creator node; derive a natural rounded lip and toe automatically. No cascade/tightness/angle choice or curvature slider. Existing gentle grades remain. Creator nodes guide the rounded profile; derived support nodes are not additional creator controls.

G10B is visually REJECTED at the join. Its three separately shaded surfaces met at endpoints but did not share normals, displacement or animation phase. Its earlier engineering PASS does not override this rejection. The G10A graded appearance remains accepted as a direction and is preserved byte-for-byte.

## Implementation

New EngineBridge/src/RiverSmoothDrop.h derives bounded shoulder/support points and stable 3D native frames. Wicked's own Catmull interpolation and generated ribbon are the final geometry authority. The support cubic is not used as a substitute geometry oracle. The proof compares actual generated row centres with EvaluateSplineAt and rejects backwards rows or an inter-row turn of 0.08 radians or more.

Tests/RiverSmoothCascadeFixture.h produces one connected native WATER mesh through the upstream, rounded descent and receiving river. Refinement interpolates actual native cross sections, shares vertices across all rows and recomputes one normal/tangent stream. A single accumulated 3D travel-time coordinate crosses lip and toe without restarting.

The receiving 2D domain starts at a derived native cap at the rounded toe. Its actual triangle bathymetry is fitted before rebuilding classification and conservative projected transport. No cached classifier from the previous bed is used. Incoming air uses measured negative boundary faces and checks their water discharge against domain inflow. The original creator node transforms remain authoritative for edit/history tests.

The new optional shader retains the native WATER lighting, reflection/refraction sampling, transparency and normal maps. It uses the continuous travel coordinate for normal motion and original owned foam artwork, and blends calibrated falling entrainment into transported receiving foam. Existing accepted surface motion is retained. No overlay mesh, foam collar, rock shading, Unreal graph/code/texture port or upstream edit.

## Scope and limits

This is an isolated BUILD proof, not Studio/Runtime adoption or the completed draw/drag integration. The proof uses real native node commands with history and generated support geometry. It does not yet expose general node picking/dragging.

The surface, gravity speed and depth recovery are bounded calibrated models, not a solved free-surface/ballistic/hydraulic-jump simulation. Receiving discharge is measured and projected conservatively; nominal rectangular falling thickness and the banked 3D cross section are not a complete coupled 3D fluid solution. Native planar reflections still use one plane. Mean geometry is used for classification/collision. Arbitrary owner terrain fitting and graph serialization are not introduced. Mountain streams remain paused.

The disposable bed has been fitted for this proof only. Native support radius is chosen internally from drop height, capped at 0.8 m; there is no user angle limit/control. Gentle slopes use the existing G10A path, unchanged.

Structural rebuilds are synchronous and slow in this verification harness. Steady frames do not regenerate the CPU geometry/domain. Whole-game FPS or production interaction latency must not be inferred from this small fixture.

## Validation and preservation

DESIGN, Windows x64/DX12, Release; existing integration BUILD/g6 cache reused. Exact command arrays, exit codes and logs are in the spline-water BUILD/river-v2-G10C-20261025-smooth directory.

New source: RiverSmoothDrop.h, RiverSmoothCascadeFixture.h, RiverSmoothDropTests.cpp, prepare_river_smooth_cascade_graphics_proof.py and prepare_river_smooth_cascade_shading.py. Only the optional target block is appended to Tests/CMakeLists.txt. Accepted G10A/G9, prior G10B evidence, other dirty source, owner projects, main/alpha and Wicked source/pin are protected. No commit, push or merge.

Failures retained: initial const-correctness compile error; native endpoint overshoot caught by CPU tests and corrected with denser shoulder support; actual mesh undersampling caught by strict crease checks and corrected with bounded height-adaptive subdivision. Across subdivision reduced to 16 to contain vertex cost. Initial render showed a bed transition line and weak falling foam; fitted bed/domain ordering and shader coverage were revised before presentation.

Final command/result manifest and visual inspection to be appended after consolidated verification. Do not mark owner visual acceptance or production release complete.


## Final consolidated result

Release configure/build and three shader compiles exit 0. All 18 river test executables PASS. Full native DX12 proof (full-final4), saved native fresh-process reopen (reopen-final4), and owned-window G/U/Y/G height edits with normal close PASS. Actual generated native geometry parity stays within approximately 2e-6 m; maximum consecutive row turn is below 0.08 radians. No backwards rows. Initial native, native restoration, bulk/detail history, source-only emission, expiry, zero flow, motion and five-view captures PASS; no shader errors.

Coordinate validation detail: the initial raw weighted-X-moment relative threshold failed (2.3789e-5 vs 2e-5), although density/age and global mass checks passed. An independent readback comparison found the actual source-centroid error was only 3.0604e-5 m, with moment L1 relative errors 3.8328e-6 and 3.3292e-6. The smooth proof now tests meaningful centroid error below 0.001 m and global X/Z moment L1 below 5e-6, alongside unchanged one-step, density, age and total-density bounds. Final C++ measurement is about 3.05e-5 m. CPU oracle and accepted compute shaders are unchanged. The failed result and diagnostic binaries are retained.

Structural work no longer computes the obsolete receiving projection before fitting the new bed. The only measured-domain build follows final fixture bathymetry. Default connected surface is 34,272 vertices; height-adaptive native sampling uses more rows for smaller, tighter curves, with 16 across subdivisions. Wall timing in the small headless fixture is about 1.1-1.2 ms/frame; this is not GPU timestamp timing or whole-game FPS.

Preservation checks PASS for the accepted G9/G10A source and artifacts, 696 pre-existing source/prefix/private-patch checks, original Tests CMake prefix, integration HEAD, Wicked pin/private diff. Diff check exits 0. Earlier preservation-harness path-separator mismatch flagged only the new owned files and was corrected; no protected file changed. No owner project/main/alpha changes or commit/push/merge.

Exact configure/build/compiler/application commands, exit codes, readback metrics, UI sequence, source/executable/shader hashes and outstanding limitations are recorded in FINAL_RESULT.json, BATCH_COMMANDS.json and associated logs. The native scene and all captures are separate disposable outputs. Owner visual acceptance remains pending; Studio/Runtime adoption and ordinary node dragging remain the next bounded step after visual approval.

Actual live readback inspected: rounded lip/toe are continuous; the previous projecting toe is absent. Native author spheres are visible; HUD confirms RUNNING and foam amount 0.3. Falling streaks and broad receiving foam remain an owner-review appearance judgement; no visual sign-off is inferred. Distinct desktop shortcut: Renegade River V2 - Smooth Cascade. Live launch/capture identifiers are in FINAL_RESULT.json. Final feature row has the canonical 16 columns; final diff check exits 0.
