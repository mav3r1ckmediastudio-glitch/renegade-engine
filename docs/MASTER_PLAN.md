# Custom Game Engine Master Build Plan

**Foundation:** Wicked Engine
**Working title:** Renegade (placeholder)
**Prepared for:** Maverick
**Plan version:** 1.0
**Date:** 29 July 2026
**Planning horizon:** 16–21 months part-time, with a usable editor alpha much earlier

## 1. Executive decision

Yes, we can build this.

The correct interpretation of “our own engine” is:

- Wicked Engine remains the rendering and low-level engine foundation.
- We maintain a traceable fork of Wicked rather than obscuring or rewriting it.
- We build our own editor application, project system, asset workflow, runtime/player, user experience, branding, documentation and release process.
- We expose every relevant Wicked capability through the appropriate surface: creator-facing UI, advanced tooling, Lua, C++ or diagnostics.
- We retain the original Wicked Editor as a reference and parity oracle, but it is not our shipped editor.

This is a substantial product-engineering programme, not a skinning exercise. The attached snapshot contains 38 registered scene-component managers, 46 editor window headers, 24 Lua binding implementation units and 355 shader files. Its root engine C++ headers and sources alone are about 161,000 lines; the existing editor contains roughly 48,000 lines of first-party editor logic once embedded assets and major third-party source files are excluded.

The recommended target is a **Windows-first engine and editor**:

- Primary: Windows x64 with DirectX 12.
- Secondary during development: Vulkan on Windows.
- Later: Linux/Vulkan and macOS/Metal.
- Post-v1 unless specifically prioritised: iOS.
- Out of scope without platform-holder access: Xbox Series and PlayStation builds, because Wicked’s console extensions are private.

## 2. What the repository gives us

The attached repository is a strong foundation rather than only a graphics renderer. It already provides:

- DirectX 12, Vulkan and Metal backends.
- A data-oriented entity-component scene system.
- PBR rendering, HDR, shadows, post-processing, path tracing and multiple real-time GI techniques.
- Jolt-based rigid-body, soft-body, character, vehicle and constraint physics.
- Skeletal and morph animation, humanoids, inverse kinematics, expressions and animation retargeting.
- Terrain, virtual textures, particles, hair/grass, ocean, fluids, splines and Gaussian splats.
- Audio, input, controller feedback, video decoding, UDP networking and pathfinding.
- Lua scripting and a sizeable documented Lua binding layer.
- Native WISCENE serialization plus OBJ, FBX, glTF/GLB, VRM/VRMA and PLY import support.
- A working editor, content browser, profiler, paint tools, graphics settings, multi-scene editing, undo/redo and component inspectors.
- CMake, Visual Studio and platform-specific build paths.

Important repository facts:

- Wicked Engine is MIT licensed. We can modify, distribute and sell our derivative product, but we must preserve the Wicked copyright and MIT licence text in copies or substantial portions.
- The repository includes a large third-party notices file. Shipping builds must include the relevant third-party notices and comply with bundled font and library licences.
- WISCENE uses a versioned archive. The supplied snapshot reports archive version 93, so upstream synchronization and project migration require explicit tests.
- The zip does not contain Git history or a verifiable commit SHA. It is suitable for analysis, but the production project should begin from a proper fork or clone of the official repository with the exact starting commit recorded.

## 3. Product boundary: what “our engine” means

### We own

- Engine name, identity, icons and visual language.
- The custom editor shell, layouts, panels and workflows.
- Project creation and project metadata.
- Asset database, import settings and reimport workflow.
- Editor service layer and command/undo model.
- Runtime/player executable and game export pipeline.
- Templates, starter projects and example games.
- Feature registry, validation, documentation and tutorials.
- Our extensions, higher-level gameplay framework and optional plug-in SDK.
- Release packaging, crash reporting, update policy and support material.

### Wicked remains the foundation

- Graphics devices and renderer.
- Scene ECS and core components.
- Resource manager and native archive implementation.
- Physics integration.
- Audio, input, job system and low-level platform services.
- Existing importers and engine subsystems that we reuse or adapt.
- Shaders and render techniques inherited from the pinned Wicked version.

### We do not do in v1

- Rewrite the renderer, ECS, physics or asset formats merely to make the source look different.
- Promise every supported Wicked platform on day one.
- Put every low-level GPU function into a GUI.
- Change WISCENE serialization without an approved architecture decision and migration test.
- Build a visual scripting system before the basic Lua and C++ workflow is dependable.
- Build a marketplace, multiplayer service, cloud collaboration or console export as part of the initial engine.

