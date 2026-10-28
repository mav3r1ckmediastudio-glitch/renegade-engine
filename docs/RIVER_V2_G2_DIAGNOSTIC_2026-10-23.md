# River V2 G2 native flow diagnostic — 23 October 2026

Status: implemented, built and self-verified on DESIGN. Owner visual sign-off
and independent verification OPEN. G3 has NOT started.
Authority: CODEX_RIVER_V2_IMPLEMENTATION_HANDOFF_2026-10-23.md, phase G2.
HEAD fc54fd4dbd6f2d2194c2cf0ceb53804086504681 remains unchanged.
All changes are uncommitted in the isolated spline-water worktree.

## Implemented boundary

Tests/RiverFlowDiagnosticProof.cpp is a standalone native RenderPath3D proof.
It consumes the unchanged G1 immutable RiverFlowFieldService and the existing
G1 saved curved flat-water scene, with real textured native terrain and rock.
No owner project or original source scene is written. No Studio, Runtime,
EngineBridge source, terrain, production WATER shader or material feature changed.
No Unreal content is accessed or copied.

Diagnostics are native renderer DrawLine/DrawPoint submissions. They are not
entities, meshes, materials, overlay planes, foam or custom WATER bindings.
The same stock water renders below them. Geometry, material identity and normal
texture remain native; there is no directed-water normal motion in G2.

There are six views of the CPU data: arrows sized by uniform authored speed and
coloured by bank distance; point-sampled valid coverage; tangent arrows;
velocity magnitude; signed lateral fraction; and projection validity/ambiguity.
The masks are diagnostic point samples, not pixel classifications of obstacles.
No rock interaction or waterline mask is claimed.
Forward/reverse batches are prepared once from immutable fields, not by scanning
the scene or rebuilding a field every frame. The current fixture has 66 arrows
(594 debug strokes), 792 mask sample positions: 406 valid, 386 outside and zero
ambiguous. Both flow directions preserve the same 1m/s authored magnitude.
Unsupported overlaps are rejected by G1 before drawing; the current projection
view correctly contains no ambiguous cells. Rejected curve authoring/repair UI
is outside this standalone proof.

Arrows originate on interpolated ACTUAL native ribbon bank rows. Anchor and
shaft endpoints must pass native-ribbon query coverage before submission.
Opaque geometry depth-tests the strokes; diagnostics are not painted onto rock.
These arrows do not turn around the rock: obstacle response belongs to G4/G5.

## Controls and owner viewer

A separate desktop shortcut was created: Renegade River V2 Flow Diagnostics.
It launches the isolated proof binary and a fresh uniquely named BUILD review
output each time. It does not replace or launch an older Studio candidate.
The interactive proof was opened and its responding native window and visible
HUD/river/arrows were captured in viewer-live.png. No pre-existing application
was stopped. Owner acceptance is not inferred from that inspection.

- F: native WATER / current debug mode.
- R: reverse CPU flow direction, preserving geometry and speed.
- 1–5: waterline, full overhead, two oblique viewpoints, and along-river view.
- M: cycle all six diagnostic modes.
- 6 coverage, 7 tangent, 8 speed, 9 lateral, 0 projection check.
- Space: pause/resume native time. Status explicitly shows PAUSED/RUNNING.
- WASD: move; arrow keys: look; Shift: faster movement; Esc: close.

Keyboard automation posts real WM_KEYDOWN messages through the same window
message handler as owner input. Native/debug, reversal and all five viewpoints
are exercised in the automated proof. The selected data mode, direction,
authored speed, legend and controls are shown in the native wiGUI HUD.
Live interactive mode skips the lengthy automated capture sequence.

## Exact build, tests and captures

Evidence root: BUILD/river-v2-G2-20261023-2030/.
baseline.ps1 captured git status, HEAD/pin, pre-existing file hashes, Wicked diff
and running process ownership before source changes. Original extended files
were backed up under before/. Resolved toolchain paths and exact working
directories are in exact-verify.ps1.

VS18 BuildTools-bundled CMake/CTest, Windows x64 Release:

~~~powershell
& $cmake --build BUILD/renegade --config Release --target RenegadeRiverFlowDiagnosticProof RenegadeRiverFlowFieldTests RenegadeRiverSplineTests RenegadeRiverFoamTests --parallel 4
& $ctest --test-dir BUILD/renegade -C Release -R '^(RenegadeRiverSplineTests|RenegadeRiverFoamTests|RenegadeRiverFlowFieldTests)$' --output-on-failure
~~~

