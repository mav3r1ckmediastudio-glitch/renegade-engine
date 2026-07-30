# Renegade Engine — Development Handoff

**Handoff date:** 2026-07-30

**Intended recipient:** the project owner, then Claude Code or another coding
agent

**Repository:** `https://github.com/mav3r1ckmediastudio-glitch/renegade-engine.git`

**Canonical branch:** `main`

**Branch awaiting review:** `phase3/editor-visual-polish`

**Branch base:** `main` at `a80e180909ed9e0a62618bc4dee5916e19621d67`

**Active phase:** Phase 3 — Studio foundation

**Current milestone:** Editor Visual Polish — implemented, **not yet built,
not yet visually accepted**

**Previous milestone:** Viewport and Proving Ground Visual Foundation — merged
to `main` as PR #1, functionally accepted on Windows

## Status in one paragraph

Editor Visual Polish is implemented on `phase3/editor-visual-polish` in three
commits. Renegade now draws its own infinite grid from a Renegade-owned shader
instead of Wicked's fixed 20x20 helper, the grid has a persisted visibility
toggle, the oversized gizmo and thick selection outline are fixed, and the
generated hierarchy no longer contains duplicate names. None of it has been
compiled: the authoring machine still has no CMake or MSVC toolchain. **The
grid in particular has never been seen.** A shader bug will not fail CI, which
only builds C++; it will show up on Windows as a viewport with no grid and a
warning in the backlog.

## This milestone: Editor Visual Polish

### Commits on `phase3/editor-visual-polish`

| Commit | Subject |
|---|---|
| `0b7d590` | Replace the stock grid helper with a Renegade shader grid |
| `5669f38` | Persist editor preferences beside the recent-project registry |
| `2f97604` | Make generated entity names unique and cover it with a test |
| (docs) | Record the Phase 3 editor visual polish |

Full detail in `docs/PHASE3_EDITOR_VISUAL_POLISH.md`, including the numbered
Windows acceptance checklist. Summary:

- `Studio/shaders/RenegadeGridVS.hlsl` and `RenegadeGridPS.hlsl` draw an
  infinite, adaptive, analytically anti-aliased ground grid in Renegade's own
  colours, injected by overriding the virtual `RenderTransparents`.
- Occlusion is done by writing `SV_Depth` from the ground intersection, so no
  depth texture is bound and reverse-Z is inherited rather than reimplemented.
- Shaders are standalone with an inline root signature and compile at runtime
  from `Content/shaders/` beside the executable, following `Example_ImGui`.
- Grid visibility toggles from the command bar or `G` and persists through a
  new `ProjectService` editor-preference store.
- Gizmo `tool_scale` 1.0 -> 0.60, thinner arms, reduced opacity. Selection
  outline thickness 2.0 -> 1.0.
- Generated entity names are unique and a test enforces it.

### Ranked risks for this milestone

1. **The grid shader has never run.** Most likely failure is a silent one: no
   grid plus a backlog warning. Check the backlog first. Candidate causes are
   the inline root signature being rejected, the constant buffer binding at
   `b0` not matching, or the HLSL failing to compile under SPIR-V for Vulkan
   even if DX12 is fine.
2. **Matrix convention.** The shader uses `mul(matrix, vector)` with matrices
   stored by `XMStoreFloat4x4` without transpose, matching Wicked's own
   `vertexcolorVS.hlsl`. If the grid appears in the wrong place or not at all,
   this is the first thing to check.
3. **`SV_Depth` and reverse Z.** If the grid draws over geometry it should be
   behind, or vanishes entirely, the depth write is wrong. `DSSTYPE_DEPTHREAD`
   compares with `GREATER`.
4. **Runtime shader compilation cost.** Compilation happens once during
   `Load()`. If startup feels slow, that is why, and precompiling to an
   embedded header at build time is the fix.
5. **Gizmo scale 0.60 is a guess.** It is one tunable in `Load()`.

## Previous milestone: Windows acceptance result

Reported by the project owner against the packaged DX12 Release for the
Viewport and Proving Ground Visual Foundation milestone, now merged:

```text
DX12 GRID PASS / GENERATED SCENE PASS / EDITING REGRESSION PASS /
SAVE REOPEN PASS / PERFORMANCE PASS
```

