# Renegade Engine — Current Handoff

**Date:** 14 September 2026  
**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`  
**Programme branch:** `feature/character-ai-programme`  
**Accepted Phase 7 baseline:** `d36918878776d0d91e0c39f88f6764a1926a6534`  
**Accepted AI-01 checkpoint:** `674b1efc3e1b81e6f55bf080f539f29a0c466db4`  
**Clean AI-02 implementation checkpoint:** `f808d38f11672fe75bfbbdeaec989bef7f43f32c`  
**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Active recovery — not accepted

**Commit:** `69cd04494986772751524869a0bd0541e11599f5` on
`fix/cw05-headless-ownership-test` (not pushed or merged).

This recovery corrects two owner-visible failures that automated CW-05 cache
checks did not prove:

- Story Flow **Add New Level** now serializes one persistent native Wicked
  navigation tile immediately. It is a bounded 64m x 16m x 64m origin tile
  (128 x 32 x 128 at 0.5m), marked as the default tile. TestGame therefore
  sees an authored grid and the existing cache reuses/rebakes that grid from
  the navigation geometry signature instead of manufacturing a hidden
  fallback-only grid.
- The importer’s external-animation file-picker button now explicitly restores
  its enabled state whenever the Character/Animation page refreshes. The only
  disabled state is while its native file dialog is already open.

Changed files: `EngineBridge/include/renegade/bridge/NavigationService.h`,
`EngineBridge/src/NavigationService.cpp`,
`EngineBridge/src/StoryFlowLevelLifecycleService.cpp`,
`Studio/src/CreatorImportPreviewWindow.h`,
`Tests/StoryFlowGate4LevelLifecycleTests.cpp`, and
`Tests/CharacterWorkflowSourceContract.cmake`.

Local evidence: `git diff --check` passed; targeted source-contract text checks
passed. A GNU syntax-only attempt was blocked before project code by the local
checkout lacking the SDL/CMake Windows build environment. No Windows build, CI,
or owner visual test has been claimed.

Required next verification on the exact commit:

1. Build Studio and `RenegadeStoryFlowGate4LevelLifecycleTests` on Windows.
2. Create a Story Flow level; save/reopen and confirm one visible Navigation
   Tile exists at the origin.
3. Add terrain or a navigation obstacle, run TestGame twice, and confirm first
   bake/rebuild then unchanged cache reuse in Runtime diagnostics.
4. In Character -> Animation, click **+ ADD ANIMATION FILES...** and confirm
   the native local file picker opens and returns selected files as slots.

## Current programme state

The active programme is **Renegade Character & AI**.

Repository-native implementation authority:

- `docs/RENEGADE_CHARACTER_AI_SYSTEM_IMPLEMENTATION_AUTHORITY.md`
- detailed gate/evidence handoff: `docs/AI_IMPLEMENTATION_HANDOFF.md`

The original private Codex AI workspace was lost before it reached GitHub. That failure changed the workflow: meaningful gate work is now checkpointed remotely so another engineer can continue from repository state alone.

## Gate status

- **AI-01 — Character Foundation:** COMPLETE; focused Windows validation green at accepted checkpoint `674b1efc3e1b81e6f55bf080f539f29a0c466db4`.
- **AI-02 — Profiles, Factions & Runtime State:** IMPLEMENTED; source/architecture audit repairs complete; clean implementation checkpoint `f808d38f...`; focused Windows validation pending.
- **AI-03 — Perception & Memory:** NOT STARTED.
- **AI-04 — Decision & Patrol:** NOT STARTED.
- **AI-05 — Combat Intelligence:** NOT STARTED.
- **AI-06 — Animation Integration:** NOT STARTED.
- **AI-07 — Squads & Communication:** NOT STARTED.
- **AI-08 — Cover:** NOT STARTED.
- **AI-09 — Smart Objects:** NOT STARTED.
- **AI-10 — Lua / Diagnostics / Performance / Packaged Hardening:** NOT STARTED.

## AI-01 accepted evidence

Focused Windows run `34821199576` proved:

- x64 configure against pinned Wicked;
- `RenegadeCharacterAiFoundationTests` Debug build and PASS;
- `RenegadeCharacterAiSourceContract` PASS;
- `RenegadeRuntime` Debug build;
- `RenegadeStudio` Debug build including Character Inspector;
- focused CTest 2/2 PASS.

The temporary AI-01 validation workflow was removed after evidence was recorded.

## AI-02 implemented scope

AI-02 adds the semantics layer needed by later cognition without beginning perception/decision logic:

- layered Character tuning: Type -> Role -> Personality -> Skill -> Awareness -> explicit overrides;
- versioned persisted Advanced AI overrides;
- built-in and creator-defined faction registry plus relationship semantics;
- transient Runtime Character records ordered/keyed by stable identity;
- stable Patrol Route and Weapon reference resolution;
- deterministic Runtime reset and failure cleanup;
- bounded AI-02 diagnostics;
- Character Inspector Skill/Awareness/Faction/effective profile and collapsed grouped ADVANCED AI controls;
- focused AI-02 regression and source-contract tests.

## AI-02 audit repairs

Before validation the source/architecture audit found and repaired:

1. incomplete creator-faction registration;
2. caller-dependent failure cleanup;
3. malformed persisted profile enums silently falling back to valid-looking defaults;
4. locale-sensitive/ambiguous advanced payload parsing;
5. no repair path for corrupt Advanced AI metadata;
6. text inputs committing one Undo entry per keystroke;
7. invalid Advanced state showing fallback-looking controls;
8. dead CUSTOM faction combo entry;
9. insufficient corruption/recovery regressions;
10. unsupported/missing owned Character schema markers being silently skipped;
11. missing repository-native Character/AI authority documentation;
12. accidental historical feature-matrix drift during evidence reconstruction.

All twelve are repaired in the clean AI-02 implementation checkpoint. `FEATURE_MATRIX.csv` now differs from accepted AI-01 by only the intended `REN-AI-001` row.

## Architecture boundaries still in force

- Studio = authoring only.
- EngineBridge = stable semantics and command-backed creator APIs.
- Runtime = transient execution/cognition.
- Wicked = Scene/ECS/native `CharacterComponent`, physics/navigation/animation authority.
- Stable Renegade IDs are persisted; raw ECS IDs are Runtime caches only.
- No Recast/parallel navigation, second physics/scene loop, second event bus, Lua ordinary brain, transform-driven NPC movement or LLM Runtime dependency.
- Faction relationship does not imply target perception or hidden position knowledge.
- AI-03 must prove hidden transforms cannot refresh last-known target state.
- Animation Set execution/materialization remains AI-06 and must reuse the accepted Phase 7 stack.

## Pre-validation closeout

- AI-02 implementation/audit work was squashed to `f808d38f11672fe75bfbbdeaec989bef7f43f32c`, directly parented by accepted AI-01.
- The intended AI-02 diff contains 18 files.
- No temporary validation workflow or corrupt recovery payload remains.
- Top-level CMake includes `Tests/CharacterAiFoundation.cmake`, so AI-01/AI-02 test targets are in the Windows graph.
- No expensive standard workflow has run for AI-02. The only programme-branch Actions run remains successful AI-01 focused run `34821199576`.

## Validation deliberately not run yet

The repaired AI-02 candidate has **not** yet been claimed to compile or pass Windows tests. This is intentional: the owner requested complete pre-validation audit/repair/evidence closeout first.

The next validation is one focused branch-only Windows job that builds the AI-01 regression target, both AI-02 test executables, Runtime and Studio, then runs the AI-01/AI-02 source/executable tests. It must not trigger the planned full four-job Debug+Release integrated matrix reserved for AI-05.

## Required next action

Run focused AI-02 Windows validation against the exact pre-validation branch head. Repair any failure before AI-03. Do not begin AI-03 until the exact AI-02 candidate is green.
