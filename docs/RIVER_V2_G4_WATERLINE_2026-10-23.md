# River V2 G4 waterline classification diagnostic — 23 October 2026

Status: isolated CPU/native DX12 proof implemented and self-tested; OWNER VISUAL GATE OPEN. G3 directed normal motion was explicitly owner accepted ("it looks great i have to say"). G4 supplies classification only. G5 foam has not started.

## Files and ownership

New:
- Tests/RiverWaterlineClassifier.h
- Tests/RiverWaterlineTests.cpp
- Tests/RiverWaterlineProof.cpp
- EngineBridge/shaders/RiverWaterlineDebugBlock.hlsli
- Tools/prepare_river_waterline_proof.py
- This document

Tests/CMakeLists.txt appends two isolated targets and one CTest registration. Architecture, HANDOFF and feature matrix record the gate. G1/G2/G3 implementation, Studio, Runtime, all owner projects and Wicked source/pin remain unchanged. The CPU helper stays proof-local under Tests until the geometry gate is accepted; no premature production interaction service or persisted schema is introduced.

## Geometric method

At a stable read point, gather actual native static mesh LOD0 triangles, transform object-local positions with ComputeEntityMatrixRecursive, and distinguish real terrain chunks using native TerrainComponent::chunks entity identity. Skip WATER and non-renderable objects. Triangles are spatially culled against the actual native ribbon bounds. One real rock object and one terrain candidate contribute 2506 and672 triangles in this fixture.

The CPU classifier intersects each world triangle with the flat water height. Actual slice segments define rock and bank contact; no camera-depth-difference contact test, absolute-depth halo, scrolling noise or hand-authored region is used. Coplanar faces do not invent a perimeter. Projected barycentric triangle heights identify shallow terrain and obstacle footprints. Signed vertical intersection winding per candidate approximates the occluded crossing-rock footprint; min/max projected height distinguishes wholly submerged and wholly above-water footprints. This is a mesh-derived geometric approximation, not a 3D SDF or collision/solid certification.

A transient1024x256 float32 texture stores category R, bank distance G, rock distance B and WATER coverage A. Coverage comes from the unchanged G1 actual native ribbon oracle. Contact band is0.12m. Projected pixel centres increase from world minX/minZ with rows increasing Z. Texture lookup uses nearest integer texels so categories never interpolate into fake classes. The grid remains visibly discrete in the diagnostic.

Categories:
-0 outside/invalid
-1 blue: open WATER
-2 yellow: real bank/shallow terrain contact
-3 red: rock-water intersection band
-4 cyan: projected submerged geometry
-5 purple: above-water footprint, excluded from contact source
-6 dark: occluded solid/dry-terrain footprint, excluded

The owned debug block runs only inside native WATER shading after lighting, using a32-byte transient buffer and userdata tag0x52474c34. It cannot execute on exposed opaque rock fragments. Normal sampling and all production/native source remain untouched. F OFF removes the custom shader and restores original userdata. There are no added scene meshes or overlays. This diagnostic uses stock native WATER as its A/B base; the accepted G3 viewer remains separate and unchanged.

Bounds: flat horizontal water including bank elevations only; at most64 surface candidates,250000 retained triangles,2million native triangle inspections in gathering,1,048,576 raster cells and32million counted raster work items. Invalid/nonfinite and unsupported sloping-water inputs refuse rather than emitting a mask. Snapshot rebuild happens only at initialization or explicit scenario change, never an every-frame scene scan. Fixed loops over candidate/cell arrays add bounded bookkeeping work beyond the raster work counter.

## Disposable bank fixture

The inherited G3 scene has waterY+0.15, terrain maximumY0 and cut bed minimumY-1.2. It therefore contains NO true terrain-water intersection. Initial G4 correctly reported bankLines0; this was not a bank-classification pass.

Only in the disposable G4 in-memory copy, all native river controls are lowered0.30m in world space to waterY-0.15. Existing terrain is untouched: no sculpt, heightmap, blendmap or generator edit. Actual native spline geometry is regenerated before rebuilding G1 coverage. The resulting84 terrain slice segments prove genuine bank contact. This is a proof fixture adjustment, not a river creation default or a change to the accepted G3 scene/source archive.

Proof scenarios restore the exact captured original rock transform/visibility before each change:
0 original;1 move +2m worldX;2 rotate/tilt and nonuniform scale;3 move +20m worldY wholly above water;4 move-20m worldY wholly submerged;5 hide/remove from candidate set. The large vertical offsets are deliberate source-exclusion tests, not realistic gameplay placement or submerged visibility proof. Parent transforms are inverted when expressing world translations in native local transforms.

## Commands and evidence

Evidence root: BUILD/river-v2-G4-20261023-1940.

Actual Release build and CTest commands, shader stage/compile, standalone proof and fresh-process reopen are recorded in final-verify.ps1 and final-render.ps1. The final-render script was used after correcting a generated input-path typo; the initial build and tests had already passed.

