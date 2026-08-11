# LP07 Gate 2 — Registry/metadata-backed asset catalogue

Status: **implementation candidate; exact-head Windows CI passed; independent review pending**.

Baseline main:
`6c09a1450b17e7bef9341ed1e29dab7b31f2a30f`
(`Add LP07 Gate 1 FBX import seam (#46)`).

Implementation evidence head before this docs-only reconciliation:
`004963c2e3984ca5440591d029b357ce716a9bc7`.

Wicked remains pinned at:
`3a800b7134aafe58461093c8abb2e274d4e64033`.

Architectural authority:
`docs/adr/0004-renegade-asset-model-and-managed-metadata.md`.

## Goal

Gate 2 turns the existing filesystem-only Asset Browser projection into a
UI-independent Renegade content catalogue without moving import, reimport,
placement, thumbnail generation, or Studio UI work forward from their later
LP07 gates.

The catalogue joins three already-defined authorities rather than inventing a
fourth:

1. `AssetBrowserService` remains the project-`Content` filesystem discovery and
   containment layer;
2. LC01 `AssetRegistry.renegade-assets` remains the sole stable asset identity,
   dependency, provenance, and moved/missing recovery authority; and
3. Gate 2 adds a small versioned metadata document keyed only by LC01 stable
   asset IDs for creator semantic tags and reliable derived model facts.

## Metadata document

The fixed project-root document is:

`AssetMetadata.renegade-assetmeta`

Schema identifier/version:

`renegade-asset-metadata` / `1`

This is deliberately **not** a new asset identity registry. Every record is
keyed by an LC01 UUID and Gate 2 fails closed if metadata references an ID that
is neither active nor retained by an LC01 missing-asset tombstone.

The document stores only information that does not belong in LC01's accepted
identity/provenance schema:

- creator semantic tags; and
- reliable derived model index facts such as mesh/material counts,
  armature/bone evidence, animation clip/channel evidence, morph-target count,
  and skinned/animated flags.

Source format, importer identity, source/product hashes, dependency IDs,
provenance health, and moved/missing identity remain derived from LC01 and are
not duplicated into this metadata file.

Keeping this as a separate document also preserves the accepted LC01 registry
schema and canonical packaged registry evidence rather than silently changing
`renegade-asset-registry` v3 during LP07.

Creator tags are persisted as deterministic trimmed terms with ASCII
case-folding, sorted and deduplicated. Ordinary creators will edit them through
Studio in Gate 5; direct text-file editing is not a supported workflow.

Metadata commits use the existing `ProjectDocumentTransaction` journal,
staging, validation, atomic replacement, rollback, and no-op behaviour.

## Catalogue state semantics

The catalogue exposes one state per creator-facing entry:

- `unregistered` — a real file discovered below project `Content` with no LC01
  identity; no temporary/fake UUID is assigned;
- `current` — registered content and, for imported products, source/product
  snapshots still match the last successful import;
- `stale` — an imported product remains present but its registered source hash
  changed since the last successful import;
- `missing` — required registered source/product content is unavailable, or the
  entry is represented by an LC01 missing-asset tombstone;
- `moved` — LC01 has just recovered an active stable ID at a new path and the
  caller supplies that recovery transition to the catalogue; after reopen the
  same ID naturally returns to `current` unless another health condition exists;
- `invalid` — active registry/product state contradicts observed content, such
  as unexpected product drift or an active path that cannot be found where the
  registry says it is available.

Health precedence is fail-safe: invalid/missing/stale conditions are not hidden
by a moved transition.

## Catalogue projection

For each registered creator-facing Content entry Gate 2 exposes, where
applicable:

- stable asset ID;
- canonical project-relative path;
- existing Renegade `AssetType` classification;
- LC01 dependency class;
- source format;
- importer and importer version for imported products;
- current source/product availability;
- model-derived metadata;
- creator tags;
- outgoing dependency asset IDs; and
- reverse `referenced by` asset IDs.

Engine-owned `.rmeta` identity sidecars are excluded from the creator catalogue.
They remain persistence/identity implementation state under ADR 0004.

## Deterministic query contract

`AssetCatalogueQuery` supports combined deterministic filters over:

- free text (name, path, stable ID, state, source format, importer, tags);
- Renegade asset type;
- catalogue state;
- source format (case-insensitive, optional leading dot);
- skinned true/false;
- animated true/false;
- static-model-only; and
- creator tags using AND semantics.

Queries do not mutate metadata, refresh LC01, import content, or touch the active
scene.

## Gate 2 acceptance proof

`RenegadeAssetCatalogueTests` is the bounded headless proof. Its fixed project
fixture covers:

- missing metadata document -> valid empty project-owned metadata state;
- tag canonicalisation, sorting, deduplication, deterministic serialization,
  transactional commit/reopen, and byte-preserving no-op;
- cross-project and corrupt metadata rejection;
- registered current imported FBX product projection;
- stale imported product after source-hash change;
- invalid imported product after product drift;
- moved transition from an LC01 recovered stable ID;
- missing tombstone retained as a stable tagged catalogue entry;
- filesystem-only FBX retained honestly as `unregistered` with no fake ID;
- engine-owned `.rmeta` sidecar exclusion;
- dependency and reverse-reference summaries;
- combined source/type/rig/state/tag query;
- stale text query;
- static/tag query;
- unregistered query;
- byte-equivalent repeated catalogue projection;
- reopen after moved recovery preserving stable ID and creator metadata;
- metadata referencing an unknown stable ID failing closed;
- cross-project registry failing closed; and
- the existing `AssetBrowserService` traversal rejection remaining intact.

The normal Gate 1 FBX proof, LP05 packaged graph proof, LC01 packaged registry
proof, LP06 standalone tests, and pinned-Wicked baselines remain required
regressions in Debug/Release CI.

## Authoritative implementation-head CI

Exact implementation/wiring head:
`004963c2e3984ca5440591d029b357ce716a9bc7`.

Renegade Studio run #264 (`31528737390`):

- Debug: success; 43/43 applicable tests passed;
- `RenegadeAssetCatalogueTests`: passed;
- Release: success; 44/44 tests passed;
- `RenegadeAssetCatalogueTests`: passed;
- `RenegadeModelImportGraphicsProof`: passed;
- `RenegadeStandalonePackageTests`: passed in Release;
- the established hosted Debug standalone-audio capability skip remained the
  only allowed Debug skip.

Canonical regression evidence remained unchanged:

- LP05 graph: 4,681 bytes, SHA-256
  `23b67f63099293d79a239997730b287f157fb38e5421aecb5505e0ca42c84384`;
- LC01 registry: 2,180 bytes, SHA-256
  `547a26c09e6a74394cc9bc67885070272928f5af14d736e8e086d022f0aeea0e`.

Windows baseline run #410 (`31528737377`):

- Debug: success;
- Release: success.

The first PR head failed only at CMake configure because the Gate 2 test source
was registered root-relative from an included CMake file. The corrective commit
changed the source reference to `${CMAKE_CURRENT_LIST_DIR}/AssetCatalogueTests.cpp`;
no production behaviour was altered by that fix.

This evidence section is a documentation-only follow-up to the green
implementation head. The final documentation head must also pass the ordinary
required workflows before independent review.

## Explicit exclusions

Gate 2 does **not**:

- import or reimport a model;
- define or write `.rasset`;
- change scene placement;
- add Asset Browser UI;
- generate universal thumbnails;
- expose stock Wicked Editor/Content Browser UI;
- change Wicked source or the submodule pin; or
- change LC01's accepted registry schema.

Those boundaries remain owned by LP07 Gates 3–6.
