# LC01 — Asset Identity and Source Tracking

## Outcome

LC01 turns LP05's read-only dependency closure into durable Renegade asset
identity and source-tracking state. Paths answer where content currently is;
UUIDs answer which asset it is.

## Gate map

1. Stable asset record contract.
2. Transactional project persistence.
3. Source-to-imported-product tracking and import settings.
4. Moved/missing asset recovery while preserving identity.
5. Packaged source-update/reopen proof and LC01 close-out.

## Gate 1 — Stable Asset Record Contract

Gate 1 introduces a UI-free `AssetRegistryService` that consumes an accepted
`DependencyGraph` and produces a versioned `AssetRegistry`.

Each project asset record contains:

- a UUID-v4 asset ID;
- the originating LP05 node ID and canonical project-relative path;
- dependency class, requirement and platform applicability;
- owning provider name/version and current content hash;
- root and source-availability state;
- dependency relationships expressed as stable asset IDs.

Refreshing an existing registry preserves an asset ID when its canonical path
is unchanged. It reports added, changed and removed asset IDs, including
content-hash changes and closure changes. The serialized JSON is sorted and
must round-trip byte-identically.

Runtime-support nodes belong to the engine/build pipeline and do not receive
project asset IDs. Missing project sources remain explicit records with
`source_available: false`; Gate 1 does not create, repair or remove files.

## Gate 1 exclusions

Gate 1 deliberately does not:

- select or write the authoritative registry path in a real project;
- infer that a newly seen path is a moved old asset;
- retain removed records as recovery tombstones;
- define source-to-imported-product or import-settings records;
- execute import or reimport;
- copy, cook or package assets;
- change the existing Content Browser UI;
- modify Wicked or its pin.

Those behaviours require the identity contract to pass first and will be
split into later bounded LC01 gates.

## Gate 1 acceptance

1. A representative LP05 graph becomes one record per project-owned node.
2. Runtime-support nodes are excluded.
3. Graph edges including cycles resolve to stable asset IDs without dangling
   references.
4. Missing sources remain explicit and unavailable.
5. An unchanged refresh preserves every asset ID and serialized byte.
6. Changed, added and removed graph content is reported accurately.
7. Cross-project registries, invalid IDs, duplicate paths and invalid
   relationships fail closed.
8. Debug and Release CI pass with Wicked pinned at
   `3a800b7134aafe58461093c8abb2e274d4e64033`.

Gate 1 is accepted.

## Implementation evidence

Implementation commit `54107a7cc66fd16c8b8f40d8e9e619bd2b7548e2`
passed Renegade Studio run 146 with 24/24 tests in Debug and Release.
`RenegadeAssetRegistryTests` passed in both configurations. The existing LP05
packaged proof also remained green with two processes and unchanged fixture
inputs in each configuration. Pinned-Wicked baseline run 153 passed Debug and
Release, and Wicked remained at
`3a800b7134aafe58461093c8abb2e274d4e64033`.

Exact final head `0cd63d844c655eecebaf3f7bf04fdacbad2d50ed`
passed Studio run 147 and baseline run 154 in Debug and Release. The project
owner independently reviewed that exact head before squash-merging PR #34 at
`580e5a5289e35b9bc60929a5b0c3cec6aaec0b2f`, completing Gate 1 acceptance.

## Gate 2 — Transactional Project Persistence

Gate 2 makes the registry a real project document. Its fixed authoritative
location is `AssetRegistry.renegade-assets` at the project root. It does not
live in `Content`, because it is metadata rather than a creator asset; it does
not live in `Saved` or `Intermediate`, because it is durable project state.

Writes serialize the validated Gate 1 model, stage and validate the exact
requested canonical bytes, then commit through `ProjectDocumentTransaction`.
The shared journal lives under `Intermediate/Transactions`, stays inside the
project containment boundary and is automatically recovered before
`ProjectService::OpenProject` activates a project.

Reads require the expected project UUID, the supported schema, complete
referential integrity and canonical byte ordering. A valid registry for a
different project, or valid but non-canonical JSON, fails closed.

### Gate 2 acceptance

1. A valid registry commits to the fixed project-root document and reloads
   byte-identically.
2. Rewriting unchanged state is a successful byte-preserving no-op.
3. Invalid in-memory state cannot create or replace the document.
4. A different valid staged registry cannot be substituted for the requested
   write.
5. A forced replacement failure preserves the exact previous bytes and leaves
   no recovery debris.
6. An interruption after replacement retains a durable journal; the next
   Project Open restores the exact previous registry and cleans all artifacts.
7. Cross-project and non-canonical documents fail to load.
8. Debug and Release CI pass with Wicked unchanged.

Gate 2 remains unaccepted until exact-final-head CI and independent review
both pass.

### Gate 2 exclusions

Gate 2 does not define source/product pairings or import settings, infer moved
assets, retain recovery tombstones, execute reimport, cook/package content or
change Studio UI. Those remain Gates 3-5.
