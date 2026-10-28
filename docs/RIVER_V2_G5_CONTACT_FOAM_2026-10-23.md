# River V2 G5: isolated contact foam proof
Date: 2026-10-23. Windows DESIGN / x64 DX12 Release. Owner visual gate OPEN.
HEAD fc54fd4dbd6f2d2194c2cf0ceb53804086504681 plus preserved dirty work.
Wicked pin 3a800b7134aafe58461093c8abb2e274d4e64033.

## Scope and files
G3 directed native WATER and G4 classifier are owner accepted. This milestone adds opt-in foam only in a separate proof executable. No Studio/Runtime integration, persisted profile, production release, mountain stream, tributary, rapids/splash or physics certification.
New files: Tests/RiverFoamTransport.h; Tests/RiverFoamTransportTests.cpp; Tests/RiverContactFoamProof.cpp; EngineBridge/shaders/RiverFoamTransportCS.hlsl; EngineBridge/shaders/RiverContactFoamBlock.hlsli; Tools/prepare_river_contact_foam_proof.py. Tests/CMakeLists.txt adds two isolated targets and one test. Architecture, feature matrix and handoff record the gate.
All generated evidence below is relative to BUILD/river-v2-G5-20261023-2015/ (B).

## Implementation
Uses the actual G4 native flat scene, real world-transformed rock triangles and native terrain. No owner scene or terrain modification. G4 disposable water height is already -0.15m; G5 does not lower it again.
CPU preparation reuses G1 native-ribbon coverage. A bounded 240-iteration harmonic potential approximates flow around solid footprints with no-flux boundaries and authored endpoint potential. This is kinematic potential flow, not a converged Navier-Stokes solution. Speed is bounded by 2.2 times authored speed. Source exists only at G4 actual rock-water contact and weak bank contact. Leading strength derives from outward contact normal dotted with local authored direction. Raising/submerging/removing rock yields zero rock source. Reversal changes leading side.
1024x256 float32 textures: derived velocity RG signed unit X/Z, B speed m/s, A fluid; source RG rock/bank emission, B bounded wake influence, A signed lateral metres. Two transient float4 state textures hold density, age mass and two material-coordinate masses. All are proof-local GPU resources.
Compute performs bounded semi-Lagrangian backtrace, solid-path checks, source addition, exponential decay and a nine-second maximum mass-weighted age. An alternating bounded wake velocity perturbation deforms existing foam; it never creates foam. Procedural detail modulates real source/density only.
Native G3 normals keep their original base spline velocity. Foam transport uses obstacle-responsive velocity. WATER-only material shading adds lit foam albedo and reduces transmitted refraction using Wicked's native ocean material operation. Existing reflection, lighting, normals and clear-water optics remain. No overlay meshes, exposed-rock shader, screen-depth halo or Unreal content.
The final breakup combines carried macro detail with metre-scaled porous clumps and antialiased small bubbles. Earlier broad ribbon-shaped patches failed visual inspection and were replaced. Natural stream footage informed morphology; no reference pixels became engine assets.
GPU payload is exactly 48 bytes; transport parameters are validated by readback. An earlier padded payload silently delivered zero turbulence; the readback test caught it. Explicit reset safely replaces both history textures. Normal live changes preserve existing foam while source/flow changes; X explicitly clears it. F native stops compute and restores original shader/userdata.
No persistent metadata/schema migration. Ephemeral descriptor IDs and custom shader IDs are stripped before native archive save. Mesh/object counts stay unchanged.

## Exact verification
Run B/polish-verify.ps1 with PowerShell -NoProfile -ExecutionPolicy Bypass -File. The script records the complete commands, native include paths, executable staging and all exit codes:
- python Tools/prepare_river_contact_foam_proof.py B/polish-shader
- BUILD/renegade/EngineBridge/Release/RenegadeRiverShaderCompiler.exe B/polish-shader/RiverContactFoamPS.hlsl WickedEngine/WickedEngine/shaders B/polish-shader/RiverContactFoamPS.cso ps
- Same compiler: EngineBridge/shaders/RiverFoamTransportCS.hlsl native shader include directory B/polish-shader/RiverFoamTransportCS.cso
- VS18 CMake: --build BUILD/renegade --config Release --target RenegadeRiverContactFoamProof RenegadeRiverFoamTransportTests --parallel 4
- CTest: --test-dir BUILD/renegade -C Release -R '^(RenegadeRiverFoamTransportTests|RenegadeRiverWaterlineTests|RenegadeRiverFlowFieldTests|RenegadeRiverSplineTests|RenegadeRiverFoamTests)$' --output-on-failure
- From owned B/polish-bin: RenegadeRiverContactFoamProof.exe B/polish-proof BUILD/river-v2-G4-20261023-1940/final-proof/native-waterline-proof.wiscene B/polish-shader
- Same executable: B/polish-reopen B/polish-proof/native-contact-foam.wiscene B/polish-shader --reopen
- Same executable: B/polish-benchmark same saved archive and shader directory --benchmark

