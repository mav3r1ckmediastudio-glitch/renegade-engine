# Roadmap

## Current milestone

**Phase 3 — Studio foundation**
Target duration: 6–8 weeks
Status: Phase 2 closed after DX12, Vulkan, DPI, input, persistence, and
Runtime separation passed on the project owner's Windows GPU; physical HDR was
not available. The first Phase 3 Project Hub/workspace launched on Windows at
a VSync-limited 75 FPS. Its first viewport-selection, outline, and fly-camera
interaction increment passed on the project owner's GPU, with later outline
thickness refinement noted. The larger editor-usability milestone compiled in
Windows Debug and Release, but `RenegadeBridgeTests` crashed because bridge
commands advanced a rendered scene without a graphics device. A focused
headless-test repair is prepared on top of that milestone.

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
- [ ] Record the Windows build-machine toolchain.

### Week 2

- [ ] Build Windows Debug and Release.
- [ ] Run the original editor and reference scenes.
- [ ] Validate DirectX 12 and check Windows Vulkan.
- [x] Add repeatable scripts, evidence records, and Windows CI.
- [ ] Capture Windows logs, timings, screenshots, and independent verification.

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

- Publish the focused headless-test repair on top of exact milestone
  `c4eb43d`.
- Require Windows x64 Debug and Release CI plus `RenegadeBridgeTests`.
- Confirm `RenegadeBridgeTests` completes rather than crashing in both
  configurations.
- Verify the hierarchy hides generated grid internals and the command bar is
  unobstructed.
- Verify position, rotation, and scale fields remain synchronised with the
  Move, Rotate, and Scale gizmos.
- Verify focus, duplicate, delete, Undo/Redo, Save, and all documented
  shortcuts on DX12.
- Close and reopen the project to confirm saved transform state persists.
- Repeat viewport selection, transforms, focus, duplicate/delete, and history
  on Vulkan.
- Fix failures before beginning scene tabs, formal dirty-state handling, or
  the Identity Handshake.

## Status rules

- A phase completes only with a runnable increment and updated handoff.
- Release gates require verification by a different AI or human.
- Visual and behavioural failures override automated success.
- New Wicked features enter the matrix at phase-boundary syncs; they do not
  silently expand an active phase.
