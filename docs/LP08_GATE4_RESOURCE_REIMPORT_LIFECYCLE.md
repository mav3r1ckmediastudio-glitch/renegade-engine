# LP08 Gate 4 — Governed Resource Reimport + Lifecycle

Status: implementation candidate. Gate 4 is not accepted until exact-head Windows Debug/Release CI, Windows baseline, independent audit, owner acceptance and merge.

Base: LP08 Gate 3 merged `main` at `b5620851c48b934ffb40892055951a6053795fd7`.

## Creator outcome

Gate 4 closes the editor lifecycle for governed non-model resources:

`retained source changes -> LC01 reports STALE -> creator chooses REIMPORT -> stored recipe replays -> same source/product stable IDs -> atomic .rasset/provenance/metadata replacement -> consumers resolve the new accepted payload`

The accepted last-good product is never overwritten merely because a source changed. Reimport is explicit.

## Stable-ID-only reimport boundary

`ResourceAssetService::ReimportResourceAsset()` accepts only:

- project root;
- project stable ID; and
- governed product stable ID.

The caller cannot supply a source path, destination path, format, importer or settings. Those are resolved from LC01 identity/provenance and checked against the accepted resource `.rasset` manifest.

Version-1 reimport requires the exact accepted Gate-2 contract:

- source provider `lp08.source_asset` v1;
- product provider `lp08.rasset` v1;
- importer `wicked.resourcemanager` v1;
- settings schema `renegade-resource-import-settings` v1;
- canonical empty-options recipe with stored resource class and source format; and
- class-correct SourceAssets/Content folders.

Gate 4 does not reinterpret or migrate LP07 model `.rasset` or LP08 resource `.rasset` schemas.

## Stale / invalid / missing / moved state

LC01 and `AssetCatalogueService` remain the sole lifecycle-state authority.

- source record hash differs from the accepted provenance snapshot -> `Stale`;
- source or product unavailable -> `Missing`;
- governed product bytes differ from accepted product provenance -> `Invalid`;
- LC01 recovers the same stable product ID at a new path -> `Moved` for that refresh;
- source/product identity recovered by LC01 is followed by reimport without allocating new IDs.

Studio resource reimport is enabled for `Current`, `Stale` and `Moved` governed resource products when both source and last-good product remain available. `Missing` and `Invalid` resources must first be recovered/repaired rather than allowing reimport to guess at authority.

LP07 model reimport retains its previously accepted recovery policy; Gate 4 does not narrow it.

## Candidate construction and last-good preservation

Before any mutation, reimport:

1. reads the current LC01 registry and accepted provenance;
2. resolves active source/product records by stable ID;
3. validates provider, dependency class, requirement and canonical folder placement;
4. checks generic `ImportedProductStatus`;
5. inspects the source using the stored format/signature contract;
6. requires the physical source hash to match refreshed LC01 source state;
7. requires the existing product bytes to match both the active product record and accepted provenance snapshot;
8. parses the existing resource `.rasset` and requires its identity/recipe to agree with LC01; and
9. builds and fully serializes the replacement candidate in memory.

Malformed or format-mismatched source candidates therefore fail before a transaction is opened and leave the last-good product/provenance/metadata untouched.

## Atomic commit

A successful candidate enters one `ProjectDocumentTransaction` containing:

1. governed resource `.rasset` replacement;
2. `AssetRegistry.renegade-assets` replacement; and
3. `ResourceAssetMetadata.renegade-resourcemeta` replacement.

The product validator rereads the retained source and requires it to remain byte-identical to the accepted candidate during the transaction, then reparses the staged `.rasset` and verifies project/source/product identity, class, format, stored recipe and exact payload.

Registry and resource metadata validators require the exact canonical requested documents.

An injected failure after physical product replacement must roll all three documents back to their prior bytes. A transaction never reports a failed reimport as if the product had not been touched when `transaction.committed` says otherwise.

After commit, reimport reopens product, registry and resource metadata from disk and verifies stable IDs, payload, hashes, provenance snapshots and derived metadata before reporting `succeeded=true`.

## Derived metadata

Reimport recomputes the same Gate-2 resource-derived metadata from the accepted replacement payload:

- exact byte count for all resource classes;
- PNG dimensions / one-mip evidence where available;
- DDS dimensions / mip evidence where available.

