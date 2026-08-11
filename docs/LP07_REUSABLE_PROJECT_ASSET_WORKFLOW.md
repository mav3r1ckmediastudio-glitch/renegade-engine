# LP07 — Reusable Project Asset Workflow

Status: **planned; implementation not started**.

Authoritative starting baseline:
`48126f859b2f9b25a60182c4311cfc6c91d98436`
(`Add LP06 Gate 5 safe rebuild and promotion (#44)`).

Wicked remains pinned at
`3a800b7134aafe58461093c8abb2e274d4e64033`.

## Why LP07 is next

Phase 4 requires assets to become repeatable project resources rather than
one-off file opens. The necessary backend pieces now exist, but they are not yet
assembled into a complete creator workflow:

- `ImportService` can isolate GLB/GLTF conversion and save/round-trip a reusable
  WISCENE product;
- `PlaceImportedModelCommand` can place a freshly converted model into the live
  scene with Undo/Redo and scale correction, but it explicitly does not own a
  reusable project asset;
- `AssetBrowserService` can safely enumerate/classify the project `Content`
  filesystem, but it does not yet project LC01 stable identity or provenance;
- LP05 can determine the dependency closure;
- LC01 owns stable project asset IDs, durable registry persistence,
  source/product provenance and moved/missing recovery; and
- LP06 can build only the governed reachable project content and produce a safe
  standalone Windows build.

LP07 connects those boundaries and expands Renegade's model-import surface around
the importers already present in the pinned Wicked source. It must not invent a
second asset database or casually add a parallel importer stack.

## Product priority — FBX first

FBX is the primary creator format for LP07, including **animated/skinned FBX**.
The first end-to-end reusable asset acceptance must therefore be FBX-based; GLB
or GLTF alone is no longer sufficient to close this programme.

The exact pinned Wicked revision already contains `ImportModel_FBX()` in
`Editor/ModelImporter_FBX.cpp`. That importer is based on the bundled `ufbx`
loader and already converts important FBX structures into native Wicked scene
components, including:

- meshes and material subsets;
- PBR/material texture references and embedded textures;
- skin deformers and armatures;
- up to eight bone influences per vertex;
- morph/blend-shape data;
- node hierarchy/transforms;
- cameras and lights;
- FBX animation stacks/takes baked into Wicked `AnimationComponent` and
  `AnimationDataComponent` translation/rotation/scale channels; and
- Mixamo-specific humanoid/animation-name handling.

That existing Wicked converter is the first implementation route. Renegade should
compile and call the converter through a UI-independent Renegade import service,
just as the current bridge compiles only Wicked's GLTF converter without
embedding the stock Wicked Editor application.

## Model-format policy

LP07 uses a format-priority model rather than promising every possible format at
once.

### P0 — release-blocking for LP07

- **FBX** — static, skinned and animated. This is the primary owner workflow and
  the principal packaged/manual acceptance format.

### P1 — required supported alternatives

- **GLTF / GLB** — retain and generalise the already-proven Renegade path.

### P2 — bring through the common import service when the pinned Wicked seam is
clean and evidence is available

- **OBJ**;
- **PLY**;
- **VRM / VRMA**, subject to an exact pinned-source seam audit before claiming
  Renegade support.

A P2 format may land during LP07 without becoming a reason to delay the P0 FBX
lifecycle if its upstream seam requires unrelated work.

## External importer policy — ASSIMP is a fallback, not the default

Renegade may evaluate an external importer library such as **Assimp** when a
required format or required FBX feature cannot be represented reliably through
Wicked's existing importer stack.

Do **not** add Assimp merely to increase the extension count. Adding a second
import stack creates real product costs: another dependency and notice set,
additional build/package maintenance, duplicated FBX/GLTF behaviour, and a
normalisation problem where the same conceptual material/armature/animation can
arrive differently depending on which importer handled it.

Before introducing Assimp or any other external importer, a bounded gate must
record:

1. the exact creator requirement Wicked/ufbx cannot satisfy;
2. a representative failing source asset;
3. the alternative library's proven output for that asset;
4. how its data maps deterministically into native Wicked scene components;
5. licence/notice and build/package implications; and
6. why extending or adapting the existing Wicked-owned converter seam is not the
   safer option.