## 4. Architectural approach

Use the official Wicked repository as a traceable upstream fork and add our product as separate layers:

```text
Official Wicked upstream
        |
        v
Pinned Wicked core in our fork
        |
        v
Engine Bridge / Editor Services
   |                     |
   v                     v
Custom Studio Editor   Runtime / Player
   |                     |
   +----------+----------+
              v
     Projects, assets and WISCENE
```

Recommended repository structure:

```text
/WickedEngine/              Upstream-owned core; modify only when unavoidable
/Editor/                    Original Wicked Editor; retained as reference
/Studio/                    Our custom editor application
/EngineBridge/              Stable adapters around Wicked APIs
/Runtime/                   Standalone game/player executable
/Tools/                     Import, shader, packaging and validation tools
/Templates/                 Starter projects and example games
/Tests/                     Unit, integration, visual and sample-project tests
/docs/                      Canonical project and handover documentation
/assets/                    Our editor-owned icons, fonts and themes
```

The bridge is essential. UI code must not reach randomly into Wicked internals. It should call stable services such as:

- `ProjectService`
- `SceneService`
- `SelectionService`
- `CommandService`
- `AssetService`
- `ImportService`
- `BuildService`
- `PlaySessionService`
- `SettingsService`
- `DiagnosticsService`

This protects the custom editor from upstream changes and allows a panel to be rewritten without rewriting its behaviour.

### UI toolkit decision

Do not lock the UI toolkit before a short, measured prototype.

The repository offers two realistic routes:

1. **Extend Wicked’s native `wiGUI`:** fastest route to parity and known to coexist with Wicked’s HDR editor output, but docking and modern editor behaviour will require our own work.
2. **Use Dear ImGui Docking:** the repo contains a working docking sample and ImGuizmo integration, but the supplied Windows sample explicitly disables HDR. HDR composition, DPI, input capture and multi-monitor behaviour must pass a prototype gate before it can be selected.

An external C#, Electron or web UI is not recommended for v1 because it adds a large native-interoperability, rendering-surface, packaging and debugging burden.

**Provisional decision:** keep editor services UI-independent. Prefer ImGui Docking if the Phase 2 prototype passes all gates; otherwise build the custom studio on `wiGUI` and add our own docking/layout layer. No production panel work begins until this decision is recorded in an ADR.

## 5. The feature-exposure contract

“All Wicked features exposed” must be precise. Each capability receives one of three exposure tiers:

| Tier | Meaning | Required surface |
|---|---|---|
| T1 Creator-facing | Used routinely to make scenes and games | Full editor UI, undo/redo, persistence, documentation and a functional test |
| T2 Advanced | Specialist rendering, simulation or diagnostic setting | Advanced UI or tool panel, persistence where relevant, documentation and a test/demo |
| T3 Programmer/low-level | Graphics-device, resource or engine API that is not sensibly represented as a control | Stable C++ access, Lua access where Wicked supports it, API documentation and a sample or automated test |

A feature is not “exposed” merely because a header can be included. Each feature-matrix row must record:

- Stable feature ID and subsystem.
- Wicked source/API evidence.
- Pinned upstream version.
- Exposure tier.
- Editor UI status.
- C++ and Lua status.
- Serialization/project status.
- Undo/redo status where relevant.
- Test or demonstration scene.
- Documentation link.
- Supported platforms and known limitations.
- Verification result and verifier.

For creator-facing features, “done” means that a user can create or import the feature, edit it, save, close, reopen, run it in the standalone player and obtain the same result.

## 6. Development and handover operating model

### Canonical sources of truth

The repository, not any AI chat, is the project memory. Maintain:

- `README.md` — build and product entry point.
- `docs/PROJECT_CHARTER.md` — goals, users, scope and non-goals.
- `docs/ARCHITECTURE.md` — component boundaries and data flow.
- `docs/FEATURE_MATRIX.csv` — complete feature-exposure ledger.
- `docs/ROADMAP.md` — current phase and milestone status.
- `docs/TEST_STRATEGY.md` — test layers and supported machines.
- `docs/AI_WORKFLOW.md` — handover and verification rules.
- `docs/UPSTREAM_SYNC.md` — pinned commit, patches and sync history.
- `docs/adr/` — numbered architecture decision records.
- `HANDOFF.md` — current session state only.
- `CHANGELOG.md` — user-visible changes.
- `AGENTS.md` — repository instructions for Codex.
- `CLAUDE.md` — short pointer to the same canonical rules for Claude.

