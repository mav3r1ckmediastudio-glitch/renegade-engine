# Renegade Engine — Current Handoff

**Date:** 2026-08-13

## Active creator recovery — PR #57

PR #57 (`agent/creator-workflow-repair`) remains Draft. The last independently
accepted pre-recovery head was `e5d4fed6ae8852562dbb0bfe75e60283967ef50f`,
with Studio and baseline Debug/Release green and 65/65 CTests passing. Owner
Release testing then exposed acceptance-scope usability/material failures, so
that head must not be merged.

The recovery work keeps the importer as the active programme. Do not begin a
new backend milestone until exact-head CI/audit and owner manual acceptance pass.
The recovery unifies preview/commit material application, corrects the generated
Surface reflectance default from full-white to neutral dielectric, persists PBR
scalars, adds editable asset name/destination, importer-owned chrome,
slider-plus-number transform/material/lighting controls, linked scale,
world-bounds dimension presets and a dynamic preview-only 1.82 m male reference.
The follow-up preview framing fix replaces the fixed close camera with a
bounds-fitted 32-degree lens; owner screenshots showed severe perspective
exaggeration in the importer while the committed mesh remained correct.
Those screenshots also exposed the primary preview-only rendering defect: the
temporary stage was at world Y=100000, where float transform precision is only
about 7.8 mm. The stage now remains beyond the normal far plane at Y=2048 while
preserving sub-millimetre precision; source mesh data is never rewritten.

GitHub CI remains authoritative because the owner CPU is confirmed unstable.
Manual merge-gate coverage remains textured GLB/GLTF, FBX, multi-material and
character import; preview/final parity; save/reopen; and packaged textured
Runtime output. PR #57 must remain Draft until all of that passes.

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Authoritative main baseline:**
`48126f859b2f9b25a60182c4311cfc6c91d98436`
(`Add LP06 Gate 5 safe rebuild and promotion (#44)`).

**Documentation reconciliation branch:**
`docs/lp06-closeout-lp07-roadmap`

**Wicked pin:**
`3a800b7134aafe58461093c8abb2e274d4e64033`

## Critical owner-machine safety rules

The project owner's local clone contains an unrelated uncommitted modification
to `Tools/Windows-Build.Common.ps1`. Direct native Git calls there use
`git.exe` and check `$LASTEXITCODE` because Windows command resolution on the
owner machine required that correction.

**Never reset, restore, stage, discard or overwrite that local modification.**

Temporary/untracked LP04/other helper patch files may also exist locally.

- Never use `git clean`.
- Never use `git add .`.
- Do not modify Wicked or move its submodule pin unless an explicit justified
  core-patch decision is made.

Owner machine context: Windows 10, 128 GB DDR4, known CPU instability with WHEA
corrected machine-check history. Local `CL.exe`/Release failures have previously
been proven hardware-related. GitHub CI Debug/Release is therefore the
authoritative compiler/build proof when local hardware is suspect.

Compilation is never sufficient behavioural proof. Creator-facing/runtime work
requires the relevant CTest, packaged/runtime evidence and owner acceptance.

## Current truth

### LP04 — Unsaved Test Level Snapshot

**Complete and accepted.**

Studio snapshots the current unsaved live scene, launches the real separate
Runtime, waits for an explicit READY handshake, displays unsaved scene content
and STOPs the Runtime cleanly without forcing an authoritative scene save.

### LP05 — Representative Dependency Extraction

**Complete and accepted.**

The production dependency system uses Renegade-owned UI-free providers across
project documents, Story Flow, Runtime Screens, WISCENE public component data,
glTF/GLB resources and explicit declared references. Lua is not scanned or
executed for guessed file paths.

Canonical packaged graph:

- bytes: `4681`
- SHA-256:
  `23b67f63099293d79a239997730b287f157fb38e5421aecb5505e0ca42c84384`

### LC01 — Asset Identity and Source Tracking

**Complete and accepted.**

PR #39 exact final head:
`3d3e780b38792aec866cd19ce6638a8260ffff4f`

Squash merge:
`01d790bda5acea0cdb6a7735557b12224c795a64`

Authoritative final CI:

