# LP07 — Reusable Project Asset Workflow

Status: **Gates 1–5 accepted and merged; Gate 6 implementation and acceptance proof are complete in PR #51. Final exact-head CI, independent audit, owner acceptance and merge remain required.**

Authoritative starting main baseline:
`defb8b55832e84be8bbf238531f8a5110d129e06`
(`Reconcile LP06 close-out and define LP07 asset workflow (#45)`).

Wicked remains pinned at
`3a800b7134aafe58461093c8abb2e274d4e64033`.

Architectural authority for the public asset/content model:
`docs/adr/0004-renegade-asset-model-and-managed-metadata.md`.

Gate 6 implementation/acceptance detail is recorded in
`docs/LP07_GATE6_PACKAGED_RUNTIME_ACCEPTANCE.md`.

## Why LP07 is next

Phase 4 requires imported content to become repeatable project resources rather
than one-off file opens. The backend foundations now exist, but they are not yet
assembled into a complete creator workflow:

- `ImportService` can isolate model conversion and prove native Wicked data
  survives serialization/reopen;
- `PlaceImportedModelCommand` can place freshly converted content into the live
  scene with Undo/Redo and scale correction, but it does not yet own a permanent
  reusable Renegade asset;
- `AssetBrowserService` can safely enumerate/classify the project `Content`
  filesystem, but it is not yet a registry/metadata-driven content catalogue;
- LP05 determines dependency closure;
- LC01 owns stable project asset IDs, durable registry persistence,
  source/product provenance and moved/missing recovery; and
- LP06 builds governed reachable project content into a safe standalone Windows
  build.

LP07 connects those boundaries and establishes the first real Renegade imported
asset lifecycle.

## Product priority — FBX first

FBX is the primary creator format for LP07, including **skinned and animated
FBX**. GLB/GLTF alone cannot close this programme.

The exact pinned Wicked revision already contains `ImportModel_FBX()` in
`Editor/ModelImporter_FBX.cpp`. It uses the bundled `ufbx` loader and converts
important FBX structures into native Wicked components, including meshes,
materials/textures, armatures/skinning, up to eight bone influences per vertex,
morph/blend-shape data, hierarchy/transforms, cameras/lights and animation
stacks/takes baked into Wicked animation channels. Mixamo-specific handling also
exists.

The existing Wicked converter is therefore the first implementation route.
Renegade compiles/calls only the converter through a UI-independent Renegade
service; it does not expose or link the stock Wicked Editor UI.

## Model-format policy

### P0 — release-blocking for LP07

- **FBX** — static, skinned and animated. Primary owner/manual/package
  acceptance format.

### P1 — required supported alternatives

- **GLTF / GLB** — retain and generalise the already-proven Renegade path.

### P2 — adopt behind the same common contract when the pinned Wicked seam is
clean and evidence is available

- **OBJ**;
- **PLY**;
- **VRM / VRMA**, subject to an exact pinned-source seam audit before claiming
  Renegade support.

A P2 format may land during LP07 without delaying P0 FBX acceptance when its
upstream seam requires unrelated work.

## External importer policy — Assimp is a fallback, not the default

Renegade may evaluate Assimp or another external importer when a required format
or required FBX feature cannot be represented reliably through Wicked's existing
importer stack.

Do **not** add an external importer merely to increase extension count. Before
introducing one, a bounded gate must record:

1. the exact creator requirement Wicked/ufbx cannot satisfy;
2. a representative failing source asset;
3. the alternative library's proven output for that asset;
4. how that output maps deterministically into native Wicked scene components;
5. licence/notice/build/package implications; and
6. why extending/adapting the existing Wicked-owned seam is not safer.

Any accepted fallback sits behind the same Renegade import contract so LC01
identity/provenance, reimport and LP06 packaging remain importer-agnostic.

## Public content model

ADR 0004 separates creator-facing concepts from Wicked serialization details:

```text
external source         imported asset        game definition        scene
Knight.fbx      ->       Knight.rasset    ->   Knight.rentity    ->   authored scene
```

For LP07:

- external FBX/GLTF/etc. is source/reimport input;
- `.rasset` is the intended Renegade-owned reusable imported-asset abstraction;
- existing authored scene `.wiscene` files remain valid and are not renamed by
  this programme;
- `.rentity` is the future game-ready/placeable definition layer and is not a
  requirement for LP07 model-asset acceptance; and
- future cooked/compressed/encrypted distribution packages remain outside LP07.

WISCENE remains valid as an **internal Wicked serialization/proof payload**.
Gate 1 deliberately continues to use WISCENE round-trip proof because it tests
whether imported native Wicked mesh/skin/animation data survives serialization.
That does not make `.wiscene` the permanent public imported-asset type.

