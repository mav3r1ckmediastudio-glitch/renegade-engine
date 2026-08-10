# LP06 Gate 5 — Safe Rebuild, Promotion and Close-out

Status: implementation contract for `agent/lp06-gate5-safe-promotion`.

Authoritative base: `715bb23aad3bd1ec63cb9066db18a842b868caa5`
(`Add LP06 Gate 4 isolated standalone parity (#43)`).

Wicked Engine remains pinned at
`3a800b7134aafe58461093c8abb2e274d4e64033`.

## Gate boundary

Gate 5 consumes the accepted LP06 boundaries:

- Gate 1 owns the deterministic Windows build plan;
- Gate 2 owns unique same-volume loose staging and exact manifests;
- Gate 3 owns the named executable and package-relative bootstrap;
- Gate 4 owns exact package integrity, detached Release DX12 standalone smoke,
  Test All parity, prerequisite policy and tamper/missing/extra failure proof.

Gate 5 owns only the remaining safe-rebuild and owner-visible promotion
boundary. It must not weaken or duplicate Gate 1-4 authority.

The owner-visible final path is:

`<Project Folder>/Builds/Windows/<Game Name> Windows Build/`

A candidate may reach that path only after the existing Gate 4 package evidence
has been accepted and the complete candidate has passed exact integrity
validation.

## Non-negotiable safety rule

**The previous successful owner-visible build remains authoritative until a
validated candidate has committed successfully.**

A failed rebuild must not partially overwrite, mutate or delete the previous
successful build. Failure evidence remains in hidden staging/transaction
locations and must never masquerade as the final build.

## Promotion state machine

### 1. Resolve governed paths

The candidate must be a direct child of the final output parent's
`.renegade-staging` directory. The final output must be a direct child of that
same output parent. This makes candidate, rollback backup and final output
same-volume siblings and rejects arbitrary move/delete targets.

The rollback location is hidden under `.renegade-staging` and is derived from
the final build folder name. Symlinked candidate/final/rollback paths are
rejected.

### 2. Recover or reject stale rollback state

If a rollback backup exists while the final output is absent, Gate 5 treats
that as evidence that an earlier replacement stopped after moving the previous
build out of the final path. The previous build must validate and be restored
to the final path before any new promotion is attempted. The current call then
fails closed and requires an explicit retry.

If both a valid final build and a valid stale rollback backup exist, the final
build remains authoritative. Gate 5 may retire the stale backup before a new
transaction. Any ambiguous or invalid stale state fails closed without deleting
it.

### 3. Validate and prepare the candidate

Before the previous build is touched, the candidate must:

1. pass `ValidateWindowsGamePackage()`;
2. contain the exact accepted Gate 4 build-report state:
   `isolated_smoke_passed_not_promoted`;
3. retain `smoke_test = passed_gate4`,
   `package_isolation = passed_gate4`, and
   `test_all_parity = passed_gate4`;
4. retain `distribution_ready = false`;
5. have its generated project/build/package metadata advanced to a Gate 5
   validated-final-path state while it is still hidden;
6. have all changed metadata hashes refreshed in `package-manifest.json`; and
7. pass complete package integrity again after that metadata update.

Gate 5 does **not** set `distribution_ready = true`. Owner-visible build
promotion is a build-system claim, not a legal/commercial redistribution
approval. Renegade's project-wide release/licensing policy remains a separate
release boundary.

A candidate already prepared by Gate 5 may be retried after a recoverable
promotion failure without rebuilding it, provided its exact integrity and
Gate 5 metadata remain valid.

### 4. Validate the previous successful build

If the final path already exists, it must itself be an exact valid Gate 5 build
before Gate 5 is allowed to replace it. The previous and candidate package
identities must agree. An unknown, corrupt, symlinked or different-project
final directory is never silently deleted or replaced.

### 5. Commit replacement transaction

For replacement:

1. rename the validated previous final directory to the hidden rollback path;
2. rename the validated candidate directory into the final path;
3. revalidate the complete final package at its owner-visible location;
4. only after that validation succeeds, retire the rollback backup.

