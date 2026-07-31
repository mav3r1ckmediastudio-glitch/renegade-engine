# Renegade Engine — Development Handoff

**Handoff date:** 2026-07-30

**Intended recipient:** Claude Code, Codex, or another coding agent

**Repository:** `https://github.com/mav3r1ckmediastudio-glitch/renegade-engine.git`

**Canonical branch:** `main`

**Verified starting commit:** `944f8991a0fbc25885f27f4ae9f278708e95e334`

**Wicked Engine pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

**Active phase:** Phase 3 — Studio foundation

**Active bounded task:** Renegade-owned Studio chrome functional slice

## Status

`main` is clean, merged, and accepted. There is no branch awaiting review and
no outstanding repair.

Two milestones landed since the last handoff and both passed packaged Windows
acceptance on DX12 and Vulkan:

- **Editor Visual Polish** — Renegade's own infinite shader grid, a persisted
  grid visibility preference, a correctly sized transform gizmo, a thinner
  selection outline, and unique generated entity names.
- **Environment Authoring** — a curated Environment inspector that authors
  Wicked's atmosphere, fog, volumetric clouds, and cloud shadows through
  undoable commands.

The creator can now change the look of a scene inside the editor rather than
by rebuilding. That was the goal set two milestones ago and it is met.

## Accepted: Renegade-owned Studio chrome visual proof

The product owner accepted the packaged DX12 visual proof from PR #7 at
`bf41c75`. The Renegade-owned canvas architecture is now the approved Studio
baseline. Do not reinterpret the layout or return to stock Wicked widget
rendering.

## Pending: first functional chrome slice

The next commit reconnects the real hierarchy, tool selection, Transform and
Environment Inspector controls, and a collapsed-by-default bottom drawer.
Renegade subclasses retain native input behaviour while overriding the render
path for every visible control pixel. See
`docs/PHASE3_STUDIO_CHROME_FUNCTIONAL_SLICE.md`.

This slice is not accepted until Windows CI is green and the product owner has
run the packaged DX12 artifact through the hierarchy, transform, weather,
drawer, Undo/Redo and save/reopen checklist. Vulkan follows DX12 acceptance.

## Superseded visual-proof notes

The previous full-shell attempts were rejected by the product owner because
they rearranged and recoloured Wicked's stock widgets instead of reproducing
the approved Renegade Studio concept. PR #6 is rejected, must be closed without
merge, and is not an implementation baseline.

The replacement starts from clean `main` at `944f899`. It adds
`RenegadeStudioChrome`, a Renegade-owned `Widget::Render` implementation
that uses Wicked only for canvas scheduling. The proof hides every stock
workspace panel and renders only the approved top application bar, scene tab,
Hierarchy edge, viewport chrome, collapsed bottom tabs, and status bar.

That bounded proof has now served its purpose and is accepted. Its deliberately
presentation-only elements are being replaced incrementally by the functional
slice above. See `docs/PHASE3_STUDIO_CHROME_VISUAL_PROOF.md` for the original
gate and `docs/PHASE3_STUDIO_CHROME_FUNCTIONAL_SLICE.md` for current scope.

## Start here

```bash
git clone --recurse-submodules \
  https://github.com/mav3r1ckmediastudio-glitch/renegade-engine.git
cd renegade-engine
git checkout main
git submodule update --init --recursive
```

Verify before changing anything:

```bash
git status --short
git rev-parse HEAD          # 944f8991a0fbc25885f27f4ae9f278708e95e334
git submodule status        # 3a800b71... WickedEngine
```

Read these files in order:

1. `AGENTS.md`
2. `README.md`
3. `docs/PROJECT_CHARTER.md`
4. `docs/ARCHITECTURE.md`
5. `docs/ROADMAP.md`
6. `docs/AI_WORKFLOW.md`
7. `docs/PHASE3_ENVIRONMENT_AUTHORING.md`
8. `docs/PHASE3_EDITOR_VISUAL_POLISH.md`
9. Relevant rows in `docs/FEATURE_MATRIX.csv`

