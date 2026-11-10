# Renegade engineering handoff

**Current reconciled baseline:** main `618b2310ec7d6a7889d490e93304e39043f7e5ff`, 10 November 2026.
**Wicked pin:** `82557d439a41d104f559b0d3134d160b7bf6154e`.
The prefab/selection branch has been merged as PR #193. Lighting/particles were
already merged as PR #192. There is no unresolved integration conflict between
those streams. Do not restart prefab implementation from an old local checkpoint.

## Completed latest work

PR #193 feature head `cb590f06267e3407633d4e8f68bbde9265dd6ec1` includes ordered
multi-selection, Shift-toggle in hierarchy/viewport, directional box selection,
shared-pivot transforms, batch duplicate/delete, Group/Edit Pieces/Unpack and
native reusable scene assemblies. Scripted prefabs retain typed properties,
resources and modules; placement assigns fresh instance identities and remaps
internal references. Delete/duplicate/Unpack use existing history/companions.
Packaged load preserves authored instance edits rather than replacing them.

Latest validation: Release Studio/Runtime/proof builds; 11 focused checks;
60 source contracts; all six earlier failures rechecked; original geometry proof;
complete functional brazier proof; actual Studio save/card drag/Unpack/Save/Reopen;
package-relative standalone render with four governed scripts started. Evidence:
[functional prefab receipt](docs/evidence/scene-prefab-functional/README.md).

Four pre-merge checks succeeded on `cb590f0`:
[Studio Debug/Release](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/42857306536) and
[Windows baseline Debug/Release](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/42857306652).
Owner approved merging and merged #193 on 10 November. This records source
integration and scoped acceptance, not every independent release gate.

## Current documentation task

Branch: `docs/current-main-reconciliation-20261110`, based on the main SHA above.
Scope: current summaries, roadmap, architecture orientation, feature ledger,
build/upstream guidance, module READMEs, active creator/assistant guides,
documentation index and stale status/encoding reconciliation. No engine code,
workflow behaviour, assets, executable outputs or submodule pointer changes.

The documentation commit uses `[skip ci]` at the owner's explicit request.
Run local documentation, source-contract and assistant trust checks; do not
dispatch CI. Skipped required checks can remain pending. Do not change branch
protection or manufacture a green result to merge documentation.

## Boundaries and known follow-ups

- Placed prefab copies are independent; linked updates/override management are
  not implemented. External entity targets must be included or cleared.
- No explicit prefab entity-count cap; large castle assemblies are untested.
- Plugin/marketplace/third-party shader-editor discussions are proposed work,
  not implemented features or an instruction to begin them.
- Preserve existing castle assets, owner worktrees, private projects and backups.
  The separate prefab and picker test projects are not distributable releases.
- Preserve the pinned Wicked gitlink. Main's CMake-generated sky/weather,
  allocator and particle overlays are documented core-patch decisions, not a
  reason to commit a dirty submodule or reset another checkout.
- Main/alpha distribution and future merges require the owner's approval.
  No fresh alpha distribution is authorized here.

## Working context and history

Read [CURRENT_STATUS](docs/CURRENT_STATUS.md), [documentation index](docs/README.md),
[ROADMAP](docs/ROADMAP.md), [ARCHITECTURE](docs/ARCHITECTURE.md) and the relevant
[FEATURE_MATRIX](docs/FEATURE_MATRIX.csv) rows before acting.
The [complete pre-reconciliation handoff](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/blob/618b2310ec7d6a7889d490e93304e39043f7e5ff/HANDOFF.md) preserves
the earlier commands, failures, snapshots and checkpoints. Dated evidence remains
in the repository. Historical “no merge”, “CI pending” and machine observations
apply to their original checkpoint, not automatically to today's main.
