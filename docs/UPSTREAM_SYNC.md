# Wicked Upstream Sync

## Current pin

| Field | Value |
|---|---|
| Upstream | `https://github.com/turanszkij/WickedEngine.git` |
| Branch | `master` |
| Commit | `3a800b7134aafe58461093c8abb2e274d4e64033` |
| Commit subject | `Fix UI flicker in env probe panel (#1687)` |
| Upstream commit date | `2026-07-24` |
| Renegade snapshot date | `2026-07-29` |
| Integration | Pinned Git submodule at `/WickedEngine` |

The submodule provides the complete Wicked source and history while keeping the
Renegade layers separate. Before the first unavoidable Wicked core patch, create
a dedicated Wicked fork and repoint the submodule to that fork. Keep the
official repository configured as its upstream.

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

## Pin currency

Upstream `master` was fetched on 2026-07-30 during a grid investigation and
resolved to `3a800b7134aafe58461093c8abb2e274d4e64033` — the exact commit
pinned here. The pin is current, not stale, and there is nothing to sync.
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
