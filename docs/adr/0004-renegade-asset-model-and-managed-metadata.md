# ADR 0004: Renegade Asset Model and Managed Metadata

**Status:** Accepted
**Date:** 2026-08-11

## Context

Renegade currently uses Wicked's WISCENE serialization for authored scenes and
for the reusable model product produced by the import proof. That is technically
valid because an imported model can contain a hierarchy of native Wicked scene
entities, meshes, materials, armatures, morph data and animation components.

It is not a sufficient public asset model for a standalone creator-facing
engine. A creator should not have to infer whether `Thing.wiscene` is a level or
an imported reusable model, and Renegade should not expose a Wicked
implementation detail as the only semantic distinction between project content.

A useful precedent in other tools is to separate heavy compiled/imported asset
data from the definition that tells the engine how a game object should use that
data. Renegade adopts that separation, but explicitly rejects a workflow where
ordinary creators must manually edit metadata text files because the editor does
not expose the required fields.

LP05 already provides dependency information, LC01 already provides stable asset
identity and source/import provenance, and LP06 already packages governed
reachable content. LP07 is therefore the correct boundary to establish the
creator-facing asset abstraction before a permanent reusable-asset catalogue and
reimport workflow are built on top of the current WISCENE proof.

## Decision

### 1. Renegade owns the public content model

Renegade distinguishes these concepts even when more than one of them uses
Wicked serialization internally:

```text
external source         imported asset        game definition        scene
Knight.fbx      ->       Knight.rasset    ->   Knight.rentity    ->   authored scene
```

The conceptual layers are:

- **Source file** — creator/DCC input such as FBX, GLB/GLTF, OBJ or another
  accepted source format. It is reimport input, not durable project identity.
- **Imported asset** — a Renegade-owned reusable project asset. The intended
  authoring extension is `.rasset`. It owns the heavy imported/compiled payload
  and may initially carry Wicked WISCENE-compatible serialization internally.
- **Entity definition** — a Renegade-owned game-ready/placeable definition. The
  working extension is `.rentity`. It can reference one imported asset plus
  other assets/components such as animation assignments, audio, effects,
  collision, physics, gameplay tags and future gameplay behaviour. A single
  imported asset may support multiple entity definitions.
- **Scene document** — an authored level/scene. Existing `.wiscene` scene files
  remain valid and authoritative for the current project format. A future
  Renegade-owned `.rscene` presentation/migration may be considered separately;
  LP07 does not rename or silently migrate existing scene files.
- **Cooked package** — a future distribution artifact, conceptually `.rpak` or
  equivalent. Cooking, compression, encryption, signing and release protection
  are separate from the authoring asset format and remain later release work.

The working Renegade extensions above are versioned product abstractions. Their
final binary/container schemas must be documented and migration-capable before a
public format is frozen.

### 2. WISCENE is an implementation mechanism, not the public asset type

Wicked scene serialization may remain inside `.rasset` or other Renegade-owned
containers for as long as it remains useful. Renegade does not need to replace a
working Wicked serializer merely to own the creator-facing abstraction.

LP07 Gate 1 may continue proving FBX/GLTF conversion by serializing and reopening
WISCENE. That proof validates preservation of native Wicked data; it does not
commit the creator-facing reusable asset format to `.wiscene`.

The permanent reusable asset transaction introduced after Gate 1 must target the
Renegade-owned asset abstraction rather than exposing imported products as
ordinary scene files.

### 3. `.rmeta` remains engine-owned identity/provenance state

Existing `.rmeta` documents/sidecars retain their Renegade identity and document
metadata responsibilities. They are not repurposed into an FPE-style creator
configuration file.

LC01 `AssetRegistry.renegade-assets` remains authoritative for stable asset IDs,
source/product provenance, importer recipe identity, hashes and moved/missing
recovery. No new asset format may create a parallel identity system.

### 4. Metadata is persistence, not the ordinary user interface

Renegade may persist structured metadata, but any ordinary creator-facing option
must be exposed and editable through Studio. A creator must not need to open a
metadata file in a text editor to finish configuring a supported asset.

