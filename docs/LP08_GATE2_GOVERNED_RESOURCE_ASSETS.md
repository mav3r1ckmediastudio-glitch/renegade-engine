# LP08 Gate 2 — Governed Resource Assets

**Status:** implementation and proof candidate; exact-head CI, independent audit,
project-owner acceptance and merge remain required.

## Purpose

LP08 Gate 2 establishes the first-import transaction for Renegade's non-model
resource assets. Gate 1 proved the resource formats and classes exposed by the
pinned Wicked resource manager. Gate 2 turns those retained project sources into
stable Renegade-owned `.rasset` products without creating a second identity or
provenance authority.

The five accepted resource classes are:

- Texture;
- Audio;
- Script;
- Video; and
- Font.

Gate 2 is deliberately UI-independent. External-file selection, source staging,
material assignment, reimport and packaged Runtime consumption belong to later
LP08 gates.

## Accepted RAsset envelope

LP07 already established the version-1 `RASSET01` container framing for reusable
model assets. Gate 2 reuses that framing exactly rather than creating a second
container protocol:

```text
8 bytes   RASSET01 magic
4 bytes   little-endian manifest byte count
8 bytes   little-endian payload byte count
N bytes   canonical JSON manifest
M bytes   payload
```

The resource asset is distinguished by its manifest contract, not by changing
the envelope:

- manifest format: `renegade-resource-rasset`;
- schema version: `1`;
- payload format: `wicked-resource-filedata`;
- importer: `wicked.resourcemanager`;
- importer version: `1`; and
- settings schema: `renegade-resource-import-settings`, version `1`.

The accepted LP07 model manifest and parser remain unchanged. Gate 2 tests prove
that an LP07 model `.rasset` still round-trips byte-deterministically, that the
resource parser rejects a model asset, and that the model parser rejects a
resource asset.

## Resource payload policy

Version 1 stores the exact accepted retained-source bytes as the governed
resource payload. Gate 2 does not invent a parallel image/audio/video/font/script
codec or preprocessor around Wicked.

That choice provides a stable Renegade product and provenance boundary now while
preserving the pinned Wicked resource manager as the native consumer seam for
later material and Runtime gates.

The manifest records a payload hash. LC01 separately records the full `.rasset`
product hash. Those hashes intentionally represent different byte domains:

- source/payload hash — exact retained resource file bytes; and
- product hash — complete Renegade `.rasset` container bytes.

## Version-1 import recipe

Gate 2 persists an explicit canonical recipe so Gate 4 reimport can replay an
accepted contract rather than infer behavior from filenames.

For a PNG texture the recipe is:

```json
{"options":{},"resource_class":"texture","source_format":"png"}
```

The same shape is used for every accepted resource class and source format.
Version 1 has no creator-tunable processing options. The caller supplies the
canonical empty options object; non-empty or non-canonical options fail closed.

## Canonical project locations

The Gate 2 backend consumes a resource source that has already been retained
under the appropriate project-owned source location and creates the governed
product under the matching Content location:

| Class | Retained source | Governed product |
| --- | --- | --- |
| Texture | `SourceAssets/Textures/...` | `Content/Textures/...rasset` |
| Audio | `SourceAssets/Audio/...` | `Content/Audio/...rasset` |
| Script | `SourceAssets/Scripts/...` | `Content/Scripts/...rasset` |
| Video | `SourceAssets/Video/...` | `Content/Video/...rasset` |
| Font | `SourceAssets/Fonts/...` | `Content/Fonts/...rasset` |

Gate 2 does not implement the Studio external-file staging workflow. A caller
must already have retained the source below the correct `SourceAssets` class
folder. Gate 3 owns the creator-facing staging and directory-creation behavior.

## LC01 identity and provenance

LC01 remains the sole stable identity and import-provenance authority.

For each first import Gate 2 records a retained source asset:

- stable source asset ID;
- dependency class `ImportedContent`;
- requirement `EditorOnly`;
- provider `lp08.source_asset`, version 1;
- project-relative retained-source path; and
- current source content hash.

It also records a governed product asset:

- stable product asset ID;
- requirement `Required`;
- provider `lp08.rasset`, version 1;
- project-relative `.rasset` path;
- complete product content hash; and
- the real resource dependency class (`Texture`, `Audio`, `Script`, `Video` or
  `Font`).