- CI: all four jobs green (Studio Debug/Release, baseline Debug/Release).
- Hierarchy shows 41 creator-facing items and no grid entities.
- The grid draws on the deck, is not selectable, is not listed, and does not
  reappear in the saved scene.
- Save, close and reopen preserve transforms, hierarchy shape and atmosphere.
  The `LoadScene` rewrite away from `wi::scene::LoadModel` is confirmed good.
- 75 FPS is the display's refresh ceiling and does not move under editor load.
  With VSync disabled the project owner measured **roughly 800 FPS** on the
  RTX 4070 Ti — about 1.25 ms per frame. The added MSAO, volumetric lights and
  realistic sky are not close to a budget. Treat the earlier
  "performance is a risk" framing as retired: there is roughly an order of
  magnitude of headroom in the editor viewport, and future rendering features
  should be judged on quality and authoring cost, not frame time.

  On runtime cost more generally, the project owner's position: Wicked's
  weather and cloud systems ship in released games without complaint, so they
  are production-proven rather than experimental. Renegade should use them as
  they are and not pre-emptively engineer around a cost that upstream users do
  not report. Scalability for lower-end hardware is a real concern but has a
  known answer — see the sky mode note in the environment milestone — so it
  does not need solving in advance.

Still outstanding: the Vulkan launcher pass, and visual comparison against the
approved reference imagery, which is still not in the repository.

## Start here

```bash
git clone --recurse-submodules \
  https://github.com/mav3r1ckmediastudio-glitch/renegade-engine.git
cd renegade-engine
git switch phase3/viewport-visual-foundation
git submodule update --init --recursive
```

Verify before changing anything:

```bash
git status --short
git rev-parse HEAD
git submodule status
```

Expected Wicked Engine pin, unchanged by this milestone:

```text
3a800b7134aafe58461093c8abb2e274d4e64033
```

Read these files in order:

1. `AGENTS.md`
2. `README.md`
3. `docs/PROJECT_CHARTER.md`
4. `docs/ARCHITECTURE.md`
5. `docs/ROADMAP.md`
6. `docs/AI_WORKFLOW.md`
7. `docs/PHASE3_PROJECT_HUB.md`
8. `docs/PHASE3_VIEWPORT_INTERACTION.md`
9. `docs/PHASE3_EDITOR_USABILITY.md`
10. `docs/PHASE3_VIEWPORT_VISUAL_FOUNDATION.md`
11. Relevant rows in `docs/FEATURE_MATRIX.csv`

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

- smoked near-black panels;
- ice-blue/cyan projected edges and interaction states;
- amber reserved for warnings;
- restrained glow with clear typography;
- a colour-accurate 3D viewport; and
- Renegade-owned layout and workflows rather than a reskinned Wicked Editor.

The approved concept imagery is still not stored in this repository. Ask the
project owner for the reference images before making subjective redesign
decisions. Do not invent a different visual direction.

## Completed and accepted baseline

### Phase 2

Closed. Project owner's Windows GPU result:

```text
DX12 PASS / VULKAN PASS / DPI PASS / INPUT PASS / HDR NOT AVAILABLE
```

### Phase 3 Project Hub

Implemented at `30c5d3c`, documented through `b1ce148`. Confirmed on Windows:
project creation, reopen and recents, generated WISCENE load, hierarchy
population, Proving Ground render, stable VSync-limited 75 FPS on an RTX
4070 Ti.

### Viewport interaction

Implemented at `dc32684`, documented through `8fc89ba`. Confirmed on Windows:
click selection, empty-space deselect, shared selection state across viewport,
hierarchy, Inspector, gizmo and outline, right-mouse freelook, WASD/QE
movement, Shift acceleration, wheel speed control, and no interference with
editor panels.

### Editor usability milestone

Implemented in `cb04cf4`, `547906a`, `399b415`, `c4eb43d`, repaired in
`08c8f12` and recorded in `d04e346`. Confirmed on Windows: filtered hierarchy,
viewport-only FPS display, unobstructed command bar, Inspector transform
editing, Move/Rotate/Scale gizmos, W/E/R shortcuts, F focus, Ctrl+D duplicate,
Delete, Ctrl+Z Undo, Ctrl+Y Redo, Ctrl+S Save, Ctrl+Shift+S Save As, and saved
transform persistence.

Known visual defects still deferred to a dedicated editor visual-polish
milestone:

