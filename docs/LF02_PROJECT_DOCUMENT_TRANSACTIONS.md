# LF02 — Project Document Transactions

## Acceptance

**Status:** PASS WITH LIMITATIONS  
**Date:** 2026-08-06  
**Branch:** `poc/lf02-project-document-transaction`  
**Accepted implementation commit:** `c58a3ddbcfea492c7e86489546d25c8779abb229`  
**Pull request:** [#20](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/20)

## Automated test evidence

| Run | Configuration | Result |
|---|---|---|
| [31131260961](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/31131260961) | Renegade Studio (Debug + Release) | **SUCCESS** |
| [31131262731](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/31131262731) | Windows baseline | **SUCCESS** |

**Full automated test result:** 19 / 19 passing (0 failures).

## Scope

LF02 introduces a UI-free, project-agnostic multi-document disk transaction boundary for Renegade-owned project documents. It replaces the primitive direct-file-write paths used by LF01 identity and LP02/LP03 document writers with a durable, recoverable, containment-enforced transaction core.

### What is included

- **Transactional multi-document commit core** (`ProjectDocumentTransaction` in `EngineBridge/src/ProjectDocumentTransaction.cpp`)
- **Deterministic same-directory staging and replacement** — staged, backup, restore and journal files live beside their destination so the final replacement remains same-volume
- **Durable journal and interrupted-transaction recovery** — a journal with explicit phase metadata permits `Recover()` to finish rollback or cleanup after interruption, power loss or crash
- **Rollback after partial replacement failure** — any commit failure rolls earlier replacements back via backup → restore → atomic replace
- **Project-root containment** — `allowedRoot` rejects destinations, journal directories and recovery artifacts that escape the project boundary
- **Transactional `.renegade` project descriptor writes** — `ProjectService::WriteProject` now routes through the transaction boundary
- **Transactional legacy project-ID migration** — `ProjectService::OpenProject` migrates v1 descriptors through a transactional write with retained last-good backup
- **Transactional document-envelope writes** — `WriteDocumentEnvelope` is now a thin wrapper over `WriteTransactionalDocument`
- **Transactional Story Flow writes** — `FlowService::WriteFlowDocument` rebuilds from a clean render file with pre-commit validation and post-replacement round-trip verification
- **Transactional Runtime Screen writes** — `ScreenService::WriteScreenDocument` uses the same transactional boundary
- **Pre-commit and post-replacement validation** — staged files are validated before commit; committed files are validated after replace with automatic rollback on mismatch

### What is NOT included (recorded limitation)

> **Sole limitation:** editor-owned Flow and Screen dirty-state tracking is deferred until mutable editor document models and authoring workspaces exist.
>
> There is currently no Studio UI to author Story Flows or Runtime Screens, so there is no live in-memory document model whose dirty flag would need to synchronize with the transactional disk boundary. When those workspaces are built, dirty-state tracking, unsaved-change prompts, and incremental auto-save will be added as a follow-on slice. The transaction boundary itself is fully capable of supporting them.

## Implementation commits

| Commit | Description |
|---|---|
| `f05f964e` | Add LF02 project document transaction core |
| `c9815202` | Integrate LF02 project descriptor transactions |
| `63a9cd01` | Integrate LF02 transactional document writers |
| `868e4f41` | Trigger LF02 CI |
| `1754aa9a` | Fix LF02 Windows compilation |
| `f311131f` | Fix LF02 transactional document reads |
| `c58a3ddbcfea492c7e86489546d25c8779abb229` | Fix FlowTests fixture pollution from LF02 transactional-update block |

## Test coverage

### `RenegadeProjectDocumentTransactionTests` — 16 cases

Proves the transaction core in isolation (no Renegade services, no Wicked, no graphics device).

| Test | What it proves |
|---|---|
| `TestSuccessfulDeterministicCommit` | Two documents commit in deterministic path order; no artifacts remain |
| `TestNoOp` | Identical content is validated but not rewritten; reported as `noChanges` |
| `TestPreparationFailure` | Pre-commit hook failure aborts before any file is touched |
| `TestJournalFailure` | Journal creation failure aborts before staging |
| `TestStagingFailure` | Staged write failure triggers cleanup, not rollback |
| `TestValidationFailure` | Validator rejection aborts before backup/replace |
| `TestBackupFailure` | Backup copy failure aborts before any replacement |
| `TestFirstReplacementFailure` | First replacement failure rolls back zero prior replacements |
| `TestLaterReplacementFailure` | Second replacement failure rolls back the first replacement |
| `TestRollbackFailureThenRecovery` | Rollback failure retains journal; `Recover()` completes rollback |
| `TestInterruptedCommitRecovery` | Simulated interruption after first replace retains journal; `Recover()` restores both originals |
| `TestNewDestinationRollback` | New file creation failure removes the new file and restores the existing one |
| `TestPostReplaceValidationRollback` | Post-replace corruption is detected and rolled back |
| `TestCommittedCleanupRecovery` | Committed transaction with failed cleanup retains journal; `Recover()` finishes cleanup without changing committed content |
| `TestAllowedRootContainment` | Destinations and journal directories outside `allowedRoot` are rejected |
| `TestMalformedJournalRejected` | Garbage journal files are rejected without touching unrelated content |

### `RenegadeProjectServiceTransactionTests` — 4 cases

Proves `ProjectService` integration with the transaction boundary.

| Test | What it proves |
|---|---|
| `TestTransactionalLegacyMigration` | Legacy v1 descriptor is migrated transactionally; previous descriptor is retained as `.bak`; no artifacts remain |
| `TestInterruptedOpenRecovery` | Interrupted descriptor write is recovered on next `OpenProject`; exact original bytes are restored; warning is surfaced |
| `TestFreshProjectHasNoPreviousBackup` | New project creation does not create a spurious `.bak` |
| `TestMigrationBackupFailurePreservesDescriptor` | Backup destination blocked by a directory causes migration to fail closed with original descriptor untouched |

### `RenegadeFlowTests` — transactional persistence

Proves Story Flow documents survive transactional write, backup creation, and round-trip reload.

- `WriteFlowDocument` commits through `WriteTransactionalDocument`
- Previous content is retained as `.bak`
- Updated content is authoritative after commit
- Identity survives file move
- Stable-ID path resolution repairs stale path hints
- Duplicate IDs are rejected

### `RenegadeScreenTests` — transactional persistence

Proves Runtime Screen documents survive transactional write, backup creation, and round-trip reload.

- `WriteScreenDocument` commits through `WriteTransactionalDocument`
- Previous content is retained as `.bak`
- Updated content is authoritative after commit
- Serialization is deterministic (byte-identical round-trip)
- Identity survives file move
- Stable-ID path resolution repairs stale path hints
- Duplicate IDs are rejected

## Files changed (implementation only)

- `EngineBridge/CMakeLists.txt`
- `EngineBridge/include/renegade/bridge/IdentityService.h`
- `EngineBridge/include/renegade/bridge/ProjectDocumentTransaction.h` *(new)*
- `EngineBridge/include/renegade/bridge/ProjectService.h`
- `EngineBridge/src/FlowService.cpp`
- `EngineBridge/src/IdentityService.cpp`
- `EngineBridge/src/ProjectDocumentTransaction.cpp` *(new)*
- `EngineBridge/src/ProjectService.cpp`
- `EngineBridge/src/ScreenService.cpp`
- `Tests/CMakeLists.txt`
- `Tests/ProjectDocumentTransactionTests.cpp` *(new)*
- `Tests/ProjectServiceTransactionTests.cpp` *(new)*
- `Tests/FlowTests.cpp`
- `Tests/ScreenTests.cpp`

## Protected unrelated local modification

`Tools/Windows-Build.Common.ps1` remains outside LF02 and was not staged, altered, reset or reverted.

## Wicked pin

Unchanged: `3a800b7134aafe58461093c8abb2e274d4e64033`

## Verification command

```bash
git log --oneline poc/lf02-project-document-transaction ^main
c58a3dd Fix FlowTests fixture pollution from LF02 transactional-update block
f311131 Fix LF02 transactional document reads
1754aa9 Fix LF02 Windows compilation
868e4f4 Trigger LF02 CI
63a9cd0 Integrate LF02 transactional document writers
c981520 Integrate LF02 project descriptor transactions
f05f964 Add LF02 project document transaction core
```