`AGENTS.md` and `CLAUDE.md` must not contain competing project truth. Both point to the same documents and commands.

### Work-unit size

Every task should be small enough to:

- Have one clear outcome.
- Be implemented and locally checked in roughly one to three focused days.
- Change a bounded subsystem.
- Include its tests and documentation in the same change.
- Be reviewed without reconstructing weeks of chat.

Large phases are delivered as two-week sprints. Each sprint ends with a runnable build, evidence and an updated handoff.

### Implementer–verifier separation

Codex, ChatGPT and Claude can all participate, but they should not silently overwrite one another’s context.

1. The implementer works from an exact commit and task acceptance criteria.
2. The implementer records changed files, commands, test results and unresolved risks in `HANDOFF.md`.
3. A different AI or human verifies the exact commit using `docs/VERIFICATION_CHECKLIST.md`.
4. The verifier records PASS, PASS WITH LIMITATIONS or FAIL with evidence.
5. A failed verification becomes a new bounded task. The verifier does not hide fixes inside the review unless explicitly assigned to implement them.
6. The user performs the final visual and behavioural acceptance for editor workflows.

### Evidence rule

A compile is necessary but not sufficient. Meaningful editor or runtime work must be:

- Built in the intended configuration.
- Run against a known fixture or sample project.
- Saved and reloaded if it affects serialized state.
- Exported and run in the standalone player if it affects gameplay.
- Visually inspected where the outcome is visual.
- Compared with the reference Wicked Editor when claiming parity.

## 7. Phased roadmap

The suggested schedule assumes:

- One primary human product owner/tester.
- AI-assisted implementation with external AI verification.
- Approximately 12–18 focused project hours per week.
- Windows x64 is the only release-blocking platform until late beta.
- Work is delivered in small, testable increments.

### Phase 0 — Charter and frozen baseline

**Duration:** 1 week
**Outcome:** A precise product target and a legally/technically traceable starting point.

Deliverables:

- Choose a working engine name and product statement.
- Fork or clone the official Wicked repository.
- Record the exact upstream commit SHA and snapshot date.
- Compare the attachment against the pinned source.
- Record Windows toolchain versions.
- Freeze the v1 platform scope and exposure-tier definitions.
- Create the initial feature matrix.
- Establish licensing and third-party notice policy.

Exit gate:

- The repository has a reproducible identity, a clean `main` branch and an approved project charter.

### Phase 1 — Reproducible build and upstream safety

**Duration:** 2–3 weeks
**Outcome:** Wicked core, original editor, tests and samples build reliably before we alter product behaviour.

Deliverables:

- Automated Windows Debug and Release builds.
- DirectX 12 editor smoke test and Windows Vulkan smoke test.
- Shader compilation/caching documented.
- Known sample scenes open, render and save.
- Baseline performance and screenshot set.
- Upstream remote and synchronization procedure.
- Core-patch ledger and rule that upstream files are changed only when required.
- Initial continuous-integration build.

Exit gate:

- A fresh machine can build and run the pinned baseline from written instructions.

### Phase 2 — Architecture and UI proof

**Duration:** 3–4 weeks
**Outcome:** A custom-branded editor shell proves the riskiest integration choices.

Prototype:

- Main window and branded splash.
- Dockable or equivalent custom workspace.
- Embedded Wicked 3D viewport.
- Scene hierarchy, selection and transform gizmo.
- One inspector editing transform and light properties.
- Save/reopen one WISCENE scene.
- Basic undo/redo command.
- DPI scaling, keyboard/mouse capture and file dialog.
- HDR, SDR and Windows Vulkan checks.

Decision gates:

- Choose ImGui Docking or `wiGUI`.
- Approve the service-layer interfaces.
- Approve project metadata format.
- Prove that editor UI and runtime renderer do not corrupt one another’s render state.

Stop condition:

- If a custom shell cannot render, edit, save and reopen a scene reliably, do not begin broad feature work.

### Phase 3 — Studio foundation

**Duration:** 6–8 weeks
**Outcome:** A dependable daily-use scene editor skeleton.

Deliverables:

