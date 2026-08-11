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
  source/product provenance and moved/missing recovery;
- LP06 can build only the governed reachable project content and produce a safe
  standalone Windows build.

LP07 connects those boundaries without inventing a second asset database or a
second importer.

## Programme outcome

A creator must be able to:

1. explicitly import a GLB/GLTF into the project as a reusable Renegade asset;
2. see the resulting product in the Asset Browser with stable identity and
   source-health/provenance state;
3. place that registered asset into one or more scenes repeatedly without
   reconverting the original external source for each placement;
4. detect that the registered source has changed;
5. explicitly reimport using the recorded importer/settings while retaining the
   stable source/product asset identities;
6. survive a failed reimport with the previous successful product still
   authoritative;
7. Save/Open and reopen the project with the same identities/references; and
8. run **Build Windows Game** successfully with the updated reachable product.

The programme is not complete until this path is proven in an assembled Release
Studio and the resulting standalone Runtime.

## Architectural rules

### One authoritative asset identity system

LC01 `AssetRegistry.renegade-assets` remains authoritative for project asset IDs,
source/product provenance and recovery state. LP07 must not create a parallel
UI-only asset ID or infer identity from filenames.

### One governed import boundary

GLB/GLTF conversion continues through `ImportService`. LP07 may add a service
that orchestrates project paths, transactions and LC01 registration around that
conversion, but must not duplicate Wicked's importer or bury conversion logic in
Studio UI code.

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

## Gate 1 — Registry-backed asset catalogue

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
  missing/moved or invalid;
- dependency/reference summary when already available from accepted LP05/LC01
  data.

Filesystem-only entries that have not yet entered the governed registry must be
represented honestly as unregistered, not assigned temporary fake IDs.

### Gate 1 exclusions

- no import or reimport;
- no file writes;
- no scene placement changes;
- no thumbnail generation;
- no UI redesign beyond what is required later to consume the model.

### Gate 1 acceptance

- deterministic catalogue output from a fixed project/registry fixture;
- stable IDs survive reopen and moved-source recovery;
- unregistered/missing/stale states are distinct;
- invalid cross-project/corrupt registry state fails closed;
- existing `AssetBrowserService` containment/security behaviour is retained;
- Debug/Release tests and pinned-Wicked baselines pass.

## Gate 2 — Governed GLB/GLTF asset import transaction

### Goal

Import a GLB/GLTF once into a reusable project-owned product rather than only
placing a transient converted scene.

### Required behaviour

A UI-free project asset import service should:

1. validate the active project and requested project destination;
2. call the existing `ImportService` isolated conversion path;
3. produce and round-trip validate the reusable WISCENE product;
4. collect/refresh the relevant LP05/LC01 project state;
5. register source-to-product provenance with explicit importer/settings schema;
6. commit product and registry state through a transaction that cannot leave one
   authoritative without the other; and
7. return the stable product asset ID and project-relative product path.

The precise source-retention policy (project-owned source copy versus a governed
project source entry) must be decided in this gate before implementation. No
absolute machine-specific path may become the durable project identity.

### Gate 2 failure contract

Conversion failure, malformed source, output write failure, registry validation
failure or transaction interruption must not destroy an earlier valid product
or commit provenance that claims an import succeeded when it did not.

### Gate 2 acceptance

- first import creates one stable governed source/product relationship;
- reopen preserves exact IDs/settings/provenance;
- external dependencies from `.gltf` remain governed by LP05 rather than being
  silently omitted;
- failed first import leaves no authoritative half-import;
- failed replacement of an existing destination preserves previous product and
  registry bytes;
- Wicked and its pin remain unchanged.

## Gate 3 — Stable explicit reimport

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
  brand-new source.

### Gate 3 acceptance

A fixed fixture proves:

- source content update -> stale provenance;
- explicit reimport -> same source/product IDs, new accepted hashes;
- project reopen -> same IDs and current status;
- injected/real conversion failure -> previous product byte-identical and still
  usable;
- retry after correction succeeds without manual identity repair.

## Gate 4 — Creator Asset Browser and repeated placement

### Goal

Expose the accepted catalogue/import/reimport semantics through Renegade Studio.

### Required creator workflow

The Asset Browser must let the creator:

- browse project folders and filter assets;
- distinguish registered/unregistered/current/stale/missing states;
- select a registered reusable model asset;
- invoke explicit import/reimport through Renegade-owned actions;
- place the registered product into the active scene repeatedly;
- see useful status/error output when a source is stale, missing or ambiguous.

Repeated placement must load the registered project product, not reconvert the
original GLB/GLTF every time.

Scene mutation remains command-backed. Each placement must Undo/Redo correctly
and persist through Save/Open.

### Gate 4 exclusions

No stock Wicked Editor windows. No fake disabled controls implying unsupported
FBX/OBJ/VRM behaviour. No automatic destructive reimport.

### Gate 4 acceptance

Owner packaged Studio acceptance proves at minimum:

- import one model;
- find/select it in the Asset Browser;
- place it twice;
- Undo/Redo a placement;
- Save, close and reopen;
- both retained placements resolve through the reusable project product;
- update the source and observe stale state;
- explicit reimport clears stale state without changing stable IDs.

## Gate 5 — Packaged lifecycle and standalone acceptance

### Goal

Prove the entire reusable asset lifecycle crosses the real Runtime/build
boundary.

### Required proof

Using a fixed owner-test project and representative GLB/GLTF asset:

1. import the asset as a governed project product;
2. place it in a scene and Save/Open;
3. change the source in a controlled way;
4. observe stale provenance;
5. explicitly reimport;
6. reopen and confirm stable identities/current provenance;
7. run **BUILD > BUILD WINDOWS GAME...**;
8. require LP05/LC01 canonical proofs and ordinary Studio tests to remain green;
9. launch the promoted named executable directly from Explorer; and
10. confirm Runtime uses the updated product.

A failed reimport/build scenario must also prove the prior successful product or
standalone build remains authoritative at its respective boundary.

### Gate 5 acceptance

- authoritative Debug/Release CI on exact final head;
- Release real standalone package smoke remains mandatory;
- only already-accepted hosted capability skips may remain;
- pinned-Wicked baseline passes;
- owner-visible packaged Studio workflow passes;
- promoted named executable passes direct launch;
- independent exact-head audit passes;
- owner explicitly accepts and squash-merges.

## Programme exclusions

LP07 does **not** attempt to complete all Phase 4 asset classes. It deliberately
excludes:

- FBX, OBJ, PLY, VRM/VRMA parity;
- texture/audio/video/font-specific import editors;
- universal thumbnails/previews;
- background import queues/watchers;
- animation clip/state-machine authoring;
- character controller/physics authoring;
- particles/effects;
- Lua gameplay framework;
- generic projectiles/magic;
- multiplayer/network-service integration;
- commercial release packaging/signing/encryption.

Those are later programmes. LP07's job is to prove one complete, reusable model
asset lifecycle end-to-end so those systems can consume a trustworthy asset
foundation.

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
