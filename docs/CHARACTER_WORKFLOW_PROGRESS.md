# Character workflow implementation progress

## Recovery authority

- **Authoritative specification:** `docs/RENEGADE_CHARACTER_WORKFLOW_IMPLEMENTATION_SPEC.md`
  (SHA-256: `dff426869be84ddeecbb98511d96126bbf9a4fa6d83bb6a588767a5c536d43d5`).
- **Programme branch:** `feature/character-workflow-programme`.
- **Accepted starting baseline:** `cc31ef318ec55db5d115b48a0242665ce571c561`
  from AI-05 PR #158, based on Phase 7 baseline
  `d36918878776d0d91e0c39f88f6764a1926a6534`.
- **CW-03 final checkpoint:**
  `24d2766cdaecd7b4cbdef29e24e23574d9d4c03f`.
- **Latest CW-04 implementation/source-contract checkpoint before this progress update:**
  `7e0dbad19cde7c8a07e98078fd710ee255545593`.

## Gate state

| Gate | Status | Notes |
| --- | --- | --- |
| CW-00 | Complete | Live branch, PR #158 checks, workflow triggers and the import/asset/placement architecture were audited. |
| CW-01 | Complete / source-validated | The normal importer persists the Model/Character designation, commits Character assets to `Content/Characters`, and registers a source contract for that path. |
| CW-02 | Complete / source-validated | Character import accepts queued external WISCENE/FBX/GLTF/GLB/VRM/VRMA animation sources, retains immutable governed snapshots (including local glTF dependencies), persists source/action provenance in the import recipe, and has a registered headless regression target. |
| CW-03 | Complete / source-validated | Character import validates/prepares a native Wicked humanoid mapping where appropriate and retargets included external animation sources through the existing `RetargetHumanoidAnimationsCommand` before the reusable Character asset payload is saved. The same recipe application path is used by governed reimport. |
| CW-04 | Complete / source-validated | Prepared Character Assets are classified from the durable import recipe during placement preparation, become governed Character scene instances automatically on placement, receive fresh per-instance identity and default gameplay settings, and route naturally into the existing default-expanded Character Inspector. |
| CW-05 to CW-08 | Not started | No implementation has begun for these gates. |

## CW-04 implementation details

- `PrepareModelAssetPlacement()` now reads the accepted reusable-model version-1
  recipe, extracts its creator `options`, and treats
  `CreatorAssetImportKind::Character` as the authoritative prepared-Character
  designation. Folder names are not used as placement authority.
- Character placement preparation stamps a transient
  `renegade.character_asset_template` marker onto top-level transforms in the
  in-memory prepared reusable payload. The accepted `.rasset` bytes are not
  rewritten, so Character products imported before CW-04 gain the new placement
  behaviour without a destructive product migration.
- `PlaceReusableModelCommand` detects that marker for both its prepared-scene
  constructor and its live drag-preview adoption constructor.
- Fresh reusable hierarchy persistent identities are assigned **before**
  Character promotion. The placement wrapper is then promoted through the
  already-accepted `MakeCharacterCommand` using a fresh
  `CharacterAuthoringSettings{}` value. No second controller or Character
  metadata implementation was introduced.
- The existing Wicked native `CharacterComponent` remains the controller owner.
  The prepared payload retains its skeleton, humanoid mapping, mesh, materials
  and native animation clips beneath the stable scene wrapper.
- Placement plus Character promotion is one outer
  `PlaceReusableModelCommand` Undo/Redo operation. The outer snapshot is captured
  only after identity assignment and Character promotion; Redo restores that
  same authored scene identity and Character state rather than requiring a new
  MAKE CHARACTER operation.
- Generic Model products carry no Character template marker and preserve the
  existing reusable placement behaviour unchanged.
- The existing Studio live drag/drop path already commits through
  `PlaceReusableModelCommand` and returns/selects its `PlacedEntity()`, so CW-04
  does not add a second Studio placement route.
- `AICharacterInspector` already has the CHARACTER section default-expanded and
  derives its active state from `InspectCharacterPromotion(...).alreadyCharacter`.
  Therefore selecting the newly placed wrapper immediately exposes active
  Character authoring controls instead of the normal MAKE CHARACTER setup path.

## CW-04 regression coverage

- Added `RenegadeCW04CharacterPlacementTests`, covering:
  - prepared Character Asset placement auto-promotes to a governed Character;
  - native Wicked `CharacterComponent` is present;
  - wrapper/payload persistent identities are valid and independent;
  - default gameplay authoring settings are fresh;
  - Undo/Redo preserves Character state and authored identity;
  - placing the same base Character Asset again produces a different scene
    identity and fresh default gameplay settings;
  - generic reusable Model placement is not promoted;
  - the live drag-preview adoption constructor performs the same automatic
    Character promotion without replacing the visible cursor wrapper.
- Added `RenegadeCW04CharacterPlacementSourceContract`, locking:
  - durable recipe classification at placement preparation;
  - prepared Character template marker ownership;
  - fresh identity before Character promotion;
  - reuse of `MakeCharacterCommand` with default settings;
  - the existing drag/drop command path;
  - default-expanded Character Inspector routing/active state.
- `Tests/CW04CharacterPlacement.cmake` registers both tests and keeps the
  executable in the targeted Windows Studio test build graph.

## Validation and known state

- AI-05 PR #158 head `cc31ef3` was accepted by the owner. The four required PR
  checks were green on 2026-09-14 before the Character Workflow programme
  began: Windows Debug, Windows Release, Renegade Studio Debug and Renegade
  Studio Release.
- Programme-branch pushes do not trigger the expensive Windows matrix; PRs
  targeting `main` do. No new full Windows CI run is claimed for CW-02 through
  CW-04.
- CW-02's `RenegadeCreatorExternalAnimationImportTests` target remains
  registered in the LP07 Gate 4 test chain.
- CW-03 extends the already-registered `RenegadePhase7Gate7BTests` executable
  and Phase 7B source contract.
- CW-04 adds its own compiled regression executable and source contract to the
  Windows test graph.
- A compare against the CW-03 checkpoint shows CW-04 limited to the Character
  template/placement contract, reusable placement preparation/instance command,
  root test registration and dedicated CW-04 tests; no unrelated subsystem was
  intentionally changed.
- No local native compilation is claimed from this connector-only execution
  environment. A full Windows CI should remain deferred until an intentional
  programme confidence point or owner request rather than spending a Windows
  matrix on every CW gate.
- Owner-facing CW-04 acceptance still requires a real prepared Character asset:
  drag it from Asset Browser/Characters, confirm it is immediately a Character
  with active Character Inspector controls, Undo/Redo it, then drag the same
  base Character again and confirm the two actors have independent scene setup.

## Exact next task

Begin CW-05 — Duplication and Character Prefabs. A configured Character instance
must duplicate with its authored gameplay setup but fresh persistent identity
and transient runtime state, and the user must be able to save a configured
Character as a reusable Character Prefab whose placements receive fresh scene
identity while restoring portable authored settings.
