# Renegade Character & AI implementation handoff

## Authority and baseline

- Programme branch: `feature/character-ai-programme`.
- Accepted Phase 7 baseline: `d36918878776d0d91e0c39f88f6764a1926a6534`.
- Accepted AI-01 checkpoint: `674b1efc3e1b81e6f55bf080f539f29a0c466db4`.
- Clean AI-02 implementation checkpoint before validation: `f808d38f11672fe75bfbbdeaec989bef7f43f32c`.
- Pinned Wicked revision: `3a800b7134aafe58461093c8abb2e274d4e64033`.
- Repository-native Character/AI authority: `docs/RENEGADE_CHARACTER_AI_SYSTEM_IMPLEMENTATION_AUTHORITY.md`.
- Original full design artifact provenance: 62,613 bytes, SHA-256 `aa396b6aa6fbf94650abeb6e0916657e29b90c3775075360438af6e58085a74e`.

Only code and documentation present on the programme branch are implementation truth. The lost private Codex workspace is not authoritative.

## Gate status

### AI-01 — Character Foundation

**COMPLETE / FOCUSED WINDOWS VALIDATION GREEN.**

Accepted checkpoint: `674b1efc3e1b81e6f55bf080f539f29a0c466db4`.

Focused Windows validation run `34821199576` proved:

- recursive checkout and pinned Wicked checkout;
- x64 CMake configure with `RENEGADE_EMBED_SHADERS=OFF`;
- Debug build of `RenegadeCharacterAiFoundationTests`;
- Debug build of `RenegadeRuntime`;
- Debug build of `RenegadeStudio`, including the Character Inspector;
- `RenegadeCharacterAiFoundationTests` PASS (`0.56 sec`);
- `RenegadeCharacterAiSourceContract` PASS (`0.03 sec`);
- focused CTest 2/2 PASS.

AI-01 provides governed MAKE/REMOVE CHARACTER, stable identity, native Wicked `CharacterComponent` adoption/ownership, command-backed authoring, persistence, Runtime discovery/reset and bounded diagnostics.

### AI-02 — Profiles, Factions & Runtime State

**IMPLEMENTED / FULL SOURCE-ARCHITECTURE AUDIT REPAIRS APPLIED / WINDOWS VALIDATION PENDING.**

Clean implementation checkpoint: `f808d38f11672fe75bfbbdeaec989bef7f43f32c`.

AI-02 is deliberately limited to creator authoring semantics and resolved transient Runtime state. It does **not** implement production sight/hearing/memory, utility decisions, patrol execution or combat cognition; those remain AI-03/04/05.

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

## AI-02 audit findings repaired before validation

The implementation received repeated source/architecture review before any AI-02 Windows validation was requested. The following defects were found and repaired:

1. **Creator-defined faction registration was incomplete.** Arbitrary IDs were accepted but there was no real known-faction registry. `FactionRegistry` now seeds built-ins, registers creator IDs deterministically and resets deterministically.
2. **Failure-state publication was too caller-dependent.** AI-02 initialization now clears published state on any profile/faction/reference failure rather than assuming callers passed an empty state.
3. **Malformed persisted profile metadata could silently fall back to valid-looking defaults.** Runtime now validates the raw persisted enum/faction metadata before resolving tuning.
4. **Advanced payload parsing accepted ambiguous input.** Serialization is locale-neutral; duplicate fields and contradictory explicit ranges/thresholds fail closed.
5. **Malformed Advanced AI payloads were not creator-repairable.** RESET ADVANCED OVERRIDES can replace corrupt raw payloads; Undo restores the exact previous raw bytes and Redo reapplies the repair.
6. **Persisted text fields committed on every keystroke.** Animation Set and custom faction text now draft while typing and commit on accepted input, producing one governed history step.
7. **Invalid Advanced AI state could display apparently valid fallback sliders.** The Inspector now shows INVALID ADVANCED AI, locks tuning sliders and leaves the repair/reset action available.
8. **The CUSTOM Faction combo entry was a dead control.** Selecting it now opens the ADVANCED AI section where the custom ID is authored.
9. **Regression coverage was insufficient for corruption/recovery.** Focused audit tests now cover malformed profile enums/factions, duplicate/contradictory advanced fields, locale-neutral payloads, malformed-payload repair/Undo/Redo, deterministic faction registration/reset and failure cleanup.
10. **Unsupported/missing Character schema markers could be silently skipped.** AI-02 now preflights owned `renegade.character` markers before profile resolution and fails closed if the marker/schema is invalid rather than treating corrupt authoring as no Character.
11. **Repository recoverability was incomplete.** The programme branch now contains `docs/RENEGADE_CHARACTER_AI_SYSTEM_IMPLEMENTATION_AUTHORITY.md`; the corrupt/incomplete recovery payload discovered during the audit was removed.
12. **Feature evidence drifted during documentation reconstruction.** Unrelated historical `FEATURE_MATRIX.csv` rows were restored exactly; the accepted diff adds only the `REN-AI-001` evidence row.

