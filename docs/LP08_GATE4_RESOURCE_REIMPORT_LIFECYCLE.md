# LP08 Gate 4 — Governed Resource Reimport + Lifecycle

Status: implementation candidate. Gate 4 is not accepted until exact-head Windows Debug/Release CI, Windows baseline, independent audit, owner acceptance and merge.

Base: LP08 Gate 3 merged `main` at `b5620851c48b934ffb40892055951a6053795fd7`.

## Creator outcome

Gate 4 closes the editor lifecycle for governed non-model resources:

`retained source changes -> LC01 reports STALE -> creator chooses REIMPORT -> stored recipe replays -> same source/product stable IDs -> atomic .rasset/provenance/metadata replacement -> consumers resolve the new accepted payload`

It also closes the mainstream deleted-product recovery case:

`governed .rasset deleted -> LC01 retains product tombstone + provenance -> retained source still available -> creator chooses REIMPORT -> product is recreated at the tombstone path with the same stable product ID -> tombstone is removed atomically`

The accepted last-good product is never overwritten merely because a source changed. Reimport is explicit.

## Stable-ID-only reimport boundary

`ResourceAssetService::ReimportResourceAsset()` accepts only:

- project root;
- project stable ID; and
- governed product stable ID.

The caller cannot supply a source path, destination path, format, importer or settings. Those are resolved from LC01 identity/provenance and, when an active last-good product exists, checked against the accepted resource `.rasset` manifest.

Version-1 reimport requires the exact accepted Gate-2 contract:

- source provider `lp08.source_asset` v1;
- product provider `lp08.rasset` v1;
- importer `wicked.resourcemanager` v1;
- settings schema `renegade-resource-import-settings` v1;
- canonical empty-options recipe with stored resource class and source format; and
- class-correct SourceAssets/Content folders.

Gate 4 does not reinterpret or migrate LP07 model `.rasset` or LP08 resource `.rasset` schemas.

### LC01 freshness precondition

`ReimportResourceAsset()` deliberately requires the active source `AssetRecord.contentHash` to describe the exact retained bytes being replayed. A caller must therefore refresh LC01 from disk before direct reimport. Studio satisfies this contract by rebuilding the creator catalogue immediately before replay.

If a future caller edits the retained source and invokes reimport without first refreshing LC01, the service fails closed with `refresh LC01 state before reimport` and does not touch the last-good product.

This ordering contract is part of the public service contract, not just a Studio convention.

## Stale / invalid / missing / moved state

LC01 and `AssetCatalogueService` remain the sole lifecycle-state authority.

- source record hash differs from the accepted provenance snapshot -> `Stale`;
- source or product unavailable -> `Missing`;
- governed product bytes differ from accepted product provenance -> `Invalid`;
- LC01 recovers the same stable product ID at a new path -> `Moved` for that refresh;
- source/product identity recovered by LC01 is followed by reimport without allocating new IDs.

Studio resource reimport is enabled for:

- `Current`, `Stale` and `Moved` governed resources when source and active product are available; and
- `Missing` governed resources when the retained source is still available and only the governed product is missing.

A missing source remains blocked because no authoritative bytes exist to replay. An `Invalid` product remains blocked because reimport must not overwrite untrusted on-disk bytes as if they were the accepted last-good product.

## Active-product candidate construction and last-good preservation

When the governed product is active, before any mutation reimport:

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

## Missing-product regeneration

When only the governed product is missing, Gate 4 does **not** invent a new identity or destination. It requires:

1. the retained source to remain an active LC01 record with the original stable source ID;
2. imported-product provenance for the original stable product ID;
3. an LC01 `MissingAssetRecord` tombstone for that product ID;
4. the tombstone provider/class/requirement to match the accepted LP08 resource relationship;
5. the tombstone's last-good `contentHash` to equal `productContentHashAtImport` from provenance;
6. the tombstone path to remain a canonical class-correct `Content/.../*.rasset` path;
7. the destination path to be physically absent and its parent to resolve inside project `Content`; and
8. the retained source bytes to match refreshed LC01 source state and the stored recipe/signature.

The replacement manifest is reconstructed from stable project/source/product IDs plus the accepted provenance recipe, and its payload/derived metadata are rebuilt from the retained source. The candidate registry removes the product tombstone and restores one active `AssetRecord` with the same stable product ID at the tombstone path.

Product recreation, tombstone removal/active-record restoration, provenance update and resource metadata update are then committed as one transaction. A failure rolls back to the pre-recovery state, including absence of the product file and retention of the tombstone.

## Atomic commit

