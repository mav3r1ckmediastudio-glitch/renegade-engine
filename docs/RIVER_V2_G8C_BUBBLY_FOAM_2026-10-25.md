# River V2 G8C: bubbling foam appearance proof

2026-10-25. Engineering checks PASS; owner subsequently requested further visual work (camouflage-like breakup and flat surface). Superseded by the separate G8D surface-motion proof; G8C was not visually accepted. Shader-only proof; no production Studio/Runtime adoption. Owner feedback: movement and coverage good, whitewater looks grainy/torn/flat; requested more bubbling and authorized shader-first refinement.

## Changes

New optional EngineBridge/shaders/RiverBubblyFoamFunctions.hlsli provides bounded deterministic2D rounded bubble caps in world-metric cells, varied radii and small smooth breathing; no emission. Tools/prepare_river_bubbly_foam_proof.py stages an owned WATER variant from unchanged accepted shader inputs. Changes foam surface lighting/normal response, opaque-froth weighting, softened derivative-aware breakup, distance/flow-warp footprint filtering and a cosine-smooth staggered phase blend. Two detail scales represent small froth groups and finer detail. No imported/Unreal textures, particles, mesh overlays, foam collars or noise-only sources. Native WATER optics and accepted flow/density/source/decay code unchanged. Flat surface remains flat geometrically; normal-based microrelief is not actual displaced foam or free-surface waves.

Tools/prepare_river_bubbly_graphics_proof.py generates a bounded derivative of the existing rapids regression harness in BUILD. Tests/CMakeLists.txt APPENDS only optional RenegadeRiverBubblyFoamProof; original bytes retained as exact prefix. No EngineBridge production deployment changes. Corrected proof performance measurement explicitly renders animated foam, rather than timing native-restored mode. Interactive B toggles old/new pixel pipelines with the same density textures/state/clock; readback asserts exact history retention. Space pauses;1-5views;F native/flow/foam;existing amount slider retained. Starting view4 is the close river view.

## Commands/results on DESIGN

I denotes integration repository; B denotes experimental BUILD/river-v2-G8C-20261025-bubbly-foam. Existing I/BUILD/g6 reused. Python is C:/Python314/python.exe -X utf8.

1. python I/Tools/prepare_river_bubbly_foam_proof.py B/shader-review.
2. I/BUILD/g6/EngineBridge/Release/RenegadeRiverShaderCompiler.exe B/shader-review/RiverContactFoamPS.hlsl I/WickedEngine/WickedEngine/shaders B/shader-review/RiverContactFoamPS.cso ps. PASS0;132508bytes.
3. cmake -S I -B I/BUILD/g6; cmake --build I/BUILD/g6 --config Release --target RenegadeRiverBubblyFoamProof --parallel2. Final PASS0/18.92s. Logs configure-review-final.log/build-review-final.log. Two staged recipe syntax/anchor errors caught during configure and corrected before final build; failed logs retained. Existing applications/executables untouched.
4. python B/verify.py initial comparison: baseline0/49.22s;initial appearance0/40.42s;fresh reopen0/39.88s. Numeric tests passed but visual iteration1 rejected because whitewater coverage thinned too far. Iteration sources/captures retained.
5. python B/verify-v2.py:0/40.69s and0/39.86s. Coverage restored; further refined group scale/contrast for final review.
6. python B/verify-review.py FINAL: new appearance0/47.90s;fresh saved-scene reopen0/46.65s;zero resource errors. Same-state old/new A/B history assertion PASS. CPU/GPU transport parity, source-off expiry to zero, zero-flow no foam, no new scene geometry and native WATER reflection checks retained from original harness. Five transported foam-state readbacks(one step/multiple steps/established/source-off/zero flow) byte-identical to original-shader baseline. Native-before/native-restored RGB pixel-identical in both final cases. comparison-review.json and verification-review.json.
7. Actual animated visible-foam timings: baseline74.9658FPS;final74.9612FPS;fresh final value recorded below/validation-final.json. These are measured with foam enabled, unlike earlier prototype native-restored timing.
8. python B/launch-preview.py: new standalone interactive proof; ready; no resource errors. Actual paused B old/new toggles log HISTORY_UNCHANGED=1 both ways; identical mass2093.92/max2.48779 and clock93.0903 retained during comparison. Live screenshots/readbacks under B/live; A/B automated same-clock images B/bubbly-review/appearance-previous.png and appearance-bubbly.png. Old accepted preview/editor remain unchanged.

## Candidate and limits

Desktop Renegade River V2 - Bubbling Foam Review. B opens previous/new appearance; Space freezes water for inspection. candidate-live.json authoritative. No editor/Runtime integration or new package export claimed for G8C: G8B stays protected and unchanged. Native water, rock textures, source response, real bed geometry and density transport preserved. This is a material improvement to review, not a claim that realistic whitewater volume is finished. No actual standing-wave silhouette, splashes, droplets or unsteady wake solver; mountain work paused.

Preservation: validation-final.json confirms accepted foam block, compute source, CPU depth model, Wicked shader source, G8A preview executable and G8B Studio executable exact SHA256 unchanged; original owner project files exact; private Wicked diff unchanged; HEAD unchanged; existing Tests/CMake prefix exact. No commits/push/merge. No new release acceptance. Old/new shaders differ only in isolated proof staging.

Current proof PID28368; exeSHA256 437388a601961ee71e8c3bfa5ae146c8c461a42345f1494238c160f6dc84aa1b; PS SHA256 01dc5f86a245cc290551ea5764c877cc5fd900a5ce90361a2c21fb7cb2c2f132.
Fresh visible-foam FPS 74.9745.