- Renegade Studio run 166: Debug/Release success;
- Windows baseline run 185: Debug/Release success.

LC01 establishes:

- stable UUID project asset records;
- transactional project-root `AssetRegistry.renegade-assets` persistence;
- source-to-imported-product provenance;
- explicit importer/settings schema and import-time hashes;
- stale provenance reporting;
- deterministic moved/missing source recovery without speculative relinking;
- packaged multi-process source-update/move/reopen proof.

Canonical packaged registry:

- bytes: `2180`
- SHA-256:
  `547a26c09e6a74394cc9bc67885070272928f5af14d736e8e086d022f0aeea0e`

LC01 does **not** execute reimport or provide the creator-facing reusable asset
workflow. That is the next programme.

### LP06 — Named Standalone Windows Build / Safe Rebuild

**Complete and accepted.**

PR #44 corrected exact head:
`99cfe1f74016bb6a53a4c35e020f6884099a52fd`

Squash merge/current main:
`48126f859b2f9b25a60182c4311cfc6c91d98436`

Authoritative final CI:

- Renegade Studio run 239: Debug/Release success;
- Windows baseline run 364: Debug/Release success;
- Release: 42/42 CTests passed;
- `RenegadeStandalonePackageTests`: passed;
- `RenegadeWindowsGameBuildProjectTests`: passed;
- all eight Gate 5 safe-promotion/failure scenarios passed;
- Debug retained only the already-accepted Gate 4 hosted XAudio2 capability
  skip;
- LP05/LC01 canonical hashes remained unchanged;
- Wicked pin remained unchanged.

LP06 provides:

- deterministic Windows build plan;
- same-volume governed staging under `.renegade-staging`;
- named game executable and package-relative startup;
- exact project/runtime-support/package manifests and integrity validation;
- required `dxcompiler.dll` beside the game executable;
- detached Release DX12 smoke from an unrelated CWD;
- Runtime Screen + PLAY + Story Flow Test All parity;
- safe previous-build preservation, rollback and promotion;
- Release Studio **BUILD > BUILD WINDOWS GAME...** integration;
- final build under
  `<Project>/Builds/Windows/<Game Name> Windows Build/`.

#### LP06 owner finding and correction

The first owner-visible build on superseded head
`74dc4f67a76fe570655a7c02c3ceed58f969310b` failed closed with Runtime exit
code 27. Package integrity, DX12 startup and Runtime Screen load had succeeded;
PLAY failed because Runtime could not resolve the Story Flow Level One scene
stable document ID.

Root cause: reachable `.wiscene` files were packaged, but their Renegade-owned
`.wiscene.rmeta` scene identity companions were absent from the owner-build
closure. Gate 4's handcrafted fixture had included those sidecars and therefore
masked the integration gap.

Correction: `WindowsGameBuildProjectService` adds an LP06-only
`SceneIdentityCompanionProvider`. Every reachable Scene emits its adjacent
`.rmeta` as Required `GeneratedData` with
`lp06.scene_identity_companion` provenance. The accepted LP05 canonical
extractor/evidence remains unchanged.

The new regression test proves sidecar graph nodes, provenance edges, LC01
records and Windows build-plan files using Gate 4's GPU-free WISCENE fixture.

Owner acceptance on the corrected Release artifact:

- Studio reported **BUILD COMPLETE**;
- promoted named standalone executable launched directly from Explorer;
- no `--project` argument was supplied;
- Runtime Screen loaded;
- PLAY entered Level One successfully.

Independent exact-head audit passed. Its sole standing note is the existing
small `CreateProcessW` -> `AssignProcessToJobObject` race in the standalone
smoke launcher; hardening with `CREATE_SUSPENDED`/assign/resume is a future
non-blocking improvement and was not bundled into the accepted correction.

LP06 promoted builds deliberately retain `distribution_ready=false`.
Commercial redistribution, signing, installer/update policy and encryption are
later release boundaries.

## Phase position

**Phase 4 — Project and asset pipeline remains active.**

The backend foundations are now much further advanced than the original Phase 4
roadmap expected: stable IDs, source provenance, dependency extraction,
moved/missing recovery and standalone asset collection/building already exist.

