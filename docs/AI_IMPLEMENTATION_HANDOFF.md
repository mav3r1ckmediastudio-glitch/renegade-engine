# Renegade Character & AI implementation handoff

## Authority and baseline

- Programme branch: `feature/character-ai-programme`.
- Accepted Phase 7 baseline: `d36918878776d0d91e0c39f88f6764a1926a6534`.
- Accepted AI-01 checkpoint: `674b1efc3e1b81e6f55bf080f539f29a0c466db4`.
- Accepted AI-02 checkpoint/evidence head: `09ee66e6b925b305a75b81cf0cf00170ca42edfc`.
- Successful AI-02 focused validation run: `34837594589`.
- Clean AI-03 implementation checkpoint: `307161dee97f0b9aa39eaab4e8d655240c072171`.
- Successful AI-03 focused validation run: `34843305044`.
- Accepted AI-04 implementation checkpoint: `42159af5d3125a2e7fec80619dd2e325ca27d86e`.
- Successful AI-04 nine-test acceptance run: `34852462926`.
- Pinned Wicked revision: `3a800b7134aafe58461093c8abb2e274d4e64033`.
- Repository-native Character/AI authority: `docs/RENEGADE_CHARACTER_AI_SYSTEM_IMPLEMENTATION_AUTHORITY.md`.

Only code and documentation present on the programme branch are implementation truth.

## Gate status

### AI-01 — Character Foundation

**COMPLETE / FOCUSED WINDOWS VALIDATION GREEN.**

Provides governed MAKE/REMOVE CHARACTER, stable identity, native Wicked `CharacterComponent` adoption/ownership, command-backed authoring, persistence, Runtime discovery/reset and bounded diagnostics.

### AI-02 — Profiles, Factions & Runtime State

**COMPLETE / FOCUSED WINDOWS VALIDATION GREEN.**

Provides deterministic Type -> Role -> Personality -> Skill -> Awareness -> Advanced override composition, versioned Advanced AI authoring, creator-defined/built-in factions, stable-ID-keyed transient Runtime Character records, governed stable reference resolution, deterministic reset and AI diagnostics. Faction relationship remains separate from perception knowledge.

### AI-03 — Perception & Memory

**COMPLETE / FOCUSED WINDOWS VALIDATION GREEN.**

Clean implementation checkpoint: `307161dee97f0b9aa39eaab4e8d655240c072171`, whose parent is accepted AI-02 `09ee66e6b925b305a75b81cf0cf00170ca42edfc`.

The clean AI-03 implementation diff contains exactly six implementation/test files:

- `Runtime/src/RuntimeApplication.h`
- `Runtime/src/RuntimeCharacterPerception.h`
- `Runtime/src/RuntimeLiveDiagnostics.cpp`
- `Tests/CharacterAiFoundation.cmake`
- `Tests/CharacterAiPerceptionTests.cpp`
- `Tests/CharacterAiPerceptionSourceContract.cmake`

Implemented AI-03 contract:

1. Runtime-owned transient cognition keyed by persistent Character stable IDs; cognition is never serialized.
2. Bounded 5 Hz cognition with stable-ID staggering; ordinary render-frame work is limited to decay/lifecycle bookkeeping.
3. Player position/velocity come from the accepted physics seam (`GetPhysicsPosition` / `GetLinearVelocity`). The Runtime player is not treated as a Wicked `Scene::characters` component.
4. Sight uses authored central/peripheral distance plus horizontal/vertical FOV and native Wicked `Scene::Intersects()` LOS.
5. LOS skips the observer's own imported hierarchy rather than allowing Character body/head geometry to block its own ray; abnormal excessive self-hits fail closed.
6. Visual reaction latency is profile-driven before Seen memory is admitted.
7. Hearing is driven only by explicit bounded `SoundStimulus` records; automatic Runtime-player footsteps feed that same seam.
8. Audio reaction latency is profile-driven. A pending sound remains committed until heard, expired or inaudible so repeated footsteps/gunfire cannot continually restart the reaction timer and starve hearing.
9. Damage knowledge enters through explicit `ReportDamageStimulus`; perception does not look up a hidden attacker's transform.
10. Memory records preserve knowledge source, subject/faction, last-known position/velocity, confidence, threat, age, seconds-since-seen and direct-sight state.
11. Memory/confidence and suspicion decay without synthesizing newer target coordinates.
12. Awareness progression supports `Unaware`, `Interested`, `Suspicious`, `Alerted`, `Combat` and `Searching`.
13. Faction hostility alone creates no target or location; legitimate perception/damage stimuli are required.
14. Runtime diagnostics extend the existing `ai` projection with awareness, suspicion, memory count, knowledge source/subject/confidence, memory age, last-known position, direct sight, cognition ticks, LOS queries, visual detections, heard/damage stimuli and sound queue pressure.
15. Scene transition/reset clears cognition transactionally. `SceneService::LoadScene()` increments the scene revision, and Screen destinations explicitly tear transient AI down.
16. Cognition is gated on `playerSceneRevision_ == sceneRevision`, preventing one-frame sampling of a stale player ECS handle after a Level transition.