CPU transport: 100253 assertions PASS; upstream source109.084 vs trailing2.38727; reverse, zero, removed, submerged/above, settings and malformed inputs checked. Focused CTest5/5. Final polish verification passed build/proof/reopen/benchmark exit0 and shader errors0; final polish logs are authoritative.
GPU density finite/bounded, no invalid/solid foam; default mass357.437. After emission stopped for2s mass419.14 ->257.707, centroid X2.26978 ->3.51082 (1.241m downstream). Additional10s gives exact zero. Continuous obstacle move preserves167.034 of178.338 mass. Reverse centroid X-1.34197. Zero flow produces zero new mass. Above/submerged/removed rock source0; weak independent bank foam remains legitimately present.
Fresh process rebuild matches default field hash1531344054962016492 and density357.437. Native archive has no transient bindings. Controlled native before/after RGB MAE0 on the review run; final polish-pixel-results.json repeats this and checks sky/clear-water regions against same-time directed water.
Inspect actual GPU stills at five views, scenario1-5, reverse, continuous move and 90-frame/3s animation. B/polish-media.py produces composited HUD reviews, motion sheet, pixel measurements and B/polish-foam-motion.mp4. No generated imagery substitutes for rendering.
Short review benchmark at1264x761: native74.9683fps; directed74.9756; foam74.9704, all at75Hz cap. Final polish benchmark: native74.9598fps; directed74.9785; foam74.9702. This is a capped single-fixture comparison, not GPU milliseconds or many-river scalability proof.
B/preservation-final.json compares every baseline dirty/untracked file hash, except authorized CMake and three documentation files, and compares existing Wicked binary diff byte-for-byte. HEAD and pin unchanged; git diff --check required.

## Review controls
Desktop: Renegade River V2 Contact Foam G5.
F native / directed / foam; R reverse; C original/moved/rotated-scaled/above/submerged/removed; Space pause; N single step; K emission off/on; Z zero/default flow; X clear history.
+/- intensity; [] lifespan; T turbulence; B source width; G breakup threshold; 1-5 views; WASD/arrows/Shift camera; Esc close.
Changes to rock, reverse, source width and zero flow retain downstream foam to move or decay. Isolated automated source-exclusion cases explicitly reset history.

## Remaining gates
Owner must judge the exact live build's movement, breakup and appearance before G6. No owner G5 approval is inferred from tests. Fixture is deliberately simple and flat. LOD0 static rest geometry, projected classification and single aggregate wake footprint are approximations; multiple separate obstacles, deforming geometry, steep slopes, device loss and many rivers remain unverified. Live dt is capped1/30s. No splash particles/vertical whitewater or hydrodynamic forces. No production save/export/Test Game/Undo integration has been attempted; these are G6 after approval. Mountain streams remain paused. No commit, push or merge.

## Reference provenance
Valve SIGGRAPH2010 flow-map paper: https://cdn.cloudflare.steamstatic.com/apps/valve/2010/siggraph2010_vlachos_waterflow.pdf
NPS natural stream footage: https://www.nps.gov/media/video/view.htm?id=9E04AF2D-D4EF-4D71-8BC6-6F2AC6E41F2C
Reference footage/frame inspection and ffmpeg helper are BUILD-only. No Unreal proprietary asset, code or texture copied.


## Final DESIGN review launch and audit
Owned polish-bin viewer launched PID26088; process responding and exact title
Renegade River V2 - Contact Foam G5; G5_INTERACTIVE_READY=1.
B/owner-launch.json records executable and unique output; B/owner-live.png is
the inspected live window with foam RUNNING. No keys sent after owner launch.
B/owner-inspect.py confirms an exposed-rock sample RGB MAE0 against same-time
flow-only rendering. This sample is not an all-view opaque-pixel certificate.
Final audit:130 baseline files checked; only Tests/CMakeLists.txt, HANDOFF.md,
docs/ARCHITECTURE.md and docs/FEATURE_MATRIX.csv differ as authorized.
Unexpected changes0; existing Wicked binary diff identical; HEAD/pin unchanged;
git diff --check exit0 (existing line-ending notices recorded). No commit/push/merge.
