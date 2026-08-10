# LP06 Gate 4 — Isolated Standalone Behaviour and Test All Parity

Status: candidate implementation on `agent/lp06-gate4-isolated-standalone`.

Authoritative base: `6db154978e6e4c9dfa8afcf67b0927d850993676`
(`Add LP06 Gate 3 named executable bootstrap (#42)`).

Wicked Engine remains pinned at
`3a800b7134aafe58461093c8abb2e274d4e64033`.

## Gate boundary

Gate 4 consumes the accepted Gate 1 build plan, Gate 2 clean loose staging and
Gate 3 named executable/package-relative bootstrap. It proves that the staged
package actually behaves as a standalone game package before any owner-visible
promotion is allowed.

Gate 4 does **not** create or replace
`<Project Folder>/Builds/Windows/<Game Name> Windows Build/`, does not preserve
or replace a previous successful final build, and does not claim final
redistribution approval. Those remain Gate 5 responsibilities.

## Production package integrity boundary

`PackageIntegrityService` re-enumerates the loose package from
`package-manifest.json` and fails closed before packaged Runtime project
bootstrap on:

- missing manifest files;
- byte-count or SHA-256 mismatch;
- unmanifested extra files;
- unsafe or escaping relative paths;
- Windows case-equivalent manifest collisions;
- symlinked package entries or non-regular filesystem entries.

The service uses the accepted Gate 2/3 manifest contract:
`package-manifest.json` is required at package root but is excluded from its own
`files` array (`self_sha256_excluded=true`). Every other package file must be
listed exactly once and match its recorded size and SHA-256.

Explicit developer/Studio `--project` launch remains authoritative and does not
invoke package validation. This preserves the accepted LP04 Test Level launch
and readiness-event boundary.

## Runtime evidence location

A package launch writes Runtime evidence outside the immutable package under the
current Windows user's Local AppData tree. The package itself therefore remains
byte-identical during gameplay and can be revalidated after process exit.

The Gate 4 CI fixture isolates `LOCALAPPDATA` to a disposable directory. Normal
explicit `--project` / Test Level launch retains the pre-existing relative
`Logs/RuntimeBootstrap.log` behaviour.

Runtime evidence schema is `renegade-runtime-bootstrap-v2`. Gate 4 adds package
integrity status, requested/actual graphics backend, graphics capability,
selected prerequisite policy, persistent "screen was loaded" evidence and
smoke completion fields while retaining the existing project/flow/screen/action
trace.

## Real staged-package DX12 smoke

The authoritative Gate 4 process test does not launch a fake process fixture.
It:

1. creates a disposable one-menu/two-level project using the accepted LP03
   Runtime-screen/Story-Flow fixture and real Wicked WISCENE content;
2. stages the project with `StageWindowsGameBuild`;
3. applies Gate 3 identity to the configuration-matched real
   `RenegadeRuntime.exe`, producing `ProofGame.exe`;
4. validates the complete stage;
5. computes the explicit-project Test All Story Flow result for Game Start plus
   two named `level.complete` outcomes;
6. moves the package to a separate consumer-style path containing spaces and
   non-ASCII text;
7. deletes the disposable source project;
8. launches `ProofGame.exe` from an unrelated current working directory with no
   `--project` argument;
9. uses the real startup Runtime screen and its existing `play` action;
10. requires the real Story Flow to enter Level One, then Level Two, then the
    Complete Game terminal node;
11. requires the packaged Runtime flow trace to match the Test All trace exactly;
12. records the accepted Runtime evidence into the staged build report;
13. re-hashes `build-report.json`, refreshes its package-manifest record and
    revalidates the complete package.

The two internal smoke switches used by CI do not bypass game systems:
`--renegade-smoke-autoplay` queues the existing Runtime-screen `play` action with
`RuntimeInputSource::Test`, and `--renegade-smoke-exit` requests normal window
shutdown only after the existing Story Flow reaches `CompleteGame`. A failed
play action or a non-terminal flow cannot report smoke success.

The successful Gate 4 build-report state is deliberately still staged:

