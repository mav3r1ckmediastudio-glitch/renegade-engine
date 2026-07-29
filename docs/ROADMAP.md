# Roadmap

## Current milestone

**Phase 0 — Charter and frozen baseline**
Target duration: 1 week
Status: In progress

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

- Establish repository and Wicked pin.
- Commit charter, roadmap, AI workflow, handoff, and feature matrix.
- Verify licence and notice obligations.
- Record the Windows build-machine toolchain.

### Week 2

- Build Windows Debug and Release.
- Run the original editor and reference scenes.
- Validate DirectX 12 and check Windows Vulkan.
- Capture screenshots, timings, logs, and initial automation.

### Week 3

- Create the first buildable `Studio`, `EngineBridge`, and `Runtime` targets.
- Load a known WISCENE in a blank custom application.
- Implement minimal scene and selection services.

### Week 4

- Prototype hierarchy, transform inspector, and gizmo.
- Implement one undoable command.
- Save and reopen the edited scene.
- Run DPI, input, HDR, and Vulkan checks.
- Record the UI-toolkit ADR.

## Status rules

- A phase completes only with a runnable increment and updated handoff.
- Release gates require verification by a different AI or human.
- Visual and behavioural failures override automated success.
- New Wicked features enter the matrix at phase-boundary syncs; they do not
  silently expand an active phase.
