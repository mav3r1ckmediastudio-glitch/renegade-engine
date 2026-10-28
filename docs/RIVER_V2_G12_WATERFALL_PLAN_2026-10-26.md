# G12 waterfall completion - agreed direction and implementation plan
2026-10-26. Owner visually accepted G11F ("that looks great") and confirmed the uploaded SHADERSOURCE Waterfall Tool is the final river feature before main integration. No main integration is authorised yet.

## Existing accepted baseline
Preserve G11F river water optics, detailed normals/downstream motion, continuous buried endpoints, banks, cascades, rock contact foam and responsive editing. Its live review executable avoids the machine's image-specific full Page Heap setting; Windows registry was not changed. G11F owner review16032 must not be closed or overwritten for this work.

## Reference evidence and limits of inspection
Supplied archive contains19 Unreal packages:18 uasset and1 demonstration umap,5774112 uncompressed bytes. Includes BP_WaterfallTool, AdditionalCascades struct, waterfall material/instance, distance-field gradient and world-aligned texture-motion material functions, a waterfall plane, top/bottom splash and mist particle systems and five textures. Embedded Blueprint comments describe spline mesh construction with per-point start/end scale, front/end fades, optional ground tracing, width-scaled particle spawning, and additional waterfall splines positioned relative to the main end. Material labels describe depth fade and world-aligned flow.
This is package/name/comment inspection, not a verified rendering or full Blueprint decompilation. No Unreal binaries or commercial textures have been copied into Renegade. Implement Renegade-owned behaviour and original visual assets using existing native WATER and emitted-particle capabilities.

## Creator outcome
Keep existing spline/node workflow. A river over a sufficiently steep drop produces a connected falling sheet, restrained lip spray, bottom splash, local drifting mist and whitewater carried into the lower river. Width and incoming flow determine effect scale; zero flow disables generation. Nodes move the whole effect consistently; Undo/Redo and native save/reopen preserve author controls. Avoid a separate editor and a large mandatory panel of technical controls. Optional additional falls use additional ordinary authored paths after single-fall parity is proven.

## Important architectural finding
Current RiverCourseService caps total height change at2.5m. RiverGradedFlow is a prescribed-surface XZ channel model with explicit grade, speed/depth and height bounds. RiverGradedTransport relies on a single-valued surface height, measured bed and conservative projected flux. A tall free-falling sheet does not satisfy these assumptions. Do not merely increase maximumHeightRange/maximumGrade, disable safety checks, invent deep water beneath airborne sheet, or push a near-vertical jet through the 2D bed solver.

## G12A - isolated visible10m proof
1. Derive explicit lip, free-fall and receiving regions from the same authored spline controls. Preserve author IDs/controls and G11F geometry on unaffected reaches. Smooth lip and toe automatically; retain an actual connected sheet and consistent longitudinal travel coordinates.
2. Model the free-flight portion separately: travel time from incoming velocity and gravity, bounded sheet width/thickness and art-directed breakup. Incoming discharge drives optical/effect strength; this is a waterfall presentation model, not a general three-dimensional fluid solver. Upper/lower wet reaches retain bed/obstacle transport. Downstream arrival requires an explicit receiving reach/landing, not a guessed flat plane under every fall.
3. Render through Renegade-owned extensions to pinned native WATER. Keep normal/reflection/refraction ownership explicit. Falling-sheet opacity/breakup may be separate from river optical thickness. No claim of independent multi-height planar reflection parity.
4. Reuse Wicked native EmittedParticleSystem for lip spray, ballistic landing splash and slower mist. Prefer managed derived resources with stable lifetime and deterministic reconstruction; no save-time render cache/temporary entities leaking into authored scene. Existing course binding scopes are a precedent, but native emitter Scene::Update/visibility/material/instance lifetime must be proven before using temporary binding. Do not add/remove partially simulated emitters naively each frame.
5. Build a disposable10m cliff fixture plus the accepted1.1m cascade comparison. Include two widths, rotated orientation, a lower river with a real rock and a dry/nonreceiving end. Capture moving water and particles together at a useful camera angle. Keep all author projects/live G11F untouched.

Acceptance for G12A: actual GPU viewer, visible continuous lip/sheet/toe, convincing restrained splash/mist, documented particle count/performance, CPU travel/region/width/zero-flow checks and original G11F regressions. Owner appearance review gates shared integration; do not present a CPU-only model as a working waterfall.

## G12B - shared editor/runtime integration
Integrate accepted geometry/effects through bridge services and the existing spline/flow controls. Cover dragging/node edits, stale worker snapshots, actual pointer release, Undo/Redo, source removal, scene replacement and resource cleanup. Effects must follow current geometry and never leave stale spray/mist at the old location. Keep unrelated rock transforms on ordinary commands. Engine resources remain executable-relative across native browsers.

## G12C - persistence, Runtime and integration readiness
Verify native save/fresh reopen contains only intended author state and reconstructs derived effects; real Test Game return/edit; public Build Game and relocated Runtime with packaged artwork/shaders; accepted G11F scene parity and actual UI performance. Record exact source/binary hashes, commands, failed attempts, limitations and owner acceptance. Independent exact-change review remains required by AGENTS before declaring a release gate complete. Main integration follows the owner's final acceptance, not this planning checkpoint.

## Current state
G12A native appearance prototype built and open for owner review. Forty CPU geometry/flight cases and20/20 river regressions pass. Actual10m/1.1m/rotated GPU captures and fresh fixture reopen pass at approximately77FPS. See RIVER_V2_G12A_WATERFALL_PROOF_2026-10-26.md for exact evidence and limitations. This is not shared Studio/Runtime integration or a completed release gate. Dry/nonreceiving and full GPU zero-flow coverage remain; owner appearance review precedes G12B. Accepted G11F editor remains open.