A successful candidate enters one `ProjectDocumentTransaction` containing:

1. governed resource `.rasset` replacement or recreation;
2. `AssetRegistry.renegade-assets` replacement; and
3. `ResourceAssetMetadata.renegade-resourcemeta` replacement.

The product validator rereads the retained source and requires it to remain byte-identical to the accepted candidate during the transaction, then reparses the staged `.rasset` and verifies project/source/product identity, class, format, importer, importer version, settings schema/version, stored recipe and exact payload.

Registry and resource metadata validators require the exact canonical requested documents.

An injected failure after physical product replacement must roll all three documents back to their prior state. For a normal reimport that restores the previous product bytes; for missing-product regeneration it removes the newly created product and restores the original registry tombstone.

After commit, reimport reopens product, registry and resource metadata from disk and verifies stable IDs, payload, hashes, provenance snapshots, absence of any product tombstone and derived metadata before reporting `succeeded=true`.

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

For a successfully reimported or regenerated texture, Studio force-refreshes live material bindings immediately. Other governed resource classes complete at the project-asset boundary in this gate; audio/script/video/font scene consumers are separate future systems.

## Acceptance proof

`RenegadeResourceAssetReimportTests` runs in Debug and Release with no external fixture and proves:

### PNG primary proof

- first import creates stable source/product IDs and a last-good governed product;
- a material binds that stable texture ID;
- editing the retained PNG and calling reimport **without** an LC01 refresh fails closed and leaves the product unchanged;
- refreshing LC01 then reports `Stale` without changing product bytes;
- explicit reimport replays the stored recipe and retains both stable IDs;
- replacement payload and PNG dimensions update;
- the catalogue returns to `Current`;
- the material force-refresh seam receives the new governed payload while retaining the same WISCENE stable ID;
- a malformed source candidate cannot alter the last-good product/provenance/metadata;
- a fault after product replacement rolls the complete transaction back byte-for-byte;
- source and product moves are recovered by LC01 stable identity and replay follows the recovered paths;
- a missing source refuses reimport safely;
- the source can recover at a new path with the original stable source ID;
- deleting only the governed product leaves a recoverable LC01 tombstone;
- `REIMPORT` remains eligible when the source is intact;
- reimport recreates the `.rasset` at the tombstone path with the original stable product ID;
- the product tombstone disappears and the active product record/provenance/resource metadata reopen coherently.

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

## Independent audit follow-up

The first exact-head audit of `41c1c1bd` reported no blockers and raised three follow-ups.

1. **Missing resource product recovery:** accepted and fixed in Gate 4 as described above. This is lifecycle/editor recovery work and is not deferred to Gate 5 packaging.
2. **Implicit LC01 refresh ordering:** accepted and promoted into the public `ResourceAssetService.h` contract, with a direct regression proving an unrefreshed edit fails closed.
3. **Duplicated internal helpers:** accepted as a maintainability risk, but not mixed into this post-green lifecycle repair. `ResourceAssetService.cpp` and `ResourceAssetReimportService.cpp` currently duplicate path/hash/metadata helpers. Extraction to one internal implementation seam is mandatory before Gate 5 changes the integrity/hash decision or adds package consumption, so Gate 5 cannot accidentally update only one copy.

The audit compared resource behavior to the Gate-3 model action policy and inferred that model reimport already regenerates a missing product. Inspection of the accepted LP07 backend found a pre-existing inconsistency: `CanReimportCreatorModelAsset()` allows the missing-product action, and the Gate-3 policy test labels that as recovery, but `ReusableAssetService::ReimportModelAsset()` currently still refuses a missing active product/last-good `.rasset`. Gate 4 does not silently change LP07 model importer behavior; this discrepancy is recorded for a separately scoped model-lifecycle repair.

## Programme items still carried forward

- FNV-1a64 remains the programme's current integrity/freshness hash and Gate 3 live-resource cache-key input. Before Gate 5 packages governed resources, LP08 must explicitly decide whether this remains an integrity-not-security contract or migrates to a collision-resistant hash.
- Before that Gate-5 hash/integrity work, shared resource path/hash/derived-metadata helpers must be extracted so first import and reimport cannot drift.
- Windows path identity/case-folding remains a deliberate follow-up aligned with LP05 rather than an incidental Gate-4 schema change.
- General streaming/large-video memory policy remains future work; Gate 3's creator texture import ceiling is unchanged.
- The pre-existing LP07 model action/backend mismatch for a physically deleted model `.rasset` requires a separately scoped repair; Gate 4 does not redefine the accepted model format or importer.

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
