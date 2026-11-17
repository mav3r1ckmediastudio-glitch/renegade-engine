# Terrain props P2 authoring

This is the bounded authoring extension to [P0/P1](TERRAIN_PROP_P1.md), based on handover commit `2853b55c55f7d7aeee8aaa8e9f69d2bba02ca2b8`. It keeps the pinned Wicked renderer, scene, archive and resource authority. Local implementation is not independent acceptance; exact commands, native evidence and outstanding limits belong in the latest terrain-prop section of HANDOFF.md.

## Creator workflow

Open a project with one finite terrain and registered static-model assets. In the Terrain workspace, expand **TERRAIN PROPS**. Choose a model from the governed project picker and press **ADD MODEL TO MIX**; use **REFRESH MODELS** after importing another model. Each entry has its own linked **DENSITY / m2** slider and numeric input. **MINIMUM SPACING** applies to the current mix, and **SEED** selects the deterministic population.

Use **GENERATE APPLY** to populate the current finite terrain, or arm **PAINT** and drag on terrain to add props along a stroke. Releasing the pointer commits one Undo operation. **ERASE** creates persistent clearings: later Generate and Paint operations respect the excluded area. Undo the erase to restore that area; painting over a clearing does not remove it. **REPROJECT TO TERRAIN** updates existing placement heights after sculpting. After expanding terrain, Generate can populate the added terrain while retaining existing candidate identities.

Name the current mix and press **SAVE MIX** to persist it with this layer as one Undo operation. Selecting a saved mix changes the working recipe; Generate Apply or a completed Paint stroke applies it. Saved mixes belong to the layer and travel with its revision through Save As/reopen; they are not a global engine catalogue. Density/seed edits remain in the working recipe until an authoring action or Save Mix commits them.

The progress label reports preparation. **CANCEL / STOP BRUSH** discards staging or an unfinished stroke; Escape cancels a brush gesture. A stale scene/history revision rejects the prepared edit and requests another Apply. The status distinguishes cancellation, failed preparation and successful application. Use normal scene Save/Save As after authoring, and normal scene Undo/Redo for completed operations.

## Ownership and edit flow

Studio captures a finite-terrain height snapshot through `CaptureTerrainPropTerrain`. Its copied grids and height sampler may be used by the authoring worker without accessing live native components. The sampler refuses points outside actual terrain coverage, including missing chunks. Expansion is discovered on the next explicit capture; Reproject deliberately updates existing placement heights.

`BuildTerrainPropEdit` accepts pages, authoring state, the placement counter, revision tokens, finite world bounds and an operation. It stages a result without changing its inputs. Progress returns false to cancel. The output includes resulting pages and authoring state, changed-page deltas, changed bounds and the next placement ID. Invalid input, capacity failure and cancellation leave the caller's output unchanged.

The palette resolves a scene's existing P0/P1 authored layer before creating a terrain-owned layer. A separate legacy layer owner remains independent of terrain deletion. Deleting the actual layer owner removes its transient display at a scene-safe point; Undo restores it from the authoritative revision. Multiple existing layers require explicit owner selection and are reported as ambiguous by this first palette. Services are reused across same-document history and discarded on document replacement.

`TerrainPropEditService` belongs to one open document. Its command is executed at the editor's existing native scene-safe point. Execute rejects a changed revision. Undo and Redo compare complete authored content while excluding only the allocation high-water mark, then apply against the current revision. This allows chained history after an Undo publishes a revision containing a newer counter; it does not allow a divergent edit to be overwritten.

The shared history service calls `ICommand::TryUndo` before moving a command between stacks. Its default preserves ordinary commands' existing Undo behavior; terrain-prop commands return failure when publication fails, leaving history and displayed content intact.

Brush stamps and clearings use a radius-aware spatial index. They retain one index entry per disc, including large clearings, with cancellation during index construction and queries. A 20-million-operation lookup budget bounds pathological overlapping records. The transient work counters used by focused tests are not serialized.

Live updates prepare changed pages and preserve shared mesh/material prototypes. Publication and projection installation form one logical operation. Failed publication rolls staged native components back and retains the old reference/display; cancellation discards staging. Restored transient components carry a runtime-only layer tag so opening an existing scene and beginning a new edit cannot leave duplicate projections. Ordinary authored WISCENE writes continue to exclude all transient projection content.

## Placement rules