- Project hub: create, open and recent projects.
- Scene tabs, new/open/save/save-as and unsaved-change prompts.
- Hierarchy/outliner with search and parent/child operations.
- Selection, multi-selection and focus.
- Translate, rotate and scale gizmos.
- Generic property/inspector framework.
- Command-based undo/redo, duplicate, copy, paste and delete.
- Preferences, layout persistence, theme and hotkey framework.
- Logs, diagnostics and crash-safe autosave/recovery.

Exit gate:

- A tester can create a project, build a small scene, close the editor, reopen it and continue without data loss.

### Phase 4 — Project and asset pipeline

**Duration:** 6–8 weeks
**Outcome:** Assets become repeatable project resources rather than ad-hoc file opens.

Deliverables:

- Project-relative asset database and stable asset IDs.
- Content browser with folders, filters, previews and drag/drop.
- Import and reimport for WISCENE, OBJ, FBX, glTF/GLB, VRM/VRMA and PLY.
- Texture, audio, video, script and font handling.
- Import settings and source-file tracking.
- Thumbnails and dependency/reference reporting.
- Missing-asset and moved-asset recovery.
- Background import jobs and visible error reports.

Exit gate:

- A reference asset pack imports, survives source updates and reopens without broken references.

### Phase 5 — Scene, world and rendering exposure

**Duration:** 8–10 weeks
**Outcome:** Core environment and visual systems are authorable from our editor.

Coverage:

- Name, layer, transform, hierarchy, metadata and object components.
- Meshes, materials, PBR textures, morph targets and custom shader selection.
- Cameras, lights, decals, environment probes and weather.
- Shadows, HDR output, exposure, anti-aliasing and post-processing.
- Reflection, ambient-occlusion and GI modes.
- Path tracing, ray-traced effects and lightmap baking where hardware allows.
- Render debug views, profiler and graphics diagnostics.
- 2D sprites, fonts and scene overlays.

Exit gate:

- A reference lighting scene can be rebuilt in our editor and matches the original Wicked Editor within documented tolerances.

### Phase 6 — Physics, audio and gameplay systems

**Duration:** 8–10 weeks
**Outcome:** A small interactive game can be authored and run.

Coverage:

- Rigid bodies, soft bodies and colliders.
- Character, vehicle and ragdoll workflows.
- Physics constraints, springs, force fields and pick/drag diagnostics.
- 3D audio, submixes and reverb.
- Input actions for keyboard, mouse and controller.
- Controller vibration/LED where supported.
- Voxel-grid navigation and path queries.
- Script components and play/stop/pause behaviour.
- UDP networking as a documented programmer-tier feature.

Exit gate:

- A packaged test project has a controllable character, collisions, audio and a scripted objective in the standalone player.

### Phase 7 — Animation, terrain and advanced simulation

**Duration:** 10–14 weeks
**Outcome:** Wicked’s specialised content systems are authorable and verifiable.

Coverage:

- Skeletal animation, animation data and timelines.
- Armatures, humanoids, retargeting, IK, expressions and morph animation.
- Terrain generation, layers, props, virtual textures and editing tools.
- GPU emitters, hair/grass, force interaction, ocean and fluid effects.
- Splines, video components, Gaussian splats and remaining scene components.
- Paint tooling and component-specific debug visualisation.

Exit gate:

- Every registered scene-component type has a classified exposure tier and either a completed editor path or an explicitly verified programmer path.

### Phase 8 — Scripting, runtime and game export

**Duration:** 10–12 weeks
**Outcome:** The editor produces standalone playable builds rather than only scenes.

Deliverables:

- Stable Lua project layout and script templates.
- Lua autocomplete/type definitions integrated with the project workflow.
- Documented engine-owned script lifecycle.
- Play in editor, pause, stop and deterministic scene reset.
- Separate runtime/player executable without editor code.
- Project settings: window, startup scene, graphics defaults, input and packaging.
- Windows build/package command with asset collection and licence notices.
- Debug and release player profiles.
- Starter first-person, third-person and blank templates.

Exit gate:

- A clean machine can open a packaged build without the editor or source tree.

### Phase 9 — Full exposure audit, performance and later platforms

**Duration:** 8–10 weeks
**Outcome:** “All features exposed” is demonstrated against the pinned v1 baseline.

Deliverables:

- Complete feature-matrix audit.
- Original-editor parity comparison.
- Serialization round-trip suite.
- Scene performance and memory baselines.
- Large-project and asset-stress tests.
- DirectX 12/Vulkan comparison on Windows.
- Linux/Vulkan build investigation.
- macOS/Metal feasibility build if Mac hardware is available.
- Accessibility and high-DPI pass for the editor.
- Documentation coverage report.

