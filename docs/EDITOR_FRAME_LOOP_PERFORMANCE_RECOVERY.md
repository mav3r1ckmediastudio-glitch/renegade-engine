# Editor Frame-Loop Performance Recovery

## Scope

Branch: `perf/editor-frame-loop-recovery`

Base: unmerged `wd01/wicked-vegetation-clean` at
`b54a22104b965f7b5d463056b76cb2194acd33b7`.

The recovery is based on a static code-path audit of the owner-reported Level
that fell from the 75 Hz VSync cap to approximately 30 FPS. It corrects proven
lifecycle work in the frame loop before any renderer or content-quality tuning.
It does not claim that source inspection alone proves the full 45 FPS recovery.

The vegetation branch still has a separate owner-reported UI interaction and
Hierarchy collapse/expand defect. This performance branch neither fixes nor
hides that blocker, and must not be merged to `main` independently of the
vegetation integration decision.

## Corrections

### Governed textures and PR58 diagnostics

- `CreatorAssetStudioChrome::Update()` no longer calls
  `RestoreMaterialTextureBindings()` every frame.
- Scene open and staged project adoption retain their explicit, deduplicated
  governed-texture rehydration paths.
- PR58 lifecycle timings remain available in Wicked's in-memory backlog around
  real lifecycle work, but no longer create/open/append
  `Saved/Diagnostics/PR58Gate1Lifecycle.log`.

### Debug renderer ownership

- Ordinary `StudioRenderPath::Update()` no longer forces native environment
  probe diagnostics on.
- Phase 5 Gate 9 Diagnostics remains the sole explicit enable/disable/reset
  authority.

### Render settings and LUT lifecycle

- `SceneService::Revision()` increments when the complete active Scene is
  cleared/replaced through new/open/load adoption.
- Studio and Runtime compare the last applied revision during Update and run
  render/LUT synchronization only after Scene replacement.
- Render commands still apply immediately and through Undo/Redo callbacks.
- A missing/invalid LUT therefore cannot repeat filesystem/path/PNG validation
  once per frame for an unchanged Scene.

### WD01 terrain vegetation lifecycle

- Idle frames compare terrain entity and chunk count only.
- `SynchronizeWickedVegetation()` walks chunks only after terrain replacement
  or asynchronous terrain generation/expansion changes the native chunk count.
- The accepted Wicked `HairParticleSystem` paint/delete algorithm and live
  resource lifetime are unchanged.

## Non-goals

- no terrain radius, spacing or resolution reduction;
- no grass density, distance or visual-quality reduction;
- no disabling AO, GI, reflections or creator-visible features;
- no second renderer, vegetation system, resource authority or timer poll;
- no changes to the rejected PR #119 implementation line;
- no attempt to fold the vegetation UI defect into this patch.

## Automated guards

`RenegadeEditorFrameLoopRecoverySourceContract` rejects the former heartbeat
patterns and requires the new Scene/render/vegetation lifecycle gates. Existing
Scene UI Gate 4 and Phase 5 Gate 3 contracts are corrected so they no longer
require the defects this recovery removes. `RenegadeBridgeTests` asserts that a
new complete Scene advances its lifecycle revision.

## Required evidence

CI must pass the existing Windows x64 Debug/Release Studio and baseline jobs.
Green CI is not performance acceptance.

Using the same build, DX12 backend, resolution, VSync setting and camera, record:

1. empty/simple Level;
2. current one-metre terrain without grass;
3. the exact approximately-30-FPS regression Level;
4. the same Level with WD01 grass;
5. the same Level with environment-probe diagnostics explicitly enabled; and
6. a Level with a missing/invalid LUT reference.

For each relevant case record average FPS, 1% low where practical, CPU frame
time and GPU frame time. Idle evidence must also confirm that
`PR58Gate1Lifecycle.log` does not change.

The branch is not accepted until the owner runs the exact regression Level.
Material recovery of the lost frame rate is required; a remaining GPU-bound
cost may be investigated only after these architecture defects are absent.