`CLAUDE.md` deliberately points back to these canonical repository documents.
Do not create a second, competing project plan in a tool-specific instruction
file.

## Non-negotiable project rules

- Wicked Engine is a pinned upstream submodule. Do not edit its source or move
  its submodule pointer unless the project owner explicitly approves a
  documented upstream-sync or core-patch task.
- Prefer implementation in `Studio`, `EngineBridge`, `Runtime`, `Tools`, and
  `Tests`.
- Studio owns presentation and input routing. Persistent scene mutations belong
  in UI-independent `EngineBridge` commands and must support Undo/Redo.
- Bridge code must not call renderer-dependent `Scene::Update()`. The
  render-capable Studio frame loop owns scene advancement.
- Keep `.renegade` project metadata above Wicked's native WISCENE format. Do not
  alter WISCENE semantics without an ADR and migration tests.
- Windows x64/DX12 is the initial target. Vulkan on Windows is the required
  development cross-check. Do not silently broaden platform scope.
- Automated compilation is not visual acceptance. GitHub Actions plus a
  human-tested downloadable Studio Release are required before claiming a
  milestone has passed.
- A visible or behavioural failure overrides green CI.
- Never commit credentials, tokens, personal data, machine-specific absolute
  paths, build output, or hidden reasoning.

## Product and visual direction

Renegade is a Windows-first game engine and authoring environment built on
Wicked Engine. Wicked supplies rendering and low-level engine systems;
Renegade owns its editor, project system, asset workflow, runtime, terminology,
visual identity, documentation, and release process.

The approved editor direction is an industrial holographic workstation:
smoked near-black panels, ice-blue/cyan projected edges and interaction states,
amber reserved for warnings, restrained glow with clear typography, a
colour-accurate 3D viewport, and Renegade-owned layout and workflows rather
than a reskinned Wicked Editor.

The approved concept imagery is still not stored in this repository. Ask the
project owner for the reference images before making subjective redesign
decisions.

## Accepted baseline

### Phase 2

Closed. `DX12 PASS / VULKAN PASS / DPI PASS / INPUT PASS / HDR NOT AVAILABLE`.

### Phase 3 Project Hub — `30c5d3c`, documented through `b1ce148`

Project creation, reopen and recents, generated WISCENE load, hierarchy
population, Proving Ground render.

### Phase 3 Viewport interaction — `dc32684`, documented through `8fc89ba`

Click selection, empty-space deselect, shared selection state across viewport,
hierarchy, Inspector, gizmo and outline, right-mouse freelook, WASD/QE
movement, Shift acceleration, wheel speed control.

### Phase 3 Editor usability — `cb04cf4`, `547906a`, `399b415`, `c4eb43d`, repaired in `08c8f12`, recorded in `d04e346`

Filtered hierarchy, viewport-only FPS display, Inspector transform editing,
Move/Rotate/Scale gizmos, W/E/R shortcuts, F focus, Ctrl+D duplicate, Delete,
Ctrl+Z Undo, Ctrl+Y Redo, Ctrl+S Save, Ctrl+Shift+S Save As, saved transform
persistence.

### Phase 3 Viewport and Proving Ground Visual Foundation — merged as PR #1

Serialized `__renegade_internal_grid_*` entities removed. Proving Ground
rebuilt from a renderer-independent blueprint. Atmosphere moved onto a
serialized weather entity. `Scene::Update()` removed from every `EngineBridge`
path and `LoadScene` switched from `wi::scene::LoadModel` to a direct
`Scene::Serialize` read.

Windows result: `DX12 GRID PASS / GENERATED SCENE PASS / EDITING REGRESSION
PASS / SAVE REOPEN PASS / PERFORMANCE PASS`.

### Phase 3 Editor Visual Polish — merged as PR #2, repaired by PR #3

Renegade's own infinite grid shader, persisted grid visibility, gizmo
`tool_scale` 1.0 → 0.60, selection outline thickness 2.0 → 1.0, unique
generated entity names, non-clipping workspace title, real Content Browser
empty state.

