# LP06 — Named Standalone and Safe Rebuild

## Outcome

LP06 completes the core Gate 3 standalone proof. A representative Renegade
project with one Runtime menu and two levels must become a named Windows x64
standalone game that launches outside Studio and the repository, follows the
same project/flow contract as Test All, and can be rebuilt without risking the
last successful distributable.

The original lifecycle P06 minimum hypothesis is retained: one menu and two
levels build into `<GameName>.exe` outside Studio/repository and match Test All.
The later proof-programme reconciliation strengthens that requirement with
clean/isolated launch, prerequisites and notices, failure injection, and
preservation of the previous successful build.

## Programme reconciliation

The Gate 0 reconciliation placed the core standalone sequence in this order:

1. LP05 — Representative Dependency Extraction.
2. LC01 — Minimal Loose Deterministic Cooker.
3. LP06 — Named Standalone and Safe Rebuild.

The repository implementation that ultimately received the LC01 identifier was
**Asset Identity and Source Tracking**. It deliberately stopped before copying,
cooking or standalone packaging. That work is accepted and is a necessary
strengthening of the asset pipeline, but it did not complete the loose-cooker
claim carried by the earlier programme mapping.

No original proof is silently cancelled. LP06 therefore absorbs the still-open
loose package construction boundary in Gates 1-2 before it attempts named
executable or safe-promotion proof. LP05 remains the dependency authority and
LC01 remains the stable asset/provenance authority consumed by that work.

## Baseline

