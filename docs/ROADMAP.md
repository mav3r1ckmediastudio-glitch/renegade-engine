# Renegade Engine Roadmap

**Current programme:** Character & AI System  
**Accepted Phase 7 baseline / current main at programme start:** `d36918878776d0d91e0c39f88f6764a1926a6534`  
**Programme branch:** `feature/character-ai-programme`  
**Accepted AI-01 checkpoint:** `674b1efc3e1b81e6f55bf080f539f29a0c466db4`  
**Accepted AI-02 checkpoint/evidence head:** `09ee66e6b925b305a75b81cf0cf00170ca42edfc`  
**Clean AI-03 implementation checkpoint:** `307161dee97f0b9aa39eaab4e8d655240c072171`  
**AI-03 focused validation run:** `34843305044`  
**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Current state

Phase 6 Playable Core and Phase 7 Character/Animation foundations are accepted programme prerequisites. Character & AI builds on those accepted systems rather than introducing parallel Runtime stacks.

Repository-native Character/AI authority:

- [`RENEGADE_CHARACTER_AI_SYSTEM_IMPLEMENTATION_AUTHORITY.md`](RENEGADE_CHARACTER_AI_SYSTEM_IMPLEMENTATION_AUTHORITY.md)
- [`AI_IMPLEMENTATION_HANDOFF.md`](AI_IMPLEMENTATION_HANDOFF.md)

The product goal is **AAA-style-ish AI architecture with GameGuru-style setup simplicity**: creators configure understandable Character/Role/Personality/Faction/Skill/Awareness choices while Runtime composes deterministic native-backed behaviour without requiring ordinary users to write Lua or author behaviour trees.

## Accepted foundations reused by Character & AI

| Foundation | Accepted ownership used by AI |
|---|---|
| Phase 6 Player/Core | Player Start, possession, gameplay lifecycle and Runtime process boundaries. |
| Phase 6 scripting/events | Governed `.rscripts`, typed stable references and existing `GameplayEventService`; AI must not create a second generic event bus. |
| Phase 6 navigation | Existing `NavigationService` over Wicked `VoxelGrid`, `PathQuery` and native `CharacterComponent`; AI must not introduce Recast or transform-driven normal movement. |
| Phase 7 animation | Native `AnimationComponent`, humanoid mapping/retarget, IK, look-at, expressions and timeline authoring; AI-06 will drive these accepted systems semantically. |
| Stable identity | `IdentityService` stable IDs are persisted; raw Wicked ECS IDs are Runtime caches only. |
| Diagnostics | Existing bounded Runtime/Studio diagnostics are extended rather than replaced. |

## Character & AI gates

### AI-01 — Character Foundation

**Status: COMPLETE / focused Windows validation green.**

Delivered:

- MAKE CHARACTER / REMOVE CHARACTER;
- stable Character identity and versioned metadata;
- non-destructive native Wicked `CharacterComponent` adoption/ownership;
- command-backed creator authoring and Undo/Redo;
- Character Inspector integration;
- deterministic Runtime discovery/activation/reset;
- baseline Character diagnostics;
- fail-closed duplicate-ID/invalid-controller handling.

### AI-02 — Profiles, Factions & Runtime State

**Status: COMPLETE / focused Windows validation green.**

Delivered:

- deterministic tuning composition: Type -> Role -> Personality -> Skill -> Awareness -> explicit overrides;
- versioned Advanced AI overrides and repairable malformed-payload handling;
- built-in and creator-defined faction registry plus semantic relationships;
- stable-ID-keyed transient Runtime Character records;
- Patrol Route and Weapon stable reference resolution;
- deterministic reset/failure cleanup;
- Character Inspector Skill/Awareness/Faction/effective profile/grouped Advanced AI controls;
- bounded AI-02 diagnostics and corruption/recovery regression coverage.

Focused Windows validation run `34837594589` completed green.

### AI-03 — Perception & Memory

**Status: COMPLETE / focused Windows validation green.**

Clean implementation checkpoint: `307161dee97f0b9aa39eaab4e8d655240c072171`.

Delivered:

- bounded 5 Hz stable-ID-staggered Runtime cognition;
- physics-authoritative Runtime-player position/velocity;
- central/peripheral sight with horizontal/vertical FOV;
- native Wicked `Scene::Intersects()` line of sight;
- observer-self hierarchy rejection in LOS;
- visual and audio reaction latency;
- explicit bounded sound stimuli plus Runtime-player footsteps;
- governed damage-attribution stimulus seam;
- Seen / Heard / DamagedBy memory;
- last-known position/velocity, confidence, threat and age decay;
- suspicion and `Unaware -> Interested -> Suspicious -> Alerted -> Combat -> Searching` progression;
- deterministic reset and stale-player scene-revision guard;
- diagnostics for awareness, suspicion, knowledge, memory, LOS and stimulus pressure;
- explicit anti-cheat tests proving hidden coordinates cannot refresh memory;
- regression for repeated-sound reaction starvation.

Focused Windows run `34843305044` built AI-01/02/03 test targets plus `RenegadeRuntime` and `RenegadeStudio` Debug and passed all **7/7** Character-AI focused tests.

AI-03 deliberately contains no utility decision/patrol execution or combat action selection; those remain AI-04/05.

### AI-04 — Decision & Patrol

**Status: NOT STARTED.**

Implement utility-scored intents with hysteresis, Guard/Patrol/Investigate/Search/Return-to-role behaviour and movement through the accepted native `NavigationService` / Wicked `CharacterComponent` authority. All target pursuit/search information must consume legitimate AI-03 memory rather than hidden transforms.

### AI-05 — Combat Intelligence

**Status: NOT STARTED.**

Implement the smallest reusable health/damage/weapon boundary required, weapon-range/ammo/reload reasoning, combat intent selection, retreat/flee/surrender and integrated diagnostics/events.

**First intended full integrated Debug+Release/full CTest Windows CI boundary: AI-05.** Earlier gates may use focused branch-only validation when necessary.

### AI-06 — Animation Integration

**Status: NOT STARTED.**

Map semantic AI intents such as locomotion/fire/reload to Animation Sets and drive the accepted Phase 7 native animation/humanoid/IK/look-at stack. No second animation runtime.

### AI-07 — Squads & Communication

**Status: NOT STARTED.**

Implement imperfect legitimate information sharing, alert propagation and squad state without magic exact hidden target knowledge.

### AI-08 — Cover

**Status: NOT STARTED.**

Implement explicit creator Cover Points first, scoring/reservation and native-navigation use. Generated cover may come later behind the same interface.

### AI-09 — Smart Objects

**Status: NOT STARTED.**

Implement reusable context points/reservations for Door, Cover, Chair, Ladder, Turret, Bed, Workbench, Guard Point and Generic Use Point semantics.

### AI-10 — Lua / Diagnostics / Performance / Packaged Hardening

**Status: NOT STARTED.**

Finish safe high-level Lua integration, complete diagnostics/overlays, cognition LOD and scheduling/budget hardening, representative 1/10/25/50/100 Character performance evidence and packaged standalone parity.

This is the second intended full integrated validation/acceptance boundary if practical.

## Non-negotiable architecture

- Studio owns authoring, not production cognition.
- EngineBridge owns stable Renegade semantics and command-backed creator APIs.
- Runtime owns transient execution/cognition.
- Wicked remains Scene/ECS/native physics/navigation/animation authority.
- No Recast or second navigation world.
- No second physics/Scene/gameplay loop.
- No transform-driven normal NPC movement.
- No second generic gameplay event bus.
- No raw ECS IDs in persisted authoring.
- No Lua ordinary-NPC brain and no unrestricted Wicked-global Lua surface.
- No hidden-target transform cheating.
- No LLM Runtime dependency.
- No naive all-pairs perception every frame.
- No compile-only definition of done.

## Integrated acceptance target

The completed programme must prove a representative Level containing Player Start, native navigation, a cautious enemy Guard on patrol, a second soldier, a timid civilian, patrol route, cover and later Smart Objects.

The final behaviour proof includes patrol, nonvisual noise investigation, legitimate visual confirmation, imperfect squad alert, cover/reload decisions, loss-of-LOS last-known search, search timeout/return-to-role, civilian flee/cower, Save/Open authoring persistence, deterministic Test Level/reset and packaged standalone parity.

## Verification policy

- A gate is not complete merely because source exists.
- Focused build/test evidence is required before moving to the next gate when the gate changes executable code.
- Visual or behavioural owner failure overrides nominal automated success.
- Save/Open is required for persisted authoring.
- Gameplay-facing state must be exercised in the real Runtime process at the appropriate integrated gate.
- Full integrated Debug+Release/full-test CI remains intentionally concentrated at AI-05 and AI-10 to avoid wasting long builds while known implementation work is still changing.
