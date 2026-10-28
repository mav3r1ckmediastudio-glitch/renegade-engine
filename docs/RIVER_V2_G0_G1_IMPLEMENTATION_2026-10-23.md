# River V2 G0/G1 implementation — 23 October 2026

Status: implemented and self-verified on DESIGN, Windows x64 Release/DX12.
Independent verification / owner gate acceptance remain open. G2 has NOT started.
Authority: CODEX_RIVER_V2_IMPLEMENTATION_HANDOFF_2026-10-23.md.
Branch feature/spline-water-v1-20261020; unchanged HEAD
fc54fd4dbd6f2d2194c2cf0ceb53804086504681. All work remains uncommitted.

## Scope and ownership

New EngineBridge RiverFlowFieldService is a CPU-only, immutable transient oracle.
It does not invoke scene update, native mesh generation, terrain regeneration,
sculpting, Jolt, shader compilation, graphics allocation, material edits or
serialization. Studio/Runtime have no call site or binding for it.
FlowService continues to mean Story Flow; it is not reused.
No Unreal content was copied, extracted or redistributed by this milestone.
Mountain streams remain paused. No commit, push, merge or candidate promotion.

## G0 evidence and preservation

Evidence root: BUILD/river-v2-G0G1-20261023-1900/.
baseline.ps1 recorded status-before.txt, manifest.txt, preexisting.patch,
preexisting-hashes.json, wicked-before.patch, candidates-before.txt,
accepted-worktrees.txt and running-before.txt. No Renegade executable was
reported running at baseline. Newer combined/importer/probe worktrees were
identified and not built or edited.

Existing dirty-state Release RiverSpline and RiverFoam tests compiled and
passed 2/2 before G1 source changes: baseline-build.log / baseline-ctest.log.

The new isolated native proof loads the already disposable, embedded-texture
native-contact-scene.wiscene through SceneService, removes rejected foam overlays
only from its loaded copy and bends its two end controls by 2.5m in Z.
It preserves the source scene file, real terrain, shallow central banks,
licensed textured rock and stock WATER optics. No source owner project is saved.
The resulting fixture is a diagnostic river, not a natural-scene quality claim.
Saved copies and captures exist only in new BUILD proof directories.

Inspected waterline and overhead PNGs and recorded 30 frames at 100ms cadence,
assembled into native-3seconds.gif. Native normals/reflection/refraction and
textured terrain/rock are visible. Material remains RGB .031/.044/.047,
depth-haze alpha .126, normal strength 1, refraction .12, custom shader disabled.
Baseline unpaced render performance was approximately 79 FPS; this is fixture
performance, not a production Studio/Runtime or many-river benchmark.
Native archive save/load preserves material identity/optics and valid normal map.
Shader error count is zero. G0 F-toggle restoration is not applicable yet:
there is no flow renderer or transient graphics binding to disable in G1.

## G1 API and geometry contract

BuildRiverFlowField accepts a const native scene + WATER spline entity,
RiverFlowSettings and optional one-per-control speed multipliers. It returns
shared_ptr<const RiverFlowField> only after complete validation; refusal leaves
the caller's existing result unchanged and returns a reason.

The service copies current node LOCAL transforms into a private native
SplineComponent, invokes native PrecomputeSplineNodeDistances/EvaluateSplineAt
and then uses scene.ComputeEntityMatrixRecursive for the root. This is the same
local-first order as RunSplineUpdateSystem and SampleSplineWorldPoints.
Re-evaluating centripetal Catmull-Rom in world space would be wrong under
nonuniform root scale. No cached scene world matrix is silently republished.

Every exact native ribbon row parameter is retained. Native control knots also
seed adaptive subdivision. Quarter/midpoint evaluations bound horizontal sample
spacing by sampleStepMeters and projected curve chord deviation by 2mm.
Widths use native scale.x Catmull interpolation, including positive-width checks.
Actual bank endpoints are transformed individually in native float32 operation
order. Reconstructing them as transformed centre +/- transformed offset caused
a real transformed-bank boundary mismatch during development and was corrected.

