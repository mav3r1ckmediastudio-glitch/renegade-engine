# Renegade Character & AI implementation handoff

## Baseline

- Original accepted Phase 7 baseline: `d36918878776d0d91e0c39f88f6764a1926a6534`.
- Programme branch: `feature/character-ai-programme`.
- Recovery-document parent before AI reconstruction: `473f311dd7960bf3763ec364c400e28ab9929519`.
- Pinned Wicked revision: `3a800b7134aafe58461093c8abb2e274d4e64033`.
- Current gate: **AI-01 — Character foundation**.
- Current AI-01 status: **COMPLETE / FOCUSED WINDOWS VALIDATION GREEN**.

The original Codex AI-01→AI-05 private workspace was lost before its implementation reached GitHub. Only code present on this programme branch is authoritative. Work is now checkpointed remotely after each meaningful gate so another engineer can continue from GitHub alone.

## Gate status

### AI-01 — Character foundation

Status: **COMPLETE / FOCUSED WINDOWS VALIDATION GREEN**.

Implemented on the recoverable branch:

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

Audit repairs applied before acceptance:

1. Runtime Character synchronization distinguishes **attempted** scene revision from **successfully synchronized** scene revision. Failed initialization cannot report `scene_synced=true`; revision-zero startup remains valid through an explicit attempt flag.
2. Runtime initialization is transactional: every authored Character is validated before any native controller is activated.
3. Duplicate persistent Character IDs are rejected explicitly.
4. Runtime Character records are ordered by stable Renegade Character ID rather than transient Wicked ECS allocation order.
5. Player Start, Navigation Grid and Navigation Destination entities are blocked from MAKE CHARACTER promotion. Navigation Agents remain intentionally promotable/adoptable because they can validly own a native `CharacterComponent`.
6. REMOVE CHARACTER snapshots the serialized native `CharacterComponent` state through Wicked's serializer and separately preserves foot-placement, which the pinned Wicked serializer does not persist. Undo therefore restores authored controller state rather than recreating only defaults.
7. Runtime Character state is explicitly deactivated/cleared when gameplay leaves a Level for a Screen destination.
8. Focused regressions cover reserved semantic rejection, stable-ID order, duplicate-ID atomic failure, invalid-character atomic failure, exact owned-controller REMOVE/Undo/Redo restoration and reusable snapshot Undo after Redo.

Important AI-01 design decision: MAKE CHARACTER is deliberately non-destructive. If a selected hierarchy already owns a native Wicked `CharacterComponent`, Renegade adopts it and records the previous active state. If Renegade creates the controller, it records ownership so REMOVE CHARACTER and Undo remove only Renegade-owned native state.

### AI-02 — Profiles, factions and runtime state

Status: **NOT STARTED ON THE RECOVERED PROGRAMME BRANCH**.

Recovery target: layered role/personality/skill/awareness tuning, optional persisted overrides, faction relationship matrix, transient Runtime records, stable-reference resolution and deterministic reset. Re-audit against accepted AI-01 before implementation.

### AI-03 — Perception and memory

Status: **NOT STARTED**.

Recovery target: staggered cognition, bounded visual/hearing perception, legitimate stimulus-only last-known-position updates, memory decay, suspicion/awareness, and anti-cheat tests proving hidden target transforms cannot refresh memory.

### AI-04 — Decision runtime and patrol

Status: **NOT STARTED**.

Recovery target: utility-scored intents with hysteresis, native `NavigationService` goals, Wicked `CharacterComponent::Turn/Move`, patrol authoring with stable references, search timeout and return-to-role.

### AI-05 — Combat intelligence

Status: **NOT STARTED**.

Recovery target: weapon descriptors, ammo/reload/range reasoning, combat intents, health/damage seam and AI gameplay events. Preserve a small reusable combat/health boundary; do not bury rifle-specific damage authority inside cognition.

## Architectural notes

