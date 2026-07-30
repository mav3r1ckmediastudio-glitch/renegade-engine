# Roadmap

## Current milestone

**Phase 3 — Studio foundation**
Target duration: 6–8 weeks
Status: Phase 2 closed after DX12, Vulkan, DPI, input, persistence, and
Runtime separation passed on the project owner's Windows GPU; physical HDR was
not available. The Project Hub, workspace, viewport selection, fly camera, and
the editor usability milestone all passed packaged DX12 testing. The Viewport
and Proving Ground Visual Foundation, Editor Visual Polish, and Environment
Authoring milestones are merged into `main` at `8787a4c` and accepted on
packaged DX12 and Vulkan. Renegade draws its own infinite shader grid, persists
editor preferences, and authors sky, fog, volumetric clouds, and cloud shadows
through undoable commands. `main` is clean with no branch awaiting review.

Remaining for the phase: light and material authoring, then the asset-facing
work that Phase 4 depends on.

## Phase plan

| Phase | Duration | Outcome |
|---|---:|---|
| 0. Charter and baseline | 1 week | Traceable product target and pinned Wicked source |
| 1. Reproducible build | 2–3 weeks | Wicked core, editor, tests, and samples build reliably |
| 2. Architecture/UI proof | 3–4 weeks | Branded editor shell proves viewport, save/reload, HDR, DPI, and input |
| 3. Studio foundation | 6–8 weeks | Dependable project, scene, hierarchy, inspector, and undo workflows |
| 4. Asset pipeline | 6–8 weeks | Repeatable import, reimport, IDs, dependencies, and content browser |
| 5. Scene/render exposure | 8–10 weeks | Core world and visual systems authorable in Renegade |
| 6. Physics/audio/gameplay | 8–10 weeks | Small interactive packaged game |
| 7. Advanced systems | 10–14 weeks | Animation, terrain, particles, fluids, and specialist components |
| 8. Scripting/runtime/export | 10–12 weeks | Lua workflow and repeatable standalone builds |
| 9. Exposure audit/platforms | 8–10 weeks | Demonstrated coverage of the pinned Wicked baseline |
| 10. Beta and V1 | 8–12 weeks | Hardened, documented, independently verified release |

Planning range: 16–21 months at approximately 12–18 focused hours per week.
The full reasoning and milestone calendar are in `docs/MASTER_PLAN.md`.

## First four weeks

### Week 1

- [x] Establish repository and Wicked pin.
- [x] Commit charter, roadmap, AI workflow, handoff, and feature matrix.
- [x] Verify licence and notice obligations.
- [x] Record the Windows build-machine toolchain.

### Week 2

- [x] Build Windows Debug and Release.
- [ ] Run the original editor and reference scenes.
      Superseded: `WICKED_EDITOR` is forced `OFF` so upstream sample and editor
      targets stay out of the Renegade build graph. The Wicked Editor remains a
      parity oracle to read, not a target to build.
- [x] Validate DirectX 12 and check Windows Vulkan.
- [x] Add repeatable scripts, evidence records, and Windows CI.
- [x] Capture Windows logs, timings, and screenshots.
- [ ] Obtain independent verification of a release gate by a second AI or
      human. Still outstanding; every gate so far was verified by the project
      owner alone.

### Week 3

- [x] Create the first buildable `Studio` and `EngineBridge` targets.
- [x] Create the first buildable `Runtime` target.
- [x] Load a known WISCENE in a blank custom application.
- [x] Implement minimal scene and selection services.
- [x] Obtain a green Windows CI build.
- [x] Capture human-observed Studio and Runtime viewport screenshots.

### Week 4

- [x] Prototype hierarchy, transform inspector, and gizmo.
- [x] Implement and visually verify repeated undoable commands.
- [x] Save and reopen the edited scene, including repeated Save As.
- [x] Complete mixed-DPI, HDR-availability, and Vulkan checks.
- [x] Record the UI-toolkit ADR.

## Immediate next gate

**Light and Material Authoring.** The Environment inspector proved the pattern;
extend it to the two component types that still block the creator from fixing
the generated scene's look.

- Add a Light inspector for `LightComponent`: type, colour, intensity, range,
  cone angles, cast-shadow, volumetrics and `volumetric_boost`.
- Add a Material inspector for `MaterialComponent`: base colour, metalness,
  roughness, reflectance, emissive colour and strength.
- Route every persistent edit through `CommandService` with Undo/Redo,
  following `SetTransformCommand` and `SetWeatherCommand`. Apply only the
  covered fields and leave everything else on the component untouched.
- Filter no-op edits from history; preview live on drag but commit one command
  on release.
- Extend `RenegadeBridgeTests` with headless Undo/Redo coverage. Component
  edits need no graphics device.
- Require Windows x64 Debug and Release CI plus `RenegadeBridgeTests`.
- Acceptance: from a freshly generated project and without rebuilding, darken
  the terrain to smoked near-black, pull the hologram core back from clipped
  white to readable cyan, and reopen with both intact.

Do not begin scene tabs, formal dirty-state handling, asset import, or the
Identity Handshake.

## Status rules

- A phase completes only with a runnable increment and updated handoff.
- Release gates require verification by a different AI or human.
- Visual and behavioural failures override automated success.
- New Wicked features enter the matrix at phase-boundary syncs; they do not
  silently expand an active phase.
