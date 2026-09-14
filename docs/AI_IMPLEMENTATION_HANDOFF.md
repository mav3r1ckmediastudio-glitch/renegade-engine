# Renegade Character & AI implementation handoff

## Authority and baseline

- Programme branch: `feature/character-ai-programme`.
- Accepted Phase 7 baseline: `d36918878776d0d91e0c39f88f6764a1926a6534`.
- Accepted AI-01 checkpoint: `674b1efc3e1b81e6f55bf080f539f29a0c466db4`.
- Accepted AI-02 checkpoint/evidence head: `09ee66e6b925b305a75b81cf0cf00170ca42edfc`.
- Successful AI-02 focused validation run: `34837594589`.
- Clean AI-03 implementation checkpoint: `307161dee97f0b9aa39eaab4e8d655240c072171`.
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

**IMPLEMENTED / SOURCE + ARCHITECTURE AUDITED / FOCUSED WINDOWS VALIDATION PENDING.**

Clean implementation checkpoint: `307161dee97f0b9aa39eaab4e8d655240c072171`, whose parent is accepted AI-02 `09ee66e6b925b305a75b81cf0cf00170ca42edfc`.

The clean AI-03 diff contains exactly six implementation/test files:

- `Runtime/src/RuntimeApplication.h`
- `Runtime/src/RuntimeCharacterPerception.h`
- `Runtime/src/RuntimeLiveDiagnostics.cpp`
- `Tests/CharacterAiFoundation.cmake`
- `Tests/CharacterAiPerceptionTests.cpp`
- `Tests/CharacterAiPerceptionSourceContract.cmake`

Implemented AI-03 contract:

1. Runtime-owned transient cognition keyed by persistent Character stable IDs; no cognition is serialized.
2. Bounded 5 Hz cognition with stable-ID staggering; ordinary render-frame work is limited to decay/lifecycle bookkeeping.
3. Player position/velocity come from the accepted physics seam (`GetPhysicsPosition` / `GetLinearVelocity`). The Runtime player is not treated as a Wicked `Scene::characters` component.
4. Sight uses authored central/peripheral distance plus horizontal/vertical FOV and native Wicked `Scene::Intersects()` LOS.
5. LOS skips the observer's own imported hierarchy rather than allowing the Character's body/head geometry to block its own ray; abnormal excessive self-hits fail closed.
6. Visual reaction latency is profile-driven before Seen memory is admitted.
7. Hearing is driven only by explicit bounded `SoundStimulus` records; automatic Runtime-player footsteps feed that same seam.
8. Audio reaction latency is profile-driven. A pending sound is committed until heard, expired or inaudible so repeated footsteps/gunfire cannot continually restart the reaction timer and starve hearing.
9. Damage knowledge enters through explicit `ReportDamageStimulus`; the perception system does not look up a hidden attacker's transform.
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

The implementation/source audit found and repaired the following before any Windows run was requested:

1. direct sight was initially cleared during render-frame decay rather than by a real cognition sample;
2. LOS could hit the observer's own imported hierarchy;
3. cognition could sample a stale Runtime-player transient entity on the first frame after a scene change;
4. malformed external stimulus data needed stronger finite/control-character validation;
5. repeated new sounds could continuously replace a pending sound and reset its reaction timer;
6. lifecycle/reset behavior was rechecked against `SceneService::Revision()` and Story Flow/Screen teardown.

## Architecture boundaries preserved

- Studio remains authoring-only; production cognition runs in Runtime.
- Wicked remains Scene/ECS/physics authority.
- Player knowledge uses accepted physics position, not hidden transform fallback.
- AI-03 creates no second physics, Scene, gameplay, navigation, animation, diagnostics or generic event system.
- No Recast and no transform-driven NPC movement were introduced.
- No Lua ordinary-NPC brain was introduced.
- No all-pairs Character perception scan was introduced; AI-03 currently samples the legitimate Runtime-player target seam and explicit sound/damage stimuli.
- Public/cross-system AI events remain reserved for the existing `GameplayEventService` in later gates.

## Validation state

Not yet claimed for AI-03:

- Windows compile/link of the clean checkpoint;
- execution of `RenegadeCharacterAiPerceptionTests`;
- execution of `RenegadeCharacterAiPerceptionSourceContract`;
- Runtime/Studio Debug build with the AI-03 exact head;
- creator-facing owner behavior proof;
- full integrated Debug+Release matrix (still intentionally deferred to AI-05).

## CI/recovery policy

- Keep `feature/character-ai-programme` remotely recoverable.
- Ordinary checkpoint pushes do not intentionally trigger the full Windows matrix.
- Do not intentionally open the integrated PR/full four-job matrix before AI-05.
- Focused branch-only validation is allowed when required to prove a gate without wasting the full matrix.
- A gate is not COMPLETE merely because source exists or compiles.

## Exact next implementation step

Run focused AI-03 Windows validation against clean checkpoint `307161dee97f0b9aa39eaab4e8d655240c072171`, covering the AI-01/AI-02 regressions plus `RenegadeCharacterAiPerceptionTests`, `RenegadeCharacterAiPerceptionSourceContract`, `RenegadeRuntime` Debug and `RenegadeStudio` Debug. If validation exposes a defect, repair it on the programme branch and rerun only the focused AI-03 validation. If green, record the run and mark AI-03 COMPLETE before beginning AI-04.