## Metadata and creator-authoring policy

Metadata is persistence and indexing state, **not the normal user interface**.
Every ordinary creator-facing option supported by Renegade must be editable in
Studio rather than requiring manual text-file editing.

`.rmeta` keeps its existing identity/document-metadata role and is not repurposed
as an FPE-style creator definition.

System-derived metadata may include:

- stable asset ID and type;
- source format;
- importer/backend/version;
- source/product hashes and provenance health;
- mesh/material counts;
- static/skinned/animated classification;
- armature/bone and animation clip/take information;
- morph-target presence;
- dependencies/references; and
- current/stale/missing/moved/invalid source state.

Creators may add semantic tags that importers cannot infer, for example
`medieval`, `enemy`, `knight`, `weapon` or `architecture`.

Future game-facing assignments such as collision, animation slots, audio,
effects, material overrides and gameplay components belong to Renegade
asset/entity authoring UI and are persisted automatically by the owning service;
manual metadata editing is never required for the supported workflow.

## Programme outcome

A creator must be able to:

1. explicitly import an FBX into the project as a reusable Renegade asset;
2. retain armature/skinning/animation data for animated/skinned FBX;
3. see the asset in a registry/metadata-driven Asset Browser with stable
   identity, searchable metadata/tags and source-health state;
4. place the registered asset repeatedly without reconverting the source;
5. detect that its registered source changed;
6. explicitly reimport using the recorded importer/settings recipe while
   retaining stable IDs;
7. survive failed reimport with the prior successful asset still authoritative;
8. Save/Open/reopen with the same identities/references; and
9. Build Windows Game successfully with the updated reachable asset.

The programme is not complete until the FBX path is proven in assembled Release
Studio and the resulting standalone Runtime. GLB/GLTF remains a required
regression path throughout.

## Architectural rules

### One authoritative asset identity system

LC01 `AssetRegistry.renegade-assets` remains authoritative for project asset IDs,
source/product provenance and recovery state. LP07 must not create a parallel
UI-only ID or infer identity from filenames.

### One Renegade import contract, multiple format adapters

`ImportService` is the Renegade-owned model-conversion boundary. Studio UI must
not call Wicked format converters directly.

The common contract returns neutral evidence sufficient to validate imported
native Wicked data, including structural and rig/animation evidence for FBX.

### WISCENE proof is not the public asset format

Gate 1's WISCENE candidate/reopen remains a valid implementation proof. The
permanent reusable-asset transaction after Gate 1 targets the Renegade-owned
`.rasset` abstraction. Its versioned container/serialization design must be
bounded and migration-capable rather than implemented as a blind extension
rename.

### Metadata is editor-owned

Studio edits supported asset properties through Renegade services/commands and
validation. Persistence updates automatically on Save/commit. Raw metadata
editing may exist later as an advanced diagnostic feature but is not required
creator workflow.

### Asset Browser is catalogue-driven

The creator-facing browser presents Renegade's registry/metadata view of project
content rather than merely mirroring directories. Filesystem enumeration remains
for safe discovery and unregistered entries, but stable IDs and metadata drive
registered asset status, search and filtering.

### Animated FBX import is not animation authoring

LP07 preserves and proves imported animation data, clip/take identity and runtime
usability. Animation timelines, state machines, retargeting UI and gameplay
animation controllers remain later programmes.

### Last-good product wins

A new import/reimport candidate is not authoritative until conversion,
serialization/container validation, project containment, registry/provenance
validation and the file/metadata transaction all succeed. Failure preserves the
prior product and registry.

### Reimport is explicit

LC01 may report stale provenance, but LP07 never silently overwrites an asset
because source bytes changed. The creator explicitly requests reimport.

### Stable references beat paths

Registered assets and future entity definitions reference durable Renegade IDs.
Paths are useful display/recovery hints, not identity authority.

### LP05/LP06 remain authoritative downstream

LP07 does not invent a separate packaging list. LP05 determines reachability and
LP06 packages/builds the governed closure.

## Gate 1 — Common model-import seam and FBX proof

### Goal

Generalise the current GLB/GLTF-specific import boundary and prove the pinned
Wicked FBX converter before permanent reusable-asset persistence is built on it.

### Required behaviour

- format-neutral model-import request/result contract;
- retain GLB/GLTF through the same contract;
- compile/call pinned Wicked FBX converter without stock Wicked Editor UI;
- deterministic supported-extension classification;
- explicit unsupported/mismatched-format rejection;
- isolated conversion, never mutation of the active Studio scene;
- WISCENE candidate + reopen proof as an **internal preservation test**;
- source files preserved byte-identically;
- rig/animation evidence capable of detecting data loss through round-trip.

