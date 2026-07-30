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
calls the base, then draws the grid. That is the same call Wicked draws its own
grid helper from, so the main render target and scene depth buffer are already
bound and the grid still receives bloom and tonemapping.

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
5. Grid colour is ice-blue including the two axis lines. No red or white lines.
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
