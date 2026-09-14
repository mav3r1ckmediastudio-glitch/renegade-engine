# Renegade Character & AI implementation handoff

## Baseline

- Original accepted Phase 7 baseline: `d36918878776d0d91e0c39f88f6764a1926a6534`.
- Programme branch: `feature/character-ai-programme`.
- Recovery-document parent before AI reconstruction: `473f311dd7960bf3763ec364c400e28ab9929519`.
- Pinned Wicked revision recorded by the recovery audit: `3a800b7134aafe58461093c8abb2e274d4e64033`.
- Current gate: **AI-01 — Character foundation**.
- Current AI-01 status: **IMPLEMENTED / AUDIT REPAIRS APPLIED / WINDOWS EXECUTABLE VALIDATION PENDING**.

The original Codex AI-01→AI-05 workspace was lost before its code reached GitHub. Its recovery notes remain useful architectural evidence, but only code present on this branch is authoritative.

## Gate status

### AI-01 — Character foundation

Status: **IMPLEMENTED / AUDIT REPAIRS APPLIED / WINDOWS EXECUTABLE VALIDATION PENDING**.

Reconstructed on the recoverable branch:

- `EngineBridge/include/renegade/bridge/CharacterService.h`
- `EngineBridge/src/CharacterService.cpp`
- `Studio/src/AICharacterInspector.h`
- `Studio/src/AICharacterInspector.cpp`
- Runtime Character discovery/state integration in `RuntimeApplication.h` and `RuntimeLiveDiagnostics.cpp`
- `Tests/CharacterAiFoundationTests.cpp`
- `Tests/CharacterAiFoundation.cmake`
- `Tests/CharacterAiSourceContract.cmake`
- CMake registration for Bridge, Studio and focused AI-01 tests.

Implemented contracts:

- versioned Character authoring in native Wicked `MetadataComponent` values;
- durable identity through existing `IdentityService` stable IDs;
- command-backed MAKE CHARACTER, REMOVE CHARACTER and settings edits;
- non-destructive adoption of a pre-existing Wicked `CharacterComponent`;
- Renegade ownership marker when MAKE CHARACTER creates the native controller;
- imported/pre-existing native controller active state restored on removal/Undo;
- Renegade-created Character controller remains inactive in Studio and is activated by Runtime;
- creator-facing Character Inspector using the existing `InspectorSectionFramework`;
- AI-01 creator controls for type, role, personality, common faction, animation-set stable ID and autonomous flag;
- Runtime discovery from serialized Character metadata without a second Scene or actor world;
- deterministic Runtime reset/teardown on non-Level transitions;
- existing diagnostics service publishes an `ai` summary with Character count, active count, first stable Character ID, scene-sync state and explicit sync-failure state;
- native entity serialization coverage for Metadata + CharacterComponent persistence;
- focused Undo/Redo coverage, including preservation of unrelated imported metadata.

Audit repairs applied after the initial reconstruction:

1. Runtime Character synchronization now distinguishes **attempted** scene revision from **successfully synchronized** scene revision. A failed Character initialization no longer reports `scene_synced=true`, and an explicit attempt flag preserves valid startup when the Scene revision is `0`.
2. Character Runtime initialization is transactional: every authored Character is validated before any native controller is activated, preventing partial-prefix activation on a later invalid Character.
3. Duplicate persistent Character IDs are rejected explicitly.
4. Runtime Character records are ordered by stable Renegade Character ID rather than transient Wicked ECS allocation order.
5. Player Start, Navigation Grid and Navigation Destination entities are blocked from MAKE CHARACTER promotion. Navigation Agents remain intentionally promotable/adoptable because they can validly own a native `CharacterComponent`.
6. REMOVE CHARACTER now snapshots the serialized native `CharacterComponent` authoring state through Wicked's own serializer and separately preserves foot-placement, which the pinned Wicked serializer does not persist. Undo therefore restores the controller state instead of recreating only MAKE CHARACTER defaults.
7. The Runtime Character state is explicitly deactivated/cleared when gameplay leaves a Level for a Screen destination.
8. Focused regressions now cover reserved semantic rejection, stable-ID order, duplicate-ID atomic failure, invalid-character atomic failure, exact owned-controller REMOVE/Undo/Redo restoration and reusable snapshot Undo after Redo.

Important AI-01 design decision: MAKE CHARACTER is deliberately non-destructive. If a selected hierarchy already owns a native Wicked `CharacterComponent`, Renegade adopts it and records the previous active state. If Renegade creates the controller, it records ownership so REMOVE CHARACTER and Undo remove only Renegade-owned native state.

### AI-02 — Profiles, factions and runtime state

Status: **NOT RECONSTRUCTED YET**.

Recovery notes from the lost workspace indicate the intended implementation used layered role/personality/skill/awareness tuning, optional persisted overrides, a faction relationship matrix, transient Runtime records, stable reference resolution and deterministic reset. Re-audit against the current AI-01 code before recreating it.

