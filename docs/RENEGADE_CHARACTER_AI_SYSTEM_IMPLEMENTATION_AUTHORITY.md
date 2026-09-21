# Renegade Character & AI System — Implementation Authority

**Programme branch:** `feature/character-ai-programme`  
**Accepted Phase 7 baseline:** `d36918878776d0d91e0c39f88f6764a1926a6534`  
**Pinned Wicked revision:** `3a800b7134aafe58461093c8abb2e274d4e64033`  
**Original full design artifact provenance:** `RENEGADE_CHARACTER_AI_SYSTEM_IMPLEMENTATION_SPEC.md`, 62,613 bytes, SHA-256 `aa396b6aa6fbf94650abeb6e0916657e29b90c3775075360438af6e58085a74e`.

This document is the repository-native implementation authority for the Character & AI programme. It normalizes the non-negotiable architecture, gate boundaries and acceptance contract from the original full design artifact so a future engineer can continue from GitHub without access to an earlier AI conversation or private workspace.

## 1. Product goal

Renegade should support GameGuru-style setup simplicity with a stronger, native Wicked-backed architecture. A creator should be able to select/import a character, choose **MAKE CHARACTER**, then configure a compact surface such as:

- Character Type: Human
- Role: Guard
- Faction: Enemy
- Combat Style: Ranged
- Weapon: Rifle
- Personality: Cautious
- Awareness: Normal

The Runtime should then provide intelligent default behaviour without requiring Lua or a creator-authored behaviour tree.

Target architecture phrase: **AAA-style-ish AI architecture, GameGuru-style setup simplicity.**

## 2. Non-negotiable ownership

Renegade must extend the systems already accepted in Phase 6 and Phase 7. It must not create parallel engine stacks.

- **Studio** owns creator authoring and inspection only.
- **EngineBridge** owns stable Renegade semantics and command-backed authoring APIs.
- **Runtime** owns transient Character/AI execution state.
- **Wicked Engine** remains authoritative for Scene/ECS, native `CharacterComponent`, physics, native navigation primitives, animation, humanoid/IK/look-at/expression and renderer/scene facilities.
- Persisted references use Renegade stable IDs. Raw Wicked ECS entity IDs are transient Runtime caches only.
- Creator-facing persistent mutations use the existing shared `CommandService` Undo/Redo history.
- Public/cross-system/script gameplay events use the existing `GameplayEventService`; do not create a second generic event bus.
- Diagnostics extend the existing structured diagnostics/Test Level IPC and Studio Diagnostics surface; do not create another diagnostics system.
- Ordinary production AI is deterministic C++/Runtime logic. Lua is optional for special cases only.
- Do not make an LLM a frame-by-frame AI brain or a Runtime requirement.

## 3. Existing accepted foundations to reuse

### Native navigation

`EngineBridge/include/renegade/bridge/NavigationService.h` is the navigation authority. It wraps Wicked `VoxelGrid`, `PathQuery` and native `CharacterComponent` movement. AI movement must use this seam and `CharacterComponent::Turn()/Move()` rather than setting transforms.

Do not introduce Recast, a second navigation world or a second character movement implementation.

### Character presentation

Phase 7 already exposes native animation playback, humanoid mapping/retarget, IK, look-at, expressions and timeline authoring. AI presentation must drive those accepted systems rather than introducing another animation runtime.

### Identity

`IdentityService` is the durable entity-reference authority. AI authoring references, Character identity and persisted links must use stable IDs.

### Events

`GameplayEventService` is the generic gameplay event authority. High-rate private cognition may remain internal state, but public AI events and script-visible events flow through the existing service.

### Diagnostics

Use existing bounded live diagnostics. AI diagnostics belong in the existing Runtime snapshot and Studio Diagnostics tab.

## 4. Character model

A governed Character has persisted creator authoring plus transient Runtime cognition.

Persisted authoring includes, at minimum:

- Character Type
- Role / Behaviour profile
- Personality
- Faction ID
- Animation Set stable ID
- Patrol Route stable entity ID
- Squad ID
- Weapon stable entity ID
- Combat Style
- Skill
- Awareness
- Autonomous flag
- Can Flee
- Can Surrender
- Can Use Cover
- Can Communicate
- optional Advanced AI overrides

Transient cognition is never saved as authoring truth.

The selected/imported character hierarchy should be promotable through **MAKE CHARACTER**, with validation for stable identity, native controller compatibility and conflicting reserved semantics. The operation must remain non-destructive: an existing native Wicked `CharacterComponent` may be adopted, while Renegade-created controller ownership must be tracked so REMOVE/Undo removes only Renegade-owned state.

## 5. Profile composition

Effective tuning is composed from creator-friendly layers rather than a rigid state machine:

`Type defaults -> Role/Behaviour -> Personality -> Skill -> Awareness -> explicit per-Character overrides`

The resulting `CharacterTuning` contains the values later gates consume, including perception ranges/FOV, hearing, reaction times, memory/suspicion values, personality traits, combat ranges, cover preference, retreat threshold, pursuit/search durations, communication range and suppression tolerance.