```powershell
cmake --build BUILD/renegade --config Release --target RenegadeRiverWaterlineProof RenegadeRiverWaterlineTests --parallel 4
ctest --test-dir BUILD/renegade -C Release -R '^(RenegadeRiverWaterlineTests|RenegadeRiverFlowFieldTests|RenegadeRiverSplineTests|RenegadeRiverFoamTests)$' --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File BUILD/river-v2-G4-20261023-1940/final-render.ps1
python BUILD/river-v2-G4-20261023-1940/verify-final-pixels.py
```

final-verify.ps1 records the actual VS18 CMake executable and complete command arguments. final-render.ps1 runs the owned final-bin executable against the unchanged G3 disposable native scene, then a SECOND process with final-proof/native-waterline-proof.wiscene --reopen. Reopen explicitly avoids lowering the already-prepared fixture twice.

Results:
- Release build exit0, final-build.log.
- Focused CTest4/4 PASS, final-ctest.log. CPU classifier628 assertions PASS; actual native-flow library regressions remain passing.
- Owned PS compilation exit0,87708 bytes. Compiler's historical output label says RIVER_UNDERWATER_SHADER_COMPILED; exact-shader/provenance.json identifies this WATER-only classification override.
- Final actual native DX12 proof exit0, shader errors0.
- Original/restored/fresh-process mask hash1861685726328513262, identical.
- Original mask:178 rock slice segments,84 bank segments;654 contact pixels,2262 bank pixels,140 submerged,3 above-footprint exclusions,3910 occluded/dry exclusions;141681 open;113494 outside.
- Moved and rotated/scaled geometry produces changed hashes and follows the mesh. Raised, submerged and removed scenarios have zero CPU rock-contact pixels and zero rendered red contact pixels.
- Fresh-process native/default-off render and rebuilt mask exit0, final-reopen.log.
- Same-phase native BEFORE versus restored: RGB MAE0/0/0.
- Exposed-rock sample crop and unchanged sky crop versus classification: RGB MAE0/0/0.
- Overhead rendered red source pixels161 in original;0 in above/submerged/removed images.
- GPU readbacks inspected from overhead, waterline and both sides. Actual composited HUD/3D *-review.png images are proof readbacks, not concept images.

Exact pixel results: final-pixel-results.json. Slice endpoints: original-waterline.csv and scenario CSVs inside final-proof. Native archive has descriptors/custom shader removed, mesh/object counts unchanged. No diagnostic texture is baked into the archive.

Initial corrected failures are retained as evidence:
1. V1 raised-rock test FAILED because imported parent scale made local+20 much less than20 world metres; test assertion caught persistent contact. Corrected world-to-parent-local transform; V2 scenarios pass.
2. Inherited above-terrain water plane had no physical bank intersections; isolated water-only fixture adjustment supplies a real terrain intersection.
3. An added native-waterline capture initially left the next named overhead capture at waterline camera; final source explicitly restores the overhead pose.
4. Generated final input path accidentally named a nonexistent G3 final-proof directory; corrected to G3 exact-proof. No source owner asset was written.
5. Pixel log parsing needed PowerShell UTF16 BOM handling; final verifier decodes it explicitly.

## Owner viewer and next gate

Desktop shortcut: Renegade River V2 Waterline G4.
Owned final-bin executable; launch-owner.ps1 loads the original G3 disposable native scene and creates a unique review output directory per launch. No accepted executable is replaced.

F native/mask; M all/rock-only/bank-only; C cycle scenarios0-5;1-5 viewpoints; Esc close. Red/yellow are classification colours, not finished water or foam. The G3 directed-water viewer remains separately available.

Owner must verify contact remains on WATER at the real rock base from different angles, tracks scenario movement/rotation, disappears without intersection and distinguishes bank contact. G4 remains open until owner approval. No foam aesthetic acceptance is inferred.

Limitations: static rest geometry in the known fixture, flat water, bounded raster resolution, LOD0 CPU geometry versus native render LOD and projected/occluded-footprint approximations. No animated/deformed mesh support, arbitrary closed-manifold guarantees, under-bed occlusion certification, live Studio object-event cache, many-rock performance proof, device-loss/Runtime packaging, automatic flow/foam or integration is delivered. Those must be addressed at their appropriate later gates. Mountain streams remain PAUSED. No commit/push/merge.


## Owner review — 23 October 2026

Owner exercised the live G4 scenario controls; the viewer log records raised,
submerged, removed and restored masks with expected source exclusions. Owner
feedback: "this is first rate work, whats next? actually building the foam etc?"
Record the isolated G4 diagnostic as owner accepted. This does not certify a
general production classifier or foam appearance. G5 contact-source transport,
breakup and decay is the next implementation gate; it has not started.
