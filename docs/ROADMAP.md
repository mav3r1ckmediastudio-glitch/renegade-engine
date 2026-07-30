# Roadmap

## Current milestone

**Phase 3 — Studio foundation**
Target duration: 6–8 weeks
Status: Phase 2 closed after DX12, Vulkan, DPI, input, persistence, and
Runtime separation passed on the project owner's Windows GPU; physical HDR was
not available. The first Phase 3 Project Hub/workspace launched on Windows at
a VSync-limited 75 FPS. Its first viewport-selection, outline, and fly-camera
interaction increment passed on the project owner's GPU. The repaired editor
usability milestone then passed packaged DX12 testing, including transforms,
gizmos, duplicate/delete, save, and Ctrl+Z/Ctrl+Y. The Viewport and Proving
Ground Visual Foundation is merged and functionally accepted. Editor Visual
Polish is implemented on `phase3/editor-visual-polish`; its infinite grid,
persisted preference, smaller gizmo, thinner outline, and presentation cleanup
await packaged DX12 and Vulkan visual acceptance.

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

- Correct the custom grid integration so it opens an explicit main colour/depth
  render pass after Wicked's transparent pass and resolves MSAA when enabled.
- Require Windows x64 Debug and Release CI plus `RenegadeBridgeTests`.
- Verify the shader grid on packaged DX12 and Vulkan: horizon coverage, stable
  spacing, depth occlusion, correct axis colours, and no Project Hub overlay.
- Verify grid visibility persists across restart and never reaches WISCENE.
- Recheck gizmo size, selection outline, unique hierarchy names, title layout,
  selection, navigation, Undo/Redo, Save, and reopen.
- Fix visual or behavioural failures before merging the visual-polish branch.
- After acceptance, begin the bounded Environment Authoring milestone. Do not
  begin scene tabs, formal dirty-state handling, or the Identity Handshake.

## Status rules

- A phase completes only with a runnable increment and updated handoff.
- Release gates require verification by a different AI or human.
- Visual and behavioural failures override automated success.
- New Wicked features enter the matrix at phase-boundary syncs; they do not
  silently expand an active phase.