World XZ arc length is cumulative in authored order. Centred finite differences
produce forward tangent/right, one-sided at ends; grade is signed drop per
horizontal metre. Speed is authored base speed times linearly interpolated
multipliers over native local control-distance knots. Flat stretches retain
positive speed unless zero is explicitly authored. No hydrology is invented.
Reversal changes tangent/right, query lateral sign, grade and upstream-relative
query distance; it does not change scene geometry or speed magnitudes.

Queries first classify against the ACTUAL native ribbon triangle topology,
using individually transformed banks, not a circular capsule beyond endpoints.
Inside points obtain nearest sampled-centreline direction/speed/along distance,
triangle-interpolated water height/lateral fraction and distance to native banks
or end caps. Stable lowest row/triangle order resolves shared edges.
Nonfinite/outside queries are invalid; ambiguous nonlocal overlaps are invalid.
No unbounded raster, texture or disk-baked velocity map is created.
A future diagnostic raster may encode signed world X/Z in RG, speed in B and
validity in A; GPU format/binding/orientation remain explicitly deferred.

A native backturn test is reproduced with the SAME strict negative-plane test
as Wicked: if native mesh generation would clamp a bank backwards, G1 refuses
rather than pretending the analytic centreline matches that ribbon. Projected
curve intersections, overlapping native banks, folded/degenerate triangles,
loops, filled/tubular splines, inverted banks and branch ambiguity also refuse.
Broad valid hairpins preserve true increasing-arc orientation and pass.

Bounds: 512 controls; at most 8192 samples and native rows; 131072 native
evaluations; 8 million topology candidate checks; recursion depth 24;
world coordinates +/-100km; total horizontal arc <=100km.
Base speed 0..50m/s, multiplier 0..10 with resulting speed <=50m/s;
sample step .001..100m. Invalid quaternion/scale/ancestry/duplicate control,
nonfinite inputs and limits fail before publishing a field.
Field storage at the absolute count caps is under 1MiB for sample/row arrays,
excluding temporary validation storage and allocator overhead.
Queries are bounded linear scans; production spatial acceleration is deferred.
Construction requires a stable scene read point; const queries need no scene.

DeterministicHash uses version-1 explicit little-endian float32 field words,
canonical signed zero, ordered samples/rows, and no padding or GPU/ECS handles.
It is a repeatability fingerprint on the tested pinned implementation/toolchain,
not a promised cross-platform persistent schema. No metadata key, serializer,
scene migration, persistent cache or descriptor format is introduced.

## Exact execution and results

The authoritative resolved command lines, working directories, exit checks and
toolchain path are in baseline.ps1 and exact-verify.ps1 in the evidence root.
The VS18 BuildTools-bundled CMake/CTest were used, not an unspecified CI build.

From the active worktree, with $cmake set to that resolved executable:

~~~powershell
& $cmake --build BUILD/renegade --config Release --target RenegadeRiverFlowFieldTests RenegadeRiverFlowFieldNativeProof RenegadeRiverSplineTests RenegadeRiverFoamTests --parallel 4
& (Join-Path (Split-Path $cmake) 'ctest.exe') --test-dir BUILD/renegade -C Release -R '^(RenegadeRiverSplineTests|RenegadeRiverFoamTests|RenegadeRiverFlowFieldTests)$' --output-on-failure
& BUILD/renegade/Tests/Release/RenegadeRiverFlowFieldTests.exe
~~~

The native proof executable and existing test Content/DLL dependencies were copied
into the evidence root's own bin. Final proof runs use that private working
directory and distinct new output directories; no accepted executable is replaced:

~~~powershell
& "$proof/bin/RenegadeRiverFlowFieldNativeProof.exe" "$proof/exact-native" "$root/BUILD/ocean-contact-foam-20261023/native-contact-scene.wiscene"
& "$proof/bin/RenegadeRiverFlowFieldNativeProof.exe" "$proof/exact-native-repeat" "$root/BUILD/ocean-contact-foam-20261023/native-contact-scene.wiscene"
python "$proof/exact-evidence-check.py"
python "$proof/preserve-check.py"
~~~