- the transform gizmo is far too large;
- the gizmo styling is temporary and visually poor; and
- the selection outline is too thick.

## This milestone: Viewport and Proving Ground Visual Foundation

### Commits on `phase3/viewport-visual-foundation`

| Commit | Subject |
|---|---|
| `92414f4` | Rebuild the generated Proving Ground from a renderer-independent blueprint |
| `e0c4d3b` | Draw the editor grid with Wicked's renderer-owned grid helper |
| `60aefdc` | Cover generated-scene structure and the headless scene round trip |
| see `git log` | Record the Phase 3 viewport visual foundation |

### Changed files

```text
EngineBridge/include/renegade/bridge/SceneService.h
EngineBridge/src/SceneService.cpp
Studio/src/StudioApplication.cpp
Tests/BridgeCommandTests.cpp
docs/PHASE3_VIEWPORT_VISUAL_FOUNDATION.md   (new)
docs/FEATURE_MATRIX.csv
HANDOFF.md
```

No Wicked Engine source was touched and the submodule pointer is unchanged.

### What changed and why

**Editor grid.** The 22 serialized `__renegade_internal_grid_*` cube and line
entities are gone. Studio calls `wi::renderer::SetToDrawGridHelper(true)`,
which generates temporary GPU line vertices inside `DrawDebugWorld` instead of
scene entities. The grid is therefore editor-only, unpickable, absent from the
hierarchy and never serialized, by construction rather than by filtering. It is
tinted ice-blue via `SetGridHelperColor` and switched off together with the
frame-rate readout whenever the Project Hub is visible. `RenegadeRuntime` never
enables it.

**Generated scene as data.** `ProvingGroundBlueprint()` returns the complete
composition as plain structs; `CreateProvingGround()` instantiates that list
verbatim. This exists for a concrete reason: Wicked's primitive factories call
`MeshComponent::CreateRenderData()`, which dereferences the global graphics
device, so `CreateProvingGround()` can never run in the headless test process.
Splitting description from instantiation is what makes the generated structure
testable at all. This is also the most likely explanation for the earlier
`RenegadeBridgeTests.exe` crash when the test called `CreateProvingGround()`
directly.

**Composition.** Everything is now centred on the world origin so the grid
helper reads as the deck's own measurement grid: generated ground relief mesh,
smoked-composite deck with ice-blue projected edges, centre pedestal and
hologram core, gateway, amber range markers, eight perimeter pylons, a stepped
retaining terrace, an equipment crate cluster, five distant background masses
and an environment probe. The ground relief is a generated treatment with a
deterministic height function — not a terrain-authoring system.

**Atmosphere.** Weather now lives on a real entity named `Environment`. This
fixes a latent defect: `Scene::weather` is only a runtime copy that
`RunWeatherUpdateSystem` overwrites from `weathers[0]` each frame, and it is not
serialized on its own, so the previous fog settings were silently lost on
reload. The scene now carries realistic sky with aerial perspective, distance
fog, height-based low-lying mist, and per-light volumetric scattering on the
sun, hologram core, gateway beam and both range markers. Studio enables MSAO,
volumetric lights, a raised bloom threshold, and fixed exposure with eye
adaption off so viewport brightness does not drift with camera aim.

**Renderer-dependent calls removed from the bridge.** `Scene::Update()` no
longer appears anywhere in `EngineBridge`. `SaveScene()` dropped it because
serialization writes local transforms only and `TransformComponent` recomputes
its world matrix on read. `LoadScene()` now deserializes the archive directly
with `Scene::Serialize` rather than `wi::scene::LoadModel`, which is an import
path that reparents every unparented transform under a temporary root, calls
`Scene::Update()`, then detaches. The new read is the exact inverse of the write
and matches what `LoadModel2` does internally minus the reparenting.

### Commands run

```text
git switch -c phase3/viewport-visual-foundation
git submodule update --init --recursive --depth 1
git diff --check          -> clean
git submodule status      -> 3a800b71... WickedEngine (unchanged)
```

### Commands NOT run

```text
cmake / MSBuild            -> no toolchain on the authoring machine
Tools/Build-Studio-Windows.ps1
RenegadeBridgeTests.exe
GitHub Actions studio.yml
packaged Release smoke test
```

## Resolved risks

Recorded because they were called out before the build and are now settled:

1. **Compilation** — resolved. All four CI jobs green.
2. **`LoadScene` behaviour change** — resolved. Save, close and reopen verified
   on Windows by the project owner.