Build exit0; focused CTest3/3 PASS. G1 CPU/native source files remain unchanged.

The executable and test Content/DLL dependencies were copied into the unique
proof bin directory. From that own bin working directory:

~~~powershell
& ./RenegadeRiverFlowDiagnosticProof.exe "$out/exact-proof" "$root/BUILD/river-v2-G0G1-20261023-1900/exact-native/native-baseline.wiscene"
& ./RenegadeRiverFlowDiagnosticProof.exe "$out/exact-reopen" "$out/exact-proof/native-diagnostic-scene.wiscene" --reopen
python "$out/verify-exact-images.py"
~~~

Both native processes exit0. Field hash14999496401268953580 matches G1.
All six diagnostic modes render. Eye-height and overhead forward/reverse captures
were visually inspected; heads follow the bends and reverse; speed remains1m/s.
Native scene is saved while diagnostics are ON. Names/transforms/meshes/objects/
materials counts remain unchanged. Archive reload preserves native WATER with
no custom shader or changed userdata; rebuilding the flow produces the same hash.
The separate SceneService process opens with debug OFF and zero submissions;
textured native water/terrain/rock were visually checked again.

For deterministic F-toggle image comparison, native GPU initialization runs for
120 positive-dt frames BEFORE pausing. Scene time remains frozen during captures.
Native-before/restored captures use the same blue-noise frame phase modulo256.
After F OFF there are zero submitted debug lines or points. The whole 3D RGB
image restores exactly: mean absolute error0/0/0. The arrow/native comparison
MAE .32774/.36367/.33441 and forward/reverse comparison .18856/.21001/.19360
confirm visible debug/reversal differences. Blank/washed-out images fail the
verification script's native-frame variance check.

Native unpaced fixture FPS approximately74.95 before diagnostics,77.72 with them
hidden afterward, same1264x761 resolution. No hidden diagnostic penalty was
observed in this short run; this is not many-river or production performance proof.
Shader error count0. Native water motion/appearance is unchanged by G2.

Files: exact-build.log; exact-ctest.log; exact-proof.log; exact-reopen.log;
exact-results.txt; pixel-results-exact.json; flow-arrows.csv inside each proof.
Raw3D/HUD images and GPU-layer-composited *-review.png captures are under
exact-proof/. Separate-process native stills are under exact-reopen/.
The reviewer composites combine the actual saved GPU3D and HUD textures;
they are not generated concept imagery.

Initial proof-v1 captures were visually FAILED despite nominal structural tests:
pausing at startup prevented required native GPU initialization and produced
washed-out/white frames. Corrected by normal positive-dt warm-up before pausing.
Those images are retained as failed evidence and are not a visual pass.
A speed colour legend mismatch was corrected before the exact final build.
The current1m/s marker colour matches its orange legend.

## Files changed for G2

- Tests/RiverFlowDiagnosticProof.cpp (new)
- Tests/CMakeLists.txt (append this one isolated proof target)
- HANDOFF.md (prepend checkpoint, retaining previous bytes)
- docs/ARCHITECTURE.md (append diagnostic ownership boundary)
- docs/FEATURE_MATRIX.csv (append standalone diagnostic exposure row)
- This document

All G1 service/tests/proof files and other pre-existing work remain unchanged.
No original Wicked source/pin change, commit, push, merge, accepted build
replacement, owner project save or third-party content redistribution.

## Next gate

Stop for owner visual review of G2. Per the primary handoff, G3 begins only
after G2 approval. G3 must first prove optional flow-driven native WATER normal
motion, preserving original optics. No foam, obstacle response, rapids,
connected waterways, mountain stream repair, creator authoring integration
or packaged Runtime parity is delivered by this diagnostic milestone.

## Owner acceptance — 23 October 2026

Owner tested the live G2 proof and explicitly confirmed: "that all works as
expected". This accepts the bounded G2 diagnostic behavior. G3 remains paused
pending an instruction to continue implementation; acceptance alone does not
expand scope. Premature isolated G3 source/target were archived under
BUILD/river-v2-G3-20261023-1900/paused-source and removed from active source/CMake.
This does not accept directed WATER motion, foam, obstacle response, mountain
streams, integration, or an independent release gate.


## Owner continuation clarification

Owner clarified that G2 acceptance correctly opens G3 and explicitly instructed
continuation. The prior agent-imposed pause is superseded. G3 isolated normal
motion proof is implemented for owner review; see RIVER_V2_G3_NORMAL_MOTION_2026-10-23.md.
