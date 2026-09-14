# Character workflow implementation progress

## Recovery authority

- **Authoritative specification:** `docs/RENEGADE_CHARACTER_WORKFLOW_IMPLEMENTATION_SPEC.md`
  (SHA-256: `dff426869be84ddeecbb98511d96126bbf9a4fa6d83bb6a588767a5c536d43d5`).
- **Programme branch:** `feature/character-workflow-programme`.
- **Accepted starting baseline:** `cc31ef318ec55db5d115b48a0242665ce571c561`
  from AI-05 PR #158, based on Phase 7 baseline
  `d36918878776d0d91e0c39f88f6764a1926a6534`.
- **Latest pushed checkpoint:** `ae002b53f6d8fa0f10693b28d4cdeb3271677f71`
  (CW-00 recovery authority).

## Gate state

| Gate | Status | Notes |
| --- | --- | --- |
| CW-00 | Complete | Live branch, PR #158 checks, workflow triggers and the import/asset/placement architecture were audited. |
| CW-01 | In progress | Persisted `Model` / `Character` import designation and Character-folder classification are implemented locally. |
| CW-02 to CW-08 | Not started | No implementation has begun for these gates. |

## Validation and known state

- AI-05 PR #158 head `cc31ef3` is accepted by the owner.
- All four required PR checks completed successfully on 2026-09-14:
  Windows Debug, Windows Release, Renegade Studio Debug, and Renegade Studio
  Release.
- Current workflow triggers were inspected. Pushes to this branch do not run
  the expensive Windows matrix; PRs targeting `main` do. This branch must not
  be renamed into `agent/**`.
- `git diff --check` passed for the CW-01 change set.
- Local native compilation is currently blocked because this Linux workspace
  does not have `cmake`; no compilation success is claimed.

## Required source audit

The current sources named in the specification were inspected from the
accepted AI-05 head, alongside the discovered reusable-asset placement,
reimport, prefab and duplication services. CW-01 intentionally reuses the
existing governed import recipe, `CreatorAssetWorkflowService` transaction and
the Asset Browser's existing `Content/Characters` classification rather than
introducing a second import or asset system.

## Exact next task

Finish CW-01: update the Character import presentation/copy and add the
source-contract coverage needed to prove the normal importer never silently
falls back to `Content/Models` after the Character selection. Then commit and
push the gate checkpoint before beginning CW-02 animation ingestion.
