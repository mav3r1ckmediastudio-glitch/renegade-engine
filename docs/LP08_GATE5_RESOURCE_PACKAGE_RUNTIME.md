# LP08 Gate 5 — Governed Resource Package and Runtime Acceptance

Status: implementation/proof candidate.

Base `main`: `884797a1c1e4721e347165f0e475df9367f20f12` (merged LP08 Gate 4 / PR #55).

## Goal

Close LP08 by proving that a durable governed resource reference crosses the existing LP05/LC01 dependency graph, LP06 Windows package and named Renegade Runtime without consulting or shipping its retained creator source.

## Architecture

- `ResourceAssetDependencyProvider` owns the `lp08.rasset` scene dependency seam.
- Saved material metadata continues to store only the stable governed texture product ID.
- During owner-build dependency discovery that ID is resolved through LC01 to the current `Content/Textures/*.rasset` product and emitted as a normal required `Texture` node.
- The existing imported-source freshness-root step then adds the matching `SourceAssets/...` record only as `EditorOnly` freshness evidence. `BuildService` therefore checks source freshness without staging the source into `GameData`.
- `PreparePackagedResourceAsset()` resolves any packaged LP08 resource class from `GameData/content-manifest.json`, validates the governed resource `.rasset` and requires package/project/stable-ID agreement. It never reads `AssetRegistry.renegade-assets` or `SourceAssets` at Runtime.
- `RefreshPackagedMaterialTextureAssets()` resolves and decodes every distinct required texture before changing any target material. A missing/corrupt required product therefore fails before partial material replacement.
- Named package-relative Runtime launches execute this resource refresh after the accepted LP07 reusable-model refresh. Runtime evidence records the resource stable ID, governed payload hash and package path.

## Integrity and live-cache decision

LP08 version-1 `.rasset` manifests and LC01 freshness records retain their accepted `fnv1a64:` compatibility token. Gate 5 does not silently mutate those durable schemas or invalidate Gate 2-4 products.

The FNV token is treated only as deterministic persisted freshness/compatibility evidence. It is **not** accepted as the sole identity of an in-memory Wicked resource because a collision there could return the wrong cached pixels after reimport.

`ResourceAssetCacheIdentityService` therefore derives the transient Windows Wicked resource-manager name from SHA-256 of the actual accepted payload bytes plus the stable product ID and original source extension. Studio material loading and packaged Runtime loading both use this seam. The cache identity is deliberately transient and never written back into `.rasset`, LC01 or WISCENE. Non-Windows compile portability uses a fresh stable UUID load token, so two preparations cannot alias even though Windows x64 remains the accepted LP08 target.

LP06 package promotion and launch integrity remain independently protected by the existing SHA-256 package-integrity contract over packaged files. Gate 5 therefore adds collision resistance where it is needed without adding a third persistent asset hash schema.

## Acceptance

`RenegadeResourceAssetCacheIdentityTests` runs Debug and Release and proves the Windows live-cache name is deterministic SHA-256 over actual payload bytes: the same payload produces the same transient cache identity and changed bytes produce a different identity without consulting the persisted FNV token.

`RenegadeResourceAssetPackageRuntimeTests` runs Debug and Release and proves:

- a WISCENE material stable texture ID resolves to the authoritative LP08 governed texture product;
- the retained source is not admitted into the Runtime dependency closure;
- packaged resource resolution uses stable ID plus `GameData/content-manifest.json` and does not require LC01 at Runtime;
- the packaged texture payload is restored onto the saved material through the governed Wicked resource loader seam;
- exact stable-ID/payload-hash/package-path evidence is returned;
- WAV, Lua, MP4 and TTF products use the same generic packaged resource resolver.

`RenegadeResourceAssetPackageAcceptance` is Release-only and exercises the real owner-build/named-Runtime boundary. It deliberately saves `LevelOne.wiscene` against the original governed texture stable ID, changes the retained PNG, proves the stale product fails the owner build before smoke while preserving prior output, explicitly reimports with the same source/product IDs, and **does not resave the scene**. The current owner build must package the reimported `.rasset` while excluding the retained source. The staged named Runtime must report the current stable texture ID/payload hash. After safe promotion, the source project is deleted and the promoted named executable is launched directly again from an unrelated working directory; it must report the same current packaged resource evidence.

Representative WAV, Lua, MP4 and TTF products remain generic package-resolution regression proof. Their full gameplay/authoring consumers are intentionally not claimed by LP08.

## Failure rules

- stale or missing authoritative source fails the owner build through existing LC01 freshness validation;
- missing/corrupt/mismatched governed package products fail Runtime resource preparation closed;
- every distinct material texture resource is prepared before any material binding is changed;
- a persisted-FNV collision cannot alias the Windows Wicked live cache because transient cache identity is derived from payload SHA-256;
- failed build/promotion continues to preserve the previous owner-visible output through the existing LP06 safe-promotion transaction.

## Explicit exclusions

No new LC01 or `.rasset` schema, no automatic reimport, no source files in `GameData`, no audio emitter/gameplay authoring, no Lua lifecycle, no video-player authoring, no font layout/UI authoring, no new material slots, no Wicked source/pin change, no stock Wicked Editor UI, and no `Tools/Windows-Build.Common.ps1` changes.
