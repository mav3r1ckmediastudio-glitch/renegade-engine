# River V2 G8D: isolated native WATER surface-motion proof

2026-10-25 DESIGN. Engineering proof PASS; owner visual review pending. This is a bounded kinematic wave foundation, not finished realistic whitewater or a hydraulic free-surface solver. G8C was not visually accepted: owner identified camouflage-like breakup and requested actual water/foam undulation, then supplied three screenshots and a 38.7-second reference recording. References informed visual goals only; no proprietary assets or code copied.

## Implementation and preservation

- New EngineBridge/src/RiverSurfaceMotionProof.h refines the ACTUAL saved native WATER triangles using shared-edge longest-edge bisection. Shared midpoint vertices preserve winding and the native XZ footprint; UVs interpolate existing native UVs. Initial naive all-edge subdivision hit the explicit geometry budget and was replaced, rather than raising that budget.
- Actual proof input: 830 vertices / 1099 triangles / 342.156 square metres. Final: 27062 vertices / 53136 triangles. Maximum target edge 0.35m; 160k vertex / 250k live-triangle / 800k refinement-work triangle ceilings. The immutable native-spline-derived junction surface remains the geometry authority; no independent spline approximation.
- Shared world-grid phase/amplitude is baked once from actual native reach arc queries and the existing measured bed/obstacle domain. Incoming arc zero aligns with outgoing arc zero at the junction. An eight-pass phase smoothing operates on the shared wet union. Boundary distance pins the edge strip; solid/dry cells have zero amplitude. Amplitude <=15% resolved local depth and <=0.10m; this fixture reaches 0.0347465m. This is a kinematic model, not a claim of physically solved wave celerity or obstacle wakes.
- New EngineBridge/shaders/RiverSurfaceMotionFunctions.hlsli and owned staging add actual vertex displacement and matching normal/tangent response to native object and transparent-shadow VS variants. Preserve native normal orientation: Wicked flips the backface normal in the PS. A first capture caught inverted orientation and it was fixed before final validation.
- Native WATER pixel optics, lighting, transparency, refraction, reflection and normal maps retained. Foam uses the same deformed surface; no extra surface geometry, collars, overlays or particles. Primary foam coverage comes continuously from transported density instead of thresholded repeated noise cutouts. Finer, low-contrast cap detail replaces the initial oversized bubble-wrap appearance. Transport sources/velocities/density/decay remain exact accepted bytes.
- Tools/prepare_river_surface_motion_proof.py and prepare_river_surface_graphics_proof.py stage separate BUILD-only shaders/harness. Tools/RiverShaderCompiler.cpp adds optional vs mode; existing default CS and ps behavior unchanged. Tests/CMakeLists.txt appends optional proof and CPU-test targets, preserving its previous exact byte prefix. No production renderer deployment, owner scene changes, Studio/Runtime replacement or new profile schema.
- CPU geometry/domain construction and CreateRenderData occur once during setup. No per-frame CPU mesh upload, triangle scan or domain rebuild. Five wave-field samples per displaced vertex provide the height and central-difference slope. GPU foam simulation is unchanged.

## Exact commands and results

I = integration repository, B = BUILD/river-v2-G8D-20261025-surface-motion in the spline-water repository. Existing I/BUILD/g6 reused. Python executable C:/Python314/python.exe with -X utf8. CMake/CTest are the Visual Studio 18 BuildTools bundled executables; machine-local COMMANDS.txt and scripts record full absolute command arguments.

1. cmake -S I -B I/BUILD/g6.
2. cmake --build I/BUILD/g6 --config Release --target RenegadeRiverSurfaceMotionProof RenegadeRiverSurfaceMotionTests --parallel 2. PASS; configure-final.log/build-final.log and final configure-review.log/build-review.log. Earlier build-2 missing iostream diagnostic corrected; all failed attempts retained.
3. cmake --build I/BUILD/g6 --config Release --target RenegadeRiverShaderCompiler --parallel 2. PASS (initial build-1.log).
4. python I/Tools/prepare_river_surface_motion_proof.py B/shader.
5. I/BUILD/g6/EngineBridge/Release/RenegadeRiverShaderCompiler.exe B/shader/RiverContactFoamPS.hlsl I/WickedEngine/WickedEngine/shaders B/shader/RiverContactFoamPS.cso ps; repeat with RiverSurfaceVS.hlsl -> RiverSurfaceVS.cso vs and RiverSurfaceShadowVS.hlsl -> RiverSurfaceShadowVS.cso vs. All PASS0. Initial local staged include lookup failed; staging now writes its actual generated include path.
6. ctest --test-dir I/BUILD/g6 -C Release -R 'RenegadeRiver(SurfaceMotionTests|RapidsTests|WaterwayTransportTests|FlowFieldTests)$' --output-on-failure. PASS4/4, 3.69s. Surface CPU0.47s: native area342.156; max refined edge0.349561; 7378 wet cells /753 pinned boundary cells; deterministic vertices/indices/field; depth amplitude bound; original native vertices unchanged; manifold edge incidence; original winding retained; unexpected multi-material input refused.
7. B/bin/RenegadeRiverSurfaceMotionProof.exe B/review-final B8A/closure-final/native-rapids.wiscene B/shader B/shader/RiverRapidsTransportCS.cso. PASS0; full commands/exits/wall times in review-final-exit.json. GPU CPU transport parity max1.33514e-05; exact history retained across wave and appearance toggles; source-off expires to0; zero-flow emits0; native before/restored RGB pixel-identical; object/mesh entity count unchanged; all five density-state readbacks byte-identical to G8C original-shader baseline; zero shader/resource errors.
8. Same command with B/reopen output and B/review-final/native-rapids.wiscene input. Fresh saved-scene load PASS0, same checks and density baseline; resource closure PASS. Exact args and wall time in reopen-exit.json.
9. python B/preserve.py; python B/audit-all.py; python B/validate.py. PASS. Owner original four files, accepted G6/G7C/G8A/G8B/G8C artifacts and original private Wicked diff/HEADs preserved. Legacy foam/compute/rapids model and G8C sources byte-exact. git diff --check PASS (line-ending advisory only). Source/binary/shader hashes in validation.json.
10. python B/compress-own.py. Reversible NTFS compression of only four new generated build files; all hashes unchanged. No files deleted; no extra cache created.

