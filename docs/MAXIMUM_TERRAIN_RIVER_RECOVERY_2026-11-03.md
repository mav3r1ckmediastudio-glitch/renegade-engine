# Maximum terrain and river recovery — 3 November 2026

Status: native hardware and bounded software checks pass; actual Studio
acceptance is in progress. Packaging remains paused. Fresh CI, exact-artifact
verification and owner release approval remain open.

The owner reproduced the 19x19 resident-terrain guard when linking a river on
a terrain already expanded to 21x21. Earlier CI proved default-terrain linking
and separate unlinked expansion. It did not test their combination. The owner's
new requirement is the supported maximum 33x33 / 1,089 chunks in both orders.

## Implementation

- Validate complete finite fixed terrain at every supported radius through 16.
- Read legacy v1/v2 and use sparse sample schema v3 for expanded coverage or
  more than 81 touched chunks. Keep the equivalent 81-grid sample budget,
  32 MiB payload, 32-layer and 32M composition-work bounds.
- Preserve old coverage hashes/transforms before trusted one-ring expansion.
  Generate the new native ring and extend/recompose coverage transactionally.
- Retain removed ring entities, chunk data and native serialization through
  Undo/Redo so subsequent command entity targets remain valid.
- Preserve the absolute generator function after explicit floor rebasing.
- Support rocky bed paint at maximum extent with spatial segment selection
  under the original worst-case work budget.

Fresh terrain retains its identity root, world-zero surface and -20..120 m
range. Existing levels receive no automatic migration.

## Required acceptance

| Combined workflow | Current result |
| --- | --- |
| Expand fresh terrain fully, draw/link shallow river without Allow Deeper Beds | Native PASS; actual Studio PASS at 1,089 chunks |
| Link before expansion, expand through all seven rings | Native PASS; actual Studio pending |
| Three long rivers touch more than 81 chunks at maximum extent | Native PASS in both orders |
| Linked river width/move, subsequent outer sculpt and expansion Undo/Redo | Native PASS in both orders |
| Existing painted masks survive expansion; maximum rocky/grass paint history | Native PASS in both orders |
| Inner bed, outer bed and outer sculpt Jolt contact | Native PASS in both orders |
| Maximum save/hydration and native first updates preserve content | Native PASS in both orders |
| Player Start preview rebuilds retain every source atlas binding | Native PASS in both orders |
| Actual Studio screenshots, Test Game, standalone and cold process reopen | Pending |
| Fresh CI and exact downloaded artifact user journeys | Pending |

New native regression modes are maximum-expanded-first and maximum-linked-first.
They generate the complete maximum native terrain and retain bounded 600-second
CTest deadlines. They do not substitute smaller terrain or fabricated GPU data.
Hosted source and preview rasterization are partitioned between the two cases:
expanded-first captures the source, linked-first captures a real preview. Each
case retains all data assertions, three preview rebuilds, twelve native updates
per rebuild and all 4,356 source atlas binding checks. Desktop presentation keeps
its existing paired rendering path.

Both ordinary native hardware cases passed before this final draw scheduling
adjustment. The final real DXGI WARP cases, each scoped to four logical
processors, passed in 503.992 and 509.036 seconds with the unchanged 600-second
limit. Earlier combined source/preview cases timed out and remain failed audit
receipts; they are not counted as acceptance.

Actual Studio created a new disposable project, generated untouched terrain at
world Y=0, expanded all seven rings to 2.18 x 2.18 km, drew a width-8 river and
linked its default-depth bed without Allow Deeper Beds. A saved flat-state
inspection confirms 1,089 chunks, root Y=0, range -20..120 m and all samples
within 0.003 m of zero. The second operation order, game/runtime traversal and
full process reopening remain open at this checkpoint.
The existing default initialization, river and preview regressions remain.

## Preservation and release state

All 145 owner original/backups match their initial hashes at the during-build
checkpoint. Inherited imnodes work is unchanged. The owner's unsaved Studio
session is preserved. Audit receipts and screenshots are retained externally
under MAX_TERRAIN_REPAIR_20261103.

The partial pre-alpha stage is preserved and packaging remains stopped.
Main is unmerged. Owner approval is required before merge or alpha distribution.
