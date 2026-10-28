# River V2 G3: isolated directed WATER normal proof — 23 October 2026

Status: implemented and self-tested on DESIGN Windows x64/DX12; OWNER VISUAL ACCEPTANCE OPEN. G2 was accepted by the owner. Owner explicitly authorized G3 continuation after correcting an unnecessary pause. No G4 work or Studio/Runtime production integration.

## Implementation and boundary

New files:
- EngineBridge/shaders/RiverFlowNormalBlock.hlsli
- Tools/prepare_river_flow_normal_proof.py
- Tests/RiverFlowNormalProof.cpp
- This document

Tests/CMakeLists.txt adds only RenegadeRiverFlowNormalProof. Architecture, feature matrix and HANDOFF record the boundary. G1 service/tests and G2 proof remain unchanged.

The proof consumes the same immutable G1 CPU field built from actual Wicked spline geometry. A transient 256x128 R32G32B32A32_FLOAT texture covers world XZ bounds. Texel centres map minX/minZ to maxX/maxZ, row index increases with world Z. RG contains signed unit X/Z direction, B authored speed in metres/second, A validity. Invalid texels are zero. Linear sampling divides interpolated direction/speed by alpha before normalizing direction; conservative alpha fade avoids extrapolating outside banks. No raster is baked to disk. DX12 texture and descriptor creation are checked.

The 48-byte proof-local raw parameter buffer contains world bounds; transient texture descriptor, time override, direction sign and debug mode; native tile metres, bounded phase period, running clock offset and enable weight. The proof tags material userdata.y with 0x52464c33 and userdata.x with its transient buffer descriptor ONLY while custom rendering is active. This is not a persisted metadata schema. No migration, cache revision contract, multiple-river integration or authoring service is introduced.

The shader preparation tool verifies pinned objectHF.hlsli SHA256:
27d73178076aa7333374bef9a162e996629db01e749556881236d978daf41ce5.
It writes a BUILD-only staged copy and an absolute-include wrapper, replacing only the two normal samples plus an explicitly selected WATER velocity-debug output. All native reflection/refraction, fog, lighting, transparency, normal strength and TBN code remains in place. No Wicked source/pin edit occurs.

World velocity is projected into native ribbon UV space using the actual WATER TBN (U points left across the native banks; V follows the native tangent). Native four-metre normal tiling and strength are retained. Each normal layer uses two staggered phase samples with triangular crossfade. A reset sample has zero blend weight; distortion remains bounded to at most one tile. The second layer has a separate phase offset. The proof uses the original native normal texture, no foam/noise substitute or overlay geometry.

GPU parameter replacement occurs only at explicit diagnostic changes, before app.Run, following WaitForGPU. The static flow raster is generated once. This blocking proof mechanism is not a proposed production per-frame update path.

## Exact verification commands and results

