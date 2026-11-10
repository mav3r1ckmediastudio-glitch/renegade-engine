# Maximum terrain and river recovery — 3 November 2026

> **Current-main reconciliation — 10 November 2026:** Maximum-terrain recovery is integrated through #189. Its 33×33 coverage and existing transaction/work/payload budgets remain; this receipt is not a performance guarantee. See [CURRENT_STATUS](CURRENT_STATUS.md).

Status: product native and actual Studio checks pass on DESIGN with the
interaction-latency qualification below. Local validation of the hosted CI
scheduling repair passes; fresh CI is the next gate. Packaging remains paused; fresh CI, exact-artifact
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
| Link before expansion, expand through all seven rings | Native PASS; actual Studio reaches 1,089 chunks |
| Three long rivers touch more than 81 chunks at maximum extent | Native PASS in both orders |
| Linked river width/move, subsequent outer sculpt and expansion Undo/Redo | Native PASS in both orders |
| Existing painted masks survive expansion; maximum rocky/grass paint history | Native PASS in both orders |
| Inner bed, outer bed and outer sculpt Jolt contact | Native PASS in both orders |
| Maximum save/hydration and native first updates preserve content | Native PASS in both orders |
| Player Start preview rebuilds retain every source atlas binding | Native PASS in both orders |
| Actual Studio screenshots, Test Game, standalone and cold process reopen | PASS on DESIGN; exact CI artifact still pending |
| Fresh CI and exact downloaded artifact user journeys | Pending |

New native regression modes are maximum-expanded-first and maximum-linked-first.
They generate the complete maximum native terrain. Release and GPU checkpoints
retain 600-second deadlines; Debug native cases receive 1,200 seconds. They do not substitute smaller terrain or fabricated GPU data.
Hosted native transactions run in those two cases. Two fixture-dependent GPU
cases then cold-load their exact saved scenes: source rendering consumes the
expanded-first fixture and the preview consumes the linked-first fixture.
Every GPU case retains all three preview rebuilds, twelve native updates per
rebuild and all 4,356 source atlas binding/height checks. Each process pays for
one real full-resolution GPU checkpoint. The source checkpoint draws after
all three preview rebuilds. Desktop native cases retain paired rendering.

CTest fixture dependencies rerun native producers and skip GPU consumers when
a producer fails. The wrapper checks the saved scene and proof executable
SHA256 before launch, rejecting stale or changed input. Prior run directories
and immutable run-local manifests remain preserved; only a consumer pointer is
updated after a producer exits successfully.

Both ordinary native hardware cases passed before this scheduling repair.
The earlier combined real DXGI WARP cases, each scoped to four logical
processors, passed in 503.992 and 509.036 seconds with the unchanged 600-second
limit. Earlier combined source/preview cases timed out and remain failed audit
receipts; they are not counted as acceptance.

Actual Studio created a new disposable project, generated untouched terrain at
world Y=0, expanded all seven rings to 2.18 x 2.18 km, drew a width-8 river and
linked its default-depth bed without Allow Deeper Beds. A saved flat-state
inspection confirms 1,089 chunks, root Y=0, range -20..120 m and all samples
within 0.003 m of zero. The second operation order, game/runtime traversal and
full process reopening subsequently passed, as recorded below.
The existing default initialization, river and preview regressions remain.

## Preservation and release state

All 145 owner original/backups match their initial hashes at the during-build
checkpoint. Inherited imnodes work is unchanged. The owner's unsaved Studio
session is preserved. Audit receipts and screenshots are retained externally
under MAX_TERRAIN_REPAIR_20261103.

The partial pre-alpha stage is preserved and packaging remains stopped.
Main is unmerged. Owner approval is required before merge or alpha distribution.


## Application resource staging follow-up

Actual output inspection found that the supplied rocky material files were
present in source and the native proof deployment but absent from the normal
Studio and Runtime output directories. The earlier pre-alpha base ZIP also
lacked them. This was a packaging gap, not a new terrain-height failure.

Studio and Runtime now stage the complete supplied rocky material directory
through one shared CMake script. The new RenegadeRockyRiverbedDeployment test
compares every required output file with source and rejects non-PNG placeholders.
It failed against the old missing outputs, then passed after normal staging.
The files are unchanged supplied assets; no executable or terrain C++ change
is part of this resource follow-up. Final CI must validate the updated head.

