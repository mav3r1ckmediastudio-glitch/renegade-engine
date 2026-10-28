# Renegade River V2 - G12A waterfall appearance continuation (R5)
Date: 2026-10-26
Status: Isolated owner-review candidate. NOT approved, NOT shared editor integration, NOT ready to merge.
Branch/worktree: feature/river-v2-g6-20261023 at C:\Users\paulw\source\repos\renegade-river-v2-integration-20261023
Baseline: G11F owner-accepted editor on process 16032, untouched. Prior G12A native proof was considered too solid/opaque by owner.

## Exact changes this session
1. `Tests/RiverWaterfallProof.cpp`: the proof-only WATER material uses a separate `RiverWaterfallPS.cso`, keeping the approved `RiverCoursePS.cso` intact. Falling forced-foam multiplier reduced (proof only) from 1 to 0.45.
2. `Tools/prepare_waterfall_breakup_shading.py`: generates an isolated shader source stage from Wicked native source and existing G11F course shader stage, patches only that copied stage with travel-time-animated longitudinal sheet breakup on steep falling spans and compiles `RiverWaterfallPS.cso`. No direct WickedEngine source, content, dependency pin or G11F shader change.
3. Proof creates `FallingAerosolSurface` from existing gravity-flight path rows, as a native triangle-domain for emitted spray; it does not enter the bed-pressure domain or change the authored mesh. Two new WickEd native emitter components, falling aerosol and falling haze, supplement lip spray, landing splash and drifting mist (5 total). All remain in native proof scene for persistent fixture save/reopen. Counts scale with existing incoming hydraulic impact energy and width, so zero-energy source has zero emitter rate.
4. Live proof reviewer title includes R5 and the camera starts closer. Unaffected tests and accepted river systems remain unchanged.

## Evidence and testing
- Built Release C++ target `RenegadeRiverWaterfallProof` and CPU `RenegadeRiverWaterfallFlightTests`, successfully; waterfall-only WATER HLSL compiled to 125440-byte pixel shader.
- 40-case CPU gravity/flight and width/rotation suite: PASS.
- CTest focused `RenegadeRiver.*Tests`: 20/20 PASS, 13.34s, exit0.
- 10m x 4m GPU final R5: 1249 live native particles, receiving foam mass 6127.08, 78.285 FPS, PASS.
- Fresh separate-process native scene reopen: 1249 native particles, foam mass 6127.08, 77.466 FPS, PASS.
- R5 small 1.1m x 2m GPU and rotated 10m GPU: both exit0; keep exact logs.
- Native visual screenshot inspected `appearance-R5-live-window.png`, reviewer started on DESIGN with PID 22716, responsive. G11F accepted editor PID 16032 still responding. R5 waterfall has animated vertically broken streaming curtains, native droplets and base mist; appearance needs further owner input. The haze is still visually subtle and visible strands can read as ribbons. Do NOT call this photorealistic, finished, or accepted.

## Artifacts
- Source proof: `Tests/RiverWaterfallProof.cpp`
- Shader build recipe: `Tools/prepare_waterfall_breakup_shading.py`
- Shader content: `BUILD/waterfall-g12a/Content/river/RiverWaterfallPS.cso`
- Test and viewer binary: `BUILD/waterfall-g12a/RenegadeRiverWaterfallProof.exe` and copied `RenegadeWaterfallVisualReviewR5.exe`
- Evidence root: `C:\Users\paulw\source\repos\renegade-spline-water-20261020\BUILD\river-v2-G12A-20261026-waterfall`
- R5 logs: `appearance-R5.log`, `appearance-R5-reopen.log`, `appearance-R5-small.log`, `appearance-R5-rotated.log`, `appearance-R5-ctest.log`, R5 screenshot and closeup `appearance-R5-live-window.png`.
- Prior stages R1-R4 preserved as diagnostic attempts including unusable dark water / over-sparse breakup; R5 supersedes them.

## Reproduction
In PowerShell on DESIGN, from source worktree:
```powershell
$repo = 'C:\Users\paulw\source\repos\renegade-river-v2-integration-20261023'
$stage = "$repo\BUILD\g6\waterfall-veil-stage"
$bin = "$repo\BUILD\waterfall-g12a"
$evidence = 'C:\Users\paulw\source\repos\renegade-spline-water-20261020\BUILD\river-v2-G12A-20261026-waterfall'
$rock = 'C:\RenegadeRiverContinuityReview_20261026\Content\Models\new rock.rasset'
& 'C:\Python314\python.exe' "$repo\Tools\prepare_waterfall_breakup_shading.py" $stage
& "$repo\BUILD\g6\EngineBridge\Release\RenegadeRiverShaderCompiler.exe" "$stage\RiverWaterfallPS.hlsl" $stage "$bin\Content\river\RiverWaterfallPS.cso" ps
cmake --build "$repo\BUILD\g6" --config Release --target RenegadeRiverWaterfallProof RenegadeRiverWaterfallFlightTests --parallel 4
ctest --test-dir "$repo\BUILD\g6" -C Release -R 'RenegadeRiver.*Tests' --output-on-failure
& "$bin\RenegadeRiverWaterfallProof.exe" "$evidence\repro-R5" $rock
& "$bin\RenegadeRiverWaterfallProof.exe" "$evidence\repro-R5-reopen" $rock --reopen "$evidence\repro-R5\waterfall-native.wiscene"
& "$bin\RenegadeRiverWaterfallProof.exe" "$evidence\repro-R5-small" $rock --height 1.1 --width 2
```
Viewer controls: arrows orbit/elevate, +/- zoom, P pause, E emitter toggle, V compare native WATER fallback, Esc close viewer. These are proof controls, not Studio UI.

## Remaining blockers before G12B/G12C
Owner appearance feedback (critical): needs assessment of realism, widthwise variability, high-fall mist, underlying-rock show-through, impact splash and sheet perceived as a few ropes.
G12A still lacks full dry/no-receiver and GPU zero-flow proof, and independent exact-change review.
G12B must bind falls to ordinary authored spline/node edits, Undo/Redo and source removal without stale spray, and cover persistent derived-emitter resource lifetimes.
G12C must verify Test Game, Build Game, Runtime export, relocated artwork/shaders, save/fresh reopen only intended authored state and stress/performance.
No commit, push, merge or main/alpha release was executed in this continuation.
