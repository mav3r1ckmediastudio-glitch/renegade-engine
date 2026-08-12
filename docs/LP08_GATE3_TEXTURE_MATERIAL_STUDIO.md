# LP08 Gate 3 — Governed Texture Material + Studio Workflow

Status: implementation candidate. Gate 3 is not accepted until exact-head Windows Debug/Release CI, Windows baseline, independent audit, owner acceptance and merge.

Base: LP08 Gate 2 merged `main` at `5abe6fbcab06971b04be1d9be873804409f309a5`.

## Creator outcome

Gate 3 makes Texture the first non-model LP08 resource class consumed by an authored scene:

`external image -> retained SourceAssets/Textures source -> governed Content/Textures .rasset -> stable texture product ID -> Asset Browser -> selected object's base-colour material slot -> WISCENE Save/Open -> stable-ID rehydration through pinned Wicked`

Models retain the accepted LP07 creator path. The existing Assets drawer remains the one project-asset UX; Gate 3 does not add a stock Wicked Editor window or a competing material/import browser.

## Scope

### Creator texture staging

`CreatorTextureWorkflowService` accepts the image formats already proven against the pinned Wicked resource manager by Gate 1 (JPG/JPEG/PNG/BMP/DDS/TGA/HDR). It:

- validates an active project and supported texture format;
- enforces a 512 MiB creator-import ceiling before copying/reading the source;
- creates class-correct `SourceAssets/Textures` and `Content/Textures` folders when needed;
- allocates collision-free retained-source and `.rasset` names with a bounded 1024-attempt ceiling;
- reports a destination-race collision distinctly instead of disguising it as a generic copy failure;
- copies the external image byte-for-byte into the project;
- delegates governed product/LC01/resource-metadata creation to the accepted Gate 2 `ResourceAssetService` transaction;
- removes the newly retained source when the Gate 2 transaction fails before commit; and
- preserves the retained source and reports `committed=true` when the transaction committed but post-commit verification reports failure, so Studio never tells the creator that nothing changed after a committed mutation.

`CreatorTextureImportResult::succeeded` and `committed` are deliberately separate. `committed=true && succeeded=false` means persistent project state may already exist and the caller must refresh/inspect rather than blindly retry.

This is deliberately a Studio staging boundary, not a second resource importer.

### Stable material texture identity

`MaterialTextureAssetService` resolves a texture by stable LC01 product ID. It requires an active, Required `DependencyClass::Texture` product from provider `lp08.rasset` plus accepted `wicked.resourcemanager` provenance, reads the governed resource `.rasset`, validates project/product identity and texture class, and prepares the exact governed payload.

The material does **not** serialize `.rasset` as a fake image filename. Gate 3 stores Renegade-owned WISCENE metadata on the material entity:

- `renegade.material_texture_binding_version = 1`
- `renegade.material_texture.base_color.asset_id = <stable product id>`

`TextureMap::name` remains empty for governed textures. The live Wicked `Resource` is loaded directly from the `.rasset` payload bytes using `wi::resourcemanager::Load(..., filedata, filesize)` with a logical cache name containing stable asset ID, payload hash and original image extension.

### Base-colour slot only

Gate 3 intentionally proves one complete material slot: `MaterialComponent::BASECOLORMAP`.

Normal, surface/ORM, emissive, displacement, occlusion and other texture slots are not claimed by this gate. They can reuse the accepted stable-ID binding pattern after the first path is certified.

### Undo / Redo

`SetMaterialBaseColorTextureAssetCommand` participates in Renegade `CommandService`. It preserves:

- the complete previous Wicked `TextureMap`, including UV set and prior resource/name;
- whether the material already had metadata;
- any previous Gate-3 binding version value; and
- any previous stable base-colour texture ID.

Undo restores that exact prior state. If the material had no metadata before Execute but another system adds unrelated metadata before Undo, Gate 3 erases only its own two keys and removes the component only when it is otherwise empty. Redo reapplies the governed stable-ID binding.

### Save / Open

WISCENE serialization persists the Renegade metadata stable ID, not a source path. After load, `RestoreMaterialTextureBindings()` inspects material metadata, resolves each stable ID back through LC01 to its governed `.rasset`, and recreates the live Wicked texture from in-memory payload bytes. Already-live bindings are skipped, making Studio's rehydration call idempotent.

A corrupt or missing governed texture no longer aborts the entire restore pass. Gate 3 continues through later bindings, restores every valid texture it can, and returns a collective failure summary when one or more bindings failed.

The existing Creator Asset Studio chrome invokes that restore seam during normal update after a project/scene is active. This is a Studio Save/Open proof; packaged Runtime dependency closure remains Gate 5.

### Asset Browser UX

The existing Assets drawer is extended rather than replaced:

- `IMPORT` accepts FBX/GLTF/GLB or supported texture images;
- model import/automatic placement remains unchanged;
- texture import creates/selects a governed project texture but does not silently assign it;
- selecting a registered current texture changes the existing action button from `PLACE` to `ASSIGN BASE`;
- `ASSIGN BASE` enables only when the current scene selection resolves through `ResolveEditableMaterialEntity()` to one unambiguous non-terrain material;
- assigning an already-live identical governed texture is a visible no-op rather than a failed command;
- texture reimport is deliberately disabled until Gate 4;
- LP07 model reimport remains enabled for registered imported models even when their governed product is unavailable, preserving reimport as the recovery action;
- existing creator tags and catalogue search continue to work by stable asset identity;
- Gate-2 resource metadata is overlaid by stable product ID so texture source-format filters are truthful for the supported image formats; and
- texture metadata enrichment is fail-open: unreadable or missing resource metadata never withholds the rest of the Asset Browser. Affected governed texture entries remain visible, are marked `Invalid`, lose their untrusted format label, and a warning is returned to the caller.