### AI-03 — Perception and memory

Status: **NOT RECONSTRUCTED YET**.

Recovery target: staggered cognition, bounded visual/hearing perception, legitimate stimulus-only last-known-position updates, memory decay, suspicion/awareness, and explicit anti-cheat tests proving hidden target transforms cannot refresh memory.

### AI-04 — Decision runtime and patrol

Status: **NOT RECONSTRUCTED YET**.

Recovery target: utility-scored intents with hysteresis, native `NavigationService` goals, Wicked `CharacterComponent::Turn/Move`, patrol authoring with stable references, search timeout and return-to-role.

### AI-05 — Combat intelligence

Status: **NOT RECONSTRUCTED YET**.

Recovery target: weapon descriptors, ammo/reload/range reasoning, combat intents, health/damage seam and AI gameplay events. Preserve a small reusable combat/health boundary; do not bury rifle-specific damage authority inside cognition.

## Architectural notes

- Studio owns authoring only; it does not run production cognition.
- EngineBridge owns stable Renegade Character semantics.
- Runtime owns transient execution state.
- Wicked remains authoritative for Scene/ECS and native `CharacterComponent` physics/movement.
- Stable Renegade IDs are the durable reference format; transient Wicked entity IDs are Runtime caches only.
- AI-01 does not introduce a navigation system, physics world, animation runtime, event bus, Lua brain or transform-driven NPC movement.
- Animation presentation remains AI-06 and must reuse the accepted Phase 7 animation/humanoid/IK stack.
- The Runtime player is a character-physics rigid body rather than a Wicked `Scene::characters` NPC component; AI-03 must use the existing player physics-position seam for perception rather than assuming a Scene CharacterComponent.

## Validation evidence

Initial reconstruction validation performed in the recovery session:

- `cmake -DRENEGADE_SOURCE_DIR=/mnt/data/ai01repo -P /mnt/data/ai01repo/Tests/CharacterAiSourceContract.cmake` — **PASS** before the later audit-repair commit.
- static delimiter/structure audit for reconstructed C++ files — **PASS** before the later audit-repair commit.
- API contract audit against current repository sources — **PASS** for existing `ComponentManager::Create/Remove`, metadata erase/removal, stable IdentityService APIs, `CharacterComponent::SetActive/IsActive/SetPosition/SetFacing`, `CommandService`, Inspector section registration and Runtime diagnostics ownership patterns.
- source whitespace sanity check performed on the reconstruction workspace — **PASS** before the later audit-repair commit.

Audit-repair review evidence:

- repair code was checked against the pinned Wicked `CharacterComponent::Serialize(wi::Archive&, EntitySerializer&)` contract;
- the pinned serializer persists `_flags`, health, width, height and scale; foot-placement is nonserialized and is therefore captured/restored explicitly by the command;
- revision-zero Runtime startup semantics were preserved with a separate `characterSceneAttempted_` flag rather than treating revision `0` as proof that no attempt occurred;
- focused test coverage was expanded to exercise every material audit finding;
- source contract was expanded to require duplicate-ID rejection, stable-ID ordering, reserved semantic guards, owned-controller restoration, explicit Runtime sync-failure state, revision-zero discovery and Runtime teardown.

Not yet claimed after the audit repairs:

- `RenegadeCharacterAiFoundationTests` executable run on Windows;
- post-repair `RenegadeCharacterAiSourceContract` execution in a full checkout;
- full `RenegadeStudio` Debug/Release compile;
- owner test inside Studio/Test Level.

The current ChatGPT execution environment does not contain a full buildable Renegade + Wicked checkout, so it cannot honestly provide those binary results. Do not mark AI-01 COMPLETE until the focused executable and normal Windows build path have run successfully.

Historical baseline note: the lost Codex Linux checkout reached `366/393` before a pre-existing DirectXMath WinAdapter `SetLastError(ERR)` macro collision with `SceneService::SetLastError(std::string error)` blocked the Linux full build. That is not evidence for or against this reconstructed AI-01 Windows build.

## CI / recovery workflow

- Normal pushes to `feature/character-ai-programme` do not match the expensive Windows workflow push branches discovered in the recovery audit.
- Keep each AI gate remotely checkpointed on this branch.
- Do not open the integrated AI PR merely to checkpoint work; opening the PR triggers the intended Windows workflows.
- Planned major remote CI remains AI-05 and AI-10 if practical.

## NEXT IMPLEMENTATION STEP

Before beginning AI-02, validate the repaired AI-01 on a build-capable checkout:

1. build `RenegadeCharacterAiFoundationTests` and run it;
2. run `RenegadeCharacterAiSourceContract`;
3. build the relevant EngineBridge/Studio targets locally if available;
4. fix any compile/test defect on this same branch and checkpoint it;
5. once AI-01 focused validation is green, reconstruct AI-02 Profiles/Factions/Runtime State from the implementation specification and these recovery notes.

Do not skip directly to AI-06 and do not treat source-contract success alone as final gate acceptance.