Exit gate:

- No feature is unclassified. All T1 and T2 features are complete or deliberately moved from v1 by an approved decision record; every T3 feature has a stable access path and evidence.

### Phase 10 — Beta hardening and v1 release

**Duration:** 8–12 weeks
**Outcome:** A supportable v1 rather than a developer-only build.

Deliverables:

- Installer or portable distribution decision.
- Crash recovery, logs and diagnostic bundle.
- Migration test from at least the previous two beta project versions.
- Clean-machine and clean-user-profile testing.
- GPU/driver compatibility matrix.
- Performance regression thresholds.
- User manual, quick start and tutorial project.
- Licence/about screens and third-party notices.
- Signed release artefacts if code-signing is available.
- v1 tag, release notes and rollback package.

Exit gate:

- Independent verification passes, the vertical-slice project is visually approved and a clean machine can install, create, build and run a project.

## 8. Suggested calendar

| Milestone | Target window | What is genuinely usable |
|---|---:|---|
| Baseline and architecture gate | Weeks 1–8 | Reproducible Wicked build plus custom editor proof |
| Studio editor alpha | Weeks 9–16 | Project, hierarchy, viewport, inspector, undo/redo and scene persistence |
| Asset/workflow alpha | Weeks 17–24 | Repeatable project and import/reimport workflow |
| Visual-authoring beta | Weeks 25–34 | Materials, lighting, weather, post effects and major render controls |
| Playable vertical slice | Weeks 35–44 | Physics, audio, input, scripting and player proof |
| Advanced-system beta | Weeks 45–58 | Animation, terrain, particles and specialist components |
| Standalone build beta | Weeks 59–70 | Templates and repeatable game packaging |
| Feature-complete candidate | Weeks 71–80 | Pinned-baseline exposure audit and performance pass |
| v1 | Weeks 81–92 | Hardened, documented and independently verified release |

This is a planning range, not a promise. At 25–35 focused hours per week, the same scope could plausibly compress to roughly 9–12 months. At under 10 hours per week, plan for 20–26 months.

## 9. Testing strategy

### Build layer

- Windows Debug and Release.
- DirectX 12 and Windows Vulkan.
- Warnings monitored, not ignored by default.
- Shader compilation and cache validation.
- Later platform builds do not block early Windows milestones.

### Automated layer

- Engine-bridge unit tests.
- Command/undo state tests.
- Project metadata and migration tests.
- Asset import/reimport fixtures.
- WISCENE save/load/compare tests.
- Lua API smoke tests.
- Standalone packaging tests.

### Visual and behavioural layer

- Golden sample scenes with captured settings and screenshots.
- Reference comparison against the original Wicked Editor.
- Manual checks for HDR, ray tracing, particles, terrain, animation and post effects.
- Input, DPI and multi-monitor tests.
- Visual failure overrides a nominal automated pass.

### Performance layer

- Editor startup time.
- Scene open/save time.
- Import time and background responsiveness.
- CPU/GPU frame time in fixed sample scenes.
- VRAM and system-memory thresholds.
- Packaged-player size and startup time.

## 10. Upstream synchronization policy

Do not continuously merge upstream while building a phase.

Recommended process:

1. Pin a Wicked commit for the duration of a phase.
2. Fetch upstream at the phase boundary.
3. Create `integration/upstream-YYYY-MM-DD`.
4. Review release notes, archive changes, component additions and binding changes.
5. Merge upstream into the integration branch.
6. Apply the core-patch ledger and resolve conflicts.
7. Run baseline build, serialization, visual and performance suites.
8. Update the feature matrix for new or changed Wicked capabilities.
9. Merge only after independent verification.
10. Record the new pinned SHA and compatibility notes.

Prefer adapter changes over edits inside `/WickedEngine`. When a core edit is unavoidable, isolate it in a dedicated commit and document:

- Why the adapter layer could not solve it.
- Exact upstream files changed.
- Tests that protect the patch.
- Expected merge-conflict area.
- Whether the patch should be proposed upstream.

## 11. Principal risks and controls

