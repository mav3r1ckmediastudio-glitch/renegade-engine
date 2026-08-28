# Phase 5 Gate 1 — Core Scene Components & Object Visibility

Status: implementation candidate on `phase5/scene-render-gate1-core-components`.

Base: post-JP01 `main` at `4f28e84f98437ac2c548bb5e85c35d7d129b2d2c`.

## Phase 5 contract

Phase 5 exposes Wicked scene/world/rendering capabilities through Renegade-owned creator UX. Gate 1 deliberately starts at the scene-component boundary needed by later rendering gates instead of rebuilding capabilities already accepted in Phase 3, Scene UI recovery, LP07/LP08, Build Game or JP01.

Already accepted and not reimplemented here: transform/hierarchy selection, governed reusable model placement, core PBR material scalars, governed base-colour textures, native lights, Environment/Sun/Weather, terrain/ocean and Jolt physics.

## Creator outcome

A selected scene entity can now author the native Wicked components that define its basic identity and render participation:

- creator-facing Name;
- full 32-bit Layer mask with ALL/NONE actions;
- native Metadata preset (`Custom`, `Waypoint`, `Player`, `Enemy`, `NPC`, `Pickup`, `Vehicle`, `Point of interest`);
- Object render participation: Renderable, Cast Shadow, Foreground, Visible In Main Camera, Visible In Reflections and Wetmap.

Every mutation uses Renegade `CommandService` and therefore participates in the same dirty/Undo/Redo history as transforms, lights, terrain and physics.

## Reusable imported assets

JP01 established the stable reusable-asset wrapper as the creator-facing asset root. Gate 1 preserves that rule.

Selecting the top asset root — or an imported child beneath it — resolves Name, Layer and Metadata authoring to the stable wrapper. The imported glTF/FBX hierarchy is not flattened or rewritten.

Object participation is a whole-asset operation: it applies to every descendant Wicked `ObjectComponent`. Undo records and restores each render object's previous individual value, so a mixed imported hierarchy is not destructively homogenized by Undo.

Layer authoring applies to the stable wrapper and every descendant render object. This is necessary because Wicked treats an entity with no `LayerComponent` as `~0`, while hierarchy propagation only constrains children which themselves have a layer. Gate 1 therefore creates missing layer components only on the creator root and actual render-object descendants. Transform-only imported helper nodes remain untouched. Undo removes components that Gate 1 created and restores pre-existing masks exactly.

## Native ownership

Wicked remains authoritative:

- `NameComponent::name` stores creator naming;
- `LayerComponent::layerMask` stores the serialized 32-bit mask;
- `MetadataComponent::preset` stores the native semantic preset;
- `ObjectComponent` owns all six render-participation flags.

Renegade does not create a parallel scene schema, renderer visibility model or metadata serializer.

## Gate 1 exclusions

This gate does not attempt the rest of Phase 5. Specifically deferred:

- arbitrary typed Metadata key/value editing;
- advanced Object controls such as transparency, alpha reference, LOD bias, draw distance, navmesh, rim highlight and cascade mask;
- full material shader/texture-slot parity and custom shaders;
- camera, decal and environment-probe authoring;
- post-processing, HDR, exposure and anti-aliasing authoring;
- AO/GI/reflection/ray-tracing/path-tracing authoring;
- lightmap baking;
- render debug/profiler/graphics diagnostics;
- 2D sprites, fonts and overlays.

Those remain subsequent bounded Phase 5 gates.

## Acceptance boundary

Gate 1 is accepted only when:

1. headless tests prove reusable-root resolution, rename, layer creation/restoration, Metadata preservation and mixed Object-state Undo/Redo;
2. source contract proves the creator Inspector is wired to the command layer rather than mutating Wicked components directly;
3. exact-head Windows baseline Debug/Release passes;
4. exact-head Renegade Studio Debug/Release builds and starts;
5. one Release owner pass confirms a reusable imported asset can be renamed, layered and visibility-authored from its top hierarchy row, with Undo/Redo; and
6. the Gate 1 PR is merged to `main`.
