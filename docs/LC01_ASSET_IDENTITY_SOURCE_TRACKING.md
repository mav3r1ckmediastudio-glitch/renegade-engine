# LC01 — Asset Identity and Source Tracking

Status: **ACCEPTED AND MERGED**.

Exact final PR head:
`3d3e780b38792aec866cd19ce6638a8260ffff4f`

Squash merge:
`01d790bda5acea0cdb6a7735557b12224c795a64`
(`Add packaged LC01 source reopen proof (#39)`).

Wicked remained pinned at
`3a800b7134aafe58461093c8abb2e274d4e64033`.

## Outcome

LC01 turns LP05's read-only dependency closure into durable Renegade asset
identity and source-tracking state. Paths answer where content currently is;
UUIDs answer which asset it is.

LC01 deliberately stops before creator-facing import/reimport execution. Its
output is the durable identity/provenance foundation consumed by the later
reusable asset workflow.

## Gate map

1. Stable asset record contract.
2. Transactional project persistence.
3. Source-to-imported-product tracking and import settings.
4. Moved/missing asset recovery while preserving identity.
5. Packaged source-update/reopen proof and LC01 close-out.

## Gate 1 — Stable Asset Record Contract

Accepted through PR #34, squash merge `580e5a5289e35b9bc60929a5b0c3cec6aaec0b2f`.

`AssetRegistryService` consumes an accepted LP05 dependency graph and produces a
versioned project asset registry. Each project-owned record carries:

- UUID-v4 asset ID;
- LP05 node identity and canonical project-relative path;
- dependency class/requirement/platform applicability;
- provider name/version and current content hash;
- root/source-availability state; and
- dependency relationships expressed as stable asset IDs.

Runtime-support nodes remain outside the project asset-ID space. Missing project
sources remain explicit unavailable records rather than being silently repaired.

Unchanged refresh preserves IDs and deterministic serialized bytes.

## Gate 2 — Transactional Project Persistence

Accepted through corrective PR #36, squash merge
`489e33d`.

The authoritative registry is:

`<Project Root>/AssetRegistry.renegade-assets`

Registry writes use the same fail-closed project-document transaction discipline
as other Renegade-owned documents: staged validation, exact-byte checks,
previous-state preservation, journaled interruption recovery and canonical
project ownership/containment validation.

A false-green result in the first Gate 2 landing reopened acceptance. The
corrective gate fixed Windows short/long path identity during recovery and made
native child-test failure propagation authoritative. The corrected gate passed a
genuine full Debug/Release test result before acceptance.

## Gate 3 — Source-to-Imported-Product Provenance

Accepted through PR #37, squash merge `c2ace926ff2486fd3b60e3717bdba6dc8d138217`.

LC01 provenance connects stable source asset IDs to stable imported product IDs
and records:

- importer identity/version;
- settings schema version;
- canonical settings JSON;
- source hash observed at successful import; and
- product hash observed at successful import.

A source may produce multiple products; a product has at most one authoritative
source/import recipe.

This gate records metadata only. It does not invoke an importer, overwrite a
product or silently reimport stale content.

## Gate 4 — Moved and Missing Asset Recovery

Accepted through PR #38, squash merge
`b45de5e369789697dba0fd4502677055b8105c1f`.

A stable asset UUID may follow a moved/reappearing source only where the recovery
evidence is bidirectionally unique. Genuine loss creates a deterministic
last-known tombstone. Ambiguous matches never auto-relink.

Provenance relationships retain their stable IDs across a uniquely recovered
move. Recovery reads/refreshes metadata; it does not move creator files or
trigger import.

## Gate 5 — Packaged Source-Update/Reopen Proof

Accepted through PR #39.

A fixed mini-project and `RenegadeAssetRegistryProcessFixture.exe` are exercised
from assembled Debug/Release Studio evidence packages. A disposable artifact
working copy crosses four independent process boundaries:

1. initial registry/provenance commit;
2. Project Open after a controlled source-content update;
3. Project Open after moving that updated source; and
4. final unchanged reopen.

The proof requires:

- stable source UUID through update and move;
- stale provenance after source-content change;
- moved-source recovery without speculative relinking;
- byte-identical final canonical registry evidence on unchanged reopen;
- immutable packaged fixture hashes before and after; and
- non-zero propagation for any identity/provenance/recovery/fixture failure.

## Final authoritative evidence

Exact final head:
`3d3e780b38792aec866cd19ce6638a8260ffff4f`

- Renegade Studio run **166**: Debug/Release SUCCESS.
- Windows baseline run **185**: Debug/Release SUCCESS.
- All four packaged lifecycle phases passed in both configurations.
- Final Debug and Release canonical registry evidence matched exactly.

Canonical registry:

- bytes: `2,180`;
- SHA-256:
  `547a26c09e6a74394cc9bc67885070272928f5af14d736e8e086d022f0aeea0e`.

Independent exact-head review completed before the owner squash-merged PR #39.

## What LC01 now guarantees

- project assets have stable UUID identity independent of current path;
- the project-root registry is durable and transactionally recoverable;
- imports can record a durable source/product recipe without absolute-path
  identity;
- later source/product changes are observable as provenance state rather than
  silently rewritten;
- uniquely moved/missing sources can retain identity deterministically; and
- these behaviours survive packaged multi-process Project Open/reopen proof.

## What LC01 intentionally does not do

LC01 does not:

- execute first import or reimport;
- copy/cook creator content as a general asset pipeline;
- overwrite a stale imported product automatically;
- provide a creator-facing registry/provenance UI;
- make the existing filesystem Asset Browser registry-backed;
- create thumbnails/previews;
- package a standalone game; or
- modify Wicked.

Those are downstream responsibilities. LP07 — Reusable Project Asset Workflow
is the next bounded programme that consumes LC01 to provide an actual creator
import/browse/reimport lifecycle.

## Final verdict

**LC01: COMPLETE AND ACCEPTED.**