The complete preceding local Release suite passed 312/312 with zero skips in
537.65 seconds. Both maximum-terrain desktop cases passed in 122.01 / 123.49 s.
The additional application-output resource check passed in 0.32 s after staging.

Actual Studio completed link-first expansion through all seven rings to the
2.18 x 2.18 km maximum. The last six ring transactions took 19.020 / 21.273 /
24.010 / 28.621 / 30.936 / 35.190 seconds in this ordinary executable session.
These are observed operation waits, not a performance improvement claim.
Actual rocky painting and its Undo/Redo passed. Test Game and standalone
Runtime crossed the maximum-size scene's channel. Full Studio process
reopening retained textured terrain and Player Start preview; re-save/native
updates preserved every chunk height hash, grass/paint hash and river-layer
hash/bytes exactly. Lower sculpt on the reopened scene, saved Undo and saved
Redo passed exact comparisons. Jolt sample errors were 0.64 mm at the bed and
0.26 mm at the most-lowered sculpt vertex. No release-wide PASS is claimed.

The sculpt automation had a prolonged hold/settle with the ordinary image's
existing heap verifier active; an explicit synthetic pointer release was
needed. Final stroke history reported one tile / 43 ms. This is not an
ordinary-input performance measurement or a responsiveness pass. No OS heap,
graphics or pinned native source settings were changed.


## Hosted CI timeout repair

Run 41699012815 Debug completed maximum expansion, all three linked rivers,
patch rejection, transaction history, rocky/grass painting and Jolt contacts.
Native work reached source-render at about 477 seconds in each case. The
expanded-first case timed out during its full-resolution WARP source draw;
linked-first passed cold hydration and timed out during its actual preview
draw. Both hit the unchanged 600-second per-test limit. These remain failed
receipts; earlier local Release passes did not establish Debug timing margin.

Separating native transactions and cold GPU checkpoints removes their combined
CPU/rasterization cost from one deadline without shrinking terrain, dropping
assertions, skipping GPU readback or changing product code. The same run's
Release job reached the aggregate 120-minute job deadline while CTest was still
active. The Studio job ceiling is now 180 minutes to admit cold compilation,
the full suite and startup/package evidence; individual tests remain bounded
at 600 seconds. This ceiling does not promise or measure a faster CI run.


## Local scheduling-repair validation

The rebuilt test driver passed all four split cases on native NVIDIA hardware
and real DXGI WARP. WARP was scoped to four logical processors; hardware used
separate processors. All three preview rebuilds, twelve updates per rebuild
and 4,356 source bindings were checked in each GPU case. Source/preview PNGs
were inspected. Individual process limits stayed at 600 seconds.

| Checkpoint | Hardware seconds | WARP seconds |
| --- | ---: | ---: |
| maximum-expanded-first-expanded-first | 159.911 | 146.136 |
| maximum-render-expanded-first--2 | 13.503 | 346.063 |
| maximum-linked-first-linked-first | 116.245 | 117.141 |
| maximum-render-linked-first-1 | 12.15 | 309.956 |

All 63 selected source/workflow/resource checks passed, as did the existing
default-recovery and four initialization/preview modes with the changed driver.
The fixture launcher refused missing/malformed manifests, changed scenes and
changed proof executables before launch. CTest registration retains both
producer/consumer fixture dependencies and all four 600-second deadlines.

Local driver-only validation reused the unchanged prebuilt Release bridge;
it does not replace an exact-commit full Debug/Release CI build. The aborted
local metadata-only bridge rebuild and both failed hosted job logs remain as
raw evidence. No application source, pinned dependency, OS graphics/heap
setting, terrain size, render resolution or product safety budget changed.

Receipts and exact commands are retained externally under
CI_TIMEOUT_REPAIR_20261103T2056Z. Fresh CI and exact downloaded-artifact
acceptance remain open. No release gate is closed by these local checks.


## Hosted Debug native-work budget correction

Run 41722770055 Release completed successfully. Debug again exceeded 600
seconds in the native maximum-terrain transaction checks, before rendering;
the dependent GPU cases were correctly withheld. Local Release timings did
not establish sufficient hosted Debug timing margin. The two native maximum
cases now receive 1,200 seconds only in Debug. Release and GPU cases keep
600 seconds. Every extent, assertion, fixture hash and product safety bound
is unchanged. The aggregate 180-minute job ceiling remains in place.
This is a test-execution budget correction; it is not a product-performance
fix, a skip or a claim that Debug has passed. The new CI result remains the
acceptance gate.
