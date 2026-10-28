# River V2 G9 shared Studio/Runtime appearance integration, 2026-10-25

Owner visually signed off G8H and requested all integration checks together, with failures resolved before presenting one consolidated candidate. G8H is closed visually; its exact source/shaders/artwork/executable are preserved. This adopts that appearance into the existing shared bridge rendering path, not another visual experiment.

## Implementation

RiverFoamAppearanceGpu owns bounded transient scalar detail and original-artwork resources for both single-river contact foam and the existing connected waterway renderer. The accepted 80-byte parameter prefix becomes128 bytes with wave/detail/artwork controls. Per-path history is GPU-derived and never serialized. Existing amounts, foam toggle, threshold, lifetime, inlet speed and source-width controls remain. The existing threshold affects optical coverage only; zero reproduces the signed-off artwork response. Physical bulk transport and source shaders are unchanged.

The existing connected opt-in measured-depth rapids mode now uses the bounded native WATER surface displacement and a refined DERIVED union mesh. Production phase construction uses actual graph reach IDs, replacing the proof's fixed outlet name. Native source geometry/UVs/winding/components remain authoritative and are restored outside Update/Render scopes. The world-space0.12m culling guard is converted through the inverse root transform before expanding local bounds, so scaled roots remain conservative. Mean reflection plane remains unchanged.

Prepare schedules bounded .025s appearance substeps; Dispatch performs the accepted two-pass limited RK2/MacCormack scalar transport after physical mass transport. It runs once per prepared update, not once per rendering view. Nonstructural parameter edits preserve mass AND detail history. Structural/obstacle edits rebuild derived resources safely. No per-frame CPU geometry rebuild, overlay mesh, collar, scrolling-noise substitute, proprietary content or new particle system.

Shared CMake deployment supplies Studio and Runtime with the same four new shaders and original PNG. Build Game explicitly includes all five files through the existing governed support/dependency manifests. All previous support shaders remain. No production profile/schema change, new editor sliders, main/alpha update or mountain-stream work.

## Consolidated verification

Windows DESIGN, Release x64/DX12, existing integration BUILD/g6 cache. Exact absolute command arrays, result codes, durations, hashes and samples are in B9/FINAL_RESULT.json, configure-exit.json, build-exit.json and suite-results.json. B9 is the G9 integration evidence directory under the spline-water worktree BUILD tree.

Commands:
- cmake -S I -B I/BUILD/g6
- cmake --build I/BUILD/g6 --config Release --target RenegadeStudio RenegadeRuntime RenegadeRiverFlowIntegrationProof RenegadeRiverWaterwayBindingProof RenegadeRiverFlowRuntimeLifecycleProof RenegadeRiverFlowPackageProof RenegadeRiverFlowFieldTests RenegadeRiverFlowProfileTests RenegadeRiverWaterwayTests RenegadeRiverRapidsTests RenegadeRiverSurfaceMotionTests RenegadeRiverWaterwayTransportTests RenegadeRiverFoamTransportTests RenegadeBuildStageTests --parallel 2
- python B9/stage.py
- python B9/suite-final.py
- python B9/standalone-integrated.py
- python B9/preserve.py

Final batch PASS:
- 8 CPU/package unit tests: flow field/profile, waterway authoring/transport, rapids, surface motion, foam transport and BuildStage.
- Single native river: enable/disable; actual spline/width edits and Undo/Redo; reverse; frozen scoped material restoration; archive save; fresh-process reopen; new project creation.
- Static obstacle cache: unrelated/foreground/above-water animation causes no rebuild; entering/moving/removing actual immersed rock invalidates correctly.
- Connected contact and measured rapids, each create AND fresh reopen: native front-face/winding/world-position parity; source geometry restored; native component counts unchanged; actual GPU mass and bounded nonzero detail; all six live parameter modes and Undo retain exact frozen mass/max/detail signature; source-width/intensity preserve topology/flux; inlet speeds rebuild once; positive native UV animation causes zero rebuilds.
- Actual Test Game: unsaved profile snapshot retains UUID/settings and excludes renderer handles; ready;30s hold; return; temporary session cleanup; edit/Undo after return.
- Actual Build Game: dependency-aware staging, support closure, native-scene cooking, validation and promotion.
- Relocated standalone: moved to an unrelated folder; launched from unrelated CWD; all five new support files byte-identical to candidate; active river; captured visible native water/foam; normal close0; no missing-resource or shader log errors; zero steady rebuilds.

The final Test Game measured 75.37 FPS and maximum sampled CPU river preparation 0.2546 ms. Relocated standalone measured 74.69 FPS and maximum preparation 0.2095 ms. These are this scene on DESIGN, not whole-engine guarantees. Standalone performance run uses alwaysactive and a post-startup steady interval; normal background-window pause is not a river performance regression.

Native editor project opening and visual river view checked. The accepted G8H proof remains the visual reference; the new Studio/Runtime candidate is integrated engineering evidence, not a new independent release acceptance.

## Failures corrected within this verification

An old integration fixture attempted to move an already positioned Player Start, correctly receiving a no-op refusal. Its test placement now differs slightly; production commands are unchanged.

Deeply nested evidence paths hit the preexisting Windows MAX_PATH limit in fixture journal creation and export copy. Disposable final review/check projects use shorter paths. Global arbitrary-length Windows path support is NOT claimed fixed.

The frozen staged candidate initially lost filesystem identity with the explicitly governed built-in river normal. Its verified built-in normal now retains exact identity. Only the disposable copied review scene's known built-in normal reference was normalized; empty/custom normal slots and owner source were preserved. Fresh reload, export cooking and relocated launch prove closure.

Temporary normalizer code double-closed a native Archive; CDB/link-map evidence localized the access violation to ZSTD compression through Archive::Close. Corrected to a single scoped archive lifetime and confirmed exit0. No Wicked source was changed. Early preparation and capture-script failures remain in evidence; final batch results are separate.

A first standalone harness looked for log.txt beside the executable; packaged Runtime correctly logs under its project-specific LocalAppData directory. The final harness copies the actual WickedRuntime.log. A concurrent foreground UI operation also paused an earlier normal window and invalidated that timing/capture; final measurements are taken without UI interference, with explicit alwaysactive and steady-interval filtering.

## Preservation and limits

Both preservation audits PASS: owner projects, prior dirty code/features, accepted binaries, upstream pin/private Wicked diff, branches and HEADs preserved. Snapshots of modified existing files are under B9/before. G8H source/art/shaders/executable hashes reverified. No commit/push/merge or accepted-build replacement.

Single rivers retain their existing contact-flow domain. Connected opt-in rapids retain the prior one validated two-inlet flat-confluence limit. This does not add hydraulic free surfaces, cascades, waterfall support, particle spray, wake turbulence, general connected graphs, deformed CPU collision/buoyancy, deformed motion vectors or displaced planar reflections. Mountain streams stay paused. Mirrored artwork repetition remains the accepted G8H tradeoff.

Next: use the integrated candidate as the single review build; independent exact-change verification is still required before a release/merge gate can close. Do not reopen G8H visual refinement without new owner feedback.
