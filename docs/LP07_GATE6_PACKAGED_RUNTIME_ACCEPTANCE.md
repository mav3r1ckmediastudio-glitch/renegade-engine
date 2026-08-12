# LP07 Gate 6 — Packaged Runtime Acceptance

Status: **implementation and acceptance proof complete; final exact-head CI, independent audit, owner acceptance and merge remain required.**

Gate 6 closes LP07 by proving that a creator-authored reusable `.rasset` survives the real Renegade scene, LP05/LC01 dependency, LP06 package, and named standalone Runtime boundaries without falling back to the original FBX/GLTF source.

Wicked remains pinned at `3a800b7134aafe58461093c8abb2e274d4e64033`. Gate 6 does not modify Wicked source or its file formats.

## What Gate 6 adds

### Persistent reusable-asset scene identity

`ReusableAssetInstanceService` introduces a Renegade-owned wrapper around each placed reusable model asset using ordinary serialized Wicked `MetadataComponent` and hierarchy state.

The wrapper stores:

- the stable LC01 product asset ID;
- a versioned Renegade reusable-instance marker; and
- creator-authored instance transform/state.

The replaceable `.rasset` payload remains a marked child hierarchy. This separates durable instance identity and authored transform from the imported payload that may change after explicit reimport.

`PlaceReusableModelCommand` is the Gate 6 placement command used by Studio. It preserves the stable asset ID through Undo/Redo and WISCENE Save/Open. Repeated placement continues to load the governed `.rasset`; it never reconverts the original source.

### LP05/LC01 dependency closure

`ReusableAssetDependencyProvider` projects the persistent scene wrapper into the existing LP05 graph instead of creating a separate LP07 packaging list.

For a saved scene it:

1. reads the stable reusable asset ID from the WISCENE metadata;
2. resolves that ID through authoritative LC01 state;
3. emits the governed `.rasset` as required project content; and
4. when the `.rasset` node is traversed, exposes the current payload's ordinary WISCENE dependencies through the same graph.

The provider deliberately keeps the accepted `lp07.rasset` provider identity so a build-time LC01 refresh cannot relabel a Gate 3/4 product and invalidate its source/product provenance.

### Source freshness without source leakage

The retained FBX/GLTF source is provenance authority, not Runtime content. The Windows owner-build preparation therefore adds the authoritative source as an **EditorOnly freshness root after ordinary transitive Runtime discovery has completed**.

This allows LC01/BuildService to hash the real source and reject a stale imported product while ensuring the source itself — and source-side glTF buffer/image references — cannot leak into `GameData` merely because they are needed for freshness validation.

### Packaged Runtime refresh

`ReusableAssetRuntimeService` resolves saved stable IDs through the already-governed `GameData/content-manifest.json` package manifest.

For every reusable instance in a package-relative Runtime scene it:

1. resolves the stable ID to a packaged `.rasset`;
2. validates package/project identity and the `.rasset` container;
3. prepares every required replacement payload before mutating the target scene;
4. instantiates the current packaged payload under the existing creator-authored wrapper; and
5. removes the old child payload only after every replacement is ready.

The Runtime never consults or converts `SourceAssets` FBX/GLTF input.

Story Flow can load multiple levels in one Runtime session, so reusable-asset evidence accumulates across level transitions. `RuntimeBootstrap.log` records discovered/refreshed counts plus stable asset ID, current payload hash and package path.

## Failure and last-good rules

Gate 6 preserves the existing LP07 and LP06 fail-closed contracts:

- failed explicit reimport leaves the prior successful `.rasset` byte-identical and authoritative;
- a stale imported product is rejected before package smoke/promotion;
- a failed build does not replace the owner-visible last-good Windows output;
- packaged Runtime refresh prepares replacements before commit; a missing/invalid packaged mapping leaves the prior saved payload and creator wrapper intact; and
- editor-only retained source files are never accepted as Runtime package content.

