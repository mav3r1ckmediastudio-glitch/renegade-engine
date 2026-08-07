# LF02 — Project Document Transaction

## Proof boundary

LF02 replaces the direct, non-transactional `wi::config::File` commits that
LF01 explicitly left as a proof primitive (see "Deliberate limits" in
`docs/LF01_STABLE_IDENTITY_PROOF.md` and "Project metadata" in
`docs/ARCHITECTURE.md`) with a real disk transaction for the `.renegade`
project descriptor and Renegade-owned envelope documents. It does not
implement Story Flow or Runtime screen *authoring* UI, does not add a
project-wide multi-document Save command, and does not change WISCENE's own
save transaction (`SceneDocumentService`), which remains the separate,
already-accepted authority for scene documents.

`ProjectDocumentTransaction` is a UI-free, project-agnostic, format-agnostic
disk transaction primitive: same-directory staged writes, pre-commit
validation via a caller-supplied validator, previous-version backup,
deterministic path-ordered atomic replacement, rollback on partial failure,
a durable journal, and a separate `Recover()` path for resuming or reporting
an interrupted transaction on next open. `ProjectService::WriteProject`,
`WriteFlowDocument`, and `WriteScreenDocument` are now thin callers over this
primitive rather than direct `wi::config::File` commits.

## Required behaviour

- Every participating document is staged to a same-directory temporary file
  and read back through its real production reader before any destination is
  touched.
- An existing destination is copied to a protected backup before being
  replaced; a new destination has no previous-version backup to create.
- Replacement across multiple documents in one transaction happens in
  deterministic path-sorted order, not insertion order.
- Any failure before the first replacement leaves every destination
  completely untouched.
- Any failure after one or more replacements have committed rolls the
  already-committed replacements back to their previous content (or removes
  them, if they were newly created) before reporting failure.
- A durable journal records transaction state; an interruption between
  stages (including mid-rollback) leaves recoverable evidence rather than an
  ambiguous partial state, and `ProjectService::OpenProject` recovers it
  automatically on next open.
- Destinations and journal directories are contained to an explicit
  `allowedRoot`; an escaped path is rejected before anything is written.
- The `.renegade` project descriptor's one-shot legacy `project_id` backfill
  (introduced by LF01) now commits through this transaction and produces a
  real `.bak.renegade` of the pre-migration descriptor, rather than the
  direct, unprotected commit LF01 shipped with.
- Story Flow (`WriteFlowDocument`) and Runtime screen (`WriteScreenDocument`)
  documents commit through the same transactional path as the project
  descriptor, each producing a `.bak` of the pre-update document and making
  the new content authoritative only on full commit.

## Deliberate limits

**Sole limitation: editor-owned Flow and Screen dirty-state tracking is
deferred until mutable editor document models and authoring workspaces
exist.** `CommandService`'s existing undo-stack-based dirty model remains
correct and unchanged for the single WISCENE scene document it already
tracks. No Studio UI currently mutates a Story Flow or Runtime screen
document interactively — both are still produced and consumed as whole-file
units by tests, the LP02 flow interpreter, and the LP03 Runtime screen
loader — so a live per-document dirty flag has no authoring surface to
attach to yet. LF02 defines document identity, transactional write safety,
and last-good recovery correctly in the meantime; the dirty-state layer is
follow-on work for whichever slice introduces interactive Flow/Screen
editing in Studio.

Also out of scope, matching the LF01 proof boundary above and not claimed
here: cross-document duplicate/ownership checking via the existing
`ValidateDocumentEnvelopes` is not yet wired into the Flow/Screen write path
(no multi-document transaction currently assembles more than one envelope
document at a time); rolling numbered backups beyond the single promoted
`.bak` are not implemented for descriptor/envelope documents (WISCENE's
ten-deep rolling backup policy is unchanged and unaffected); and a
general multi-version migration chain is not built beyond making the one
existing LF01 migration step transactional, since `CurrentFormatVersion` has
never incremented past `1`.

## Automated evidence

Two new suites and extensions to two existing suites, all `int main()` /
sequential-check executables matching the established `Tests/` idiom, wired
into `Tests/CMakeLists.txt` and `RenegadeBridgeTests`' explicit CI build
dependency chain the same way as every prior slice.

### `RenegadeProjectDocumentTransactionTests` (16 checks)

Exercises `ProjectDocumentTransaction` in isolation against synthetic
documents, using its `operationHook` fault-injection seam rather than
filesystem permission tricks:

- deterministic path-sorted commit across multiple documents;
- no-op save (validator still runs; content unchanged);
- forced failure at every transaction stage — preparation, journal write,
  staged write, pre-commit validation, backup, first replacement, later
  replacement, and post-replacement re-validation — each proving the
  affected destination(s) are left byte-identical to their pre-transaction
  content;