The next missing product boundary is the creator-facing reusable asset workflow.

## Next programme — LP07 Reusable Project Asset Workflow

Status: **planned; implementation has not started.**

Canonical design:
`docs/LP07_REUSABLE_PROJECT_ASSET_WORKFLOW.md`

### Owner format priority

**FBX is P0 for LP07, including skinned and animated FBX.** The owner reports
that the overwhelming majority of real project assets are FBX/animated FBX, so a
GLB/GLTF-only lifecycle would not satisfy the intended creator workflow.

The exact Wicked pin already includes a dedicated `ImportModel_FBX()` converter
based on bundled `ufbx`. Exact-source review confirms conversion of meshes,
materials/textures, armatures/skinning, up to eight bone influences, morph data,
node hierarchy, FBX animation stacks/takes and baked translation/rotation/scale
animation into native Wicked components. Mixamo-specific handling also exists.

Therefore the implementation order is **Wicked/ufbx first**. Do not add Assimp
or another parallel importer merely to increase extension count. An external
importer is permitted only after a bounded capability-gap proof demonstrates a
required format or FBX feature that the pinned Wicked path cannot represent
reliably.

### LP07 format priority

- P0: FBX static/skinned/animated — primary owner/manual/package acceptance.
- P1: GLTF/GLB — required supported alternative and regression path.
- P2: OBJ/PLY through the common Renegade import contract when the pinned Wicked
  seams are proven clean; VRM/VRMA after exact seam audit.
- External fallback: Assimp or another library only after evidence-driven gap
  analysis, with licence/notices/build/normalisation consequences recorded.

Target outcome:

> import one animated/skinned FBX -> create a stable reusable project asset ->
> browse it -> place it repeatedly -> detect a source change -> explicitly
> reimport safely while retaining IDs -> reopen -> Build Windows Game ->
> standalone Runtime uses the updated product.

GLB/GLTF must remain functional throughout.

Gate map:

1. common format-neutral model-import seam + pinned Wicked FBX static/skinned/
   animated WISCENE round-trip proof; GLB/GLTF regression and OBJ/PLY/VRM seam
   assessment;
2. registry-backed UI-free asset catalogue;
3. governed reusable model-asset import transaction, FBX primary and GLB/GLTF
   required;
4. stable explicit reimport with last-good preservation and stored
   importer/backend/settings recipe;
5. creator Asset Browser workflow and repeated command-backed placement, owner
   acceptance using representative animated/skinned FBX;
6. packaged Save/Open/reimport/Build Windows Game/standalone acceptance using
   FBX, with GLB/GLTF regression retained.

Imported FBX animation data must survive and be usable, but LP07 does not build
animation timelines, state machines, retargeting UI or gameplay animation
controllers. Those remain later programmes.

## Existing foundations LP07 must reuse

- current `ImportService` GLB/GLTF isolated conversion/WISCENE round-trip path;
- pinned Wicked `ImportModel_FBX()` and other exact converter seams only through
  a Renegade-owned format-neutral import service;
- `PlaceImportedModelCommand` for command-backed scene placement;
- `AssetBrowserService` for safe project Content enumeration/classification;
- LP05 dependency collection;
- LC01 asset registry/provenance/recovery;
- `ProjectDocumentTransaction`-style fail-closed persistence discipline;
- LP06 Build Windows Game and package validation.

Do not create parallel identity, dependency, importer or packaging systems.

## Repository rules

- Renegade Studio owns the UX; do not expose or embed stock Wicked Editor
  windows.
- UI code should call Renegade-owned services; persistent semantics stay out of
  chrome callbacks.
- Persistent scene mutations require command-backed Undo/Redo and Save/Open
  evidence.
- A visible/behavioural failure overrides green CI.
- Hosted GPU/audio limitations are documented, not 'fixed' by modifying Wicked.
- Wicked pin stays
  `3a800b7134aafe58461093c8abb2e274d4e64033` until an explicit upstream/core
  decision changes it.
- Keep lifecycle slices bounded and independently auditable.
- Do not merge an implementation gate without exact-head CI, owner acceptance
  where behavioural/visual, and independent review.
