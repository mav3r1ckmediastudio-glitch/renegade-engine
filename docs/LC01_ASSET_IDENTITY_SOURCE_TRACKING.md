# LC01 — Asset Identity and Source Tracking

## Outcome

LC01 turns LP05's read-only dependency closure into durable Renegade asset
identity and source-tracking state. Paths answer where content currently is;
UUIDs answer which asset it is.

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

Gate 1 remains unaccepted until exact-final-head CI and independent review
both pass.

## Implementation evidence

Implementation commit `54107a7cc66fd16c8b8f40d8e9e619bd2b7548e2`
passed Renegade Studio run 146 with 24/24 tests in Debug and Release.
`RenegadeAssetRegistryTests` passed in both configurations. The existing LP05
packaged proof also remained green with two processes and unchanged fixture
inputs in each configuration. Pinned-Wicked baseline run 153 passed Debug and
Release, and Wicked remained at
`3a800b7134aafe58461093c8abb2e274d4e64033`.

This proves the implementation commit. Gate 1 remains unaccepted until the
documentation-complete exact final head passes CI and receives independent
review.