## AI-03 anti-cheat boundary

AI-03 explicitly enforces the programme rule that hidden target transforms cannot refresh knowledge.

`ApplyVisualObservation()` handles `visible=false` before reading position/velocity fields. The focused regression deliberately supplies hidden coordinates `(999,999,999)` after legitimate sight is lost and asserts that the prior last-known position and velocity remain unchanged.

Additional regressions assert:

- faction relationship alone produces no knowledge;
- render-frame decay does not fabricate sight loss between 5 Hz samples;
- malformed/NaN sound stimuli are rejected;
- duplicate Character stable identities fail closed;
- sound queues are bounded deterministically;
- repeated sound arrival cannot starve the pending hearing reaction;
- reset/reinitialization restores deterministic stagger state.

## AI-03 audit repairs before validation

The implementation/source audit found and repaired the following before Windows validation:

1. direct sight was initially cleared during render-frame decay rather than by a real cognition sample;
2. LOS could hit the observer's own imported hierarchy;
3. cognition could sample a stale Runtime-player transient entity on the first frame after a scene change;
4. malformed external stimulus data needed stronger finite/control-character validation;
5. repeated new sounds could continuously replace a pending sound and reset its reaction timer;
6. lifecycle/reset behavior was rechecked against `SceneService::Revision()` and Story Flow/Screen teardown.

## AI-03 focused Windows validation evidence

Focused run `34843305044` on `windows-2025` completed green against the AI-03 programme state plus a temporary validation-only workflow file.

Build step PASS:

- `RenegadeCharacterAiFoundationTests`;
- `RenegadeCharacterAiProfilesTests`;
- `RenegadeCharacterAiProfilesAuditTests`;
- `RenegadeCharacterAiPerceptionTests`;
- `RenegadeRuntime` Debug;
- `RenegadeStudio` Debug.

CTest PASS: **7/7** in **0.76 sec**:

- `RenegadeCharacterAiFoundationTests`;
- `RenegadeCharacterAiSourceContract`;
- `RenegadeCharacterAiProfilesTests`;
- `RenegadeCharacterAiProfilesAuditTests`;
- `RenegadeCharacterAiProfilesSourceContract`;
- `RenegadeCharacterAiPerceptionTests`;
- `RenegadeCharacterAiPerceptionSourceContract`.

The run also confirmed recursive checkout with Wicked pinned at `3a800b7134aafe58461093c8abb2e274d4e64033` and successful x64 CMake configure with `RENEGADE_EMBED_SHADERS=OFF`.

Therefore AI-03 is accepted as **COMPLETE** for its defined gate scope.

### AI-04 — Decision & Patrol

**COMPLETE / FOCUSED WINDOWS VALIDATION GREEN.**

Accepted implementation checkpoint: `42159af5d3125a2e7fec80619dd2e325ca27d86e`.

Implemented AI-04 contract:

1. Runtime-owned utility decision layer with deterministic intent scores, hysteresis and commitment rather than a second behavior-tree/runtime world.
2. Role defaults support Idle/Guard/Patrol, while AI-03 memory can legitimately drive Observe/Investigate/Search/Chase/Hold decisions.
3. Investigate and Search consume AI-03 last-known memory only; the decision layer has no direct live-player transform/physics-position lookup.
4. Search is bounded. Once a search times out, the same stale memory is latched as exhausted and cannot immediately re-arm another full search. A genuinely refreshed AI-03 memory sample clears the exhaustion and can become actionable again.
5. Search timeout returns the Character to its normal role rather than leaving it permanently Searching.
6. Patrol Routes and Patrol Points are governed Scene authoring data with persistent stable IDs and deterministic ordered traversal.
7. Patrol traversal supports Loop, PingPong and deterministic Random modes plus authored wait-at-point duration.
8. Creator-facing `CreateCharacterPatrolRouteCommand` creates and assigns a route as one command-backed Undo/Redo transaction using the route stable ID.
9. Creator PATROL Inspector allows route assignment, route creation/assignment, adding Patrol Points, route mode and wait-time editing. Patrol points remain ordinary transformable Scene entities.
10. The PATROL Inspector is registered in the existing creator Inspector stack and is compiled into `RenegadeStudio`.
11. Runtime route references are resolved from stable authoring identity. Raw ECS IDs are only transient runtime/UI handles.
12. Movement remains on the accepted Phase-6/Wicked path: `NavigationService::SetCharacterNavigationGoal`, Wicked `CharacterComponent::pathquery`, `Turn()` and `Move()`.
13. AI-04 does not write transforms for normal movement and does not add Recast or a second navigation world.
14. Repath/stuck handling requests native repaths and explicitly avoids teleport recovery.
15. Runtime diagnostics expose current/previous intent, top utility scores, decision/path counters, route/search state and transition reason through the existing diagnostics projection.