3. **Terrain winding** — resolved. The ground renders correctly.
4. **Performance** — resolved. 75 FPS is the display's VSync ceiling and does
   not move under editor load.

## Open defects and limitations

The project owner has accepted the current look as a working baseline. The
visual items below are recorded as gaps against the approved direction, not as
blockers, and they are to be closed through editor authoring rather than by
hand-tuning constants. See "Visual tuning is deliberately NOT in this
milestone" further down.

Visual, ranked by distance from the approved direction:

1. **The scene reads as a warm desert, not an industrial holographic
   workstation.** The terrain and distant masses are authored at roughly 3%
   albedo but render mid-tan: the realistic sky's sun and ambient overwhelm the
   materials. Sun pitch, intensity, `skyExposure` and `ambient` were all chosen
   without seeing a frame.
2. **Blown-out emissives.** The hologram core and alignment pedestal clip to
   white through bloom, losing the cyan that is the approved interaction
   colour. Emissive strength 4.0 plus `bloomThreshold` 1.35 is too hot.
3. **Terrain relief is invisible.** Roughly +/-1.5 units over a 180-unit span
   is too gentle to read from editor camera height. The amplitude needs raising
   or the wavelength shortening.
4. **Low-lying mist is not reading.** Aerial haze appears at the horizon but
   nothing sits on the deck. `fogHeightStart`/`fogHeightEnd` need retuning.
5. **The grid is finite and not fully Renegade-coloured.** See the grid finding
   below.
6. **Hard terrain edge** where the 180-unit ground mesh ends against the sky.

Non-visual:

7. **Duplicate hierarchy names.** Eight `Perimeter Pylon`, five
   `Distant Structure`, four `Deck Edge` and four `Equipment Crate` rows are
   unnumbered, so they cannot be told apart in the hierarchy.
8. **The workspace title label clips.** `RENEGADE STUDIO // PROVING GROUND`
   overflows its box in `ResizeLayout`.
9. **Headless test coverage is partial.** The save/reload round trip runs
   against a mesh-free scene only, because `MeshComponent::CreateRenderData()`
   needs a graphics device. Full generated-scene reload remains a packaged
   Release acceptance step, not an automated one.
10. **Legacy projects are not migrated.** Projects generated before this
    milestone still contain the old serialized grid entities in their saved
    WISCENE. They remain hidden from the hierarchy by the
    `__renegade_internal_` filter, but they are still in the file. Only newly
    created projects are clean.

## Grid finding: the stock helper cannot expand

Verified against the pinned Wicked source, not against documentation.

There is **no grid shader**. `PSO_debug[DEBUGRENDERING_GRID]` is built from
`VSTYPE_VERTEXCOLOR` / `PSTYPE_VERTEXCOLOR` with `PrimitiveTopology::LINELIST`,
`DSSTYPE_DEPTHREAD` and `BSTYPE_TRANSPARENT`. There is no infinite-plane
raymarch, no distance fade and no screen-space line width to enable.

The adaptive behaviour *does* exist in `DrawDebugWorld`: it projects the frustum
corners through the inverse view-projection onto the ground plane, selects
`gridStep = xdif > 200.0f ? 10.0f : 1.0f`, and fades minor lines against major
ones. All of it is inside `if (gridHelper2D)`. That same branch also applies:

```cpp
viewProj = XMMatrixRotationX(XM_PIDIV2) * viewProj;
```

which rotates the grid 90 degrees about X for a 2D editor view. Enabling
`SetGridHelper2D(true)` in a 3D perspective viewport would stand the grid up as
a wall. The 3D branch is a hardcoded `gridRes3D = 20` with its minor-alpha lines
commented out.

Renegade also cannot fully own the stock grid's colour. `gridHelperColor` tints
only the non-centre lines; the two centre lines are hardcoded red and blue axis
indicators and `channel_min = 0.2f` is baked in.

Conclusion: the stock helper is the correct choice for *removing serialized grid
entities*, which it did, but it can never be the expanding holographic grid the
approved direction wants. That requires a Renegade-owned grid.

Upstream `master` was fetched during this investigation and resolves to
`3a800b7134aafe58461093c8abb2e274d4e64033` — the exact commit we are pinned to.
The pin is current, not stale, and there is no newer grid implementation to
sync to.