The LC01 `ImportedProductRecord` associates those stable IDs and persists:

- importer/backend and version;
- settings schema/version;
- canonical recipe JSON;
- source hash at successful import; and
- product hash at successful import.

No LP08 Gate 2 change is made to the LC01 schema.

## Resource-derived catalogue metadata

Gate 2 adds:

`ResourceAssetMetadata.renegade-resourcemeta`

This is a presentation/index companion, not an identity registry. Each record is
keyed only by the LC01 stable product ID and can contain:

- resource class;
- source format;
- exact payload byte count; and
- reliable texture width, height and mip evidence when available.

PNG dimensions are derived from the IHDR fields. DDS dimensions/mip count are
derived when the accepted header evidence is available. Other resource classes
currently persist the exact byte count only.

The companion document deliberately contains no path authority and no separate
source/product provenance. It also avoids changing the accepted LP07
`AssetMetadata.renegade-assetmeta` schema. Joining this resource-derived data
into the creator-facing catalogue/Studio projection is Gate 3 work.

## Atomic first-import transaction

A successful Gate 2 first import commits three documents through the shared
`ProjectDocumentTransaction` journal boundary:

1. the governed resource `.rasset`;
2. `AssetRegistry.renegade-assets`; and
3. `ResourceAssetMetadata.renegade-resourcemeta`.

Before replacement, every staged document is validated. The `.rasset` validator
also re-reads the retained source and requires it to remain byte-identical to the
source accepted at preparation time. Source drift during the transaction
therefore fails before a product is accepted.

The first-import boundary does not overwrite an existing product. Explicit
stable-ID reimport belongs to Gate 4.

## Failure and rollback contract

Malformed or unsupported input fails before commit and must not create product,
registry or metadata state.

Gate 2 additionally proves rollback after the governed product has genuinely
been replaced. `RenegadeResourceAssetRollbackTests` observes the actual target
`.rasset` during the transaction's `AfterReplace` hook, confirms that the file
exists, deliberately fails the transaction, and then requires:

- the newly created `.rasset` to be removed;
- the exact previous registry bytes to be restored;
- no resource metadata from the failed first import to remain;
- retained source bytes to remain unchanged; and
- the restored LC01 registry to reopen without failed-import identity or
  provenance.

This is stronger than a failure injected merely by document index because the
transaction sorts destination paths before commit.

## Acceptance tests

Gate 2 registers two headless tests in both Debug and Release.

### `RenegadeResourceAssetTests`

Proves:

- PNG governed import with 2x3 dimension evidence;
- exact source/payload byte preservation;
- canonical version-1 recipe persistence;
- WAV, Lua, MP4 and TTF through the same generic service;
- five distinct stable source/product identities;
- LC01 provenance and real product dependency classes after reopen;
- resource-derived metadata after reopen;
- malformed input produces no partial state;
- unsupported v1 options fail closed; and
- LP07 model/resource `.rasset` compatibility and cross-kind rejection.

### `RenegadeResourceAssetRollbackTests`

Proves actual product replacement followed by transaction rollback and exact
last-good restoration.

Existing LP07 model lifecycle and package tests remain regression coverage for
the already accepted model asset path.

## Explicit exclusions

Gate 2 does **not** add:

- Studio resource import/staging UI;
- material texture-slot assignment;
- thumbnails/previews;
- resource reimport;
- moved/missing creator recovery UX;
- LP05 dependency-provider integration for resource `.rasset` products;
- LP06 package changes;
- packaged Runtime resource resolution;
- creator-tunable resource processing options;
- stock Wicked Editor UI;
- Wicked source or submodule-pin changes;
- LC01 schema changes;
- LP07 model `.rasset` format/parser changes;
- existing `AssetMetadata.renegade-assetmeta` schema changes; or
- `Tools/Windows-Build.Common.ps1` changes.

## Closeout conditions

Gate 2 is accepted only when all of the following are true at one exact PR head:

1. Renegade Studio Windows x64 Debug passes;
2. Renegade Studio Windows x64 Release passes;
3. `RenegadeResourceAssetTests` passes in Debug and Release;
4. `RenegadeResourceAssetRollbackTests` passes in Debug and Release;
5. Windows baseline Debug and Release pass against the pinned Wicked revision;
6. independent exact-head audit has no blocking finding;
7. the project owner accepts the gate; and
8. PR #53 is merged only after those conditions are met.