### FBX acceptance fixtures

At minimum the evidence set contains:

1. static FBX mesh/material fixture;
2. skinned FBX fixture with armature/bone weights;
3. animated FBX fixture with at least one animation stack/take; and
4. preferably a representative owner/Mixamo-style animated asset when licensing
   permits it as retained test evidence.

A test that only proves `ImportModel_FBX()` returned without crashing is
insufficient.

### Secondary-format assessment

Audit exact pinned seams for OBJ, PLY and VRM/VRMA and record what can safely
join the common service without new third-party integration. Distinguish
"Wicked supports it" from "Renegade has compiled, tested and accepted it".

### Gate 1 acceptance

- Debug builds/links the real graphics-proof target and passes the
  format-neutral/headless import contract tests;
- Release executes the real static plus skinned/animated FBX graphics proof,
  including structural, rig/animation and animation take/clip-name preservation
  through WISCENE reopen;
- GLB/GLTF regression stays green;
- no source mutation;
- no stock Wicked Editor UI;
- Wicked source/pin unchanged;
- authoritative Debug/Release and pinned-Wicked baselines pass.

## Gate 2 — Registry/metadata-backed asset catalogue

### Goal

Turn the existing filesystem Asset Browser projection into a stable,
UI-independent Renegade content catalogue.

### Required behaviour

For registered project content expose, where applicable:

- stable asset ID;
- canonical project-relative path;
- Renegade asset type/classification;
- source format and importer identity;
- static/skinned/animated and other reliable derived model metadata;
- source availability and provenance health;
- current/stale/missing/moved/invalid state;
- dependency/reference summary from accepted LP05/LC01 data; and
- creator semantic tags through one governed metadata model.

The catalogue must support deterministic query/filter semantics over both system
metadata and creator tags. Examples include model type, FBX source, animated,
skinned, current/stale state and semantic tags such as `medieval`.

Filesystem-only entries that have not entered the governed registry remain
honestly **unregistered** and receive no temporary fake identity.

### Gate 2 exclusions

- no import/reimport transaction yet;
- no scene placement changes;
- no universal thumbnail generation;
- no stock Wicked Content Browser UI.

### Gate 2 acceptance

- deterministic catalogue output from fixed project/registry fixtures;
- stable IDs survive reopen/moved-source recovery;
- unregistered/current/stale/missing states are distinct;
- deterministic metadata/tag searches return expected assets;
- corrupt/cross-project state fails closed;
- existing `AssetBrowserService` containment/security behaviour retained;
- Debug/Release and pinned-Wicked baselines pass.

## Gate 3 — Governed `.rasset` import transaction

### Goal

Import a model once into a permanent Renegade-owned reusable asset rather than a
transient converted scene or user-facing WISCENE product. **FBX is primary;
GLB/GLTF remains required.**

### Required behaviour

Before implementation, Gate 3 defines and versions the minimum `.rasset`
container/serialization contract. It may reuse Wicked WISCENE serialization as
its initial heavy payload, but the public product is a Renegade asset and must be
validated as such.

A UI-free project asset import service then:

1. validates active project and destination;
2. dispatches through accepted common `ImportService`;
3. validates imported native data using Gate 1 evidence;
4. writes/round-trips the versioned `.rasset` candidate;
5. collects/refreshes relevant LP05/LC01 state;
6. records source-to-product provenance and canonical importer/settings recipe;
7. commits asset product + metadata/registry state transactionally; and
8. returns stable product asset ID and project-relative `.rasset` path.

No absolute machine-specific path becomes durable identity. The source-retention
policy must be explicitly decided in this gate.

### Gate 3 acceptance

- first FBX import creates one stable governed source -> `.rasset` relationship;
- animated FBX asset retains armature/animation data after reopen;
- GLB/GLTF proves the same transaction;
- reopen preserves IDs/settings/provenance;
- failed first import leaves no half-import;
- failed replacement preserves prior asset and registry bytes;
- `.rasset` validation rejects wrong/corrupt/version-incompatible containers;
- Wicked and its pin remain unchanged.

## Gate 4 — Stable explicit reimport

### Goal

Use LC01's recorded recipe to replace an imported asset payload safely when its
source changes without changing the asset's identity.

### Required behaviour

- only a registered imported asset can reimport;
- source/product IDs remain stable;
- stored importer/backend/version/settings are authoritative recipe;
- stale status is visible before reimport;
- successful reimport transactionally replaces `.rasset` payload and updates
  derived metadata/provenance;
- failed reimport leaves last-good `.rasset` and provenance authoritative;
- moved-source recovery is honoured;
- format/backend comes from stored recipe, not extension re-guessing.

### Gate 4 acceptance