### DeepWiki indexes an older commit — worked example

`https://deepwiki.com/turanszkij/WickedEngine` was consulted during this
investigation and it describes a **different, older** grid implementation:

| | DeepWiki | Pinned `3a800b7` |
|---|---|---|
| Grid block | `wiRenderer.cpp:8353` | `wiRenderer.cpp:8555` |
| Accessors | `wiRenderer.h:1192/1193` | `wiRenderer.h:1223/1224` |
| Vertex buffer | `if (!grid.IsValid())` then `CreateBuffer(&bd, verts, &grid)`, cached once | `device->AllocateGPU(...)`, rebuilt every frame |
| Draw call | `Draw(gridVertexCount, 0, cmd)` | `Draw(count * 2, 0, cmd)` |

`gridVertexCount` and `grid.IsValid()` do not exist anywhere in our
`wiRenderer.cpp`. Both versions agree on the substance — a CPU-built, finite
20x20 line list bound to `PSO_debug[DEBUGRENDERING_GRID]` — but the line numbers
and the buffer strategy differ.

Note the trap in DeepWiki's phrasing: "bind grid pipeline" describes a
`PipelineState` object, which reads like a dedicated grid shader. It is not.
That PSO points at `VSTYPE_VERTEXCOLOR` / `PSTYPE_VERTEXCOLOR`, a six-line
pass-through that transforms a position and interpolates a vertex colour.

This is exactly the hazard the earlier handoff warned about. Verify every
Wicked claim against the pinned submodule before acting on it.

### Approved approach for the Renegade grid

A shader-based infinite grid, owned by Renegade, with no Wicked source or pin
change. Feasibility was checked against the pinned source:

- `RenderPath3D::RenderTransparents` is `virtual`, and it is where
  `DrawDebugWorld` — and therefore the current grid — is already called from.
  Overriding it, calling the base, then appending a Renegade grid pass puts the
  grid in exactly the same slot: depth buffer bound so scene geometry occludes
  it, and still upstream of bloom and tonemapping.
- Shader delivery: `wi::renderer::LoadShader` falls through to
  `wi::shadercompiler` when the shader dump misses, and `dxcompiler.dll` already
  ships beside the executable. Preferred route is to precompile
  `RenegadeGrid.hlsl` to an embedded header at build time and call
  `device->CreateShader` directly, which avoids both the dump-miss error log and
  deploying HLSL next to the Release.
- Technique: full-screen triangle, reconstruct the view ray per pixel, intersect
  the `y = 0` plane, derive line coverage analytically from screen-space
  derivatives for stable anti-aliasing at any distance, fade with distance, and
  reject against the scene depth buffer so it is occluded correctly.

Rejected alternatives, recorded so they are not revisited:

- **Enable `SetGridHelper2D(true)`** — rotates the grid into the vertical plane.
- **Patch Wicked to decouple the adaptive extent from 2D mode** — smallest code
  change, but it forks pinned upstream source and would require an ADR plus a
  core-patch ledger entry. Not approved.
- **Replicate the adaptive line generation on the CPU in Studio** — viable and
  cheaper, but hairline lines with no analytic anti-aliasing. Held as the
  fallback if the shader build plumbing proves troublesome.

## Required verification before this milestone can pass

### Automated

- Build `RenegadeStudio` and `RenegadeBridgeTests` for Windows x64 Debug and
  Release through `.github/workflows/studio.yml`.
- Run `RenegadeBridgeTests` in both configurations.
- Confirm the Wicked pin is exactly `3a800b71` and the submodule is clean.
- Run `git diff --check`.

### Human Windows acceptance

Test the exact packaged Release artifact through both:

```text
Run-RenegadeStudio-DX12.cmd
Run-RenegadeStudio-Vulkan.cmd
```

Follow the numbered checklist in
`docs/PHASE3_VIEWPORT_VISUAL_FOUNDATION.md`. Required report:

```text
DX12 GRID PASS / GENERATED SCENE PASS / ATMOSPHERE PASS /
EDITING REGRESSION PASS / SAVE REOPEN PASS / PERFORMANCE PASS /
VULKAN PASS
```

Visual comparison must use the project owner's approved reference imagery.
Green CI alone cannot pass this milestone.

## Editor Visual Polish scope, as implemented

Implemented on `phase3/editor-visual-polish`. Retained here as the record of
what was agreed and delivered.