## AI-04 validation evidence

Windows validation run `34850214108` proved that all product-facing AI-04 integration compiled and linked successfully, including:

- `RenegadeCharacterAiDecisionTests`;
- `RenegadeRuntime` Debug;
- `RenegadeStudio` Debug;
- `AIPatrolRouteInspector.cpp` in the actual Studio build.

That run's CTest phase exposed validation-harness issues rather than product failures: older AI-01/02/03 executables had not been requested as build targets, and the AI-04 source contract looked for a qualified traversal token in the enum-definition file instead of the runtime decision file. The source contract was corrected without weakening the acceptance rules, and the final focused acceptance run built all five executable test targets.

Successful focused acceptance run: `34852462926` on `windows-2025`, Wicked pinned at `3a800b7134aafe58461093c8abb2e274d4e64033`.

Build PASS:

- `RenegadeCharacterAiFoundationTests`;
- `RenegadeCharacterAiProfilesTests`;
- `RenegadeCharacterAiProfilesAuditTests`;
- `RenegadeCharacterAiPerceptionTests`;
- `RenegadeCharacterAiDecisionTests`.

CTest PASS: **9/9** in **1.75 sec**:

- `RenegadeCharacterAiFoundationTests`;
- `RenegadeCharacterAiSourceContract`;
- `RenegadeCharacterAiProfilesTests`;
- `RenegadeCharacterAiProfilesAuditTests`;
- `RenegadeCharacterAiProfilesSourceContract`;
- `RenegadeCharacterAiPerceptionTests`;
- `RenegadeCharacterAiPerceptionSourceContract`;
- `RenegadeCharacterAiDecisionTests`;
- `RenegadeCharacterAiDecisionSourceContract`.

Therefore AI-04 is accepted as **COMPLETE** for its defined gate scope.

## Architecture boundaries preserved

- Studio remains authoring-only; production cognition and decision execution run in Runtime.
- Wicked remains Scene/ECS/physics/navigation authority.
- Player knowledge uses the accepted AI-03 perception/memory seam; AI-04 cannot read hidden player transforms to make decisions.
- AI-04 extends the existing native `NavigationService`; it introduces no Recast/second navigation stack.
- NPC locomotion uses native Wicked `CharacterComponent::Turn()` / `Move()` and native `PathQuery`; no normal transform-driven movement or teleport stuck recovery was introduced.
- AI-01 through AI-04 create no second physics, Scene, gameplay, animation, diagnostics or generic event system.
- No Lua ordinary-NPC brain was introduced.
- Public/cross-system AI events remain on the existing `GameplayEventService` boundary for later gates.

## CI/recovery policy

- Keep `feature/character-ai-programme` remotely recoverable.
- Ordinary checkpoint pushes do not intentionally trigger the full Windows matrix.
- Do not intentionally open the integrated PR/full four-job matrix before AI-05.
- Focused branch-only validation is allowed when required to prove a gate without wasting the full matrix.
- A gate is not COMPLETE merely because source exists or compiles.

## Exact next implementation step

Begin **AI-05 — Combat Intelligence** from accepted AI-04 checkpoint `42159af5d3125a2e7fec80619dd2e325ca27d86e` and the green AI-01→AI-04 acceptance evidence in run `34852462926`. Preserve the existing AI-01→04 boundaries while adding a reusable combat foundation and combat reasoning: weapon descriptors/range bands, bounded ammo/reload state, health/damage seams, Attack/Chase/Hold/Retreat/Flee/Surrender scoring, safer reload decisions, deterministic bounded-accuracy hit resolution where the architecture requires it, and public AI combat events through the existing `GameplayEventService`. Keep combat mechanics outside cognition, do not hide weapon/health rules inside the decision layer, and do not start AI-06 animation integration until AI-05 reaches its integrated checkpoint and required Windows validation.