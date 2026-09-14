# Character workflow implementation progress

## Recovery authority

- **Authoritative specification:** `docs/RENEGADE_CHARACTER_WORKFLOW_IMPLEMENTATION_SPEC.md`
  (SHA-256: `dff426869be84ddeecbb98511d96126bbf9a4fa6d83bb6a588767a5c536d43d5`).
- **Programme branch:** `feature/character-workflow-programme`.
- **Accepted starting baseline:** `cc31ef318ec55db5d115b48a0242665ce571c561`
  from AI-05 PR #158, based on Phase 7 baseline
  `d36918878776d0d91e0c39f88f6764a1926a6534`.
- **Latest pushed checkpoint:** pending first CW-00 checkpoint.

## Gate state

| Gate | Status | Notes |
| --- | --- | --- |
| CW-00 | In progress | Remote state and workflow triggers audited; source checklist audit in progress. |
| CW-01 to CW-08 | Not started | No Character workflow implementation code has been changed. |

## Validation and known state

- AI-05 PR #158 head `cc31ef3` is accepted by the owner.
- All four required PR checks completed successfully on 2026-09-14:
  Windows Debug, Windows Release, Renegade Studio Debug, and Renegade Studio
  Release.
- Current workflow triggers were inspected. Pushes to this branch do not run
  the expensive Windows matrix; PRs targeting `main` do. This branch must not
  be renamed into `agent/**`.
- No local build has been run for the Character workflow yet.

## Required source audit

The current sources named in the specification are being inspected from the
accepted AI-05 head, alongside the discovered reusable-asset placement,
reimport, prefab and duplication services. No historical snapshot is being
used as implementation authority.

## Exact next task

Finish the CW-00 source and ownership audit, commit and push this recovery
checkpoint, then begin CW-01 by adding the persisted `Model` versus
`Character` import designation and Character asset classification while
preserving generic Model import.