The recipe stores a seed, page cell size, fixed candidate lattice step and current mix. Each mix entry references a stable profile and supplies density in placements per square metre, minimum spacing and uniform scale range. The UI may present density per 100 square metres by converting this canonical field.

A candidate key derives from seed, world lattice coordinates and profile identity. Density is a deterministic threshold on that lattice. Page coordinates do not enter the key, so adjacent cells and later terrain expansion use the same candidates. The lattice step controls the available candidates; density is bounded by that resolution. Candidate generation has an explicit one-million-candidate operation limit and reports an error when a smaller region is required.

Spacing considers candidates across cell boundaries and existing retained placements. Minimum spacing uses the larger requirement of the involved profiles. Generated yaw and scale derive from the candidate key. Existing key-to-ID bindings survive density reductions and deletion; returning candidates reuse their IDs. New IDs advance a persisted counter and history never rewinds that counter.

Paint and Erase sample the travelled horizontal path at a fixed distance, carrying the remainder between pointer segments. Pointer-event frequency therefore does not change the stamps. One completed stroke produces one command. Erase stores circular clearings as well as tombstones for removed candidates; regeneration respects both. Explicit overrides preserve authored transforms and identities during regeneration. Legacy placements with candidate key zero remain manual placements.

## Version 2 storage and version 1 compatibility

Storage remains immutable, digest-addressed project products selected by the authored layer revision metadata. Save, Save As, backups and reopen therefore retain their selected revision. Neither native entity IDs nor GPU/physics handles enter placement records.

| Product | Version 2 change |
| --- | --- |
| Manifest | Header begins `RPROPS 2`; the remaining identity, counter, profile and page-digest fields retain their v1 order. An authoring block follows the page digests. |
| Placement page | The header uses version 2. Each placement is 40 bytes: the original 32-byte record followed by a little-endian u64 candidate key. |
| Recipe | Seed, cell size, candidate lattice step and current named mix. |
| Saved mixes | Named entry lists containing profile ID, density, spacing and scale range. |
| Candidate bindings | Nonzero candidate key and persistent placement ID. |
| Tombstones and clearings | Suppressed candidate keys and world-space X/Z/radius circles. |
| Overrides | Candidate key, placement ID, profile ID, XYZ, yaw and uniform scale. |

The authoring block starts with `AUTHORING 1 seed cellSize candidateStep savedMixCount bindingCount tombstoneCount clearingCount overrideCount`. The current mix follows, then saved mixes, bindings, tombstones, clearings and overrides in that order. A mix starts with a quoted name and entry count. Strings use the standard quoted-string escaping; floating-point output uses classic locale and round-trip binary32 precision.

Readers accept both manifest versions and both page versions. A v1 page receives candidate key zero; a v1 manifest receives empty authoring state and the default recipe. Its IDs and allocation counter are preserved. New writes use v2.

Validation rejects unknown versions, truncated/trailing data, digest mismatches, invalid identities/transforms, repeated cells/IDs/candidate keys, undefined profile references, mismatched bindings and IDs beyond the counter. Unsigned manifest identities reject signed and overflowing decimal input. Mixes allow at most 64 entries and 256-byte names; authoring state is bounded to 1,024 saved mixes and one million bindings, tombstones, clearings or overrides each. Manifests and cumulative encoded pages each have a 128 MiB budget. Failed reads retain caller state.

## Validation and stage boundary

Focused suites are `RenegadeTerrainPropPageTests`, `RenegadeTerrainPropAuthoringTests`, `RenegadeTerrainPropTerrainTests` and `RenegadeTerrainPropEditTests`. The existing native `RenegadeTerrainPropSliceProof` exercises real imported models, authored-only WISCENE saves, full authoring-state reopen, v1 migration, Save As and failure rollback. The slice also retains its earlier standalone/shared-resource/collision/residency checks; those are scoped technical fixtures.

Completion additionally requires the documented Debug/Release local preflight, WARP native checks, renderer overlay regression, inspected wide/narrow palette views, normal editor authoring and standalone-player captures with matched performance settings. A test being listed here is not a PASS claim; use the handover receipts.

Collision-interest streaming, full asset/texture streaming, budgets, eviction and automatic cooking remain later stages. Existing basic collision profiles and the native texture residency diagnostic are retained. Proposed later interfaces remain in TERRAIN_PROP_P1.md; P2 adds no new residency service.
