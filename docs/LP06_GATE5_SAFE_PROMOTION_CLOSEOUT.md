# LP06 Gate 5 — Safe Rebuild, Promotion and Close-out

Status: **ACCEPTED AND MERGED**.

Accepted PR head:
`99cfe1f74016bb6a53a4c35e020f6884099a52fd`

Squash merge/current LP06 main baseline:
`48126f859b2f9b25a60182c4311cfc6c91d98436`
(`Add LP06 Gate 5 safe rebuild and promotion (#44)`).

Wicked Engine remained pinned at
`3a800b7134aafe58461093c8abb2e274d4e64033`.

## Gate boundary

Gate 5 consumes the accepted LP06 boundaries:

- Gate 1 owns the deterministic Windows build plan;
- Gate 2 owns unique same-volume loose staging and exact manifests;
- Gate 3 owns the named executable and package-relative bootstrap;
- Gate 4 owns exact package integrity, detached Release DX12 standalone smoke,
  Test All parity, prerequisite policy and tamper/missing/extra failure proof;
- Gate 5 owns safe rebuild/promotion and the owner-visible **Build Windows
  Game** workflow.

The final owner-visible path is:

`<Project Folder>/Builds/Windows/<Game Name> Windows Build/`

## Non-negotiable safety rule

**The previous successful owner-visible build remains authoritative until a
validated candidate commits successfully.**

A failed rebuild cannot partially overwrite, mutate or silently delete the
previous successful build. Gate 5 uses same-volume directory rename boundaries
and hidden rollback state rather than file-by-file overwrite.

## Accepted promotion contract

A candidate must be a governed direct child of the final output parent's
`.renegade-staging` directory. Symlinked or escaped candidate/final/rollback
paths fail closed.

Before the previous build is touched, the candidate must:

1. pass complete `ValidateWindowsGamePackage()` integrity;
2. retain the exact accepted Gate 4 state:
   `isolated_smoke_passed_not_promoted`;
3. retain `smoke_test = passed_gate4`,
   `package_isolation = passed_gate4` and
   `test_all_parity = passed_gate4`;
4. retain `distribution_ready = false`;
5. advance generated package/build metadata to the Gate 5 final-path state;
6. refresh hashes for changed generated metadata; and
7. pass exact integrity again before promotion.

If a previous final build exists, it must itself validate as a compatible Gate 5
build before replacement.

Replacement uses:

1. previous final -> hidden rollback;
2. candidate -> final;
3. exact final-path package revalidation;
4. rollback cleanup only after successful final validation.

Recoverable failure restores the previous successful build. Ambiguous/corrupt
stale rollback state fails closed rather than deleting evidence.

## Accepted failure matrix

Eight explicit Gate 5 cases remain part of the normal CTest suite:

1. first promotion;
2. successful replacement;
3. incomplete candidate rejection;
4. failed-smoke candidate rejection;
5. locked final-output preservation;
6. interruption after previous-build backup;
7. post-move final validation rollback; and
8. stale rollback recovery.

The optional ZIP/archive direction is not part of LP06. Gate 5 governs the
implemented loose package. Any later archive format needs its own temporary-file
and last-good preservation proof.

## Owner-visible Build Windows Game

The Release Studio action remains a thin adapter over Renegade-owned services.
The active-project preparation path:

- rebuilds the current production LP05 dependency closure;
- refreshes/persists LC01 stable identity without silently importing/reimporting
  creator content;
- computes the deterministic Story Flow Test All trace through the accepted
  `FlowInterpreter` contract; and
- feeds the existing LP06 plan/stage/named-executable/verification/promotion
  services.

Release Studio then:

1. creates the deterministic plan;
2. materializes a unique staged package;
3. creates the named executable identity;
4. launches the real staged named executable on DX12 from an unrelated
   temporary working directory;
5. requires fresh Runtime evidence;
6. verifies exact Gate 4 parity/integrity; and
7. safely promotes the candidate.

The BUILD menu exposes **BUILD WINDOWS GAME...**. Debug Studio retains the
regression mirror but keeps the owner-visible standalone claim Release-only.

## Owner acceptance failure that reopened the candidate

The first owner-visible attempt used superseded head
`74dc4f67a76fe570655a7c02c3ceed58f969310b` and correctly failed closed with
Runtime exit code 27.

Runtime evidence proved:

- package integrity passed;
- DX12 started;
- the Runtime Screen loaded;
- automated PLAY fired; but
- Story Flow could not resolve Level One scene document ID
  `65000000-0000-4000-8000-000000000001` inside the packaged project.