If such a gap is proven, the fallback must sit behind the same Renegade import
contract so LC01 identity/provenance, reimport and LP06 packaging are importer-
agnostic.

## Programme outcome

A creator must be able to:

1. explicitly import an FBX into the project as a reusable Renegade asset;
2. for an animated/skinned FBX, retain the armature/skinning/animation content in
   the generated project product and through WISCENE reopen;
3. see the resulting product in the Asset Browser with stable identity and
   source-health/provenance state;
4. place that registered asset into one or more scenes repeatedly without
   reconverting the original external source for each placement;
5. detect that the registered source has changed;
6. explicitly reimport using the recorded importer/settings while retaining the
   stable source/product asset identities;
7. survive a failed reimport with the previous successful product still
   authoritative;
8. Save/Open and reopen the project with the same identities/references; and
9. run **Build Windows Game** successfully with the updated reachable product.

The programme is not complete until this path is proven in an assembled Release
Studio and the resulting standalone Runtime. GLB/GLTF must remain functional as a
regression/secondary format throughout.

## Architectural rules

### One authoritative asset identity system

LC01 `AssetRegistry.renegade-assets` remains authoritative for project asset IDs,
source/product provenance and recovery state. LP07 must not create a parallel
UI-only asset ID or infer identity from filenames.

### One Renegade import contract, multiple format adapters

`ImportService` becomes the Renegade-owned model-conversion boundary rather than
remaining GLTF-shaped. Format-specific adapters may call the exact pinned Wicked
converters, but Studio UI must not call `ImportModel_FBX`, `ImportModel_GLTF`, or
other Wicked Editor functions directly.

The common contract must return enough neutral evidence to validate the produced
native Wicked scene, including at minimum mesh/material/armature/animation counts
and a deterministic structural summary suitable for round-trip tests.

### Animated FBX import is not animation authoring

LP07 must preserve and prove imported animation data, clip/take identity and
runtime usability. It does **not** build the later animation timeline, state
machine, retargeting UI or gameplay animation controller. Those remain separate
programmes once the asset itself can be trusted.

### Last-good product wins

A new import/reimport candidate is not authoritative until:

- conversion succeeds;
- its reusable WISCENE product round-trips successfully;
- project containment/path rules pass;
- the intended registry/provenance update validates; and
- the file/metadata transaction commits successfully.

If any step fails, the previous successful product and registry remain
recoverable and authoritative.

### Reimport is explicit

LC01 can report stale provenance, but LP07 must not silently overwrite a creator
asset because its source hash changed. The creator explicitly requests reimport.
Background scanning may report state later; automatic destructive reimport is
outside this programme.

### Studio remains a thin adapter

Creator UI consumes UI-independent catalogue/import/reimport services. Persistent
scene placement continues through Renegade-owned command boundaries and must
retain Undo/Redo plus Save/Open evidence.

### LP05/LP06 remain authoritative downstream

LP07 does not invent a separate packaging list. Once an asset product is
registered and referenced normally, LP05 determines reachability and LP06
packages/builds that governed closure.

## Gate 1 — Common model-import seam and FBX proof

### Goal

Generalise the current GLB/GLTF-specific Renegade import boundary and prove the
pinned Wicked FBX converter before any creator-facing reusable-asset transaction
is built on it.

### Required behaviour

- add a format-neutral model-import request/result contract in `ImportService`;
- retain the established GLB/GLTF path through that contract;
- compile/call the pinned Wicked FBX converter without linking or exposing the
  stock Wicked Editor application;
- classify supported extensions deterministically;
- reject unsupported/mismatched formats explicitly;
- keep conversion isolated from the active Studio scene;
- produce a reusable WISCENE candidate and round-trip summary;
- preserve source files byte-identically.

### FBX acceptance fixtures

At minimum the automated/packaged evidence set must contain:

1. a static FBX mesh/material fixture;
2. a skinned FBX fixture with armature/bone weights;
3. an animated FBX fixture with at least one animation stack/take; and
4. preferably a representative owner/Mixamo-style animated asset once licensing
   permits it to be used as test evidence.

The tests must prove the expected armature/animation data survives the
FBX -> native Wicked scene -> WISCENE -> reopen path. A test that only proves
`ImportModel_FBX()` returned without crashing is insufficient.

