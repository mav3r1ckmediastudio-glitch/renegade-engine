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

## Integrity decision

LP08 version-1 `.rasset` and LC01 freshness records retain their accepted `fnv1a64:` compatibility token. Gate 5 does not silently mutate those durable schemas or invalidate Gate 2-4 products.

This token is treated as deterministic freshness/identity evidence, not as a security digest. LP06 package promotion and launch integrity remain independently protected by the existing SHA-256 package-integrity contract. Gate 5 therefore adds no third persistent hash schema.

## Acceptance

`RenegadeResourceAssetPackageRuntimeTests` runs Debug and Release and proves:

- a WISCENE material stable texture ID resolves to the authoritative LP08 governed texture product;
- the retained source is not admitted into the Runtime dependency closure;
- packaged resource resolution uses stable ID plus `GameData/content-manifest.json` and does not require LC01 at Runtime;
- the packaged texture payload is restored onto the saved material through the governed Wicked resource loader seam;
- exact stable-ID/payload-hash/package-path evidence is returned;
- WAV, Lua, MP4 and TTF products use the same generic packaged resource resolver.

The final Gate 5 Release acceptance extends this to the real Windows owner build and named Runtime: save scene -> change source -> explicit stable-ID reimport without resaving scene -> build -> package contains the current `.rasset` and no retained source -> remove source project -> launch promoted executable from an unrelated working directory -> Runtime reports the current packaged resource stable ID/hash.

## Failure rules

- stale or missing authoritative source fails the owner build through existing LC01 freshness validation;
- missing/corrupt/mismatched governed package products fail Runtime resource preparation closed;
- every distinct material texture resource is prepared before any material binding is changed;
- failed build/promotion continues to preserve the previous owner-visible output through the existing LP06 safe-promotion transaction.

## Explicit exclusions

No new LC01 or `.rasset` schema, no automatic reimport, no source files in `GameData`, no audio emitter/gameplay authoring, no Lua lifecycle, no video-player authoring, no font layout/UI authoring, no new material slots, no Wicked source/pin change, no stock Wicked Editor UI, and no `Tools/Windows-Build.Common.ps1` changes.
