# Renegade Character & AI implementation handoff

## Authority and baseline

- Programme branch: `feature/character-ai-programme`.
- Accepted Phase 7 baseline: `d36918878776d0d91e0c39f88f6764a1926a6534`.
- Accepted AI-01 checkpoint: `674b1efc3e1b81e6f55bf080f539f29a0c466db4`.
- AI-02 implementation checkpoint before validation: `f808d38f11672fe75bfbbdeaec989bef7f43f32c`.
- AI-02 parser repair: `1355f28bf1fa9997b3a41b445d70358721cfdc01`.
- Successful AI-02 focused validation run: `34837594589`.
- Pinned Wicked revision: `3a800b7134aafe58461093c8abb2e274d4e64033`.
- Repository-native Character/AI authority: `docs/RENEGADE_CHARACTER_AI_SYSTEM_IMPLEMENTATION_AUTHORITY.md`.

Only code and documentation present on the programme branch are implementation truth.

## Gate status

### AI-01 — Character Foundation

**COMPLETE / FOCUSED WINDOWS VALIDATION GREEN.**

Accepted checkpoint: `674b1efc3e1b81e6f55bf080f539f29a0c466db4`.

AI-01 provides governed MAKE/REMOVE CHARACTER, stable identity, native Wicked `CharacterComponent` adoption/ownership, command-backed authoring, persistence, Runtime discovery/reset and bounded diagnostics.

### AI-02 — Profiles, Factions & Runtime State

**COMPLETE / FOCUSED WINDOWS VALIDATION GREEN.**

AI-02 remains deliberately limited to creator authoring semantics and resolved transient Runtime state. It does **not** implement production sight/hearing/memory, utility decisions, patrol execution or combat cognition; those remain AI-03/04/05.

Implemented AI-02 surfaces:

- `EngineBridge/include/renegade/bridge/CharacterProfileService.h`
- `EngineBridge/include/renegade/bridge/FactionService.h`
- AI-02 lifecycle additions in `CharacterService.h/.cpp`
- `Runtime/src/RuntimeCharacterSystem.h`
- Runtime scene-sync/diagnostics integration in `RuntimeApplication.h` and `RuntimeLiveDiagnostics.cpp`
- extended existing `Studio/src/AICharacterInspector.cpp`
- `Tests/CharacterAiProfilesTests.cpp`
- `Tests/CharacterAiProfilesAuditTests.cpp`
- `Tests/CharacterAiProfilesSourceContract.cmake`
- CMake registration in `Tests/CharacterAiFoundation.cmake`

Implemented contract:

1. Effective tuning composition is deterministic: Type -> Role -> Personality -> Skill -> Awareness -> explicit Advanced AI overrides.
2. Advanced overrides use versioned Character-owned metadata and CommandService history.
3. The existing Character Inspector exposes Skill, Awareness, built-in/custom Faction authoring, effective profile feedback and a collapsed grouped ADVANCED AI section.
4. Built-in and creator-defined faction IDs are held in a deterministic transient `FactionRegistry`; relationship semantics remain separate from perception knowledge.
5. Runtime Character records are keyed/sorted by stable Character ID and contain resolved authoring, overrides, effective tuning and transient entity-reference caches.
6. Patrol Route and Weapon stable entity references resolve through the existing `EntityIdentityIndex` and missing references fail closed without mutating authoring.
7. Scene initialization publishes AI-02 state only after the whole candidate resolves successfully. Failure clears candidate state and AI-01 native controller activation is rolled back by the Runtime integration boundary.
8. Runtime reset clears resolved Character records and restores the faction registry to built-ins.
9. Diagnostics extend the existing `ai` projection; no second diagnostics service was introduced.
10. No navigation, physics, event-bus, animation-runtime, Lua-brain or cognition loop was added by AI-02.

## AI-02 audit repairs

Before validation, the source/architecture audit found and repaired:

1. incomplete creator-defined faction registration;
2. caller-dependent failure-state publication;
3. malformed persisted profile metadata silently falling back to valid-looking defaults;
4. ambiguous Advanced payload parsing and locale-dependent serialization risk;
5. malformed Advanced AI payloads that were not creator-repairable;
6. persisted text fields committing once per keystroke;
7. invalid Advanced state displaying fallback-looking controls;
8. dead CUSTOM faction control;
9. insufficient corruption/recovery regressions;
10. unsupported/missing Character schema markers being silently skipped;
11. missing repository-native Character/AI authority documentation;
12. accidental historical feature-matrix drift during evidence reconstruction.

Validation then exposed one additional parser defect: after successfully extracting a numeric override, the parser applied `std::ws` at EOF and treated the resulting failbit as an invalid number. This caused well-formed serialized Advanced AI payloads to fail round-trip tests. The parser was repaired in `1355f28bf1fa9997b3a41b445d70358721cfdc01` to reject only extraction failure, non-finite values, or non-whitespace trailing content.

## Focused Windows validation evidence

Initial AI-02 validation run `34835988549` proved the full compile surface was green:

- recursive checkout and pinned Wicked checkout: PASS;
- x64 CMake configure: PASS;
- `RenegadeCharacterAiFoundationTests` build: PASS;
- `RenegadeCharacterAiProfilesTests` build: PASS;
- `RenegadeCharacterAiProfilesAuditTests` build: PASS;
- `RenegadeRuntime` Debug build: PASS;
- `RenegadeStudio` Debug build: PASS.

The initial CTest step failed only because of the parser EOF defect described above.

After the parser repair, focused Windows validation run `34837594589` completed successfully. GitHub reports every job step green, including:

- checkout: PASS;
- configure: PASS;
- build AI-01/AI-02 + Runtime + Studio targets: PASS;
- run AI-01/AI-02 focused tests: PASS;
- validation-log upload: PASS.

The focused test regex covered:

- `RenegadeCharacterAiFoundationTests`;
- `RenegadeCharacterAiSourceContract`;
- `RenegadeCharacterAiProfilesTests`;
- `RenegadeCharacterAiProfilesAuditTests`;
- `RenegadeCharacterAiProfilesSourceContract`.

Therefore AI-02 is accepted as **COMPLETE** for its defined gate scope.

## Important boundaries

- Studio owns authoring only; Runtime owns transient execution state.
- Wicked remains Scene/ECS/native Character authority.
- Navigation later uses the accepted `NavigationService`; no Recast or transform-driven NPC movement.
- Faction relationship does not imply perception/detection.
- `animationSetId` remains persisted governed authoring in AI-02. Resolution and semantic animation materialization are AI-06 responsibilities.
- AI-03 must preserve the no-cheating rule: hidden target transforms cannot refresh last-known information.
- Public AI events later use `GameplayEventService`.

## CI/recovery policy

- Keep the long-lived `feature/character-ai-programme` branch remotely recoverable.
- Normal gate checkpoint pushes do not intentionally trigger the full integrated matrix.
- Do not intentionally trigger the standard four-job Debug+Release matrix before AI-05.
- Focused gate validation may use a temporary branch-only workflow when no local Windows checkout is available; remove temporary workflow scaffolding after evidence is recorded.
- Temporary AI-02 validation workflow has been removed after successful validation.

## Next action

Begin **AI-03 — Perception & Memory** from the validated AI-02 branch state. AI-03 must implement bounded sight/hearing/damage stimuli, last-known-position memory, confidence/decay, suspicion/awareness progression, and explicit anti-cheat tests proving hidden target transforms cannot refresh memory.