LP07 `sourceFormat` remains reliable for accepted model records without a persisted `sourceFormat` field: import retains the original source filename under `SourceAssets/Models/...`, and catalogue projection reconstructs the product format from that active source path or the missing-source tombstone's `lastKnownPath`. Gate 3 therefore does not infer model type from the `.rasset` extension and does not have a legacy-empty persisted format field to migrate.

## Acceptance proof

### Dual-config headless

`RenegadeMaterialTextureAssetTests` runs in Debug and Release and proves:

- LP07 action policy: an available model is placeable/reimportable, a missing model product is not placeable but remains reimportable, and textures never enter the model reimport path;
- external PNG staging retains exact source bytes and creates a governed `.rasset` with stable source/product IDs;
- texture catalogue metadata enrichment makes the PNG source-format filter find the governed product;
- removing the governed texture's resource-metadata record does not blank or shrink the catalogue: the texture remains visible as `Invalid`, its untrusted format is cleared, and enrichment reports a warning;
- the material preparation seam resolves the texture by stable LC01 product identity and preserves exact payload bytes;
- a test loader creates a non-GPU Wicked `Resource` from the payload so command semantics remain headless;
- base-colour assignment leaves `TextureMap::name` empty and preserves the existing UV set;
- persistent version + stable-ID metadata is attached to the material entity;
- normal `CommandService` dirty/Undo/Redo behavior applies;
- unrelated metadata added between Execute and Undo survives;
- WISCENE Save/Open preserves the stable texture ID while not serializing a fake external image path;
- restore rehydrates the resource from the governed `.rasset`;
- a second restore is a no-op once the live resource exists; and
- one missing/corrupt binding does not prevent a later valid binding from restoring.

### Release-only graphics

`RenegadeMaterialTextureGraphicsProof` is built in Debug and Release but executed Release-only on the hosted DX12 runner, matching the accepted hosted-graphics policy used by prior Wicked importer proofs. It proves:

- the pinned Wicked resource manager decodes the governed in-memory PNG payload into a valid GPU texture;
- the real material command installs that GPU-backed texture while keeping the governed stable ID in metadata;
- WISCENE Save/Open retains the binding;
- clearing Wicked's resource cache then restoring forces the payload to be decoded again from the governed `.rasset`; and
- deleting the original external file after import does not prevent preparation from the project-owned governed product.

Both tests have explicit CTest timeouts. No external test fixture is required; the representative 1x1 PNG is immutable test data embedded in the acceptance executables.

## Independent audit repair pass

The first independent audit at `3ce3a551bb5bb77eed125635e702ec6f56a87d4e` found one blocker: Studio had reused the model placement predicate for reimport, accidentally disabling LP07 model recovery when `productAvailable == false`. The repair separates placeability from reimportability and adds headless policy coverage.

The same repair pass also addresses the two medium findings instead of deferring them: creator texture destination allocation is bounded, and material texture restore continues after per-binding failures. Minor findings were addressed where safe: unrelated metadata survives Undo, same-texture assignment is a creator-visible no-op, the committed/unverifiable result contract is documented, and retained-source destination races report distinctly.

The second audit identified one new medium creator-experience issue in the enrichment introduced by those repairs: one missing texture metadata record could fail the entire Asset Browser refresh. Gate 3 now makes enrichment fail-open, leaves bad textures visible as `Invalid`, and proves that the catalogue remains usable.

The audits also asked whether LP07 `sourceFormat` is reliable. The answer is yes for accepted LP07 imports because the catalogue reconstructs format from the retained source record path (or its tombstoned last-known path); there is no persisted legacy `sourceFormat` field whose empty value could silently disable model actions.

## Gate 2 audit items carried forward

Gate 3 acts on two prior nonblocking findings without rewriting accepted Gate 2:

1. **Large payload memory amplification:** normal creator texture import now fails before Gate 2 above 512 MiB. A general resource/video streaming policy remains future work.
2. **Committed-but-unverifiable result:** `CreatorTextureImportResult` exposes `committed`, and Studio explicitly distinguishes that state from an ordinary no-change failure.

Still intentionally unresolved programme items:

- FNV-1a64 remains the existing integrity/freshness hash convention. Gate 3 now also uses the payload hash as part of the live Wicked resource cache key, so a collision has a concrete wrong-resource/wrong-pixels failure mode rather than only an abstract provenance risk. Before Gate 5 lets packaged Runtime resolve these keys, LP08 must explicitly decide whether to migrate to a collision-resistant hash.
- Windows path containment comparisons still follow the existing filesystem-component behavior; reconcile with LP05's ordinal case-folding rule in a deliberate path-identity change, not as an incidental Gate-3 edit.

## Explicit exclusions

Gate 3 does not add:

- texture reimport/stale/moved/missing recovery (Gate 4);
- packaged Runtime resource dependency closure or refresh (Gate 5);
- material slots beyond base colour;
- a full creator material inspector;
- audio/video/Lua/font scene consumers;
- thumbnails/previews;
- texture processing/compression/import options beyond the accepted fixed Gate-2 v1 recipe;
- Wicked Editor UI;
- Wicked source or submodule-pin changes;
- LC01 schema changes;
- LP07 model `.rasset` changes;
- the accepted Gate-2 resource `.rasset` schema change; or
- `Tools/Windows-Build.Common.ps1` changes.

## Closeout

Gate 3 may be merged only after:

1. exact-head Renegade Studio Windows x64 Debug and Release pass;
2. exact-head Windows baseline Debug and Release pass;
3. independent audit of the exact candidate head reports no blocking findings;
4. project-owner acceptance; and
5. merge verification against `main`.