Personality dimensions include at least:

- Aggression
- Courage
- Curiosity
- Alertness
- Loyalty
- Accuracy

Presets are bundles that feed decisions, not hard-coded scripts.

Advanced creator controls are optional and collapsed by default. Resetting Advanced AI returns the Character to profile-driven values.

## 6. Factions

Faction relationship semantics are separate from perception.

Knowing that `Enemy` is hostile does **not** mean an NPC knows that an enemy exists or where it is.

Relationships include:

- Ally
- Friendly
- Neutral
- Suspicious
- Hostile

Built-in factions provide useful defaults; creator-defined faction IDs are supported through a deterministic Runtime registry. Unknown creator-defined faction pairs default to Neutral unless a future explicit relationship authoring layer defines otherwise.

## 7. Perception and anti-cheat contract

AI-03 must implement legitimate information flow only.

Characters may learn about targets through:

- direct visual perception;
- hearing/noise stimuli;
- damage attribution;
- legitimate ally/squad reports;
- explicit governed script events.

They must not refresh exact hidden player/target coordinates from the underlying Scene transform after line of sight or legitimate knowledge is lost.

Visual perception should account for range, FOV, line of sight and reaction delay. Hearing consumes explicit sound/noise stimuli rather than inspecting hidden target position.

## 8. Memory and awareness

Memory records include a knowledge source such as Seen, Heard, DamagedBy, AllyReport or Script; last-known position/velocity; confidence; threat; timestamps; hostile/dead flags and event memories.

Awareness progresses through meaningful states such as:

`Unaware -> Interested -> Suspicious -> Alerted -> Combat -> Searching -> decay/return`

When a target is lost, search behaviour operates from remembered legitimate information, not the current hidden transform.

## 9. Decision architecture

Use a hybrid state + utility model.

High-level candidate intents include:

- Idle
- Patrol
- Guard
- Investigate
- Search
- Observe
- Warn
- Alert Allies
- Chase
- Hold Position
- Take Cover
- Attack
- Reload
- Retreat
- Flee
- Surrender
- Interact
- Scripted
- Dead

Utility scores are influenced by threat/danger, confidence, injury, ammunition, cover, allies, direct sight vs hearing, time since last seen, personality and effective weapon range. Use hysteresis/commitment to avoid rapid oscillation.

## 10. Movement and patrol

Movement commands are high-level requests such as MoveTo, MoveToEntity, Stop, Face and Strafe. They route through the existing native navigation/CharacterComponent authority.

Normal path recovery must not teleport actors.

Patrol Routes are creator-authored stable-reference entities with loop/ping-pong/random traversal, home position/facing, roaming/leash semantics as appropriate.

## 11. Squad communication

Squad communication is imperfect and information-limited. Members may share alerts, last-known position and confidence, but not magically distribute exact hidden target coordinates.

Communication respects Character capability and range.

## 12. Cover

Start with explicit creator-authored Cover Points behind a stable cover interface. AI scores and reserves them. Automatic generated cover may be added later behind the same interface.

Cover must not introduce a second navigation system.

## 13. Combat

Combat intelligence reasons about weapon-effective range, ammunition, reload safety, line of sight, cover, suppression, health, allies and personality.

A reusable weapon descriptor should expose style, ranges, fire/reload/ammunition behaviour and other AI-relevant semantics.

If production health/damage/weapon authority is missing, build the smallest reusable service boundary needed. Do not bury rifle-specific damage code inside cognition.

Characters may advance, hold, retreat, flee or surrender according to circumstances and authored traits.

## 14. Smart Objects

Contextual interaction should use reusable Smart Objects/context points rather than custom one-off AI branches. Planned examples include Door, Cover, Chair, Ladder, Turret, Bed, Workbench, Guard Point and Generic Use Point.

Smart Objects expose usable slots/reservations, navigation alignment and semantic action/animation intent.

## 15. Animation semantic layer

AI requests semantic presentation intents rather than hard-coded clip names, for example:

- `Locomotion.RunForward`
- `Combat.Rifle.Fire`
- `Combat.Rifle.Reload`

Animation Sets map semantics to native accepted clips with a defined fallback chain. AI-06 owns this integration and must reuse Phase 7 animation/humanoid/IK/look-at systems.

## 16. Runtime lifecycle and performance

Runtime Character state is keyed by stable Character ID with transient ECS caches.

Scene synchronization must be deterministic and transactional. Failure must not leave a prefix of Characters active or stale resolved AI state published.

Save/Open/Test Level/Reset/Screen transitions must have explicit teardown/rebuild semantics. Cognition is transient and resets deterministically.

Performance requirements:

- no naive per-frame all-pairs `O(N²)` perception;
- multi-rate cognition ticks;
- deterministic/stable-ID staggering;
- spatial filtering;
- bounded ray/LOS budgets;
- repath thresholds;
- cognition LOD tiers such as Full / Reduced / Dormant;
- benchmark representative Character counts such as 1, 10, 25, 50 and 100.