- `status = isolated_smoke_passed_not_promoted`
- `stage_only = true`
- `distribution_ready = false`
- `smoke_test = passed_gate4`
- `package_isolation = passed_gate4`
- `test_all_parity = passed_gate4`
- `promotion = not_attempted_gate4`

## Windows prerequisite policy

Gate 4 selects:

`system-vc-redist-2015-2022-x64`

Renegade does not copy arbitrary Visual C++ runtime DLLs from System32 and the
Gate 4 package proof rejects app-local `vcruntime*`, `msvcp*`, `concrt*` or
`ucrtbase.dll` at package root. The actual Runtime process must start
successfully on the authoritative GitHub Windows runner under that selected
system prerequisite policy.

This is a deployment-policy decision and CI proof for the current build runner;
it is not a claim that every future consumer machine already has the required
Microsoft redistributable installed. Gate 5/final release evidence must preserve
the selected policy and owner-facing prerequisite documentation.

`dxcompiler.dll` remains the explicit app-local Runtime support dependency beside
the named executable, matching the accepted Gates 1–3 package contract.

## Graphics contract

### DX12

DX12 is the primary Gate 4 graphics path. The real packaged Runtime must create
and run its Wicked graphics application and report the actual device tag as
`DX12`; a package/bootstrap-only process is not sufficient for this proof.

### Vulkan

Gate 4 records explicit Vulkan capability behaviour without pretending that a
loader check is a full Vulkan gameplay launch. The package-relative
`--renegade-capability-probe` checks the Windows system Vulkan loader and
`vkGetInstanceProcAddr` before device startup:

- loader present: process succeeds with
  `graphics_capability=VULKAN_LOADER_AVAILABLE`;
- loader absent: process exits with the structured
  `GRAPHICS_PREREQUISITE_MISSING` Runtime code and
  `graphics_capability=VULKAN_LOADER_MISSING`.

A later Vulkan gameplay-validation tranche can demand a real Vulkan device/run
when that becomes an owner acceptance requirement; Gate 4's claim is the
explicit capability/failure policy stated above.

## Tamper and omission process proof

After the successful package smoke, the process test creates separate copies and
injects three faults:

1. mutate a manifest-listed Story Flow file;
2. remove a manifest-listed Level Two WISCENE;
3. add an unmanifested content file.

Each copy launches the actual named Runtime in pre-device capability mode. Each
must terminate with `PACKAGE_INTEGRITY_FAILED` and an external structured log
showing `package_integrity=FAIL`. This proves the production Runtime enforces the
same exact package boundary, rather than only a build-time test helper checking
it.

## Isolation evidence

The Gate 4 process proof additionally requires:

- the source project is physically removed before package launch;
- launch current working directory is unrelated to the package;
- the moved package validates after relocation;
- normalized package/build-report evidence contains no repository/source path;
- the owner-visible final output path remains absent throughout Gate 4;
- Runtime evidence is outside the package and therefore is not an unmanifested
  package mutation.

## Authoritative tests

Gate 4 adds two CTest targets to the normal Studio CI dependency graph:

- `RenegadePackageIntegrityTests`
- `RenegadeStandalonePackageTests`

The expected suite count increases from 30 to **32 tests** in both Debug and
Release. Release is the distribution-configuration proof; Debug runs the same
process/integrity logic as a regression mirror. GitHub Actions Debug and Release
remain authoritative because the owner machine has known CPU instability.

The existing LP05 and LC01 packaged hashes and the pinned Wicked baseline must
remain unchanged.

## Gate 4 acceptance

Gate 4 is mergeable only when the exact PR head has:

1. Studio Windows x64 Debug green with 32/32 CTest;
2. Studio Windows x64 Release green with 32/32 CTest;
3. pinned-Wicked Windows baseline Debug and Release green;
4. real `RenegadeStandalonePackageTests` pass in both configurations;
5. unchanged LP05 and LC01 packaged evidence hashes;
6. unchanged Wicked submodule pin;
7. independent review of the exact candidate head;
8. no owner-visible promotion/final-build claim.

Gate 5 then owns safe rebuild failure injection, last-good preservation, atomic
promotion, final closeout and owner-visible standalone acceptance.
