# Terrain props P0/P1

## Baseline and ownership

The implementation starts at Renegade `db8d620f75a5219ae5a3906e2837ed2d4165c741` (merged PR 197), Wicked `82557d439a41d104f559b0d3134d160b7bf6154e`, and bundled Jolt 5.6.0. The earlier research used Renegade `3ee58170`. Revalidation found the UI merge composes its wireframe changes on the existing coherent-UMA renderer correction. This branch uses that renderer and keeps the Wicked gitlink. ProjectDocumentTransaction orders replacement by destination path. SceneDocumentService journals the initial scene and identity together, and atomically replaces an existing WISCENE.

## Data contract

| Product | Version 1 representation |
| --- | --- |
| Layer reference | A normal authored entity carries two WISCENE metadata strings, `renegade.terrain_props.project` (project UUID) and `renegade.terrain_props.revision` (lowercase SHA-256 of manifest bytes). Older scenes without this reference have no prop layer. |
| Storage | `Content/TerrainProps/Revisions/<sha256>.rprops` and `<sha256>.rppg` inside the project. Both filenames address the bytes of the entire file. Reopen verifies both kinds of digest. |
| Manifest | Classic-locale whitespace-delimited text. Header fields in order are `RPROPS 1 projectUUID layerUUID nextPlacementID profileCount pageCount`. Each profile row is `profileHex modelAssetUUID shape centerX centerY centerZ halfX halfY halfZ`. One page digest follows per page. Float output uses round-trip precision. |
| Collision | Shapes 0/1/2 are none, upright trunk capsule and boulder box. Centers and extents are model-local metres. Capsule X is radius and Y is half the straight central segment, matching Jolt. The placement's yaw and uniform positive scale transform the shape. |
| Placement page | Little-endian Windows x64. 24-byte header: ASCII `RPPG`, u32 version 1, i32 cell X, i32 cell Z, u32 profile count, u32 placement count. Each profile is 16 opaque identity bytes in stored order, represented as 32 hex digits in the manifest. Each 32-byte placement is u64 ID, u32 local profile index, three IEEE binary32 XYZ coordinates in layer/world space, binary32 yaw in radians and binary32 uniform scale. No native entity or GPU handle is persisted. |
| Identity | The pair (layer UUID, nonzero placement ID) identifies a placement. The next unused ID is persisted. Edits and reordering preserve ID. Profile-table indices are private to each page. Decode rejects unknown versions, short/trailing page data, duplicates, invalid profile references, nonfinite transforms and nonpositive scales. |
| Models | A profile references a registered static model product by stable asset UUID. The existing reusable model placement preparation reads that product once per profile during restore. Only static object parts, meshes and materials enter the projection. |

## Archive decision

Wicked writes whole component arrays and separately registers material resources while writing them. Hiding objects with layer masks, filtering only transform records, and deleting/recreating active physics bodies around Save would each leave a correctness gap. The bounded core overlay in `EngineBridge/patches/TerrainPropArchive.patch` gives native metadata a runtime-only ownership bit. File content and generic editable string metadata cannot set that bit. A whole-scene write builds an exclusion set at the save boundary, omits every component and resource registration belonging to the excluded entities, and writes null for references to them. Entity archive recursion skips projected children. Direct serialization and duplication refuse projected roots. The ordinary whole-component write path remains available when the exclusion set is empty. EngineBridge also rejects projected entities from regular hierarchy selection.

The marker travels with native components through scene adoption and disappears on clear. The archive version and read layout stay the same. Configure applies the patch against the pinned subtree and accepts its already-applied form. Rebuild all native consumers together after changing these headers. The overlay runs before Wicked target configuration. No separate renderer, physics world or streaming pump is added.

## Publication and rollback

`PublishTerrainPropRevision` validates the layer and its model references, serializes pages, and writes the immutable products through `ProjectDocumentTransaction`. The live authored revision reference advances only after that transaction succeeds. The next normal scene save publishes that one reference on disk. An older WISCENE, Save As or backup keeps its own revision while its products remain in the project. A crash between product publication and scene replacement can leave unreachable immutable files. It cannot change the previous scene's revision. Product garbage collection must account for backups and undo history and is deferred.