## 17. Diagnostics contract

By the integrated vertical slice, diagnostics should expose enough state to explain AI decisions, including:

- stable Character ID;
- profile/role/personality/faction/squad;
- awareness/suspicion;
- current intent and age;
- target stable ID;
- knowledge source/confidence;
- last seen / last-known information;
- navigation goal/path/repath/stuck state;
- health/ammunition when combat exists;
- cover/reservation;
- top utility scores;
- last transition and reason.

Useful overlays later include vision/hearing/path/memory/cover/squad/look-at/search evidence.

## 18. Lua boundary

Lua may query and issue safe high-level AI operations using stable IDs. It does not become the ordinary NPC brain and must not gain raw unrestricted Wicked-global authority.

## 19. Persistence and Undo/Redo

Persist creator authoring only. Do not serialize active suspicion, current target, search timer, utility score or other transient cognition as project truth.

All persistent creator edits are command-backed and must have Undo/Redo semantics. Save/Open round trips must preserve governed metadata and references.

## 20. Failure handling

Malformed profile metadata, unsupported schema versions, duplicate stable IDs, malformed overrides, missing governed references and invalid setup must fail closed with useful diagnostics. Runtime must not silently normalize corrupt persisted authoring into apparently valid behaviour.

## 21. Implementation gates

### AI-01 — Character Foundation

MAKE CHARACTER, Inspector, native `CharacterComponent` binding/adoption, stable identity, persistence, Undo/Redo, Runtime discovery/reset and baseline diagnostics.

### AI-02 — Profiles, Factions & Runtime State

Layered profile composition, persisted Advanced AI overrides, faction semantics/registry, resolved transient Runtime Character records, stable reference resolution, deterministic reset and diagnostics. No production perception or decision loop yet.

### AI-03 — Perception & Memory

Sight, hearing, legitimate stimuli, memory, confidence, suspicion/awareness and explicit anti-cheat coverage proving hidden transforms cannot refresh memory.

### AI-04 — Decision & Patrol

Utility decisions/hysteresis, patrol, investigate, search, return-to-role and native movement through existing `NavigationService` / `CharacterComponent`.

### AI-05 — Combat Intelligence

Weapon/health/damage service seam as required; ammunition/reload/range reasoning; combat intents; retreat/flee/surrender; combat events and integrated diagnostics.

**First intended integrated Debug+Release/full Windows CI boundary: after AI-05.** Focused gate validation may occur earlier when needed without opening the integrated PR.

### AI-06 — Animation Integration

Semantic Animation Sets driving accepted Phase 7 native animation/humanoid/IK/look-at systems.

### AI-07 — Squads & Communication

Imperfect information sharing, squad state and legitimate alert propagation.

### AI-08 — Cover

Explicit Cover Points, scoring/reservation and native navigation integration.

### AI-09 — Smart Objects

Reusable contextual interaction points and reservations.

### AI-10 — Lua / Diagnostics / Performance / Packaged Hardening

Safe scripting surface, complete diagnostics, scaling/LOD/budget hardening and standalone parity. This is the second intended full integrated CI/acceptance boundary if practical.

## 22. Forbidden shortcuts

Do not introduce:

- Recast or a parallel navigation implementation;
- transform-driven normal NPC movement;
- a second physics/scene/gameplay loop;
- a parallel animation runtime;
- a second generic event bus;
- raw ECS IDs in persisted authoring;
- Lua as ordinary production AI;
- global unrestricted Wicked Lua as the Character AI surface;
- hidden-target transform cheating;
- an LLM Runtime dependency;
- creator-facing behaviour-tree complexity for normal setup;
- all-pairs perception every frame;
- normal teleport-based stuck recovery;
- stock Wicked editor windows as Renegade creator UX;
- production cognition running inside Studio;
- compile-only claims of completion.

## 23. Acceptance scene

The integrated acceptance scene should contain at least:

- Player Start;
- native Navigation Grid;
- cautious enemy `Guard_A`, rifle, Squad Alpha, patrol route;
- balanced soldier `Guard_B`;
- timid `Civilian_A`;
- patrol route;
- cover points;
- a Door/Smart Object once that gate exists.

The scene must ultimately prove:

1. patrol;
2. nonvisual noise investigation;
3. no hidden exact target knowledge;
4. visual hostile confirmation;
5. imperfect squad alert;
6. cover/reload behaviour;
7. loss of LOS -> last-known search;
8. search timeout -> return to authored role;
9. civilian flee/cower behaviour;
10. Save/Open persistence of authoring;
11. deterministic Test Level/reset behaviour;
12. packaged standalone parity.

## 24. Definition of done

The creator can import/select a character, choose MAKE CHARACTER, select useful presets, optionally assign weapon/patrol/squad and Advanced AI overrides, then run the Level and observe intelligent behaviour without writing Lua. The behaviour can be explained through diagnostics, authoring survives Save/Open, Runtime cognition resets correctly, and standalone behaviour matches Test Level within the accepted platform scope.
