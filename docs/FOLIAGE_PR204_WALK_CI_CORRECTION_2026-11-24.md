# PR 204: resident grass-walk fixture correction

Date: 24 November 2026. Failed head:
`fe1d8c688ac6750b6bfbbf462149b64194a0554d`; tested merge:
`1623d8b222b6461f818bb2242477515496f10c2d`.
Correction: `cbdbf59ec9d3e871d5696d313fd6da838d3b9320`.

## Hosted result and reproduced cause

Studio run 45470313728: Release job 138716182258 passed the full build,
registered tests and startup. Debug job 138716182682 failed only
`RenegadeVegetation_grass-palette` with `Walking stopped for a resident terrain
tile` (216.69 s). Both baseline jobs passed. The previously corrected assistant
workflow suite passed both Studio configurations.

Instrumented the existing native walking proof, then called that same function
repeatedly on freshly loaded copies of its saved scene, using Debug and real
WARP. Run 4 reproduced the exact failure at frame 139, Z -6.37132:
collision requests grew from four ready cells to six required cells; the two
new Z=1 records were not yet published, while native terrain generation was busy.
The runtime correctly held simulation for unknown collision support. The test
had generated nine terrain chunks but had not prepared the complete measured
route's collision interests. Its uninterrupted-resident-walk assertion therefore
also depended on asynchronous streaming admission timing.

## Correction and retained assertions

`Tests/GrassPaletteProof.h` pins the existing radius-one fixture through a scoped
native collision interest. Before the measured walk it requires all nine real
collision cells ready, driving normal native publication with simulation held.
Preparation is bounded by 1,800 iterations and a 30-second deadline. The interest
is removed on scope exit, including exceptions.

Every existing movement, camera-relative arm continuity and final-distance
assertion remains. The final stale-collision check now additionally requires a
live old body and exactly one blocked cell: nine required, eight ready, with the
invalidated cell still unpublished. Collision failure diagnostics identify the
frame, requested cells and native body state. They do not inspect a concurrently
mutating terrain chunk inventory.

No engine, renderer, shader, source fingerprint, workflow policy, timeout,
upstream pointer, importer, atlas or gameplay behavior changes. This fixture
measures a prepared resident route; the runtime's streaming hold remains intact.
Existing streaming, readiness, teleport and invalidation tests remain registered.

## Local verification

- A focused native cold-load harness calling the original proof failed on run 4;
  the corrected proof passed 20 consecutive cold loads, including all original
  walking checks and all 20 strengthened stale-collision refusals.
- Complete registered grass proof via `Tests/RunRiverDefaultRecovery.cmake`,
  `RENEGADE_RIVER_MODE=grass-palette`, `RENEGADE_HOSTED_HEADLESS_GRAPHICS=1`:
  Debug WARP exit 0 in 67.365 s; Release WARP exit 0 in 57.774 s.
- Both complete runs report native GPU atlas variation, cursor/player vertex
  response, nine-cell prepared walking, eight-of-nine stale-tile refusal, all
  eight stock profiles/overlap/settings/ground/history/reopen, and Custom palette
  mix/import/history/cold-reopen passes.
- `cmake -DRENEGADE_SOURCE_DIR=<repo> -P Tests/WD01VegetationSourceContract.cmake`:
  exit 0. All 161 assistant workflow evidence records retain their exact needles
  and whole-file digests. `git diff --check`: pass.

Native builds recompiled the changed bridge sources and complete proof against
matching cached Windows dependencies in each configuration (Debug /Od /RTC1
/MDd, Release /O2 /MD). Real WARP selection uses the existing test-only adapter
configuration from `cmake/RenegadeWarpTestDevice.cmake`; production selection is
unchanged. These are bounded local builds, not a full clean Studio build.
Final tested proof header raw SHA-256:
`2c15235ff6536831269b904828f988479b5453b24c94bc3ec12869924cbd7757`.

Commands in the owner QA workspace: `build-ci-final.cmd`, with the corrected
Release relink `link-ci-Release.cmd`; `QAColdWalk.exe <saved-proof-scene>`;
`cmake -DRENEGADE_RIVER_PROOF=<configuration-proof-exe> -DRENEGADE_RIVER_OUTPUT_ROOT=<fresh-output> -DRENEGADE_RIVER_MODE=grass-palette -P <repo>/Tests/RunRiverDefaultRecovery.cmake`.

Portable reproduction and final complete-run logs are under
`docs/evidence/foliage-ci-walk-2026-11-24/`. Full scratch build scripts and native
captures remain in the owner QA workspace. No owner application was closed or
restarted for this correction.

Next: evaluate fresh full hosted Debug/Release checks on the latest pushed head.
No merge, auto-merge or release approval is claimed.