## Automated proof

### Debug and Release headless proofs

`RenegadeReusableAssetInstanceTests` proves:

- persistent stable-ID wrapper creation;
- creator transform isolation from the payload;
- Undo/Redo; and
- WISCENE Save/Open retention of stable identity and authored transform.

`RenegadeReusableAssetRuntimeTests` proves:

- repeated instances of one stable `.rasset` both refresh to the current packaged payload;
- creator-authored wrapper transforms survive replacement; and
- a missing packaged stable ID fails before removing the last-good payload.

These tests execute in both Debug and Release.

### Release end-to-end packaged acceptance

`RenegadeReusableAssetPackageAcceptance` is intentionally Release-only on the hosted Windows runner because the representative FBX conversion and real standalone Runtime require Wicked graphics initialization. Debug still compiles/links the target and executes the headless Gate 6 proofs.

The end-to-end acceptance performs the real creator/build lifecycle:

1. import representative animated/skinned FBX through `CreatorAssetWorkflowService`;
2. verify catalogue metadata and creator tags;
3. place the governed `.rasset` through the persistent stable wrapper;
4. Save/Open the scene while it still contains the initial payload;
5. force a failed reimport and prove the last-good `.rasset` bytes remain unchanged;
6. replace the retained source with a different valid FBX;
7. prove LP05 reaches the `.rasset` and retains the source only as `EditorOnly` freshness evidence;
8. prove the stale owner build fails at imported-product freshness before smoke and preserves prior owner output;
9. explicitly reimport and prove source/product stable IDs remain unchanged while the payload hash changes and creator tags remain;
10. **do not resave the scene**, leaving its baked child payload deliberately old;
11. build through the real LP06 Windows owner workflow;
12. prove the `.rasset` is in the package while the retained FBX source is absent;
13. launch the staged named Runtime from an unrelated working directory and require Runtime evidence for the new payload hash and absence of the old hash;
14. safely promote the package and validate its integrity;
15. delete the entire source project; and
16. launch the promoted `<GameName>.exe` directly a second time from an unrelated working directory and require the same current `.rasset` evidence.

GLTF/GLB support remains a required regression contract throughout LP07.

## Proof-input policy

The representative FBX fixtures are immutable pinned upstream inputs staged by CI before configure. Gate 6 also derives a disposable complete Story Flow fixture from the accepted LP03 identity documents plus the already-proven self-contained Level Two WISCENE fixture.

If any mandatory Gate 6 proof input is absent, CMake configuration fails with `FATAL_ERROR`. The gate may not silently disappear from CTest while CI remains green.

## Test configuration policy

The Release suite contains additional graphics-backed LP07 tests that are intentionally not executed in Debug on the hosted DX12 runner. Their executables are still built in Debug for compile/link coverage. Gate 6 itself contributes two Release-only CTest entries: the derived package fixture and the full package acceptance.

This distinction is deliberate and must remain explicit in PR/closeout claims: **dual-config headless coverage is not the same claim as dual-config execution of the full standalone package proof.**

## Explicit exclusions

Gate 6 does not add:

- `.rentity` authoring;
- thumbnail generation;
- stock Wicked Editor UI;
- a parallel packaging/dependency list;
- automatic/background destructive reimport;
- Wicked source or pin changes;
- LC01 schema changes;
- Gate 2 asset-metadata schema changes; or
- commercial cooking/signing/encryption.

`Tools/Windows-Build.Common.ps1` is outside Gate 6 scope.

## Closeout gate

The implementation/proof is ready for closeout only when all of the following are true on the final documentation head:

- Renegade Studio Debug passes;
- Renegade Studio Release passes, including the full Gate 6 package acceptance;
- Windows baseline Debug/Release passes;
- mandatory proof inputs cannot be silently skipped;
- independent exact-head audit passes;
- project owner accepts the candidate; and
- only then is PR #51 merged.