### Phase 3 Environment Authoring — merged as PR #4

Curated Environment inspector: Clear/Scattered/Overcast/Storm presets, three
sky modes, aerial perspective, sky exposure, ambient, distance and height fog,
volumetric cloud coverage/altitude/thickness, and cloud shadows. Every field
edit and preset is one `SetWeatherCommand` with full Undo/Redo.

Windows result on `8787a4c`, both launchers:
`DX12 ENVIRONMENT PASS / CLOUDS PASS / CLOUD SHADOWS PASS / UNDO-REDO PASS /
SAVE-REOPEN PASS / TRANSFORM REGRESSION PASS / VULKAN ENVIRONMENT PASS`,
and `DX12 GRID PASS / GRID PERSISTENCE PASS / GIZMO PASS / OUTLINE PASS /
HIERARCHY PASS / REGRESSION PASS / PERFORMANCE PASS / VULKAN PASS`.

## Lessons worth keeping

Two mistakes in this phase cost a build cycle each. Both were the same class of
error: an assumption about Wicked recorded as if it were a verified fact.

**The render pass is not left open.** The first grid implementation drew from
inside an override of `RenderPath3D::RenderTransparents` on the assumption that
the base call leaves the main colour and depth attachments bound. It does not —
Wicked ends every render pass before that function returns, so the draw went
nowhere. The grid now opens its own explicit pass over `rtMain_render` and
`depthBuffer_Main`, resolving MSAA when enabled. Fixed in `eeb4c3b`.

**DeepWiki indexes an older commit than the pin.** It described a different,
older grid implementation with a cached vertex buffer and a `gridVertexCount`
symbol that does not exist in our tree. Upstream `master` was fetched during
that investigation and resolves to `3a800b71` — the exact commit we pin — so
the pin is current and there is nothing to sync to. Verify every Wicked claim
against the pinned submodule before acting on it.

## Closed questions

- **Do volumetric clouds need a weather map texture?** No. There is no
  reference to `volumetricCloudsWeatherMap` anywhere in `EngineBridge` or
  `Studio`; clouds render procedurally from `weatherScale`. This was previously
  recorded as unverified.
- **Is there enough performance headroom?** Yes, by a wide margin. With VSync
  disabled the project owner measured roughly 800 FPS on an RTX 4070 Ti before
  clouds, and the 75 FPS figure seen in screenshots is the display's refresh
  ceiling rather than a budget. Rendering features should be judged on quality
  and authoring cost, not frame time. This is an editor viewport on high-end
  hardware and says nothing about `RenegadeRuntime` on a mid-range GPU.
- **Can Renegade own a shader without forking Wicked?** Yes. Standalone HLSL
  with an inline `[RootSignature]` and its own constant buffer, compiled at
  runtime from `Content/shaders/` beside the executable, following the
  `Example_ImGui` pattern in the pinned tree.

## Known limitations

- Environment inspector fields are identified by tooltip on hover rather than
  an inline label. Deliberate for now; it will scale poorly as the inspector
  grows to lights and materials.
- Sun and light-component authoring, material authoring, and skybox asset
  selection are not built. The generated scene's warm cast and clipped
  hologram-core emissive therefore still cannot be corrected by the creator.
- The second cloud layer, cloud weather maps, rain, wind, ocean, and advanced
  scattering parameters are untouched by `WeatherState` and have no UI.
- `SetVolumetricCloudsReceiveShadow` and the per-light
  `SetVolumetricCloudsEnabled` are never set.
- Viewport post-processing (exposure, bloom, AO) is not authorable, and where
  it should be persisted has not been decided.
- Projects generated before the Visual Foundation milestone still contain the
  old serialized grid entities in their saved WISCENE. They stay hidden by the
  `__renegade_internal_` filter but are still in the file. Only newly created
  projects are clean.
- Headless tests cannot cover mesh reload, because
  `MeshComponent::CreateRenderData()` requires a graphics device.