The directory renames are same-volume commit boundaries. Gate 5 never copies
individual files into an existing final directory.

For a first build, the validated candidate is renamed directly into the final
path and then revalidated there.

### 6. Roll back on failure

If replacement fails after the previous final has moved to the rollback path,
Gate 5 restores that previous directory to the final path before returning
failure.

If the candidate has already moved to the final path and final validation then
fails, Gate 5 moves the failed candidate back to hidden staging and restores
the previous build. With no previous build, the failed first-build candidate is
moved back out of the final path.

The previous successful build must be byte-for-byte identical after every
recoverable failed rebuild.

If the operating system prevents the rollback itself, Gate 5 reports a distinct
rollback failure and preserves every recoverable directory rather than deleting
evidence.

## Failure-injection acceptance matrix

Gate 5 automated proof must cover all of the following against a previously
successful final build where applicable:

1. **First promotion** — a valid Gate 4 candidate becomes the final build and
   remains an exact valid package.
2. **Successful replacement** — a second valid candidate replaces the first;
   no rollback directory remains after successful cleanup.
3. **Interrupted/incomplete candidate materialization** — a missing manifest
   file fails before the previous build is touched.
4. **Failed smoke candidate** — a package that has not reached the accepted
   Gate 4 smoke state is rejected before the previous build is touched.
5. **Locked final output** — a Windows sharing/rename failure leaves the
   previous final byte-identical and the candidate hidden.
6. **Interruption after previous-build backup** — injected failure between the
   old-final rename and candidate-final rename restores the previous build
   byte-for-byte.
7. **Post-move final validation failure** — injected mutation after candidate
   rename but before final acceptance removes the failed candidate from the
   final path and restores the previous build byte-for-byte.
8. **Stale rollback recovery** — an interrupted transaction with the previous
   build present only in the rollback path restores it before any new promotion.

The optional ZIP/archive direction from the R06 assessment is not implemented
by Gates 1-4 and is not invented in Gate 5. The interrupted-copy proof therefore
covers the implemented loose-package materialization boundary. If ZIP/archive
output is added later, its own temporary-file and last-good preservation tests
are required before it can become authoritative.

## Test and implementation discipline

- Promotion remains UI-free in `EngineBridge`.
- Production promotion has no public fault-injection switches.
- Deterministic failure injection may use a private EngineBridge test seam.
- Do not modify Wicked source or its submodule pointer.
- Do not modify `Tools/Windows-Build.Common.ps1`.
- Do not weaken `PackageIntegrityService` to make promotion pass.
- Do not perform file-by-file overwrite of the owner-visible final build.
- Do not claim success merely because compilation succeeds.

The Gate 5 transaction tests are expected to add eight explicit CTest entries,
raising the normal Studio suite from 33 to **41 tests**. The existing Gate 4
Debug XAudio2 environment skip remains exactly as accepted; Gate 5 must not
broaden that skip.

## Owner-visible Build Windows Game boundary

After the promotion transaction is proven, Gate 5 must wire the owner-approved
**Build Windows Game** Studio action through the existing Renegade-owned build
services rather than implementing build semantics in the UI. That action must
produce the governed final path above from the active project and must surface
clear success/failure evidence.

The final owner acceptance is the actual named Release executable launched from
the owner-visible final directory outside Studio/repository assumptions.

## Gate 5 / LP06 acceptance

LP06 is complete only when the exact final Gate 5 head has:

1. all Gate 5 promotion/failure tests passing in Windows Debug and Release;
2. the accepted Gate 4 Release real standalone DX12 package proof still passing;
3. the accepted Gate 4 Debug regression contract still passing without a new
   skip or waiver;
4. pinned-Wicked Windows baseline Debug and Release green;
5. unchanged LP05 and LC01 packaged evidence hashes;
6. unchanged Wicked pin;
7. no modification to the protected local build-wrapper file;
8. an independently reviewed exact candidate head;
9. a successful owner-visible **Build Windows Game** run; and
10. owner acceptance of the actual named Release executable from the promoted
    build directory.

Only after those conditions are satisfied may LP06 be marked complete and the
next lifecycle/tranche begin.
