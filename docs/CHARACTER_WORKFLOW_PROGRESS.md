# Character workflow implementation progress

## Recovery authority

- **Authoritative specification:** `docs/RENEGADE_CHARACTER_WORKFLOW_IMPLEMENTATION_SPEC.md`
  (SHA-256: `dff426869be84ddeecbb98511d96126bbf9a4fa6d83bb6a588767a5c536d43d5`).
- **Programme branch:** `feature/character-workflow-programme`.
- **Accepted starting baseline:** `cc31ef318ec55db5d115b48a0242665ce571c561`
  from AI-05 PR #158, based on Phase 7 baseline
  `d36918878776d0d91e0c39f88f6764a1926a6534`.
- **CW-03 final checkpoint:** `24d2766cdaecd7b4cbdef29e24e23574d9d4c03f`.
- **CW-04 implementation/source-contract checkpoint:**
  `7e0dbad19cde7c8a07e98078fd710ee255545593`.
- **Latest CW-05 implementation checkpoint before this progress update:**
  `373285f0a02492abe414dbc6a56257298a1d4c15`.

## Gate state

| Gate | Status | Notes |
| --- | --- | --- |
| CW-00 | Complete | Live branch, PR #158 checks, workflow triggers and the import/asset/placement architecture were audited. |
| CW-01 | Complete / source-validated | The normal importer persists the Model/Character designation, commits Character assets to `Content/Characters`, and registers a source contract for that path. |
| CW-02 | Complete / source-validated | Character import accepts queued external WISCENE/FBX/GLTF/GLB/VRM/VRMA animation sources, retains governed provenance and has a registered regression target. |
| CW-03 | Complete / source-validated | Character import validates/prepares Wicked humanoid mapping and retargets included external animation sources before final Character Asset commit; governed reimport uses the same recipe path. |
| CW-04 | Complete / source-validated | Prepared Character Assets automatically become governed Character scene instances on placement with fresh identity/default gameplay settings and route directly into the Character Inspector. |
| CW-05 | Implementation complete / Windows validation pending | Character-aware duplication, portable Character Prefabs, Asset Browser placement, script companion Undo/Redo and the explicit Character Inspector save action are implemented and remotely checkpointed. Dedicated functional/source-contract targets are registered. No Windows compile/CTest result is claimed yet. |
| CW-06 to CW-08 | Not started | Begin only after CW-05 compile/test repair and owner acceptance as appropriate. |

## CW-05 implementation details

### Configured Character duplication

- The existing `DuplicateEntityCommand` remains Studio's normal Ctrl+D / Duplicate
  authority; CW-05 does not add a parallel duplication workflow.
- `CharacterDuplicateCommand.cpp` gives that command Character-aware behaviour:
  - Wicked duplication occurs first;
  - every copied persistent entity identity in the new hierarchy is replaced;
  - copied native Character controller/transient state is stripped;
  - the Character is rebuilt through the accepted `MakeCharacterCommand`;
  - authored `CharacterAuthoringSettings` and advanced AI overrides are restored;
  - Undo/Redo snapshots retain the new actor's own identity.
- `DuplicateEntityCompanionFactory` lets Studio include governed external authoring
  state in that same command. The Studio hook uses
  `DuplicateEntityScriptAttachments`, so duplicated Actions/Scripts receive fresh
  `ScriptInstanceId` values while retaining their source/configuration.

### Portable Character Prefab product

- Character Prefabs use canonical version-1 `.rcharprefab` documents under
  `Content/Prefabs` and appear through the existing `AssetType::Prefab` browser
  classification.
- A prefab stores:
  - stable base Character Asset ID;
  - portable Character gameplay/AI settings;
  - advanced AI overrides;
  - entity Action/Script templates and properties.
- It does **not** duplicate mesh/skeleton/native animation payload. Physical and
  animation preparation remains owned by the base Character Asset.
- Save is committed atomically with the LC01 asset registry through
  `ProjectDocumentTransaction`.
- The registry record retains the base Character Asset as a stable dependency.

### Reference safety

- Patrol Route and Weapon stable IDs are treated as scene-local in CW-05 and are
  cleared from the portable prefab with explicit save warnings.
- Script self-entity references are represented portably and remapped to each new
  placed Character's fresh persistent ID.
- Other script entity references are cleared with explicit warnings rather than
  silently persisting a broken level-local reference.

### Asset Browser placement

