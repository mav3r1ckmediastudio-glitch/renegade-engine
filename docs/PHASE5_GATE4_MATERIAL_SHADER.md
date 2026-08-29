# Phase 5 Gate 4 — Material and Shader Exposure

## Status

Implementation active on `phase5/scene-render-gate4-material-shader`, based on post-Gate-3 main `237b0dc270ff4de7f92ca6b0c17519a6651ea11b`.

## Purpose

Expose Wicked Engine's native creator-facing material and shader capabilities through Renegade-owned services and Studio UI without introducing a second material model, unmanaged texture references, stock Wicked Editor windows or sidecar serialization.

Gate 4 extends the existing `MaterialService` and governed `MaterialTextureAssetService`. Native Wicked `MaterialComponent` remains authoritative and WISCENE remains the persistent scene format.

The creator outcome is:

- select a scene object or reusable imported asset and choose which referenced material to edit;
- author useful PBR, blend, UV and native shader properties with immediate viewport feedback;
- assign, replace and remove governed project textures for the supported material slots;
- Undo/Redo persistent material edits;
- save/reopen and package the same native material state without depending on the original external texture path.

## Pinned Wicked capability

Gate 4 targets the Renegade-pinned Wicked commit `ad283cdf10ac4989078c77fc8b02a6d8daec6699`.

The pinned native material shader types are:

- PBR;
- PBR Planar Reflection;
- PBR Parallax Occlusion Mapping;
- PBR Anisotropic;
- Water;
- Cartoon;
- Unlit;
- PBR Cloth;
- PBR Clearcoat;
- PBR Cloth + Clearcoat;
- PBR Terrain Blended;
- Interior Mapping.

The pinned native material blend modes exposed by Wicked's own editor are:

- Opaque;
- Alpha;
- Premultiplied;
- Additive;
- Multiply;
- Inverse.

## Creator-facing scope

### 1. General scene material targeting

Gate 4 adds a reusable material-target discovery seam.

For a directly selected material, that material is the target unless it is terrain-owned.

For a selected object or reusable asset root, Renegade collects the distinct material entities referenced by mesh subsets on that object and its descendant render objects. Multi-material assets remain intact: imported hierarchy and mesh subset/material bindings are not flattened or rewritten.

The Scene Inspector exposes a material selector when more than one editable material is referenced. A one-material object selects that material automatically.

Terrain-owned materials remain owned by the Terrain workspace and are deliberately excluded from the generic material editor.

### 2. Core material state

The existing `MaterialState` / `SetMaterialCommand` is extended rather than replaced.

Required general controls:

- Shader Type;
- Blend Mode;
- Base Colour R/G/B;
- Opacity;
- Metalness;
- Roughness;
- Reflectance;
- Normal Map Strength;
- Alpha Cutoff / Alpha Reference;
- Emissive Colour R/G/B;
- Emissive Strength;
- Receive Shadow;
- Cast Shadow;
- Use Vertex Colours;
- Double Sided.

All persistent changes use the shared Renegade `CommandService` and native Wicked material fields/setters.

### 3. UV transform

Expose the native material texture transform:

- Tiling U;
- Tiling V;
- Offset U;
- Offset V.

These map directly to native `MaterialComponent::texMulAdd` and affect the material texture coordinate transform without creating Renegade-only UV state.

### 4. Shader-specific material controls

Expose the bounded parameters that make the supported native shader modes useful:

- PBR + Parallax Occlusion Mapping: POM Strength;
- PBR + Anisotropic: Anisotropy Strength and Rotation;
- Cloth: Sheen Colour R/G/B and Sheen Roughness;
- Clearcoat: Clearcoat Strength and Clearcoat Roughness;
- Transmission / Water-capable materials: Transmission and Refraction;
- Terrain Blended: Blend With Terrain Height and Mesh Blend;
- Interior Mapping: Scale X/Y/Z, Offset X/Y/Z and Rotation.

