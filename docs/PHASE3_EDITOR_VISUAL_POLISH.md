# Phase 3 Editor Visual Polish

## Outcome

The editor stops borrowing Wicked's debug presentation and starts owning its
own. Renegade draws its own infinite grid, the transform gizmo and selection
outline stop dominating the viewport, and the generated hierarchy becomes
readable.

## Renegade grid

### Why the stock helper had to go

Verified against the pinned Wicked source at `3a800b7`:

- `PSO_debug[DEBUGRENDERING_GRID]` is built from `VSTYPE_VERTEXCOLOR` /
  `PSTYPE_VERTEXCOLOR`. There is no grid shader; the whole grid is a CPU-built
  line list.
- The adaptive path - frustum projection onto the ground plane, `gridStep`
  selection, major/minor alpha - exists only inside `if (gridHelper2D)`, and
  that branch also applies `XMMatrixRotationX(XM_PIDIV2)`, which stands the
  grid up in the vertical plane. It cannot be used from a 3D viewport.
- The 3D branch is a hardcoded `gridRes3D = 20`.
- `gridHelperColor` tints only the non-centre lines. The two axis lines are
  hardcoded red and blue and `channel_min = 0.2f` is baked in.

Removing the serialized grid entities was the right call in the previous
milestone, and the stock helper was the right interim. It can never be the
expanding holographic grid the approved direction wants.

### How the Renegade grid works

`Studio/shaders/RenegadeGridVS.hlsl` and `RenegadeGridPS.hlsl` are deliberately
standalone - no `globals.hlsli`, no `ShaderInterop`, no input layout - with an
inline `[RootSignature]` and a single constant buffer at `b0`. Wicked's own
`Example_ImGui` sample uses the same pattern, which is what makes it possible
for an application to own a shader without deploying Wicked's shader source
tree.

- The vertex stage emits one full-screen triangle from `SV_VertexID`. No vertex
  buffer is bound; the draw is `Draw(3, 0, cmd)`.
- The pixel stage unprojects the pixel into a world-space ray, intersects the
  `y = 0` plane, and derives line coverage from `ddx`/`ddy` of the world
  position. Lines stay roughly one pixel wide at any distance or grazing angle,
  which is the whole reason this is a shader rather than a line list.
- Spacing adapts in powers of ten and cross-fades between decades, so pulling
  the camera back does not pop.
- Minor lines, major lines and both axis lines are Renegade-coloured.

### Depth without a depth texture

The pixel shader projects the ground intersection with the scene's own
view-projection matrix and writes `clip.z / clip.w` to `SV_Depth`. The hardware
depth test then occludes the grid behind scene geometry.

