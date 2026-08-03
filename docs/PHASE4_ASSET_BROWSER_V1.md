# Phase 4 — Asset Browser V1

## Boundary

This slice wires the accepted Renegade bottom Asset Browser drawer to the
active project's real local `Content` directory.

It does not copy GameGuru MAX's global library architecture and does not expose
Wicked's stock Content Browser window.

## Implemented

- Complete default local project folder scaffold.
- Project-rooted `AssetBrowserService`.
- Recursive, safe `Content` folder index.
- Embedded collapsible folder hierarchy inside the approved bottom drawer.
- Immediate-folder asset cards.
- Static type tiles only; no animated thumbnails.
- Folder navigation and item selection.
- Wheel scrolling for the folder tree and asset grid.
- Headless safety and classification tests.

## Deliberately deferred

- Static rendered thumbnail cache under `Saved/Thumbnails`.
- Functional search and type filters.
- Import into `Content/Models/<AssetName>`.
- Drag payloads and viewport placement from reusable WISCENE assets.
- Rename, move, delete and dependency warnings.
- Stable asset IDs and reference repair.
- Material Editor integration.
- Build/cooker dependency traversal.

## Acceptance

1. Create or open a Renegade project.
2. Open Asset Browser from the existing bottom toolbar.
3. Confirm the embedded folder pane mirrors the project's real `Content`.
4. Collapse and expand folders.
5. Select folders and confirm the card grid updates.
6. Add a file through Windows Explorer beneath `Content`, close/reopen the
   drawer, and confirm it appears.
7. Confirm `Saved`, `SourceAssets`, `Intermediate` and `Builds` never appear.
8. Confirm all cards remain static.
9. Run `RenegadeAssetBrowserTests`.
10. Verify packaged DX12 and Vulkan Studio builds before merging.