Release build exit 0. Focused CTest 3/3 pass. Two separate CPU processes pass
11010 assertions each, with identical hash 13056308274502041147.
Cases include straight +X; left/right 90-degree curves; reverse; flat and
varying grade; speed and width profiles; zero speed; very short path; broad
hairpin; +/-180-degree direction crossing; stale caches; parent rotation and
nonuniform root scale; end caps; negative Catmull width overshoot; malformed,
duplicate, nonfinite, inverted, cyclic, intersecting, sharp-edit and oversized
inputs; output preservation on failure; deterministic repeatability and bounds.

A 160-control ~1.59km curve used 1112 samples / 57824 sample bytes, approximately
2–3ms CPU construction in the recorded run. Tight sampling refuses at the cap.

Native DX12 proof exits 0 in two separate processes. The oracle is actual
Scene::Update / RunSplineUpdateSystem mesh vertices and stored vertex tangents,
not a second mathematical Catmull implementation. It checks the real saved
river plus five fixtures, forwards and backwards: right 90-degree, left
90-degree, parent transform/nonuniform scale, width profile and edited control.
Maximum measured centre discrepancy 0.000005416m; actual bank and full-width
discrepancy zero; minimum tangent dot approximately .999903.
The 72-row fixtures preserve exact native row parameters in the adaptive field.
Native sharp-edit reproduction has four collapsed bank edges and is refused.
Native WISCENE reload rebuilds the same field hash without serializing flow data.

CPU construction between two readbacks leaves that same native frame PNG
byte-identical; no material, mesh or index array changes. This proves no graphics
side effect of construction, not a future flow-renderer A/B acceptance.
Separate-process native parity lines/hashes match. Animation changes are
measured and recorded in exact-evidence-check.json.

Final logs: exact-build.log; exact-ctest.log; exact-cpu-1.log;
exact-cpu-2.log; exact-native.log; exact-native-repeat.log; exact-results.txt.
Images/animation/native archive: exact-native/ and exact-native-repeat/.
Sample table is printed in each CPU log. No debug arrows or foam are rendered.

Development failures were not acceptance evidence: initial proof compilation
used two nonexistent MaterialComponent getters, corrected to pinned native
members; the first transformed bank query exposed the endpoint-operation-order
error described above. Final runs use the corrected implementation.
Earlier intermediate captures/logs remain separate from the exact final runs.

## Exact files changed by this milestone

- EngineBridge/include/renegade/bridge/RiverFlowFieldService.h (new)
- EngineBridge/src/RiverFlowFieldService.cpp (new)
- Tests/RiverFlowFieldTests.cpp (new)
- Tests/RiverFlowFieldNativeProof.cpp (new; G0 and native G1 oracle)
- EngineBridge/CMakeLists.txt (only two new source entries)
- Tests/CMakeLists.txt (only the two new test/proof targets and CPU registration)
- docs/ARCHITECTURE.md (additive implemented CPU boundary)
- docs/FEATURE_MATRIX.csv (additive T3 CPU-only row)
- HANDOFF.md (new top checkpoint; all earlier content retained)
- This document

Preservation checks compare every pre-existing modified/untracked top-level
file hash, allowing only the five intentionally extended existing files above.
All other captured pre-existing files remain byte-identical.
The Wicked binary diff remains byte-identical before/after, SHA256
8C2BAF4B25E9FCF4516DB413EF0C6101E21355387E671DC81C186BFEDDCA37D4.
Original objectHF.hlsli SHA256 remains
27D73178076AA7333374BEF9A162E996629DB01E749556881236D978DAF41CE5.
HEAD/pin unchanged; git diff --check passes.

## Stop boundary / unresolved work

Stop here. G2 diagnostic arrows require the next authorized gate; G3 directed
normal motion, GPU flow textures, automatic obstacle/waterline masks, foam,
rapids, connected waterways, Studio/Runtime lifecycle, authoring persistence
and all packaged gameplay integration remain unimplemented by this milestone.
Existing optics/reflection-plane limitations and mountain issues are not fixed.
No owner visual acceptance, independent exact-state review, or release closure
is inferred from these engineering passes.
