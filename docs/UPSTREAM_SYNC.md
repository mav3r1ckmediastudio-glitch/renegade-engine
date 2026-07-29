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

## Attachment comparison

The original planning analysis used `WickedEngine-master(1).zip`. A file-level
comparison with this Git pin is still outstanding because that attachment is
not present in the current workspace. Do not claim the ZIP and pin are identical
until the comparison is rerun and recorded.
