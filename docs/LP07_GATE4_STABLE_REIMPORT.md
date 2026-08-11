# LP07 Gate 4 — Stable explicit reimport

Status: **implementation candidate on `agent/lp07-gate4-stable-reimport`**.

Baseline `main`:
`4de53bd5a4e7b26aef8cd0aa6aebdba1d3721f7b`
(`Add LP07 Gate 3 RAsset import transaction (#48)`).

Wicked remains pinned at:
`3a800b7134aafe58461093c8abb2e274d4e64033`.

The older status header in `docs/LP07_REUSABLE_PROJECT_ASSET_WORKFLOW.md`
still predates the Gate 1–3 merges. Its Gate 4 contract remains authoritative;
this document records the current implementation/evidence state without spending
a separate CI cycle on a broad historical-status rewrite.

## Goal

Gate 4 replaces the payload of an existing imported `.rasset` safely after its
project-owned source changes, while retaining the already accepted LC01 source
and product stable IDs.

Reimport is explicit. A stale source never silently overwrites a product.

## Authority model

The caller supplies only:

- project root;
- project stable ID; and
- the registered reusable **product stable ID**.

`ReusableAssetService::ReimportModelAsset()` resolves everything else from the
accepted project state:

- product path from the LC01 product `AssetRecord`;
- source ID from `ImportedProductRecord`;
- current source path from the active LC01 source `AssetRecord` (therefore moved
  source recovery is automatically honoured);
- source format from the stored canonical recipe;
- importer/backend and version from stored provenance;
- settings schema/version/JSON from stored provenance; and
- existing product identity/recipe from the last-good `.rasset` manifest.

No caller-supplied source path, destination path, format, backend or settings can
redirect reimport.

The stored recipe is the selection authority. The existing `ImportService`
`expectedFormat` contract is used as a consistency guard: a source whose current
extension contradicts the stored recipe fails closed rather than being silently
reclassified to a different importer.

LP07 version 1 has no creator-tunable conversion options. Its canonical
`options` object must therefore be empty. New imports reject non-empty v1
settings before conversion, and reimport rejects any already-stored v1 recipe
whose `options` object is non-empty before converter dispatch. The lifecycle
therefore never preserves unsupported values while pretending they were replayed.

## Preconditions and fail-closed checks

Before conversion, Gate 4 requires:

1. valid project/product IDs;
2. a canonical LC01 registry belonging to the project;
3. exactly one imported-product provenance record for the requested product;
4. active source and product records with the accepted LP07 providers/classes;
5. canonical contained `SourceAssets` and `Content/*.rasset` paths;
6. current source bytes matching the LC01 source record (refresh first if not);
7. the last-good product bytes matching both its LC01 record and imported
   provenance snapshot;
8. a valid version-1 `.rasset` whose project/source/product IDs and complete
   importer/settings recipe agree with LC01 provenance; and
9. a supported canonical version-1 recipe whose backend agrees with its stored
   source format and whose `options` object is empty.

A changed source is allowed and is exposed in `statusBefore`; an externally
changed/missing product is not accepted as a safe replacement base.

## Replacement transaction

Reimport converts into an isolated temporary WISCENE and reuses the accepted
Gate 1 structural + rig/animation round-trip proof. The Gate 3 non-modal
malformed-import adapter remains the converter failure surface.

Only after conversion, round-trip validation, derived metadata generation,
source immutability proof and `.rasset` container validation succeed does Gate 4
prepare the replacement transaction.

The transaction atomically replaces:

1. the existing `.rasset` bytes at the same product path;
2. `AssetRegistry.renegade-assets`, updating the same product record hash and
   only the requested imported-product record's source/product import snapshots;
   unrelated imported products may remain stale/missing without becoming a
   prerequisite for this product's reimport; and
3. `AssetMetadata.renegade-assetmeta`, updating derived model facts under the
   same product ID while preserving creator tags.

The source/product IDs, importer/backend/version and canonical settings recipe do
not change.

If conversion fails, no persistent write starts. If any replacement write or
validator fails, `ProjectDocumentTransaction` rolls all three documents back to
their pre-reimport bytes. The previous `.rasset` therefore remains the
last-good authoritative product.

## Gate 4 proof

`RenegadeReusableAssetReimportRecipeTests` is a headless Debug/Release regression
for the version-1 recipe boundary. It first proves a new import with a canonical
but non-empty settings object is rejected before graphics conversion or
persistent product/registry creation. It then constructs an otherwise valid
registered product containing a canonical but non-empty stored `options` object
and proves explicit reimport rejects it before graphics conversion while leaving
the authoritative `.rasset` and registry bytes unchanged.

`RenegadeReusableAssetReimportGraphicsProof` is built in Debug and Release and
runs in hosted Release, following the accepted Gate 1/Gate 3 DX12 runner policy.
It uses the same immutable exact-upstream FBX fixtures already staged by Studio
CI.

The FBX lifecycle proves:

- initial static FBX -> stable `.rasset` through the accepted Gate 3 path;
- creator metadata/tag state exists before reimport;
- source bytes change to the skinned/animated FBX fixture;
- the real LC01 `RefreshAssetRegistry()` preserves IDs and produces stale state;
- Gate 2 catalogue reports `Stale` before explicit reimport;
- reimport is requested only by product stable ID;
- source/product IDs remain unchanged;
- payload/product hash and derived metadata update;
- armature/skinning/animation facts are present after reimport;
- creator tag survives;
- reopen provenance returns to `Current`;
- malformed FBX reimport returns a structured failure without changing the
  last-good `.rasset`, stale registry or metadata bytes;
- retry with a valid source succeeds without identity repair;
- forced failure after `.rasset` replacement rolls `.rasset` + registry +
  metadata back byte-identically;
- retry after rollback succeeds;
- moving the unchanged source is recovered by the real LC01 moved-source logic
  with the same source ID and its new canonical path;
- because only the **source** moved and its bytes are unchanged, the registered
  `.rasset` product correctly remains `Current` rather than being falsely marked
  `Moved`;
- changing bytes at the recovered source path makes the product `Stale`; and
- explicit reimport follows that recovered path while retaining the same source
  and product IDs, then returns the product to `Current`.

A separate self-contained GLTF project proves source update -> LC01 stale ->
explicit reimport -> same IDs/current provenance through the stored
`wicked.gltf` recipe.

## Explicit exclusions

Gate 4 does **not** add Studio Asset Browser UI, creator import/reimport buttons,
scene placement from `.rasset`, thumbnail generation, `.rentity`, or packaged
Runtime acceptance. Those remain Gates 5–6.

No Wicked source, Wicked submodule pin, LC01 schema, Gate 2 metadata schema, or
`Tools/Windows-Build.Common.ps1` change is required.