**Renegade-owned infinite grid**

1. Add `RenegadeGrid.hlsl` and a build step that precompiles it to an embedded
   header. Do not add it to Wicked's shader dump.
2. Override `StudioRenderPath::RenderTransparents`, call the base, then draw
   the grid: full-screen triangle, per-pixel view-ray reconstruction, `y = 0`
   plane intersection, analytic anti-aliased lines from screen-space
   derivatives, distance fade, depth rejection against the scene depth buffer.
3. Adaptive spacing with major and minor lines, and Renegade-owned colour
   throughout, including the axis lines.
4. Disable `wi::renderer::SetToDrawGridHelper` once the Renegade grid replaces
   it, and keep the grid off on the Project Hub.
5. Add a grid visibility toggle to the command bar, persisted through
   `ProjectService`'s settings file so it survives a restart. Wicked's Editor
   already does exactly this at `Editor/GeneralWindow.cpp:162-170`: create the
   checkbox, apply on click, write to `config["options"]["grid_helper"]`, and
   restore on startup. Copy the pattern, not the widget. This is also the first
   editor preference Renegade persists at all, so it establishes the shape for
   camera speed and layout later.
6. Update `docs/FEATURE_MATRIX.csv` row `REN-REN-004`.

**Deferred defects from the editor usability milestone**

7. Reduce the transform gizmo to a sane screen-space size.
8. Restyle the gizmo to the approved holographic language.
9. Thin the selection outline.

**Presentation cleanup**

10. Number duplicate generated entity names in `ProvingGroundBlueprint()`.
11. Fix the clipping workspace title label in `ResizeLayout`.
12. Replace the Content Browser placeholder text with a real empty state.

### Visual tuning is deliberately NOT in this milestone

The project owner has accepted the current look as a working baseline and
decided that sky, lighting, exposure and material values should become
**editable in the editor** rather than be hand-tuned in C++.

This is the correct call and it supersedes the earlier plan. Tuning a
subjective value by editing a constant, pushing, waiting roughly 30 minutes for
CI and re-testing a packaged Release is the wrong loop. The same change through
an Inspector field is immediate and the creator drives it.

Open visual defects 1, 2, 3, 4 and 6 are therefore **not** fixed by rebalancing
constants. They become the first real test of the environment authoring UI in
the milestone below. Do not silently hand-tune them in the meantime; that would
hide exactly the gap the UI needs to close.

Noted for later, not this milestone. Wicked's Editor has two further patterns
worth taking when the matching Renegade feature is scheduled:

- `Editor/Editor.cpp:1257-1267` saves every debug visualisation flag, force
  hides them for cinema mode, then restores. That is the right shape for a
  Renegade presentation or playtest mode.
- `wiRenderer_BindLua.cpp:167` exposes `SetGridHelperEnabled` to Lua. Relevant
  once Renegade scripting comes online under `REN-SCR-001`.

## Milestone after that: Environment and Material Authoring

The Inspector currently edits position, rotation and scale. This milestone
grows it into a real component inspector so the creator can author the look of
a scene without a rebuild. It is the direct consequence of the decision above.

Suggested branch `phase3/environment-authoring`. This is larger than a normal
unit and may need splitting; scope it properly before starting.

**Inspector: Environment (WeatherComponent)**

Shown when the `Environment` entity is selected. Sun and sky exposure, ambient,
horizon and zenith, `fogStart`, `fogDensity`, height-fog toggle with
`fogHeightStart`/`fogHeightEnd`, realistic-sky and aerial-perspective toggles.
Already serialized, because this milestone moved weather onto a real entity.

Sky mode is a single choice rather than a pile of independent switches:

1. **Realistic sky** — `REALISTIC_SKY`, physically based atmosphere.
2. **Realistic sky + volumetric clouds** — adds `VOLUMETRIC_CLOUDS`.
3. **Skybox texture** — `skyMapName`, with `sky_rotation` for scrolling.

Mode 3 is the scalability answer and it needs no new engine work: `skyMapName`
is already a serialized texture resource on `WeatherComponent`, and
`sky_rotation` is documented upstream as "horizontal rotation for skyMap
texture (in radians)". Animating it gives scrolling cloud cover for a fraction
of the cost. Expose all three modes in the same panel from the start; do not
design a separate low-end path later.

**Inspector: Light (LightComponent)**