- Character Prefabs reuse the existing creator Asset Browser PLACE/drag pipeline.
- The public reusable placement preparation router detects registered Character
  Prefabs by stable asset identity, resolves the stable base Character Asset,
  prepares that accepted `.rasset`, and carries the prefab authoring layer only
  as transient in-memory metadata.
- The normal `PlaceReusableModelCommand` still owns placement. Its CW-05 companion
  callback is invoked **after** fresh Scene identity and automatic Character
  promotion, then before the outer Undo/Redo snapshot is captured.
- The Studio placement hook:
  - applies prefab Character settings and advanced overrides;
  - keeps `renegade.reusable_asset_id` bound to the physical base Character Asset;
  - stores prefab origin/base/version separately;
  - consumes/removes the transient prefab marker;
  - instantiates prefab Actions/Scripts with fresh script identities/self refs;
  - returns ScriptDocument before/after callbacks so placement Undo/Redo includes
    the `.rscripts` companion transactionally from the creator's perspective.
- A prefab containing Actions/Scripts fails closed in an unsaved Level because no
  governed `.rscripts` document can yet own those attachments; it is never placed
  while silently dropping script setup.

### Character Inspector action

- The accepted AI Character inspector implementation is wrapped rather than
  copied/rebuilt.
- A bounded default-expanded `CHARACTER PREFAB` section is registered immediately
  after `CHARACTER` and before `ADVANCED AI`.
- It exposes the explicit creator action **SAVE CHARACTER PREFAB**.
- Saving uses the selected Character name, captures the current governed script
  companion when the Level is saved, commits the prefab, and refreshes/reveals
  the Asset Browser catalogue.
- Existing Humanoid/Retarget, Animation, IK/Look-at/Expressions, Timeline and
  Advanced AI tooling remains unchanged and available.

## CW-05 regression coverage

- `RenegadeCW05CharacterPrefabTests` covers:
  - canonical prefab serialization/deserialization;
  - configured Character duplication with fresh identity;
  - advanced override preservation;
  - direct prefab placement with fresh identity;
  - script template instantiation/fresh IDs;
  - self-reference remapping;
  - Undo/Redo identity/configuration preservation;
  - portable-reference clearing/warnings.
- `RenegadeCW05CommandCompanionTests` covers the actual central command seams used
  by Studio:
  - normal `DuplicateEntityCommand` creates a fresh Character identity and does
    not leak active native controller state;
  - external duplicate companion Execute/Undo/Redo follows the same command;
  - normal `PlaceReusableModelCommand` calls its companion only after fresh
    identity and Character promotion;
  - placement companion Undo/Redo remains symmetric and preserves identity.
- `RenegadeCW05CharacterPrefabSourceContract` locks the portable product,
  reference safety, central duplicate/placement architecture, `.rscripts`
  integration, inspector action, Asset Browser policy and CMake isolation seams.
- Both CW-05 executables are dependencies of `RenegadeBridgeTests` and all CW-05
  tests are registered in the normal Windows CTest graph.

## Validation and known state

- The programme branch remains based on the accepted Phase 7 / AI-05 lineage and
  has not been merged.
- Programme-branch pushes do not trigger the expensive Windows matrices; PRs
  targeting `main` do. No full Windows CI run is claimed for CW-01 through CW-05
  at this checkpoint.
- This connector execution environment cannot perform the Windows native Studio
  build locally. Source/API integration has been audited against the current
  branch, but **compilation is not claimed until Windows CI actually compiles it**.
- Current workflow configuration still runs both Windows baseline and Renegade
  Studio Debug/Release matrices on pull requests targeting `main`.
- CW-05 owner acceptance after a green artifact should verify:
  1. configure a placed Character's faction/role/personality/combat/Actions;
  2. Ctrl+D it and confirm authored setup is retained but the actor behaves as an
     independent instance;
  3. Undo/Redo the duplicate;
  4. use **SAVE CHARACTER PREFAB**;
  5. confirm the prefab appears under `Content/Prefabs` / Asset Browser;
  6. drag/place it more than once and confirm the same authored setup with
     independent actors;
  7. confirm cleared patrol/weapon/scene-reference warnings are truthful;
  8. Save/Reopen and repeat one prefab placement/duplication check.

## Exact next task

Trigger the intentional CW-05 Windows confidence/acceptance build, inspect the
exact-head Debug/Release compile + CTest results, repair any failures on this same
programme branch, then owner-test the resulting Studio artifact. Do not begin
CW-06 or claim CW-05 green until that validation is complete.