- rollback of a newly-created (not pre-existing) destination on a later
  failure, proving it is removed rather than left as an orphaned file;
- a rollback-time failure retaining a durable journal and requiring a
  separate `Recover()` call, which then completes the rollback;
- a simulated process interruption after a partial commit, recovered by
  `Recover()` on the same journal;
- a committed transaction whose cleanup step fails, proving it is still
  reported as committed (not confused with a rolled-back transaction), with
  `Recover()` finishing cleanup afterward without touching content;
- `allowedRoot` containment rejecting both an escaped destination and an
  escaped journal directory before writing anything;
- a malformed/foreign journal file rejected by `Recover()` without touching
  unrelated content.

### `RenegadeProjectServiceTransactionTests` (4 checks)

Exercises the transaction through the real `ProjectService` production
caller, not synthetic fixtures:

- the LF01 legacy `project_id` migration now commits transactionally and
  retains an exact-byte `.bak.renegade` of the pre-migration descriptor,
  verified via `InspectProject` re-validation afterward;
- `ProjectService::OpenProject` recovers a descriptor transaction interrupted
  mid-replacement, restoring the exact original bytes and surfacing the
  recovery through `LastWarning()`;
- a blocked backup destination (an existing directory occupying the `.bak`
  path) fails migration cleanly, leaving the original legacy descriptor
  byte-for-byte unchanged and the project not activated;
- fresh project creation produces no spurious `.bak.renegade` (nothing to
  protect on first write) and the resulting descriptor round-trips through
  `InspectProject`.

### Flow and Screen persistence (`RenegadeFlowTests`, `RenegadeScreenTests`)

Both suites were extended with a transactional-update block proving the
same write/backup/reopen contract for their respective document types:

- `RenegadeFlowTests` renames a Story Flow node, commits the change
  transactionally, confirms the resulting `.bak` still holds the prior node
  name and the live file holds the new one, then reverts the change back to
  the base fixture (committing that revert to disk as well) so the
  downstream deterministic-interpreter assertions in the same file continue
  to exercise the documented fixture rather than the leftover rename. See
  the "Fix FlowTests fixture pollution" commit below for why the revert step
  matters.
- `RenegadeScreenTests` renames a Runtime screen widget's title text,
  commits transactionally, and proves the same backup/authoritative-update
  contract for `ScreenDocument`.

### Full suite result

`ctest -C <Debug|Release>` from `BUILD/renegade`: **19/19 passing**, run
locally in both configurations and confirmed independently by CI (below).
Before the fix described next, `RenegadeFlowTests` failed with
`game.start did not enter Level One` — a test-fixture bug (the
transactional-update block above left `document.nodes[1]` renamed and never
reverted it before later assertions reused the fixture), not a defect in the
transaction implementation itself. Fixed in commit
`c58a3ddbcfea492c7e86489546d25c8779abb229`.

## CI and packaged evidence

- **Accepted implementation commit:**
  `c58a3ddbcfea492c7e86489546d25c8779abb229`
- **Pull request:** #20, "Implement LF02 project document transactions"
- **Renegade Studio** workflow run
  [`31131260961`](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/31131260961):
  **SUCCESS** — Windows x64 Debug and Release, both build and full CTest
  suite.
- **Windows baseline** workflow run
  [`31131262731`](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/31131262731):
  **SUCCESS** — Windows x64 Debug and Release. The first attempt's Release
  job was not a code failure: the hosted runner was never acquired
  ("The job was not acquired by Runner of type hosted even after multiple
  attempts"), a GitHub Actions infrastructure condition. A re-run of the
  failed job completed successfully with no code changes.
- **Full automated test result:** 19/19, confirmed in both workflows above
  and independently reproduced on the project owner's machine in both Debug
  and Release before pushing.

No Wicked source was modified and the pinned commit
`3a800b7134aafe58461093c8abb2e274d4e64033` is unchanged.
`Tools/Windows-Build.Common.ps1` carries an unrelated, pre-existing local
modification (a `git`/`git.exe` PATH-resolution fix) that was not staged,
committed, or altered by LF02 work.

## Acceptance result

**PASS WITH LIMITATIONS.**

**Sole limitation:** editor-owned Flow and Screen dirty-state tracking is
deferred until mutable editor document models and authoring workspaces
exist. See "Deliberate limits" above.

LF02 is accepted as the transactional foundation LF01 explicitly deferred.
Per the canonical proof order (`LP03 -> LF02 -> LP04`), LP04 — Unsaved Test
Level snapshot — is the next lifecycle proof.