LP06 starts from merged `main` commit
`01d790bda5acea0cdb6a7735557b12224c795a64` (PR #39, LC01 Gate 5) with Wicked
pinned at `3a800b7134aafe58461093c8abb2e274d4e64033`.

The initial standalone target is Windows 10/11 x64 Release, with DX12 as the
primary launch proof. Vulkan remains a cross-check with an explicit loader /
driver prerequisite rather than an assumed package dependency.

## Architectural rules

- Studio UI does not own build semantics. LP06 services remain UI-free.
- Project content and Runtime/engine support are separate governed dependency
  classes and separate manifests.
- LP05 dependency closure decides which project resources are reachable.
- LC01 stable asset IDs and import provenance must agree with the LP05 graph.
- A required missing project dependency fails before package construction.
- A stale imported product fails before package construction; LP06 does not
  silently reimport it.
- Editor-only and unreachable creator content must not enter a used-assets-only
  package.
- Runtime support comes from an explicit allowlist; it is not inferred from
  arbitrary files beside Studio or from the developer machine.
- Current support includes `dxcompiler.dll` beside the executable until a
  separate proven DLL-directory/removal decision changes that contract.
- Temporary/staging output must never look like a completed distributable.
- The previous successful build remains authoritative until a candidate has
  passed all validation and smoke proof and is promoted successfully.
- Package protection/encryption is BP01/P09 work after a reliable loose LP06
  build. LP06 must not trade reliability for premature protection.

## Gate map

### Gate 1 — Deterministic standalone build-plan contract

Introduce a UI-free `BuildService` contract that combines:

- project/game/save identity;
- the accepted LP05 dependency graph;
- the accepted LC01 asset registry and imported-product provenance; and
- an explicit Runtime-support allowlist.

It produces canonical, deterministic plan JSON only. It performs no copies,
renames, executable patching, staging, promotion or Studio UI work.

#### Gate 1 acceptance

1. A valid Windows x64 Release request produces a stable named executable and
   build-folder identity.
2. Only graph-reachable project content enters the plan.
3. Editor-only and graph `RuntimeSupport` nodes do not enter `GameData`.
4. Required missing project content fails closed; optional missing content is
   explicitly excluded and reported.
5. Every included project node must agree with its LC01 stable asset record.
6. An included imported product whose recorded source or product hash is stale
   fails closed rather than being packaged or reimported implicitly.
7. Runtime support is a separate explicit input and includes the named Runtime
   executable plus the current `dxcompiler.dll` requirement.
8. Outside-project/escaped dependency diagnostics, unsafe/reserved Windows
   names, unsafe destinations and Windows case-equivalent destination
   collisions fail closed.
9. Reordering logically equivalent graph/support inputs produces byte-identical
   canonical build-plan JSON.
10. Debug and Release Windows CI pass with Wicked unchanged.

#### Gate 1 exclusions

Gate 1 does not:

- copy or cook creator files;
- generate the final content/runtime-support manifests;
- add licences/notices to an output;
- choose or deploy the Visual C++ Runtime;
- rename or resource-stamp `RenegadeRuntime.exe`;
- change Runtime bootstrap to double-click/package-relative startup;
- launch a staged game;
- modify Studio UI;
- replace/promote an existing build.

### Gate 2 — Clean loose cooker and governed staging

Consume the Gate 1 plan into a unique same-volume staging directory. Copy only
approved project content and Runtime support, generate complete normalized
content/runtime-support/package manifests, add version-controlled licence and
notice inputs, reject missing/extra/colliding files, and prove repeated
unchanged inputs produce the same normalized content manifest.

This gate absorbs the loose deterministic cooker function that the original
programme placed immediately before LP06 but which the implemented LC01
identity/source-tracking lifecycle intentionally did not perform.

Gate 2 still does not publish a final build or claim standalone launch parity.

### Gate 3 — Named executable and package-relative bootstrap

Create the named `<GameName>.exe` identity from the clean Release Runtime and
make the packaged Runtime resolve its startup project from the build itself so
double-click/Explorer launch does not require Studio or a developer-supplied
`--project` path.

Prove executable identity/version metadata and package-relative paths. Preserve
existing Test Level `--project` / readiness-event behaviour.

### Gate 4 — Isolated standalone behaviour and Test All parity

Launch the actual staged build from clean/unrelated locations and prove:

- no Studio/repository/source-tree access;
- moved-package and unrelated-current-directory launch;
- the selected Windows prerequisite policy;
- DX12 startup and explicit Vulkan capability behaviour;
- one menu and two levels following the same Game Start / named outcomes as
  Test All;
- stable completion/quit result, logs and build report;
- missing/tampered/unmanifested content fails clearly.

### Gate 5 — Safe rebuild, promotion and LP06 close-out

Add failure-injected rebuild proof for stale staging, interrupted copy/archive,
failed smoke, locked output and replacement of an existing build. A candidate
may become the final owner-visible build only after validation; failure must
preserve the previous successful build byte-for-byte.

The final exact head must pass Debug/Release Studio CI and pinned-Wicked
baseline checks, the packaged standalone evidence, independent exact-head
review and owner-visible acceptance of the actual named executable.

## Minimum loose package direction

The R06 evidence recommends a transparent auditable loose package before later
protection work. LP06 converges on this responsibility split (exact filenames
may evolve only with recorded evidence):

```text
<Game Name> Windows Build/
  <GameName>.exe
  dxcompiler.dll
  GameData/
    project.manifest.json
    content-manifest.json
    Content/
  Engine/
    runtime-support-manifest.json
  Licences/
    ...deterministic selected-component notices...
  ReadMe.txt
  build-report.json
```

A Visual C++ deployment policy is a required LP06 prerequisite decision and
must be proven on a clean consumer Windows image. Arbitrary DLL copying from a
developer machine or `System32` is not an acceptable policy.

## Source basis

This lifecycle contract reconciles the owner-approved programme and current
repository truth using:

- `Renegade_Lifecycle_Feasibility_and_Evidence_Plan_v1.0`;
- `Renegade_Proof_Programme_Reconciliation_Addendum_v1.0`;
- `Renegade_Tranche_1_R06_Build_Package_and_Redistribution_Assessment_2026-08-05`;
- accepted LP05 and LC01 repository evidence on current `main`.

The earlier research documents describe candidate architecture rather than an
implementation merge by themselves. This LP06 branch is the owner-authorised
bounded implementation of that evidence, beginning with Gate 1 only.