`ReadTerrainPropRevision` reads and checks the complete document into temporary state before replacing caller output. Studio and Runtime restore models and projections in detached scenes before adopting the new document. A missing page or model fails the open. Direct archive inspection remains available through `PrepareWickedSceneOpen` for checking authored content independently of restoration.

## Services and fixtures

- `TerrainPropDocument::AllocatePlacementId`, `ValidateTerrainPropPage`, `EncodeTerrainPropPage` and `DecodeTerrainPropPage` are the ID and page boundaries.
- `PublishTerrainPropRevision(root, projectId, document, scene, layerOwner, error, transactionOptions)` installs a revision. It does not refresh an already displayed projection. `ReadTerrainPropRevision`, `ValidateTerrainPropRevisionLinks` and `RestoreTerrainPropRevisions` validate and reconstruct it.
- `TerrainPropProjection::Register` takes a prepared static scene and model-local collision profile. It retains a native mesh and material per unique source entity within that profile, with each part's accumulated model-space transform. `Place` creates object components referencing those shared mesh IDs and a separate static body per collision-enabled placement.
- The original, purpose-built GLBs under `Tests/fixtures/terrain-props` are technical fixtures for this repository. Tree has separately transformed trunk and offset crown meshes and materials. Bush and boulder each have one mesh. The fixture creates 32 placements of each, eight metres apart. It exercises import through the governed model commit service. These are not stock production artwork.
- `RenegadeTerrainPropResidencyProbe` generates a complete RGBA8 1024-square DDS with 11 mips, sets an explicit file span and requests native streaming. It compares the native texture descriptor with device memory usage. The locked-file phase, followed by retry, distinguishes a functioning backing read from a retained CPU copy. Resident payload bytes derive from the current texture descriptor. Driver usage includes allocation granularity and deferred release.

## Interfaces proposed for later stages

| Stage | Proposed interface and ownership |
| --- | --- |
| P2 authoring | Studio issues one brush stroke or Generate Apply command to a bridge layer-edit session. Input is layer ID, mix, operation, sampled world-space path, seed and brush settings. The result is changed pages, stable IDs, bounds, a before/after revision and progress/cancel state. Add a live projection replacement method that prepares new parts and swaps at a scene-safe point. Undo applies the inverse delta and preserves the ID high-water mark. |
| Collision streaming | A collision residency service accepts player and gameplay interest bounds, then creates/removes validated static shapes at physics-safe points. A separate all-map query accepts a ray/sweep plus distance and returns stable placement/profile IDs. Resident Jolt IDs stay private to this service. |
| Asset streaming | A residency service receives prioritized cell requests with document revision and cancellation token, returns leases on shared prototype products, and discards obsolete completions. Last release controls eviction. |
| Texture streaming | The governed product resolver returns content identity, semantic, native load flags and a checked container path/offset/length. A diagnostic and budget adapter observes native requested versus resident mips and backing-read failures. The existing Wicked frame pump owns updates. |

## P2 implementation order for Sol

1. Add persisted authoring state and migration for layer recipes, seeds, density, exclusions and overrides. Keep v1 pages readable. Record deltas for changed pages with one logical command per stroke or Apply. Implement live reprojection before wiring widgets.
2. Retain the governed model picker. Build the Terrain Props palette with linked density sliders and numeric boxes per asset. Add saved mixes. Follow the accepted Studio typography, spacing and numeric input behavior.
3. Generate deterministically using stable candidate keys and world positions. Apply cross-cell spacing, respect clearings and retain IDs for edited or unchanged placements. Paint by travelled distance. Erase must persist across regeneration. Handle terrain expansion and deliberate reprojection.
4. Stage bulk edits away from the live document, support cancel before commit, and verify Undo/Redo, Save As, ordinary save and reopen. Inspect narrow and wide panels and the result in the standalone player. Keep all streaming implementations outside P2.

See the terrain-prop section in `HANDOFF.md` for branch state, measured results and pre-push checks.

P2 authoring extends these formats and interfaces; see [Terrain props P2](TERRAIN_PROP_P2.md) for the v2 schema, v1 migration and command boundary. The measurements above retain their original P0/P1 scope.