- Scene tabs, docking, formal dirty-state tracking, unsaved-change prompts,
  crash recovery, asset import, terrain authoring, arbitrary local fog volumes,
  persisted camera speed and editor layout, and the Identity Handshake are all
  not started.

## Next bounded milestone: Light and Material Authoring

The Environment inspector proved the pattern. Extend it to the two remaining
component types that block the creator from fixing the generated scene's look.

Suggested branch `phase3/light-material-authoring`.

**Inspector: Light (LightComponent)**

Shown when a light entity is selected. Type, colour, intensity, range, inner
and outer cone angle, cast-shadow toggle, volumetrics toggle and
`volumetric_boost`.

**Inspector: Material (MaterialComponent)**

Shown when an object with a material is selected, alongside Transform. Base
colour, metalness, roughness, reflectance, emissive colour and strength.

**Architecture requirements, non-negotiable**

- Every persistent edit goes through `CommandService` with full Undo/Redo,
  following `SetTransformCommand` and `SetWeatherCommand`. Capture before and
  after state; do not mutate components directly from widgets.
- Filter no-op edits out of the undo history.
- Live preview while dragging is allowed, but restore the before-state and
  commit one command on release — the discipline the gizmo already uses.
- Apply only the fields the state struct covers, leaving every other value on
  the component untouched, exactly as `ApplyWeather` does.
- Keep widgets in Studio and commands in `EngineBridge`.
- Extend `RenegadeBridgeTests` with headless Undo/Redo coverage. Component
  edits need no graphics device, so these are fully testable.

**Acceptance**

From a freshly generated project, without rebuilding, the creator should be
able to darken the terrain to smoked near-black, pull the hologram core back
from clipped white to readable cyan, and reopen the project with both intact.

## Working and delivery model

Each GitHub push and build cycle costs roughly 30 minutes:

- group roughly 4–8 related improvements into one coherent milestone;
- retain separate, reviewable commits for major internal concerns;
- push one development branch;
- open one PR;
- run CI once for the completed batch;
- perform one meaningful Windows test session; and
- collect minor defects for the next relevant batch.

Do not turn this into an unbounded mega-change. A normal unit should still
produce one testable vertical outcome.

## Workspace stabilization accepted

The owned chrome follow-up adds the official wordmark asset, square Inspector
host styling, bold high-contrast workspace text, click-through isolation,
functional File/Edit/View menus, honest unavailable states and a drawer that
closes by chevron, active-tab click, Escape or outside click. Drawer open state,
last tab and grid visibility persist through `ProjectService` editor
preferences. See `docs/PHASE3_WORKSPACE_STABILIZATION.md` for the full Windows
acceptance pass. Do not mark this passed from CI alone.

Packaged review subsequently proved the Inspector was transparent and the
owned shell had removed panel resizing. The corrective commit makes the
Inspector opaque, adds persistent Hierarchy/Inspector/drawer splitters, and
introduces owned Environment slider-plus-number controls with live preview and
one Undo command per drag. Its authoritative regression list is
`docs/PHASE3_WORKSPACE_STABILIZATION_CORRECTION.md`. These defects must pass on
packaged DX12 and Vulkan before PR #7 leaves draft. The product owner completed
that pass successfully and PR #7 merged to `main` at `beefe97`.

## Environment workspace and precipitation follow-up

The next bounded slice moves Environment out of Scene Hierarchy and into a
dedicated top-right `SCENE / ENVIRONMENT` workspace switch. Studio resolves the
primary serialized Weather entity directly without discarding the creator's
current object selection.

Rain uses Wicked's native GPU precipitation path. Snow is a functional slow,
large, unstretched flake profile over that emitter with native rain splashes
disabled. Both modes expose intensity, fall speed, particle size, wind
direction, wind strength and turbulence through `SetPrecipitationCommand` with
live preview and one Undo step per drag. WISCENE stores the underlying native
weather values. Snow accumulation, footprints, temperature and snow material
coverage remain explicitly deferred rather than represented by dummy controls.