No new resource metadata schema is introduced.

## Texture consumer refresh

Gate 3 WISCENE materials persist the governed texture **stable product ID**, not a source/product path. Gate 4 therefore does not rewrite scene references after texture reimport.

`RefreshMaterialTextureBindingsForAsset()` force-reloads only material bindings whose persisted stable ID equals the successfully reimported texture product. It prepares the newly accepted `.rasset` payload and replaces the transient Wicked `Resource` behind the existing base-colour binding.

This is deliberately not a `CommandService` operation and does not create an authored scene edit: the durable stable-ID reference has not changed. Wicked's material is marked dirty only so the renderer consumes the refreshed resource.

If no live material uses the texture, resource reimport still succeeds. If the governed transaction succeeds but a live texture refresh fails, Studio reports the product as current with a live-refresh warning rather than misreporting the persistent transaction as failed.

## Studio workflow

The existing Assets drawer and `REIMPORT` button remain the only creator reimport UX.

Before governed resource replay, the Studio worker rebuilds the creator catalogue so LC01 observes external source edits, moves or missing files. It then calls the stable-ID resource reimport boundary.

For a successfully reimported texture, Studio force-refreshes live material bindings immediately. Other governed resource classes complete at the project-asset boundary in this gate; audio/script/video/font scene consumers are separate future systems.

## Acceptance proof

`RenegadeResourceAssetReimportTests` runs in Debug and Release with no external fixture and proves:

### PNG primary proof

- first import creates stable source/product IDs and a last-good governed product;
- a material binds that stable texture ID;
- editing the retained PNG and refreshing LC01 reports `Stale` without changing product bytes;
- explicit reimport replays the stored recipe and retains both stable IDs;
- replacement payload and PNG dimensions update;
- the catalogue returns to `Current`;
- the material force-refresh seam receives the new governed payload while retaining the same WISCENE stable ID;
- a malformed source candidate cannot alter the last-good product/provenance/metadata;
- a fault after product replacement rolls the complete transaction back byte-for-byte;
- source and product moves are recovered by LC01 stable identity and replay follows the recovered paths;
- missing source and missing product states refuse reimport safely; and
- restored source/product bytes recover their original stable IDs.

### Multi-resource proof

The same generic transaction is exercised for:

- WAV;
- OGG;
- Lua;
- MP4;
- H264; and
- TTF.

Each source is changed, LC01 reports stale, explicit reimport retains source/product IDs, and the reopened `.rasset` contains the second payload with updated provenance/resource metadata.

Gate 3 already proves the unchanged production `wi::resourcemanager::Load()` texture decoder against hosted DX12. Gate 4's new consumer behavior is the selection of the newly accepted stable-ID payload and forced reload, so its dual-config proof injects a resource loader and asserts the exact second payload handed to the consumer rather than duplicating Gate 3's decoder proof.

## Programme items still carried forward

- FNV-1a64 remains the programme's current integrity/freshness hash and Gate 3 live-resource cache-key input. Before Gate 5 packages governed resources, LP08 must explicitly decide whether this remains an integrity-not-security contract or migrates to a collision-resistant hash.
- Windows path identity/case-folding remains a deliberate follow-up aligned with LP05 rather than an incidental Gate-4 schema change.
- General streaming/large-video memory policy remains future work; Gate 3's creator texture import ceiling is unchanged.

## Explicit exclusions

Gate 4 does not add:

- packaged Runtime dependency closure or refresh (Gate 5);
- build/package dependency proof for governed resources (Gate 5);
- new resource `.rasset` schema fields;
- new LC01 schema fields;
- new import processing/compression options;
- material slots beyond Gate-3 base colour;
- a full material inspector;
- audio/video/Lua/font scene consumers;
- automatic source filesystem watchers;
- Wicked Editor UI;
- Wicked source or submodule-pin changes;
- LP07 model `.rasset` changes; or
- `Tools/Windows-Build.Common.ps1` changes.

## Closeout

Gate 4 may be merged only after:

1. exact-head Renegade Studio Windows x64 Debug and Release pass;
2. exact-head Windows baseline Debug and Release pass;
3. independent exact-head audit reports no blocking findings;
4. project-owner acceptance; and
5. merge verification against `main`.