Studio changes flow through UI-independent Renegade services/commands, validation
and transactional persistence. Where a property is undoable, the same command
boundary must provide Undo/Redo. Save/Open must preserve the resulting state.

Raw metadata inspection/editing may exist later as an explicitly advanced or
diagnostic feature, but it is not the supported primary workflow.

### 5. Separate system metadata from creator semantics

Renegade automatically derives/indexes machine-known asset information where it
can do so reliably, for example:

- stable asset ID and asset class;
- source format and importer/backend/version;
- source/product hashes and provenance health;
- mesh/material counts;
- whether the asset is skinned;
- armature/bone information;
- animation/take/clip information;
- morph-target presence;
- dependency/reference information;
- source current/stale/missing/moved state; and
- future generated thumbnail/preview state.

Creators may add semantic information the importer cannot know, such as
`medieval`, `enemy`, `knight`, `architecture`, `weapon` or other project tags.

Game-facing configuration such as animation assignments, collision profiles,
audio/effect references, material overrides and future gameplay components
belongs to the entity/definition layer rather than being baked into the source
FBX or requiring direct metadata text editing.

### 6. The Asset Browser is registry/metadata-driven

The Asset Browser presents Renegade's understanding of project content, not just
a directory listing. Filesystem enumeration remains useful for containment,
discovery and showing unregistered source entries, but registered content is
identified and queried through stable IDs plus structured metadata.

The browser must be able to search/filter both system-derived metadata and
creator tags. This enables queries and filters such as:

- model / character / material / audio / effect / entity / scene;
- FBX / GLTF / other source format;
- static / skinned / animated;
- current / stale / missing / moved / invalid;
- creator tags such as `medieval` or `enemy`;
- dependency/reference relationships when available; and
- later queries such as unused assets or assets used by a selected scene.

Filename/path remains useful display information but is never stable reference
authority.

### 7. References use stable IDs

Entity definitions, scenes and future gameplay configuration reference other
project assets by Renegade stable asset/document identity. Paths may be stored as
human-readable hints or recovery information, but moved files must not require
manual retargeting when LC01 can prove identity.

### 8. Reimport replaces payload, not identity

Successful explicit reimport may replace the heavy imported payload and update
derived metadata, but it retains the authoritative asset identity and existing
references. Failed reimport leaves the previous successful asset product and
metadata authoritative.

The source/import recipe and product update remain transactional and governed by
LC01 provenance. Reimport is never a reason to assign a new ID merely because
source bytes changed.

### 9. Proprietary extensions are not encryption

`.rasset`, `.rentity` or any other Renegade extension provides semantic ownership
and versioning, not security by itself. Renaming or wrapping WISCENE does not
protect creator content.

If Renegade later supports encrypted/compressed commercial game content, that
protection belongs in an explicit cooked/package format and build pipeline with
real cryptographic/integrity design. The authoring asset model established here
makes that future boundary cleaner, but does not claim distribution protection.

## Consequences

- LP07 Gate 1 remains a valid importer/round-trip proof even though WISCENE is no
  longer the intended public imported-asset type.
- LP07's permanent reusable-asset gate must define/version the `.rasset`
  container/serialization boundary before committing products to the catalogue.
- The Asset Browser must evolve from filesystem-only projection into an
  LC01/metadata-backed content catalogue with search/filter/index semantics.
- Future entity/prefab authoring can use `.rentity` without duplicating imported
  geometry and can configure the same `.rasset` in multiple game-ready ways.
- Existing WISCENE scenes remain compatible; scene-format migration is not hidden
  inside LP07.
- Wicked remains the native rendering/ECS/serialization substrate where useful;
  this ADR adds a Renegade-owned product abstraction rather than a parallel
  rendering/scene system.
- Future packaging/encryption has a clean cooked-content boundary without
  pretending that an authoring extension is a security mechanism.

Any future decision to collapse these layers, expose WISCENE again as the public
imported-asset type, require ordinary users to hand-edit metadata, or replace the
stable-ID authority requires an explicit ADR update.