Evidence root: BUILD/river-v2-G3-20261023-1900.
Run from the active isolated repository:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File BUILD/river-v2-G3-20261023-1900/exact-verify.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File BUILD/river-v2-G3-20261023-1900/exact-reopen.ps1
python BUILD/river-v2-G3-20261023-1900/exact-verify-pixels.py
```

The exact-verify script records the actual VS18 CMake path and commands:
- python Tools/prepare_river_flow_normal_proof.py <evidence>/exact-shader
- RenegadeRiverShaderCompiler.exe <staged wrapper> <native shader include directory> <owned output CSO> ps
- cmake --build BUILD/renegade --config Release --target RenegadeRiverFlowNormalProof RenegadeRiverFlowFieldTests RenegadeRiverSplineTests RenegadeRiverFoamTests --parallel 4
- ctest --test-dir BUILD/renegade -C Release -R '^(RenegadeRiverSplineTests|RenegadeRiverFoamTests|RenegadeRiverFlowFieldTests)$' --output-on-failure
- exact-bin/RenegadeRiverFlowNormalProof.exe <exact-proof> <G2 disposable native scene> <exact-shader>
- Existing G2 proof in a separate process with <exact-proof/native-flow-proof.wiscene> --reopen.

Release build exit0. Shader compilation exit0, 89060-byte PS. Existing compiler prints its historical RIVER_UNDERWATER_SHADER_COMPILED label; shader-provenance.json identifies the actual staged normal proof. Focused CTest3/3 pass. Actual DX12 proof exit0, shader errors0. 18556 valid raster pixels, CPU field hash14999496401268953580 unchanged from G1/G2. Scene object/mesh counts and checked optical values preserved. Bindings removed before native archive save; archive assertions pass. Separate-process SceneService reopen exit0, debug OFF, no custom shader, stable counts and identical CPU field hash.

Deterministic captures use positive-dt native initialization before freezing scene/game clocks; frame count aligned modulo256 for still comparisons:
- Native BEFORE versus F-OFF/native-restored: zero changed RGB pixels, MAE0/0/0.
- Native versus directed t2: 45196 changed pixels; RGB MAE .133920/.178689/.085713 (0..255 scale), confirming intended override is actually used.
- Directed t2 versus t2.25: 45186 changed pixels.
- Forward versus reverse at t2.25: 45074 changed pixels.
- Unchanged sky/upper terrain crop: RGB MAE0/0/0.
- Reset neighbourhood 3.999 ->4.000 ->4.001 seconds: RGB MAE approximately .148/.162/.112 on each side; no large asymmetric jump.
- Three-second actual GPU animation and six-frame waterline contact sheet inspected. Water stays textured and reflective; no exposed-rock white polygons or added foam. These still/sequence checks do not substitute for owner judgement of natural downstream motion.

Important numerical limitation: leaving the separately compiled custom shader active with enable weight0 (native-fallback) is not bit-identical to the embedded native shader: 4192 pixels differ, global RGB MAE .001744/.002201/.001068, maximum channel difference15. It executes the original normal sample expressions, but compiler/pipeline numerical equivalence is not guaranteed. Explicit F OFF removes the custom shader and restores original userdata; that native image IS byte-equivalent in the controlled test. Do not claim bit-exact custom fallback.

## Owner viewer

Separate owned binary: <evidence>/exact-bin/RenegadeRiverFlowNormalProof.exe.
Desktop shortcut: Renegade River V2 Directed Water G3.
launch-owner.ps1 creates a unique disposable BUILD review directory per launch, loading the saved native proof. It never replaces Studio, Runtime, an accepted executable or an owner project.

F: native/directed. R: reverse. V: coloured velocity map toggle (turn off to judge water). Space: pause/resume. N: advance directed phase by0.05s while paused. 1-5: waterline/overhead/side viewpoints. Esc: close.

Live viewer was opened as PID7912 and actual window screenshots inspected. Proof-local control messages and screenshots exercised pause, step, reverse, mode and viewpoint changes. Initial control automation found two visible process windows and stopped before sending anything; it was corrected to identify the exact G3 window title. Screenshots establish rendered native/directed/velocity and paused/running states; no precise timing or independent behavioural acceptance is inferred from this smoke check. Viewer remains available for owner review.

## Remaining gates

Owner must judge natural motion DOWN the curve, reversal, close normal quality and appearance through phase resets. This is a flat, shallow single-river fixture with constant authored1m/s, not hydrology or production performance proof. Two-phase blending repeats periodically; no turbulent flow/physical obstacle response is claimed. The proof expects its native four-metre normal tile and UV0 layout; general scaling, invalid UV layouts, device loss, cache invalidation and multi-river ownership remain later integration work.

No foam, waterline classifier, rapids, connected waterways, mountain repair, editor authoring, Undo metadata, Test Game/export parity or production deployment is delivered. Mountain streams remain PAUSED. G4 begins only after the G3 visual gate. No commit/push/merge. All pre-existing dirty work and Wicked modifications preserved; see final preservation audit.


## Owner visual acceptance

Owner reviewed the live G3 viewer and confirmed: "it looks great i have to say".
G3 isolated visual result is accepted. G4 classification diagnostic is next;
this is not foam or production integration acceptance.