Type, colour, intensity, range, inner and outer cone angle, cast-shadow toggle,
volumetrics toggle and `volumetric_boost`.

**Inspector: Material (MaterialComponent)**

Base colour, metalness, roughness, reflectance, emissive colour and strength.

**Volumetric clouds and cloud shadows**

Wicked supports both at the pin, driven entirely from `WeatherComponent`:

- `SetVolumetricClouds()`. `RenderPath3D` allocates the cloud resources
  automatically when the flag is set (`wiRenderPath3D.cpp:477`), so no Studio
  render wiring is needed.
- `SetVolumetricCloudsCastShadow()` — clouds shadow the world, via a dedicated
  `volumetricCloud_shadow` compute shader.
- `SetVolumetricCloudsReceiveShadow()` — clouds receive scene shadows.
- `LightComponent::SetVolumetricCloudsEnabled()` — per-light cloud interaction.
- Two full cloud layers, `layerFirst` and `layerSecond` (archive version 88).

The problem is parameter volume, not cost. `VolumetricCloudParameters` has
roughly 25 top-level fields plus two `VolumetricCloudLayer` structs of about 30
each — 85+ floats including `beerPowderPower`, `anvilDeformationMedium` and
`curlNoiseHeightFraction`. Exposing that raw would be unusable.

Ship a curated set plus presets (Clear, Scattered, Overcast, Storm): coverage
amount and minimum, cloud start height and thickness, type amount, rain amount,
wind speed and angle, albedo, and `phaseG`. Put the full block behind an
Advanced disclosure. The whole struct serializes either way, so nothing is lost.

Unverified: the `volumetricCloudsWeatherMapFirst`/`Second` slots are optional
texture resources and empty names are expected to fall back to procedural noise
driven by `weatherScale`. That fallback path has not been traced end to end.
Confirm it the first time clouds are enabled rather than assuming.

**Viewport post-processing**

Exposure, bloom enable and threshold, AO mode and power. Decide deliberately
where these live: they are Studio viewport state, not scene data, so they
probably belong in project settings rather than the WISCENE. Record the
decision in an ADR if it affects `.renegade`.

**Architecture requirements, non-negotiable**

- Every persistent edit goes through `CommandService` with full Undo/Redo,
  following the existing `SetTransformCommand` pattern. Capture before and
  after state; do not mutate components directly from widgets.
- Filter no-op edits out of the undo history, as `SetTransformCommand` already
  does for microscopic transforms.
- Live preview while dragging a slider is allowed, but restore the before-state
  and commit one command on release — the same discipline the gizmo uses.
- Keep the widgets in Studio and the commands in `EngineBridge`. The Inspector
  must not reach into Wicked components directly.
- Extend `RenegadeBridgeTests` with headless Undo/Redo coverage for the new
  commands. Component edits do not need a graphics device, so unlike scene
  generation these are fully testable.

**Acceptance**

The real test is the open visual defects. From a freshly generated project the
creator should be able to, without rebuilding: darken the terrain to smoked
near-black, pull the hologram core back from clipped white to readable cyan,
raise the low mist so it reads on the deck, and reopen the project with all of
it intact.

Still not started and explicitly out of scope until scheduled: scene tabs,
docking, formal dirty-state tracking, unsaved-change prompts, crash recovery,
asset import, terrain authoring, arbitrary local fog volumes, persisted camera
speed and editor layout, legacy-project grid migration, and the Identity
Handshake.

## Working and delivery model

Each GitHub push and build cycle costs roughly 30 minutes, so the project owner
wants fewer, larger transfers:

- group roughly 4–8 related improvements into one coherent milestone;
- retain separate, reviewable commits for major internal concerns;
- push one development branch;
- open one PR;
- run CI once for the completed batch;
- perform one meaningful Windows test session; and
- collect minor defects for the next relevant batch rather than immediately
  triggering another build.

Do not turn this into an unbounded mega-change. A normal unit should still
produce one testable vertical outcome.

## Final source-of-truth note

At this handoff, GitHub `main` is still
`d04e346b0b9ee4a1f30f8d649ffc705d9bfde212`. The work described above lives only
on `phase3/viewport-visual-foundation` and has not been merged. A leading `-`
from `git submodule status` only means the Wicked submodule has not been
downloaded in that checkout; it does not mean Renegade is out of sync.
