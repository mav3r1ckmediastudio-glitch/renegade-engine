# Phase 4 — Asset Browser V1.1

## Implemented

- Existing projects backfill the standard Renegade directory structure when opened.
- GLB/GLTF imports allocate duplicate-safe assets under `Content/Models`.
- The existing Wicked prepared scene is saved and round-trip validated as WISCENE.
- The same prepared scene is passed to the existing undoable placement command.
- The selected source is archived under `SourceAssets/Models/<asset-name>/`.
- A common same-stem GLTF `.bin` sidecar is archived when present.
- Content import logs are redirected to `Saved/ImportLogs`.
- The browser refreshes to `Content/Models` after successful import.
- Repeated imports use `_2`, `_3`, and so on instead of overwriting.

## Later slices

- Arbitrary GLTF URI dependency harvesting.
- Texture/material extraction as first-class assets.
- Asset IDs and metadata.
- Browser double-click placement and drag-and-drop.
- Search, thumbnails, rename, move, delete, and dependency repair.
