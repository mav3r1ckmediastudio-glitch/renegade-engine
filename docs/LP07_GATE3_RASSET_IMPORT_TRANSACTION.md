# LP07 Gate 3 — Governed `.rasset` import transaction

Status: **implementation candidate on `agent/lp07-gate3-rasset-import-transaction`**.

Baseline main:
`41afafc3b3d734f58e362fb0bb1c208b1c4b92d0`
(`Add LP07 Gate 2 asset catalogue (#47)`).

Wicked remains pinned at:
`3a800b7134aafe58461093c8abb2e274d4e64033`.

## Goal

Gate 3 turns the accepted Gate 1 model converter and Gate 2 catalogue into the
first permanent Renegade reusable imported model asset.

The public product is `.rasset`. Wicked WISCENE remains an internal payload and
preservation mechanism, not the creator-facing asset type.

## Source-retention policy

The authoritative reimport source is project-owned below `SourceAssets`, for
example:

```text
SourceAssets/Models/Knight.fbx
```

The reusable product is project content, for example:

```text
Content/Models/Knight.rasset
```

Gate 3 deliberately does not persist an absolute external-machine source path.
The UI-free transaction requires the source to already be below `SourceAssets`.
Gate 5 can stage creator-selected external files/bundles into `SourceAssets`
before invoking this service. This also avoids pretending that copying only a
`.gltf` JSON file is sufficient when it may have sibling buffers/textures.

Source records are LC01 `ImportedContent` with `EditorOnly` requirement. The
`.rasset` product is `ImportedContent` with `Required` requirement. Source and
product are associated through LC01 `ImportedProductRecord`; the source is not a
runtime dependency of the payload merely because it is provenance.

## Version-1 `.rasset` container

The binary container is intentionally small and versioned:

```text
8 bytes   magic: RASSET01
4 bytes   little-endian canonical manifest length
8 bytes   little-endian WISCENE payload length
N bytes   canonical JSON manifest
M bytes   internal Wicked WISCENE payload
```

The manifest records:

- Renegade `.rasset` schema/version;
- project ID;
- stable product asset ID;
- stable source asset ID;
- source format (`fbx`, `gltf`, or `glb`);
- accepted importer/backend and version;
- versioned canonical import recipe;
- internal payload format (`wicked-wiscene`); and
- FNV-1a payload hash.

Validation rejects wrong magic, unsupported versions, malformed/non-canonical
manifest JSON, invalid/cross-linked identities, unsupported source formats,
recipe/manifest contradictions, invalid payload lengths, trailing bytes and
payload corruption.

The version-1 recipe is canonical JSON:

```json
{"options":{},"source_format":"fbx"}
```

There are no creator-tunable conversion options in Gate 3 yet, but recording the
format and an explicit empty options object now means Gate 4 reimport can replay
the accepted recipe rather than infer it from a renamed extension.

## Governed transaction

`ReusableAssetService::ImportModelAsset()` is UI-independent and performs:

1. validate project identity and contained canonical paths;
2. require source below `SourceAssets` and product below `Content`;
3. reject an existing/tombstoned product path (replacement belongs to Gate 4);
4. read/create the LC01 registry and read Gate 2 metadata fail-closed;
5. assign/reuse stable source identity and assign a new stable product identity;
6. convert through the accepted format-neutral `ImportService` into an isolated
   temporary WISCENE under `Intermediate/Imports`;
7. reuse Gate 1 structural + rig/animation WISCENE round-trip validation;
8. derive Gate 2 model metadata directly from the accepted Wicked scene;
9. wrap the WISCENE bytes in the version-1 `.rasset` container;
10. add LC01 source/product records and the exact import recipe/provenance;
11. add derived model metadata keyed by the product stable ID; and
12. atomically commit `.rasset` + `AssetRegistry.renegade-assets` +
    `AssetMetadata.renegade-assetmeta` in one `ProjectDocumentTransaction`.

The temporary WISCENE is never authoritative and is removed before commit.
Creator source bytes are never modified.

If validation or commit fails, no `.rasset` becomes authoritative and the prior
registry/metadata bytes are restored. Gate 3 refuses replacement of an existing
`.rasset`; Gate 4 owns safe replacement/reimport and last-good preservation.

## Proof

`RenegadeReusableAssetTests` runs headlessly in Debug and Release and proves:

- deterministic `.rasset` serialization/reopen;
- wrong magic rejection;
- unsupported schema rejection;
- payload corruption rejection;
- recipe/manifest contradiction rejection;
- source containment below `SourceAssets`;
- existing `.rasset` refusal without byte mutation;
- non-canonical settings rejection; and
- cross-project registry rejection.

`RenegadeReusableAssetGraphicsProof` is always built in Debug/Release and runs
in hosted Release alongside the existing immutable Gate 1 FBX fixtures. It
proves:

- static FBX -> stable `.rasset`;
- skinned/animated FBX -> `.rasset` retaining structure, rig/animation evidence
  and named animation take/clip data after reopening the internal payload;
- self-contained GLTF -> the same `.rasset` transaction;
- LC01 source/product identity and provenance reopen;
- Gate 2 metadata/catalogue projection as a current registered imported asset;
- project-owned source byte immutability;
- forced multi-document commit failure rolls `.rasset`, registry and metadata
  back together; and
- an attempted first-import replacement leaves the accepted `.rasset`
  byte-identical for Gate 4 to handle later.

## Explicit exclusions

Gate 3 does not implement reimport/replacement, Studio Asset Browser UI, creator
tag UI, scene placement from `.rasset`, thumbnail generation, `.rentity`, or
packaged Runtime acceptance. Those remain Gates 4–6.

No Wicked source or submodule pin change is required.