### Gate 1 secondary-format assessment

During this gate, audit the exact pinned seams for OBJ, PLY and VRM/VRMA and
record which can safely join the same service without new third-party
integration. This assessment must distinguish "Wicked Editor supports it" from
"Renegade has actually compiled, tested and accepted it".

### Gate 1 acceptance

- FBX static import passes Debug/Release proof;
- FBX skinned/animated import passes structural and WISCENE round-trip proof;
- GLB/GLTF regression remains green;
- no stock Wicked Editor UI is linked/exposed;
- no creator source file is modified;
- Wicked source and submodule pin remain unchanged;
- authoritative Debug/Release and pinned-Wicked baselines pass.

## Gate 2 — Registry-backed asset catalogue

### Goal

Turn the existing filesystem Asset Browser projection into a stable,
UI-independent project catalogue that understands LC01 records and provenance.

### Required behaviour

For each current project Content entry the catalogue can expose, where
applicable:

- stable asset ID;
- canonical project-relative path;
- asset type/classification;
- source availability;
- whether it is an imported source, imported product or ordinary project asset;
- importer/settings identity for registered imported products;
- current provenance health: current, source changed, product changed, source
  missing/moved or invalid; and
- dependency/reference summary when already available from accepted LP05/LC01
  data.

Filesystem-only entries that have not yet entered the governed registry must be
represented honestly as unregistered, not assigned temporary fake IDs.

### Gate 2 exclusions

- no import or reimport transaction yet;
- no scene placement changes;
- no thumbnail generation;
- no stock Wicked Content Browser UI.

### Gate 2 acceptance

- deterministic catalogue output from a fixed project/registry fixture;
- stable IDs survive reopen and moved-source recovery;
- unregistered/missing/stale states are distinct;
- invalid cross-project/corrupt registry state fails closed;
- existing `AssetBrowserService` containment/security behaviour is retained;
- Debug/Release tests and pinned-Wicked baselines pass.

## Gate 3 — Governed reusable model-asset import transaction

### Goal

Import a model once into a reusable project-owned product rather than only
placing a transient converted scene. **FBX is the primary acceptance format.**
GLB/GLTF uses the same transaction and remains required.

### Required behaviour

A UI-free project asset import service should:

1. validate the active project and requested project destination;
2. dispatch through the accepted common `ImportService` format adapter;
3. produce and round-trip validate the reusable WISCENE product;
4. collect/refresh the relevant LP05/LC01 project state;
5. register source-to-product provenance with explicit importer/settings schema;
6. commit product and registry state through a transaction that cannot leave one
   authoritative without the other; and
7. return the stable product asset ID and project-relative product path.

The precise source-retention policy (project-owned source copy versus a governed
project source entry) must be decided in this gate before implementation. No
absolute machine-specific path may become durable project identity.

Importer identity must distinguish the format/backend/version sufficiently for a
later reimport to reproduce the accepted recipe. A future Assimp-backed adapter,
if ever approved, must therefore not masquerade as the Wicked/ufbx FBX recipe.

### Gate 3 acceptance

- first FBX import creates one stable governed source/product relationship;
- animated FBX product retains its imported armature/animation data after
  project reopen;
- first GLB/GLTF import proves the same common transaction path;
- reopen preserves exact IDs/settings/provenance;
- failed first import leaves no authoritative half-import;
- failed replacement of an existing destination preserves previous product and
  registry bytes;
- Wicked and its pin remain unchanged.

## Gate 4 — Stable explicit reimport

### Goal

Use LC01's recorded recipe to update an imported product safely when its source
changes.

### Required behaviour

- only a registered imported product can reimport;
- the source/product asset IDs remain stable across a successful reimport;
- recorded importer identity/version and canonical settings are the recipe;
- stale source status is visible before reimport;
- successful reimport updates the product and provenance hash snapshots
  transactionally;
- unsuccessful reimport leaves the last-good product and provenance state
  authoritative;
- moved-source identity recovery from LC01 is honoured rather than treated as a
  brand-new source; and
- format/backend selection comes from the stored recipe, not from re-guessing the
  extension at reimport time.

### Gate 4 acceptance

A fixed FBX fixture proves:

- source content update -> stale provenance;
- explicit reimport -> same source/product IDs, new accepted hashes;
- changed animation/geometry is represented in the new product evidence;
- project reopen -> same IDs and current status;
- injected/real conversion failure -> previous product byte-identical and still
  usable; and
- retry after correction succeeds without manual identity repair.

GLB/GLTF reimport remains a required regression case.

## Gate 5 — Creator Asset Browser and repeated placement

### Goal

Expose the accepted catalogue/import/reimport semantics through Renegade Studio.

### Required creator workflow

The Asset Browser must let the creator:

- browse project folders and filter assets;
- distinguish registered/unregistered/current/stale/missing states;
- import an FBX or another accepted model format through Renegade-owned actions;
- select a registered reusable model asset;
- invoke explicit reimport;
- place the registered product into the active scene repeatedly; and
- see useful status/error output when a source is stale, missing or ambiguous.

Repeated placement must load the registered project WISCENE product, not
reconvert the original FBX/GLTF every time.

Scene mutation remains command-backed. Each placement must Undo/Redo correctly
and persist through Save/Open.

For an animated FBX, Gate 5 needs only enough creator feedback to prove that the
registered product contains the expected animation(s). Full animation editing is
still excluded.

### Gate 5 acceptance

Owner packaged Studio acceptance proves at minimum:

- import one representative animated/skinned FBX;
- find/select it in the Asset Browser;
- place it twice;
- Undo/Redo a placement;
- Save, close and reopen;
- both retained placements resolve through the reusable project product;
- update the FBX source and observe stale state; and
- explicit reimport clears stale state without changing stable IDs.

A GLB/GLTF import/placement regression must also remain functional.

## Gate 6 — Packaged lifecycle and standalone acceptance

### Goal

Prove the entire reusable asset lifecycle crosses the real Runtime/build
boundary.

### Required proof

Using a fixed owner-test project and representative **animated/skinned FBX**:

1. import the asset as a governed project product;
2. place it in a scene and Save/Open;
3. prove the expected imported armature/animation structure survived;
4. change the source in a controlled way;
5. observe stale provenance;
6. explicitly reimport;
7. reopen and confirm stable identities/current provenance;
8. run **BUILD > BUILD WINDOWS GAME...**;
9. require LP05/LC01 canonical proofs and ordinary Studio tests to remain green;
10. launch the promoted named executable directly from Explorer; and
11. confirm Runtime uses the updated product.

A failed reimport/build scenario must also prove the prior successful product or
standalone build remains authoritative at its respective boundary.

GLB/GLTF remains part of automated regression evidence but does not substitute
for the FBX owner acceptance.

### Gate 6 acceptance

- authoritative Debug/Release CI on exact final head;
- Release real standalone package smoke remains mandatory;
- only already-accepted hosted capability skips may remain;
- pinned-Wicked baseline passes;
- owner-visible packaged Studio FBX workflow passes;
- promoted named executable passes direct launch;
- independent exact-head audit passes; and
- owner explicitly accepts and squash-merges.

## Programme exclusions

LP07 does **not** attempt to complete every possible asset class or importer. It
deliberately excludes:

- adding Assimp without a proven required capability gap;
- DCC-format parity for dozens of rarely used formats merely to increase a
  supported-extension count;
- texture/audio/video/font-specific import editors;
- universal thumbnails/previews;
- background import queues/watchers;
- animation clip editing/timeline/state-machine authoring;
- character controller/physics authoring;
- particles/effects;
- Lua gameplay framework;
- generic projectiles/magic;
- multiplayer/network-service integration; and
- commercial release packaging/signing/encryption.

LP07's job is to prove a trustworthy **FBX-first, multi-format model asset
lifecycle** end-to-end. Broader formats can then be added behind the same common
contract without destabilising identity, reimport or standalone packaging.

## Standing repository/safety rules

- Do not modify Wicked or move its pin without an explicit justified core-patch
  decision.
- Do not touch the project owner's local modification to
  `Tools/Windows-Build.Common.ps1`.
- Do not use `git clean` or `git add .` against the owner's clone.
- GitHub CI Debug/Release remains authoritative where owner hardware instability
  affects local compiler proof.
- Compilation is not behavioural proof.
- Persistent scene mutations require Undo/Redo and Save/Open evidence.
- Owner-visible/behavioural failure overrides green CI.