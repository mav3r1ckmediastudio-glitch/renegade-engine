# LP06 Gate 3 — Named Executable and Package-Relative Bootstrap

## Baseline

Gate 3 starts from merged Gate 2 `main` commit
`cc83e5f111800cbf82f4bc01bcaf15dae988187a` (PR #41), with Wicked pinned at
`3a800b7134aafe58461093c8abb2e274d4e64033`.

Gate 1 owns the deterministic standalone build plan. Gate 2 owns clean loose
staging and exact staged-file manifests. Gate 3 consumes those accepted
boundaries; it does not broaden dependency discovery or publish a final build.

## Outcome

Gate 3 turns the already staged named Release Runtime into a governed Windows
application identity and makes that Runtime able to find its packaged project
without Studio or a caller-supplied `--project` path.

The operation remains stage-only. Actual isolated game launch, moved-package
proof, DX12/Vulkan behavior, Test All menu/two-level parity and tamper behavior
remain Gate 4. Promotion and preservation/replacement of the last successful
owner-visible build remain Gate 5.

## Implementation contract

### Staged executable identity

`BuildIdentityService` applies identity only to the named executable already in
the Gate 2 staging directory. It never patches the source `RenegadeRuntime.exe`.
The caller supplies deterministic identity inputs; Gate 3 does not call the
clock or invent a build ID.

The staged PE receives:

- `ProductName` / display identity from the accepted game name;
- `CompanyName` / developer-publisher;
- public product/file version plus numeric Windows version fields;
- file description and copyright;
- `OriginalFilename` matching the Gate 1 executable name;
- internal build ID and UTC build timestamp;
- stable Save Data ID as explicit VERSIONINFO evidence;
- a governed icon resource; and
- an application manifest policy of
  `asInvoker+PerMonitorV2+longPathAware+utf8`, including Windows 10+
  compatibility metadata.

Identity resources are applied before the final Gate 3 hashes are recorded.
After the PE changes, Gate 3 recalculates the executable SHA-256 and rewrites
the project, Runtime-support, build-report and package manifests to describe
the post-identity bytes. `ValidateWindowsGameBuildStage()` must pass again.

### Package-relative Runtime bootstrap

The Runtime keeps the existing explicit argument parser as the first authority.
A valid explicit `--project` therefore remains the Test Level/tooling path.
Malformed or duplicate explicit arguments fail as before and never fall back to
a package silently.

Only a genuine missing-project result may use package bootstrap. The resolver:

1. receives the operating system's executable path;
2. derives the package root from that executable, not from current working
   directory;
3. reads `GameData/project.manifest.json`;
4. requires Gate 3 schema/version/bootstrap/identity fields;
5. requires the manifest executable name to match the running executable using
   Windows case semantics;
6. rejects unsafe, escaping or symlinked project paths;
7. opens the packaged `.renegade` descriptor through `ProjectService`; and
8. requires the descriptor stable project ID to match the package manifest.

The existing Runtime project/flow/screen resolution continues after this
bootstrap selection. The existing Test Level readiness-event handshake remains
unchanged.

## Acceptance criteria

1. The source Runtime is byte-identical before and after Gate 3 identity work.
2. The staged `<GameName>.exe` has readable Windows VERSIONINFO containing the
   expected product, publisher, public version, original filename, Save Data
   ID, build ID and build timestamp.
3. The staged executable contains the governed icon resource and the Gate 3
   application manifest policy.
4. Repeated Gate 2 stages with byte-identical Runtime/content and identical
   Gate 3 identity inputs produce the same stamped executable SHA-256 and the
   same Gate 3 project/Runtime/package manifest JSON.
5. The Runtime-support and package manifests contain the post-stamp executable
   SHA-256, not the source Runtime hash.
6. The completed Gate 3 stage passes the existing exact-file/no-extras Gate 2
   validator and the owner-visible final output path remains absent.
7. Zero-argument package bootstrap succeeds from an unrelated current working
   directory using the executable directory as authority.
8. Explicit `--project` remains authoritative even when a valid package
   manifest is present; malformed explicit project arguments do not fall back.
9. Executable-name mismatch, escaping project paths, project-ID mismatch and
   missing Gate 3 identity fields fail closed.
10. GitHub Windows Debug and Release Studio CI pass, including the new Gate 3
    CTests, with the pinned-Wicked Debug/Release baseline unchanged.
11. A different AI/human audits the exact final Gate 3 candidate before merge.

## Gate 3 exclusions

Gate 3 does **not**:

- claim the staged game has completed a real graphics launch;
- prove DX12 or Vulkan package behavior;
- prove menu → Level 1 → Level 2/Test All parity;
- prove moved-package launch or clean-consumer-machine prerequisites;
- select/deploy a Visual C++ Runtime policy;
- promote, replace or archive an owner-visible build;
- implement last-good preservation/failure-injected rebuild;
- add package protection/encryption; or
- modify Wicked Engine or its pinned revision.

## Candidate evidence

The authoritative proof is the exact-head GitHub Windows Debug/Release run plus
pinned-Wicked baseline after this branch is published. Until those jobs pass and
an independent exact-head audit is complete, Gate 3 remains a candidate rather
than an accepted lifecycle gate.
