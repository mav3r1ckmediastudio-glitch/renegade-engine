# Wicked Upstream Sync

## Current pin

| Field | Value |
|---|---|
| Fork | `https://github.com/mav3r1ckmediastudio-glitch/WickedEngine.git` |
| Official upstream | `https://github.com/turanszkij/WickedEngine.git` |
| Branch | `renegade-rocky-riverbed-v1` |
| Commit | `ceb816b844b4ab2fa56a159ac60a4f65c931bdfc` |
| Pin authority | PR #186, merged main `150eca4b761d3f62f122df3ab5e8536ec084f926` |
| Prior official baseline | `3a800b7134aafe58461093c8abb2e274d4e64033` |
| Renegade integration date | `2026-10-28` |
| Integration | Pinned Git submodule at `/WickedEngine` |

The submodule now uses the river-enabled Renegade fork accepted through PR #186.
Preserve this gitlink and fork URL during inventory integration. The previous
official baseline below is historical, not the active checkout authority.
See RIVER_G13E14_SINGLE_COMBINED_TEST_2026-10-28.md and the integration validation report.
Existing configure-time overlays remain separate from the pinned commit.

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
resolved to `3a800b7134aafe58461093c8abb2e274d4e64033` â€” the exact commit
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