A fixed FBX fixture proves source update -> stale -> explicit reimport -> same IDs
+ new accepted payload/hash/metadata -> reopen current. A forced conversion or
commit failure leaves the previous `.rasset` byte-identical and usable, and a
retry succeeds without manual identity repair. GLB/GLTF remains regression proof.

## Gate 5 — Creator Asset Browser, metadata authoring and repeated placement

### Goal

Expose accepted catalogue/import/reimport semantics through Renegade Studio as a
modern creator workflow.

### Required creator workflow

The Asset Browser must let the creator:

- browse and search Renegade project content;
- filter by type, source format, animation/skinning state and source health;
- search creator tags as well as system-derived metadata;
- distinguish registered/unregistered/current/stale/missing/moved states;
- import FBX or another accepted model format;
- select a registered `.rasset`;
- invoke explicit reimport;
- edit supported creator metadata/tags through Studio rather than text files;
- place a registered asset repeatedly; and
- see useful status/validation errors.

Repeated placement loads the registered `.rasset`; it does not reconvert the
original FBX/GLTF. Scene mutation remains command-backed with Undo/Redo and
Save/Open.

LP07 does not require `.rentity` authoring yet. Direct model-asset placement is
sufficient for this programme; the future entity layer will add game-ready
composition without changing imported asset identity.

### Gate 5 acceptance

Owner packaged Studio acceptance proves at minimum:

- import representative animated/skinned FBX;
- find it by name and by at least one metadata/tag query;
- inspect system-derived model/animation/source state;
- add/edit a creator tag through Studio and reopen with it preserved;
- place the asset twice;
- Undo/Redo placement;
- Save/close/reopen;
- both placements resolve through the reusable `.rasset`;
- update source and observe stale state;
- explicit reimport clears stale state without changing stable IDs.

GLB/GLTF import/placement remains functional.

## Gate 6 — Packaged lifecycle and standalone acceptance

### Goal

Prove the reusable Renegade asset lifecycle crosses the real Runtime/build
boundary.

### Required proof

Using an owner-test project and representative **animated/skinned FBX**:

1. import source as governed `.rasset`;
2. locate/inspect it through catalogue metadata;
3. place it in a scene and Save/Open;
4. prove armature/animation structure survived;
5. change source in a controlled way;
6. observe stale provenance;
7. explicitly reimport;
8. reopen and confirm stable identity/current metadata;
9. run **BUILD > BUILD WINDOWS GAME...**;
10. retain LP05/LC01 canonical proofs and ordinary Studio tests;
11. launch promoted named executable directly; and
12. confirm Runtime uses the updated asset.

A failed reimport/build case must prove the previous successful asset or
standalone build remains authoritative at its respective boundary.

### Gate 6 acceptance

- exact-final-head Debug/Release CI passes;
- Release real standalone package smoke remains mandatory;
- only accepted hosted capability skips remain;
- pinned-Wicked baseline passes;
- owner-visible Studio FBX workflow passes;
- promoted named executable direct launch passes;
- independent exact-head audit passes; and
- owner explicitly accepts/merges.

Gate 6 now implements the required stable scene wrapper, LP05/LC01 dependency
projection, editor-only source freshness, LP06 packaging, package-relative
Runtime `.rasset` refresh, last-good failure preservation, and direct named
executable proof. See `docs/LP07_GATE6_PACKAGED_RUNTIME_ACCEPTANCE.md` for the
implementation and test contract. Only the final exact-head process gates above
remain before merge.

## Programme exclusions

LP07 deliberately excludes:

- adding Assimp without a proven required capability gap;
- broad extension-count parity for rarely needed formats;
- full texture/audio/video/font-specific import editors;
- universal thumbnail/preview generation;
- background destructive reimport;
- animation timeline/state-machine/retargeting authoring;
- full `.rentity` game-object authoring;
- `.rscene` migration of existing WISCENE scenes;
- character controller/physics authoring;
- particles/effects;
- Lua gameplay framework;
- projectiles/magic;
- multiplayer/network integration; and
- commercial cooking/signing/encryption.

LP07's job is to prove a trustworthy **FBX-first, metadata-driven Renegade model
asset lifecycle** end-to-end.

## Standing repository/safety rules

- Do not modify Wicked or move its pin without an explicit justified core-patch
  decision.
- Do not touch the owner's local modification to
  `Tools/Windows-Build.Common.ps1`.
- Never use `git clean` or `git add .` against the owner's clone.
- GitHub CI Debug/Release remains authoritative where owner hardware instability
  affects local compiler proof.
- Compilation is not behavioural proof.
- Persistent scene mutations require Undo/Redo and Save/Open evidence.
- Owner-visible/behavioural failure overrides green CI.