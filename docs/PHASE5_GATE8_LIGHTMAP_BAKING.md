# Phase 5 Gate 8 — Lightmap and Baking

## Status

Gate 8 begins from merged Gate 7 `main` at `4a89c05475eadb8bc060f50816b3ca7d766cce74` and pinned Wicked Engine `3a800b7134aafe58461093c8abb2e274d4e64033`.

Gate 8 exposes Wicked's existing static-lighting authoring through Renegade Studio. Renegade owns the creator workflow, service boundary, Undo/Redo integration, status and diagnostics. Wicked continues to own the scene components, bake execution and runtime consumption of baked data.

## Architecture

- Do not expose or instantiate Wicked Editor windows.
- Do not create a competing lightmap format, lightmap ECS component or runtime bake model.
- Native baked data remains on Wicked `ObjectComponent` / `MeshComponent` state and travels through normal WISCENE serialization.
- Runtime and packaged builds consume the baked scene data automatically; they do not expose baking controls.
- Atlas generation may reuse Wicked's pinned `xatlas` utility source as an implementation dependency. This is a utility dependency only, not a dependency on the stock Wicked Editor application.
- Gate 8 implementation belongs in a dedicated `LightmapBakeService` plus dedicated Studio Gate 8 source. `StudioApplication.cpp` receives only bounded exact-anchor integration where unavoidable.
- Gate 7 path tracing remains a Studio reference renderer and is not used as the persistence model for baked lightmaps.

## Creator scope

### LIGHTMAP // SELECTED STATIC GEOMETRY

Operate on the selected object or selected object set.

Controls:

- Bake target/status readout.
- Lightmap resolution: powers of two from 32 through 8192. Default 512.
- UV source:
  - Generate Atlas — use pinned Wicked/xatlas atlas generation.
  - Keep Atlas — preserve existing `vertex_atlas` data.
  - Copy UV 0 — copy `vertex_uvset_0` into the lightmap atlas channel.
  - Copy UV 1 — copy `vertex_uvset_1` into the lightmap atlas channel.
- Block Compression (BC6H): enabled by default.
  - Enabled uses Wicked BC6H lightmap storage.
  - Disabled uses Wicked R11G11B10_FLOAT storage.
- START BAKE.
- STOP & SAVE.
- CLEAR LIGHTMAP.
- Preview of the selected object's current baked lightmap when one exists.

Native behaviour:

- Starting a bake clears the previous lightmap for the targets, prepares the chosen atlas source, assigns native lightmap dimensions and requests Wicked lightmap rendering.
- Generate Atlas performs atlas generation once per unique selected mesh, matching Wicked's native multi-selection behaviour.
- Stop & Save clears the native render request and calls the native save path. If the pinned Wicked denoiser is active, its normal finalisation behaviour remains authoritative.
- Clear removes the native baked lightmap from the selected targets.
- Scene acceleration-structure refresh is requested when atlas/render data changes require it.

### VERTEX AO // SELECTED STATIC GEOMETRY

Gate 8 also exposes Wicked's native object-baking Vertex AO workflow because it is part of the pinned object's baking surface.

Controls:

- Ray count: 8–1024, default 256.
- Ray length: 0–1000, default 100.
- COMPUTE VERTEX AO.
- CLEAR VERTEX AO.
- Status readout.

The implementation must use the pinned Wicked scene geometry/BVH semantics and must not invent a second AO asset format.

## Eligibility and fail-closed rules

- Baking targets must resolve to valid Wicked `ObjectComponent` + `MeshComponent` pairs.
- Lightmap baking is intended for static geometry. Unsupported animated/deforming/soft-body targets must be rejected with a creator-readable status rather than partially mutated.
- Copy UV 0 / Copy UV 1 must fail closed when the selected mesh does not contain a complete matching UV set.
- Keep Atlas must fail closed when the selected mesh has no complete atlas channel.
- Generate Atlas must fail closed when xatlas rejects the mesh.
- Invalid targets do not leave a partially started bake.
- Multi-selection uses each unique mesh once for atlas preparation while each object receives its own native lightmap bake request.

## Undo / Redo and dirty state

Creator-visible destructive changes must participate in Renegade `CommandService` history.

- Lightmap clear is Undo/Redo capable.
- Vertex AO compute/clear is Undo/Redo capable.
- Lightmap bake completion is recorded as one authored change: Undo restores the pre-bake baked/atlas state and Redo restores the completed state without requiring the creator to rerun the bake.
- A failed or cancelled preparation must not create a dirty/Undo entry.
- Normal Save / Save As persists completed baked state and resets dirty state through the existing document transaction.

Because Generate Atlas can alter a shared mesh's atlas/render data, command snapshots must cover every affected selected object and each unique affected mesh, not only the first selected entity.

## Studio UX

Gate 8 is added to the existing Renegade RENDER workspace after Gate 7 controls.

- It must preserve the Gate 5 input-lifecycle rule: attach child widgets while the containing window is enabled, then hide the completed workspace.
- Selection/status refresh must not rebuild controls every frame.
- START BAKE is enabled only when the current selection is eligible.
- STOP & SAVE is enabled while one or more selected targets are actively baking.
- CLEAR is enabled when selected targets contain baked data.
- Vertex AO controls reflect whether selected targets contain Vertex AO data.
- Baking must not silently force path-trace preview on or change persisted realtime render settings.

The later overall RENDER UI/UX tidy-up remains separate. Gate 8 does not add the deferred common bottom action strip or LUT-reference-thumbnail system merely to justify another CI cycle.

## Persistence / Runtime parity

Acceptance requires:

1. Completed lightmap survives Save → close/reopen.
2. Completed lightmap is visible in Test Level Runtime.
3. Completed lightmap is visible in packaged Runtime.
4. Vertex AO survives Save → close/reopen and is consumed by normal Wicked material rendering where applicable.
5. No bake-only Studio state leaks into Runtime.

## Automated regression requirements

Gate 8 must add cheap source/service regression coverage that proves:

- native Object/Mesh lightmap state is used;
- xatlas generation is bounded to the dedicated service;
- resolution and UV-source validation fail closed;
- native Start / Stop+Save / Clear seams are wired;
- Vertex AO compute/clear seams are wired;
- multi-selection deduplicates shared meshes during atlas preparation;
- retained Gate 5, Gate 6 and Gate 7 source contracts still pass;
- temporary staging workflows/scripts self-clean before the product commit is promoted.

Compilation and green CI are necessary but are not functional acceptance. Owner validation of a real bake remains required.

## Deferred beyond Gate 8

- Render diagnostics/debug views remain Gate 9.
- Automatic whole-level bake orchestration, distributed/farm baking and background bake queues are not required for Gate 8.
- New custom bake shaders or a second lightmapper are not required.
- The end-of-phase RENDER UI tidy-up remains separate from this gate.