## Performance

Actual visible foam, same refined geometry and camera, alternating wave-off/on twice. Presentation VSync disabled only inside the isolated benchmark; foam stepping fixed at 1/75 second per frame so uncapping does not silently reduce the simulation work. 60 warm frames and 240 measured frames per arm; GPU completion awaited. These are wall frame-throughput measurements, not isolated GPU timestamps. Early first-arm settling is visible; do not report a negative cost or pretend the fixture result proves whole-game performance.

review-final: OFF 1.24914ms, ON 0.919727ms, OFF 0.915939ms, ON 0.917469ms.

reopen: OFF 1.22409ms, ON 0.918833ms, OFF 0.919324ms, ON 0.914547ms.

The warm paired differences are below useful measurement resolution in this fixture; no measurable motion-only regression established. Refinement overhead relative to the original 830-vertex mesh has not been isolated by this A/B, which uses the refined mesh for both arms. No claim of ~1000 FPS in an actual game. Live preview retains normal presentation pacing.

## Review and unresolved work

Desktop shortcut: Renegade River V2 - Surface Motion Proof. Title same; candidate-live.json authoritative. V waves on/off without resetting density; Space pause; 1-5 views; B previous/new foam appearance; F native/directed/foam; existing foam amount slider retained. Separate original accepted previews/editor preserved.

Surface relief is intentionally small and this remains a visual proof. Dense foam can still be too uniform; cohesive density coverage is a foundation, not final froth structure. Wave frequencies are prescribed rather than derived from a time-dependent shallow-water solution. No breaking crests, hydraulic jumps, spray, splashes or unsteady obstacle wakes. The native planar-reflection camera remains at the mean plane; geometric water query/collision/buoyancy is also still the mean plane. These need explicit treatment before production adoption. TAA uses the existing native transparent path; no new deformation motion-vector output was added, so inspect temporal stability rather than claiming it solved.

One static flat confluence only. No mountain/slope/cascade work; reference sloped rivers do not authorize unpausing it. No full Studio integration or exported Runtime adoption of G8D, no release acceptance, no commits/push/merge. Next: owner in-motion review of actual surface/foam relationship; refine crest/foam structure and then bounded integration only after the proof is visually convincing.

## Coverage revision after owner rejection (2026-10-25)

Owner screenshot/feedback: too much homogeneous white. Initial G8D continuous-coverage appearance is NOT accepted. The new isolated shader uses lower density-to-coverage gain (.65 versus2), a wider gradual coverage response (.10..70 versus.015..42), age-dependent visible thinning, and broad low-contrast flow-aligned packing variation. No thresholded noise cutout islands reintroduced; no changed foam generation/transport. Existing amount slider remains1x in the comparison. Surface motion VS/geometry/compute/executable unchanged byte-for-byte. Separate shader-coverage-review folder retains the white-blanket PS as PreviousFoamPS.cso for exact-state B comparison.

Exact command: python B/coverage-revision.py stages and compiles the PS then executes the existing proof against B8A/closure-final/native-rapids.wiscene. Shader128824bytes; full GPU regression PASS0/69.82s. Density readbacks five cases exact; wave/appearance history exact; native before/restored RGB exact; zero shader/resource errors. Benchmark warm frames ~1.04ms for either wave state in this small fixture; historical previous shader ~.92ms is not a controlled pixel-shader cost comparison. Do not claim zero appearance cost or whole-game FPS. No CPU rebuild or per-frame geometry work added. coverage-validation.json pins current source/PS hashes; older validation.json remains the previous-iteration manifest.

Current viewer PID24880; candidate-live.json and original desktop shortcut now select the revised shader. Superseded disposable proof was normally closed via its own PID/title; accepted editor/builds unchanged. B toggles revised vs white blanket at same state. More clear water and thinner tails visible in captured comparison; owner review pending. No commits/push/merge. Exact commands/results under BUILD; no production adoption.