| Risk | Why it matters | Control |
|---|---|---|
| “All features” becomes a moving target | Wicked continues to evolve | Define v1 parity against a pinned commit; classify later additions at sync gates |
| Existing editor logic is UI-coupled | Rewriting panels can duplicate behaviour | Extract services and commands before broad panel work |
| ImGui HDR limitation | The supplied sample disables HDR | Mandatory Phase 2 HDR composition gate; retain `wiGUI` fallback |
| WISCENE changes across upstream versions | Existing projects can silently break | Version fixtures, migration policy and round-trip tests |
| Platform scope explodes | Three render APIs and multiple OSes multiply QA | Windows/DX12 first; add platforms only at explicit gates |
| Asset import edge cases | Import success does not guarantee correct animation/materials | Curated fixture library plus visual reimport tests |
| AI context drift | Long sessions can lose decisions and repeat work | Repository-first docs, exact commits, bounded tasks and independent verification |
| Automated success hides visual failure | Render/animation defects can pass numeric tests | Human visual acceptance is a release gate |
| Upstream modifications become unmergeable | Core edits accumulate | Patch ledger, isolated commits and adapter-first rule |
| Licensing notices are omitted | Creates avoidable distribution risk | Automated notice packaging and release checklist |
| Solo-maintainer overload | Support and polish can overwhelm feature work | Early vertical slice, strict v1 scope and two-week review gates |

## 12. First four weeks

### Week 1

- Choose the working title.
- Create the official Git fork.
- Record the exact baseline commit.
- Add charter, roadmap, AI workflow and handoff templates.
- Generate the first feature matrix from `features.txt`, scene components, editor windows and Lua bindings.

### Week 2

- Reproduce Windows Debug and Release builds.
- Build and run the original editor.
- Capture sample scenes, screenshots, timings and logs.
- Prove DirectX 12; check Windows Vulkan.
- Set up the initial build automation.

### Week 3

- Create `/Studio`, `/EngineBridge`, `/Runtime` and `/docs`.
- Build a blank custom application linked to Wicked.
- Embed a 3D viewport and load a known WISCENE.
- Implement a minimal scene and selection service.

### Week 4

- Prototype hierarchy, transform inspector and gizmo.
- Implement one command with undo/redo.
- Save/reopen the modified scene.
- Run DPI, input, HDR and Vulkan checks.
- Write the UI-toolkit ADR and decide the production path.

At the end of Week 4 we should have evidence, not merely scaffolding: our branded executable opens a real Wicked scene, selects and transforms an entity, saves it, reopens it and produces the same result.

## 13. Recommended acceptance decisions now

Approve these defaults unless a product requirement changes them:

1. Windows x64 and DirectX 12 are the v1 release target.
2. The official Git repository—not the zip—is the traceable source baseline.
3. The original `/Editor` remains buildable as a parity reference.
4. Our product lives in separate `Studio`, `EngineBridge` and `Runtime` layers.
5. WISCENE remains the native scene format for v1.
6. Lua remains the first gameplay scripting language.
7. The UI toolkit is chosen only after the Phase 2 HDR/DPI/input proof.
8. Every phase ships a runnable increment and an updated handoff.
9. A different AI or human verifies release-gate work.
10. Visual and behavioural failure overrides automated success.

## 14. Source basis

This plan was grounded in the supplied `WickedEngine-master(1).zip`, including:

- `README.md`
- `features.txt`
- `LICENSE.txt`
- `third_party_software.txt`
- Root, engine and editor `CMakeLists.txt` files
- `WickedEngine/wiScene.h`
- `WickedEngine/ArchiveVersionHistory.txt`
- `Editor/Editor.h` and `Editor/Editor.cpp`
- Editor component-window inventory
- Lua binding inventory
- The 79-page Wicked Editor manual
- The C++ and Lua documentation indexes
- `Samples/Example_ImGui_Docking`
- The repository build workflow

Current public references:

- [Official Wicked Engine repository](https://github.com/turanszkij/WickedEngine)
- [Official Wicked Engine website](https://wickedengine.net/)

## 15. Final recommendation

Proceed, but treat the first eight weeks as a proof programme. We should not spend months rebuilding 46 panels before proving the custom shell, persistence, HDR path and upstream workflow.

If the Week 8 gate passes, the project has a credible route to a distinctive, owned game-authoring product on a mature technical foundation. The first meaningful product target is not “all features”; it is a reliable custom editor that creates a project, imports assets, edits and saves a scene, then runs that scene in our standalone player. From there, the feature matrix turns Wicked’s breadth into controlled, verifiable work.