## Important boundaries

- Studio owns authoring only; Runtime owns transient execution state.
- Wicked remains Scene/ECS/native Character authority.
- Navigation later uses the accepted `NavigationService`; no Recast or transform-driven NPC movement.
- Faction relationship does not imply perception/detection.
- `animationSetId` remains persisted governed authoring in AI-02. Resolution and semantic animation materialization are AI-06 responsibilities; AI-02 must not prematurely create a second asset/animation runtime.
- AI-03 must preserve the no-cheating rule: hidden target transforms cannot refresh last-known information.
- Public AI events later use `GameplayEventService`.

## Pre-validation evidence

- AI-02 implementation/audit history was collapsed into the clean checkpoint `f808d38f11672fe75bfbbdeaec989bef7f43f32c` whose parent is accepted AI-01 `674b1efc3e1b81e6f55bf080f539f29a0c466db4`.
- The final AI-02 diff contains 18 intended files. No temporary validation workflow or corrupt recovery payload remains in the checkpoint.
- `docs/FEATURE_MATRIX.csv` differs from AI-01 by exactly one added `REN-AI-001` row.
- Top-level `CMakeLists.txt` includes `Tests/CharacterAiFoundation.cmake`, so the AI-01/AI-02 executable and source-contract tests are reachable in the Windows test graph.
- No post-AI-01 expensive workflow has been triggered by ordinary programme-branch checkpoints; the only branch workflow run remains successful AI-01 focused run `34821199576`.

## AI-02 validation state

Not yet claimed:

- Windows compile/link of the repaired AI-02 exact validation head;
- execution of `RenegadeCharacterAiProfilesTests`;
- execution of `RenegadeCharacterAiProfilesAuditTests`;
- execution of `RenegadeCharacterAiProfilesSourceContract` after all audit repairs;
- repaired `RenegadeRuntime` and `RenegadeStudio` build on Windows;
- creator-facing owner interaction in Studio;
- full integrated Debug+Release/full CTest matrix (still intentionally reserved for AI-05 unless a genuine blocker requires it).

No source audit finding is intentionally deferred into validation.

## Planned focused AI-02 validation

Use one branch-only focused Windows job, not the integrated AI-05 matrix:

1. recursive checkout and pinned Wicked SHA check;
2. x64 CMake configure;
3. build `RenegadeCharacterAiFoundationTests` (AI-01 regression guard);
4. build `RenegadeCharacterAiProfilesTests`;
5. build `RenegadeCharacterAiProfilesAuditTests`;
6. build `RenegadeRuntime` Debug;
7. build `RenegadeStudio` Debug;
8. run `RenegadeCharacterAiFoundationTests`;
9. run `RenegadeCharacterAiSourceContract`;
10. run `RenegadeCharacterAiProfilesTests`;
11. run `RenegadeCharacterAiProfilesAuditTests`;
12. run `RenegadeCharacterAiProfilesSourceContract`.

Any compile or focused-test failure remains AI-02 work and must be repaired before AI-03 begins.

## CI/recovery policy

- Keep the long-lived `feature/character-ai-programme` branch remotely recoverable.
- Normal branch checkpoints must not open the integrated PR merely to preserve work.
- Do not intentionally trigger the standard four-job Debug+Release matrix before AI-05.
- Focused gate validation may use a temporary branch-only workflow when no local Windows checkout is available; remove temporary workflow scaffolding after evidence is recorded.
- After AI-02 is accepted, checkpoint it cleanly before AI-03.

## Next action

Run the focused AI-02 Windows validation above against the exact pre-validation branch head. Do **not** begin AI-03 until that exact AI-02 candidate passes the focused validation.