Controls may be shown conditionally by selected shader type to keep the Scene Inspector readable.

### 5. Governed texture slots

Gate 4 reuses the existing governed project texture pipeline and extends it to a complete creator-facing scene-material workflow for the five slots already supported by Renegade:

- Base Colour;
- Normal;
- Surface;
- Emissive;
- Occlusion.

For every slot, the creator can:

- select/import a local supported image;
- replace the current texture;
- remove the current binding;
- see enough current-binding information to know which texture is active.

Local source images are retained/registered through Renegade's existing governed texture workflow. Runtime and save/reopen use stable asset IDs and project products, never the original arbitrary absolute source path.

Texture assignment and removal are CommandService-owned and Undo/Redo-able.

### 6. Material texture interpretation

The Surface slot remains Wicked's native packed surface-map input. Gate 4 does not silently invent separate roughness/metalness file slots when the pinned renderer expects a packed surface texture.

The Inspector must label/tooltip this clearly enough that a creator understands the slot is the native packed surface map.

### 7. Persistence and runtime parity

All exposed scalar, colour, boolean, shader, blend and UV state is native `MaterialComponent` state serialized by WISCENE.

Governed texture slot bindings retain their stable asset IDs in existing serializable material metadata and are restored through the established `MaterialTextureAssetService` path.

Gate 4 acceptance requires Editor, save/reopen, Test Level and packaged Runtime parity for a representative material.

## Backend requirements

Before Studio UI is added, Gate 4 must have headless/source-contract coverage for:

- material collection from a directly selected material;
- one-material object resolution;
- multi-material mesh collection;
- reusable/root descendant material collection and de-duplication;
- terrain-owned material exclusion;
- capture/apply/sanitize of every newly exposed material field;
- SetMaterialCommand Undo/Redo for extended state;
- native shader and blend-mode validation;
- governed texture assignment remains compatible with all five supported slots;
- governed texture removal clears the native Wicked slot and stable-ID metadata and is Undo/Redo-able.

## Explicitly deferred

Gate 4 does not expose:

- arbitrary custom shader code editing or compilation;
- `RegisterCustomShader()` authoring UI;
- a node/material graph;
- unmanaged direct filesystem texture bindings;
- stock Wicked `.ini` material preset load/save;
- renderer-internal material flags;
- user/engine stencil controls;
- variable-rate shading controls;
- sampler descriptor overrides;
- texture streaming/debug policy;
- camera-source materials;
- displacement, transmission, sheen, clearcoat, anisotropy or other additional texture-map slots beyond the five already governed by Renegade;
- texture packing/conversion authoring tools.

Those can be exposed later where they have a governed asset model and a clear creator workflow.

## Architecture rules

- Renegade Studio owns the UX; do not embed stock Wicked Editor windows.
- Native Wicked `MaterialComponent` is authoritative.
- Extend `MaterialService`; do not create a second material state model.
- Reuse `MaterialTextureAssetService` and `CreatorTextureWorkflowService` for texture assets.
- Do not flatten reusable imported hierarchy or rewrite mesh subset bindings merely to make material editing easier.
- Terrain-owned materials remain isolated from generic scene material editing.
- Persistent mutations require CommandService, dirty state, Undo/Redo and WISCENE save/open parity.
- Green CI is necessary but owner visual/behavioural validation remains authoritative.

## Owner acceptance

One Release owner pass should prove:

1. select a multi-material imported asset and switch between its materials;
2. change a core PBR value and immediately see the result;
3. switch between representative native shader modes and blend modes;
4. verify Normal Strength / Alpha Cutoff / UV tiling and offset;
5. exercise at least one conditional shader-specific section;
6. assign, replace and remove Base Colour, Normal, Surface, Emissive and Occlusion textures using governed project assets;
7. Undo/Redo representative scalar and texture edits;
8. save/reopen and confirm material/shader/texture state persists;
9. Test Level and packaged Runtime render the same material state without editor-only material UI or external-source dependencies.
