# Environment probe authoring repair

New probes previously had no marker in the GUI overlay, while native captured-sphere diagnostics defaulted off. Adding a probe after the native scene update could also crash RefreshEnvProbes: the component count increased after the parallel probe-bounds array had been prepared.

## Result

- Queue creation at EVENT_THREAD_SAFE_POINT before Scene::Update, guarded by scene revision, session, scene-loading and Test Game state.
- Enable native captured spheres as the editor diagnostic default.
- Enumerate probes in the existing GUI marker overlay with procedural fallback, hierarchy visibility and selection.
- New Add > Environment Probe entities enable native real-time capture with update interval zero. Existing saved static/live policies are preserved; the Inspector checkbox still allows static capture.
- Native shaders, capture implementation and serialization remain unchanged.

## Validation

Baseline main: 11bbb90e4214776d5e51c8bc4cba2e7f1ccde296. Probe-only code isolated from the local combined importer/terrain candidate. Unrelated importer, Player Camera Preview, assistant handoff and river changes are excluded.

VS18 x64 Release Studio build passed in the combined candidate, including the exact probe creation action/function and overlay files isolated here. Native probe command tests and existing ownership source contract passed 2/2. Exact isolated branch source contract is run again before publication. Clean-branch Windows CI remains required.

The original crash dump reported C0000005 at module offset 0x8ca731, mapped to RefreshEnvProbes+0x1b1. Following the frame-boundary correction, ten consecutive native Add operations, Undo/Redo and save/reopen passed in DX12. Native sphere visible at 74-75 FPS in the small fixture.

The first sphere-only result did not satisfy the owner: static captures retained the original sky after clouds changed. A fresh native probe captured the actual clouds; enabling the native real-time checkbox updated capture. The owner subsequently explicitly confirmed the live reflection behaviour with 'visually confirmed'.

Owner visual acceptance covers the observed behaviour, not full release certification. Large-scene performance, packaged Runtime/export and independent exact-commit verification remain open. Source diagnostics and detailed local evidence remain in the development worktree's ignored BUILD directory.

## Integration

Owner authorised merging this bounded probe repair into main on 23 October 2026. Merge through a dedicated PR after repository checks pass. Preserve unrelated dirty work and owner projects.