This matters twice over: nothing has to bind or sample the depth buffer, and
Wicked's reverse-Z convention (`XMMatrixPerspectiveFovLH(fov, aspect, zFarP,
zNearP)` with `ComparisonFunc::GREATER`) is inherited rather than
reimplemented and got wrong.

### Where it is drawn

`StudioRenderPath` overrides the virtual `RenderPath3D::RenderTransparents`,
calls the base, then opens a separate pass that loads Wicked's main colour and
depth attachments. With MSAA enabled, the pass also resolves into `rtMain`.
The main viewport and scissor are rebound before the draw. Wicked ends its own
passes before `RenderTransparents` returns, so this explicit ownership is
required for both DX12 and Vulkan. The grid is still written before
post-processing and therefore receives bloom and tonemapping.

### Shader delivery

Renegade shaders are not in Wicked's shader dump, so they compile at runtime
from source. `Studio/CMakeLists.txt` copies them to `Content/shaders/` beside
the executable, which `Tools/Build-Studio-Windows.ps1` already packages
wholesale. `LoadGridResources` temporarily points
`wi::renderer::SetShaderSourcePath` at that directory and restores it
immediately, exactly as `Example_ImGui` does, so Wicked's own shader lookup is
unaffected.

A failure to load the shaders or create the pipeline logs a warning and skips
the grid. A missing grid is a visual downgrade, not a reason to take the editor
down, and every draw path checks the pipeline first.

## Grid visibility preference

The command bar gains a grid toggle, also bound to `G`. The setting persists
through `ProjectService::SetEditorPreference`, written to an `editor` section
of the same `wi::config` state file that already holds recent projects.

This is the first editor preference Renegade persists. Camera speed and editor
layout are both still session-only and should follow this route rather than
inventing a second one. Preferences describe how the creator likes Studio to
behave, so they never reach a WISCENE or the `.renegade` descriptor.

## Deferred defects cleared

- **Gizmo size.** `Translator` sizes itself as
  `distance-to-camera * 0.05 * tool_scale`, so `tool_scale` is a direct
  screen-space multiplier. The default of 1.0 dominated the viewport;
  Renegade now sets 0.60.
- **Gizmo styling.** Thinner arms, slightly reduced opacity, and harder
  darkening of negative axes so it reads as a projected instrument.
- **Selection outline.** Thickness was 2.0, double Wicked's default, which read
  as a heavy halo. Now 1.0.

## Presentation cleanup

- Generated entity names are unique. Deck edges and range markers are named by
  compass direction; pylons, terraces, crates and distant structures are
  numbered. `RenegadeBridgeTests` fails on any duplicate.
- The workspace title no longer clips: font size 16 with fit-text enabled, which
  also keeps long project names inside the label.
- The Content Browser placeholder is a real empty state rather than a note
  about a future phase.

## Windows acceptance

Test the exact packaged Release artifact through both
`Run-RenegadeStudio-DX12.cmd` and `Run-RenegadeStudio-Vulkan.cmd`.

1. The grid renders and extends to the horizon rather than stopping at a
   20-unit square.
2. Lines stay crisp when the camera is low and looking along the ground. No
   crawling or moiré in the distance.
3. Flying up and back changes the grid spacing smoothly, with no visible pop.
4. Scene geometry occludes the grid correctly. The grid does not draw over the
   deck, the pedestal or the distant structures.
5. Grid minor/major lines and the Z axis are ice-blue; the X axis is the
   deliberate amber orientation accent. No stock red or white lines appear.
6. The grid toggle in the command bar and the `G` key both work. The label
   tracks the state.
7. Close Studio with the grid off, reopen, and confirm it is still off. Turn it
   back on and confirm that persists too.
8. The grid never appears over the Project Hub.
9. Nothing about the grid appears in the hierarchy or in a saved scene.
10. The transform gizmo is a usable size and all three modes still drag
    correctly.
11. The selection outline is a thin edge, not a halo, and is still
    save-isolated.
12. The hierarchy shows uniquely named entities.
13. The workspace title is not clipped, including after opening a project with
    a long name.
14. Selection, navigation, Inspector editing, Undo/Redo, Save and reopen are
    all unaffected.
15. Frame rate with VSync disabled shows no meaningful regression from the
    previously measured ~800 FPS.

Required report:

```text
DX12 GRID PASS / GRID PERSISTENCE PASS / GIZMO PASS / OUTLINE PASS /
HIERARCHY PASS / REGRESSION PASS / PERFORMANCE PASS / VULKAN PASS
```

If the shaders fail to compile the viewport will simply have no grid and the
backlog will carry a warning. That is a shader bug to fix, not a crash.

## Acceptance record

Accepted by the project owner against `main` at
`8787a4cb0d3287057fe2f61833084ad653b99ff6`, Wicked pinned at `3a800b71`.

```text
DX12 GRID PASS / GRID PERSISTENCE PASS / GIZMO PASS / OUTLINE PASS /
HIERARCHY PASS / REGRESSION PASS / PERFORMANCE PASS / VULKAN PASS
```

The grid extends to the horizon with stable adaptive spacing, is correctly
occluded by the deck and scene geometry, is ice-blue throughout including the
axis lines, toggles from the command bar and `G`, persists across restart, and
never appears over the Project Hub or in a saved scene.

### Repair before acceptance

The first implementation of this milestone shipped a grid that never appeared,
and the cause is worth recording because the reasoning failure is repeatable.

`DrawEditorGrid` was called from an override of
`RenderPath3D::RenderTransparents` on the assumption that the base call leaves
the main colour and depth attachments bound. It does not. **Wicked ends every
render pass before `RenderTransparents()` returns**, so the draw was issued
with no render pass open and rendered nowhere. The pipeline was created
successfully and no warning was logged, which made the failure look like a
depth or visibility problem rather than a missing render pass.

The repair, in `eeb4c3b`, opens an explicit pass over `rtMain_render` and
`depthBuffer_Main`, adds an MSAA resolve to `rtMain` when the sample count is
greater than one, binds the viewport and the internal-resolution scissor, draws
the grid, and ends the pass.

Two lessons:

1. An assumption about Wicked's frame structure is not a fact until it is read
   in the pinned source. This one had even been written into a code comment as
   though it were established.
2. A successfully created pipeline proves the shader compiled and the state is
   valid. It proves nothing about whether the draw reaches a render target.

The 2 cm grid plane offset added in `90531fb` was a separate and still-correct
change: the generated deck's top surface sits at exactly `y = 0`, so a grid
drawn at `y = 0` would be coplanar with it and lose the `GREATER` depth test
under reverse Z. It was not, however, the reason the grid was invisible.