The failure was therefore not a GPU, DX12, package-integrity or Story Flow
syntax failure.

### Root cause

The owner-build dependency closure contained reachable `.wiscene` scene files,
but not the adjacent Renegade-owned `.wiscene.rmeta` identity companions that
Runtime uses to resolve a Story Flow scene stable document ID.

Gate 4's handcrafted standalone package fixture had explicitly included those
sidecars, so that integration gap was hidden from the earlier automated proof.

### Correction

`WindowsGameBuildProjectService` now registers an LP06-only
`SceneIdentityCompanionProvider` during owner-build dependency preparation.
Every reachable `Scene` emits:

- `<scene>.rmeta`;
- `DependencyClass::GeneratedData`;
- `DependencyRequirement::Required`;
- provider `lp06-scene-identity-companion`; and
- edge provenance `lp06.scene_identity_companion`.

This is deliberately local to LP06 owner-build preparation. It does **not**
modify the accepted LP05 canonical extractor or change its packaged graph hash.
LC01 refresh assigns normal governed records to the sidecars and the standard
build plan packages them as project content.

A new `RenegadeWindowsGameBuildProjectTests` regression proves:

- both reachable scene identity sidecar graph nodes exist;
- class/requirement/provider/hash are correct;
- scene->sidecar provenance edges exist;
- LC01 registry records exist;
- both sidecars appear under `GameData/Content/Scenes/...` in the Windows build
  plan; and
- the established Test All route remains unchanged.

The test uses Gate 4's deliberately GPU-free WISCENE fixture; the earlier
version incorrectly used Wicked's stock cube WISCENE in a headless test and
segfaulted because that scene can touch graphics resources while deserializing.
That test-fixture mistake was corrected without changing production code.

## Final authoritative CI

Exact accepted head:
`99cfe1f74016bb6a53a4c35e020f6884099a52fd`

Renegade Studio run **239** (`31443096866`):

- Debug: SUCCESS;
- Release: SUCCESS;
- Release: **42/42 CTests passed**;
- `RenegadeStandalonePackageTests`: PASS;
- `RenegadeWindowsGameBuildProjectTests`: PASS;
- all eight promotion/failure tests: PASS;
- Debug retained only the already-accepted Gate 4 hosted XAudio2 capability
  skip.

Windows baseline run **364** (`31443096863`):

- Debug: SUCCESS;
- Release: SUCCESS.

Accepted canonical evidence remained unchanged:

- LP05 graph: 4,681 bytes, SHA-256
  `23b67f63099293d79a239997730b287f157fb38e5421aecb5505e0ca42c84384`;
- LC01 registry: 2,180 bytes, SHA-256
  `547a26c09e6a74394cc9bc67885070272928f5af14d736e8e086d022f0aeea0e`;
- Wicked pin:
  `3a800b7134aafe58461093c8abb2e274d4e64033`.

## Final owner acceptance

Using the corrected Release Studio artifact on the same deterministic LP06
owner-test project:

- Studio reported **BUILD COMPLETE**;
- the promoted build appeared under
  `Builds/Windows/LP03 Runtime Screen Fixture Windows Build/`;
- the named executable was launched directly from Windows Explorer;
- Studio was not required;
- no `--project` argument was supplied;
- Runtime started on DX12;
- the Runtime Screen loaded; and
- PLAY successfully entered Level One.

This directly proves the corrected scene identity companions fixed the owner
failure that invalidated `74dc4f67...`.

## Independent audit

A fresh independent audit of exact head
`99cfe1f74016bb6a53a4c35e020f6884099a52fd` returned a clean merge verdict.
It independently confirmed:

- exact Debug/Release CI logs;
- 42-test suite and all promotion cases;
- unchanged LP05/LC01 canonical hashes;
- LP06-only scope of `SceneIdentityCompanionProvider`;
- regression coverage for graph nodes, provenance edges, LC01 records and build
  plan inclusion; and
- no new blocking finding.

Standing non-blocking note: `RunStandaloneSmoke` creates the child process and
then assigns it to the Job Object, leaving a small containment race. A future
hardening may use `CREATE_SUSPENDED`, assign the process, then resume it. This
was accepted as low risk and was not bundled into the LP06 correction.

## Final verdict

**LP06 Gate 5: PASS.**

**LP06: COMPLETE AND ACCEPTED.**

Squash merge:
`48126f859b2f9b25a60182c4311cfc6c91d98436`.

LP06 promoted builds intentionally remain `distribution_ready=false`.
Commercial redistribution, code signing, installer/update policy, protection or
encryption remain later release work.