- Studio owns authoring only; it does not run production cognition.
- EngineBridge owns stable Renegade Character semantics.
- Runtime owns transient execution state.
- Wicked remains authoritative for Scene/ECS and native `CharacterComponent` physics/movement.
- Stable Renegade IDs are the durable reference format; transient Wicked entity IDs are Runtime caches only.
- AI-01 introduces no navigation system, physics world, animation runtime, event bus, Lua brain or transform-driven NPC movement.
- Animation presentation remains AI-06 and must reuse the accepted Phase 7 animation/humanoid/IK stack.
- The Runtime player is a character-physics rigid body rather than a Wicked `Scene::characters` NPC component; AI-03 must use the existing player physics-position seam for perception rather than assuming a Scene CharacterComponent.

## AI-01 validation evidence

### Static / architectural review

- Initial AI-01 source contract: **PASS**.
- Static delimiter/structure audit: **PASS**.
- API contract audit against current Renegade/Wicked patterns: **PASS**.
- Audit repairs were checked against pinned Wicked `CharacterComponent::Serialize(wi::Archive&, EntitySerializer&)` behaviour. The serializer persists `_flags`, health, width, height and scale; foot-placement is nonserialized and is therefore preserved separately.

### Focused Windows validation

A temporary branch-only GitHub Actions workflow was used solely to validate AI-01 without invoking the planned AI-05 Debug+Release/full-test CI matrix.

Validation run: GitHub Actions run `34821199576` on Windows Server 2025 / Visual Studio 2026 runner.

Exact validation path:

1. recursive checkout — **PASS**;
2. pinned Wicked checkout `3a800b7134aafe58461093c8abb2e274d4e64033` — **PASS**;
3. CMake configure, x64, `RENEGADE_EMBED_SHADERS=OFF` — **PASS**;
4. build `RenegadeCharacterAiFoundationTests` Debug — **PASS**;
5. build `RenegadeRuntime` Debug — **PASS**;
6. build `RenegadeStudio` Debug, including `AICharacterInspector.cpp` — **PASS**;
7. `RenegadeCharacterAiFoundationTests` — **PASS** (`0.56 sec`);
8. `RenegadeCharacterAiSourceContract` — **PASS** (`0.03 sec`);
9. focused CTest result — **100% passed, 2/2 tests** (`0.68 sec` total).

No AI-01 compile, link or focused test failure remains known at this checkpoint. Existing unrelated Wicked/Renegade compiler warnings were present but did not fail the targeted build.

This focused run is sufficient to accept AI-01 and begin AI-02. It does **not** replace the intended integrated Debug+Release/full CTest CI at AI-05.

## CI / recovery workflow

- Normal pushes to `feature/character-ai-programme` do not match the expensive standard Windows workflow push branches.
- Keep each AI gate remotely checkpointed on this branch.
- Do not open the integrated AI PR merely to checkpoint work; opening the PR triggers the normal Windows workflows.
- Planned major integrated CI remains AI-05 and AI-10 if practical.
- Temporary AI-01 validation workflow is removed after recording this evidence; it is not part of the permanent programme architecture.

## NEXT IMPLEMENTATION STEP

Begin **AI-02 — Profiles, Factions and Runtime State** from the accepted AI-01 branch state.

Implement and locally/focused validate:

1. layered Role + Personality + Skill + Awareness profile composition;
2. persisted creator overrides without destroying profile defaults;
3. faction definitions and relationship matrix;
4. transient Runtime Character records keyed by stable Character ID;
5. stable reference resolution for authored Character references;
6. deterministic Runtime reset;
7. diagnostics sufficient to inspect the resolved AI-02 profile/faction/runtime state;
8. focused tests proving profile distinction, faction symmetry/direction as designed, stable-ID ownership and reset behaviour.

Checkpoint AI-02 remotely before beginning AI-03. Do not trigger the full integrated Windows CI until AI-05 unless a genuine blocker requires it.
