# Wicked Upstream Sync

## Current pin

| Field | Value |
|---|---|
| Fork | `https://github.com/mav3r1ckmediastudio-glitch/WickedEngine.git` |
| Official upstream | `https://github.com/turanszkij/WickedEngine.git` |
| Branch | `renegade-rocky-riverbed-v1` |
| Commit | `82557d439a41d104f559b0d3134d160b7bf6154e` |
| Pin authority | PR #188, merged main `f64d31977ccc073c626d42a8973dbfa37acb3eed`; retained through PR #193 |
| Prior river pin | `ceb816b844b4ab2fa56a159ac60a4f65c931bdfc` (PR #186/#187) |
| Earlier official baseline | `3a800b7134aafe58461093c8abb2e274d4e64033` |
| Renegade integration date | `2026-11-01`; reconciled `2026-11-10` |
| Integration | Pinned Git submodule at `/WickedEngine` |

The current gitlink and Tools/Windows-Build.Common.ps1 agree on 82557d4.
The previous river-only and official pins below are historical.

## Reproducible overlays are separate from the pin

CMake applies documented Renegade overlays for sky/weather and the shared
allocator/particle authoring paths. Relevant decisions include
[shared allocator](SHARED_ALLOCATOR_CORE_DECISION_2026-11-08.md),
[particle fields](PARTICLE_MOVEMENT_FIELDS_CORE_DECISION.md),
[lifetime](PARTICLE_LIFETIME_CORE_DECISION.md) and
[shapes/timing](PARTICLE_SHAPES_TIMING_CORE_DECISION.md).
The patch scripts/files in the current CMake graph are the reproduction authority.
No feature documentation update authorizes arbitrary upstream source changes.

Shared Windows build-script guards expect a clean tracked submodule before
starting; CMake can then generate the approved patch state. Do not repeatedly
reset patched owner worktrees. Use an isolated clean build checkout and preserve
all existing work. A dirty generated source tree is not a changed gitlink.
See [BUILD_WINDOWS](BUILD_WINDOWS.md).

## Phase-boundary sync procedure

1. Keep the current Wicked commit pinned during an active phase.
2. Fetch official upstream at the phase boundary.
3. Create `integration/upstream-YYYY-MM-DD`.
4. Review release notes, archive versions, scene components, bindings, shaders,
   third-party notices, and build requirements.
5. Update the submodule on the integration branch.
6. Reapply and review every documented core patch.
7. Run build, serialization, visual, and performance suites.
8. Update `docs/FEATURE_MATRIX.csv` for added or changed capabilities.
9. Obtain independent verification.
10. Merge and record the new SHA and compatibility notes here.

## Core-patch rule

Prefer `EngineBridge` changes. When a Wicked source change is unavoidable,
record:

- Why an adapter cannot solve it.
- Exact upstream files changed.
- Protecting tests.
- Expected future conflict area.
- Whether the patch should be proposed upstream.

## Historical official baseline currency (July 2026)

Upstream `master` was fetched on 2026-07-30 during a grid investigation and
resolved to `3a800b7134aafe58461093c8abb2e274d4e64033` — the exact commit
pinned here. That July observation describes the previous official baseline; the active
river fork pin is recorded above.
Recheck at the next phase boundary rather than assuming drift.

## Do not trust third-party indexes of Wicked

`https://deepwiki.com/turanszkij/WickedEngine` indexes a different, older commit
than this pin. During Phase 3 it described a grid implementation with a cached
vertex buffer and a `gridVertexCount` symbol that does not exist in our tree,
and its line numbers were several hundred lines out. It cost a round of
investigation before the discrepancy was spotted.

Such indexes may help navigate the architecture, but the pinned submodule is
the only authority. Verify every claim about Wicked against
`/WickedEngine` at the pinned SHA before acting on it.

## Attachment comparison

The original planning analysis used a `WickedEngine-master(1).zip` attachment
that is no longer present. That comparison is obsolete: the Git submodule at
the pinned SHA is now the sole baseline of record and the ZIP has no standing.

## Historical Weather V2 integration note (2026-11-01)
The upstream baseline above remains unchanged. The Renegade fork checkpoint for the owner-approved Weather V2 candidate is c0cb547b6e5f2f8b3587ba4b30d0fe5fc704d3ee. Core-patch rationale and verification are recorded in WEATHER_V2_G4_CRITICAL_REPAIR_CORE_DECISION_2026-10-30.md and the associated performance decision. This checkpoint is local and requires approved publication before fresh remote CI can fetch it. No upstream sync is implied.

Native Windows solution CI correction: fork checkpoint 82557d439a41d104f559b0d3134d160b7bf6154e adds direct wiArguments.h includes in wiRenderer.cpp and wiScene.cpp. CMake PCH had supplied these declarations implicitly; the native solution requires explicit includes. No function or shader behaviour changes. Both build paths remain required.
