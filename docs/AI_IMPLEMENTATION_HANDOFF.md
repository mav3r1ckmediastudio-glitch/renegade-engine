# Renegade Character & AI implementation handoff

## Baseline

- Original accepted Phase 7 baseline: `d36918878776d0d91e0c39f88f6764a1926a6534`.
- Programme branch: `feature/character-ai-programme`.
- Current recoverable branch head before this document commit: `d36918878776d0d91e0c39f88f6764a1926a6534`.
- Current gate: AI-01 to AI-05 recovery. Status: **PARTIAL / RECOVERY REQUIRED**.
- An earlier ephemeral workspace contained local checkpoint `7c2937711a869ca6d597e1bfcd03d02583f1a902` (`Implement Character AI foundations through combat intent`) plus additional uncommitted improvements. The workspace was erased between execution turns before that commit was pushed. The object is not present on GitHub and was not found elsewhere under `/workspace`; its implementation must be reconstructed from the specification and the recovery notes below.

## Completed gates

No AI gate is currently marked **COMPLETE** in the recoverable Git branch. Do not infer completion from the lost local checkpoint identifier.

### AI-01 — Character foundation

Status: **PARTIAL / LOST LOCAL IMPLEMENTATION TO RECONSTRUCT**.

The lost implementation had:

- a `CharacterService` schema stored in Wicked `MetadataComponent` values;
- stable UUID references through `IdentityService`;
- command-backed MAKE/REMOVE CHARACTER and settings edits;
- native Wicked `CharacterComponent` promotion, inactive in Studio;
- a compact Character Inspector registered through the existing inspector-section framework;
- Runtime discovery and live diagnostic registration;
- focused authoring, persistence-contract and Undo/Redo tests.

### AI-02 — Profiles, factions and runtime state

Status: **PARTIAL / LOST LOCAL IMPLEMENTATION TO RECONSTRUCT**.

The lost implementation had layered role, personality, skill and awareness tuning; optional persisted overrides; a faction relationship matrix; transient Runtime character records; stable reference resolution; and deterministic reset. Cautious, Aggressive and Timid profile distinctions were tested.

### AI-03 — Perception and memory

Status: **PARTIAL / LOST LOCAL IMPLEMENTATION TO RECONSTRUCT**.

The lost implementation had staggered 5 Hz cognition, distance/FOV/LOS vision using Wicked scene intersection, bounded sound stimuli, player footsteps, a damage-report seam, last-known-position memory, decay, suspicion/awareness and diagnostics. A focused test explicitly proved that memory decay cannot read or update from a hidden target transform.

### AI-04 — Decision runtime and patrol

Status: **PARTIAL / LOST LOCAL IMPLEMENTATION TO RECONSTRUCT**.

The lost implementation had utility-scored intents with hysteresis, native `NavigationService` goals, Wicked `CharacterComponent::Turn/Move`, repathing/stuck recovery, search timeout/return-to-role, patrol modes and stable patrol-point references. Later uncommitted work added command-backed Patrol Route creation/editing, an ADD-menu entry, route/weapon entity pickers, a Patrol Route Inspector, patrol waiting, vertical/peripheral sight and visual reaction latency.

### AI-05 — Combat intelligence

Status: **PARTIAL / LOST LOCAL IMPLEMENTATION TO RECONSTRUCT**.

The lost implementation had weapon descriptors, ammo/reload timing, range reasoning, Attack/Chase/Hold/Retreat/Flee/Surrender scoring, health reads, damage reports and AI gameplay events. Later uncommitted work introduced a separate reusable `RuntimeCombatService` boundary for deterministic bounded-accuracy hit and transient health resolution so ordinary combat did not depend on Lua or bury authoritative health inside cognition. This boundary had only compile-level validation and needs architectural review during reconstruction.

## Current work

The immediate task is recovery, not new AI-06 work.

What works in the current recoverable branch:

- The accepted Phase 7 baseline is present at the exact required SHA.
- The pinned Wicked submodule resolves to `3a800b7134aafe58461093c8abb2e274d4e64033`.
- A long-lived AI programme branch exists.
- Push-trigger policy has been inspected: a push to `feature/character-ai-programme` does **not** match the expensive workflow push branches.

What remains unfinished:

- Reconstruct every lost AI source, test, CMake and documentation change.
- Re-run all focused local validation after reconstruction.
- Audit AI-01 through AI-05 against the specification before declaring any gate complete.
- Push the reconstructed checkpoint before further substantial work.
- Do not start AI-06 until the integrated AI-05 Windows CI is green.

Known validation blocker from the lost workspace:

- A Linux CMake build reached `366/393` before failing in pre-existing baseline code because the DirectXMath WinAdapter macro `SetLastError(ERR)` collides with `SceneService::SetLastError(std::string error)`. Representative diagnostics were `invalid pure specifier (only '= 0' is allowed) before '::' token` at `EngineBridge/include/renegade/bridge/SceneService.h:142` and `expected ';' at end of member declaration` at line 145. This was not introduced by Character AI, but it prevents using a full Linux build as Windows parity proof.

## Architectural notes

- Studio owns authoring only; it must not simulate production cognition.
- EngineBridge owns stable Character, Patrol Route, weapon descriptor and health semantics.
- Runtime owns transient cognition, perception, decisions and execution.
- Wicked remains authority for scene/ECS, collision queries, `VoxelGrid`/`PathQuery`, physics and `CharacterComponent` movement.
- Persisted route and weapon references are Renegade stable UUIDs, never transient Wicked entity numbers.
- AI events use the existing `GameplayEventService`; no second general event bus is permitted.
- Ordinary AI is C++ Runtime behaviour and must not require Lua.
- Last-known position may only be refreshed by a legitimate sensed/reported stimulus. Memory decay never receives hidden live world coordinates.
- The repository has no mature general weapon/projectile/health subsystem. AI-05 therefore needs the smallest reusable combat/health/weapon boundary; it must not contain rifle-specific logic in `RuntimeCharacterSystem`.
- The Runtime player is a Wicked character-physics rigid body, not a Wicked `Scene::characters` component. Perception and footsteps must obtain its position through the existing physics-position service; the lost checkpoint initially got this wrong and the later uncommitted repair corrected it.
- Animation presentation remains AI-06 and must reuse the existing Phase 7 animation/humanoid/IK facilities.

## Tests and workflow evidence

Commands that passed in the lost workspace before erasure:

- `cmake -DRENEGADE_SOURCE_DIR="$PWD" -P Tests/CharacterAiSourceContract.cmake` — **PASS**.
- `git diff --check` — **PASS** before the earlier checkpoint commit.
- Direct GNU C++17 object compilation of `Runtime/src/RuntimeCharacterSystem.cpp` — **PASS**.
- Direct GNU C++17 object compilation of `Runtime/src/RuntimeCombatService.cpp` after the later combat-boundary edit — **PASS**.
- Direct GNU C++17 object compilation of `Tests/CharacterAiFoundationTests.cpp` — **PASS**.
- Normal CMake/Ninja compilation of `EngineBridge/src/CharacterService.cpp` — **PASS**.
- Full Linux CMake/Ninja build — **BLOCKED at 366/393 by the pre-existing `SetLastError` macro collision described above**.

Commands executed after workspace recovery:

- `git ls-remote --heads https://github.com/mav3r1ckmediastudio-glitch/renegade-engine.git 'feature/character-ai*' 'refs/heads/main'` — **PASS**; only `main` was present, at the required baseline SHA.
- `git clone --recurse-submodules https://github.com/mav3r1ckmediastudio-glitch/renegade-engine.git renegade-engine` — **PASS**.
- `git rev-parse HEAD` — **PASS**, `d36918878776d0d91e0c39f88f6764a1926a6534`.
- Workflow inspection of `.github/workflows/windows-baseline.yml`, `.github/workflows/studio.yml` and `.github/workflows/lp04-runtime-handshake.yml` — **PASS**. `windows-baseline.yml` runs on push only for `main` and `agent/**`; `studio.yml` runs on push only for `main`; the runtime handshake is manual-only. A normal push of `feature/character-ai-programme` is therefore a legitimate non-CI remote checkpoint. Opening a pull request against `main` will trigger the Windows workflows and must wait for the integrated AI-05 checkpoint.

## NEXT IMPLEMENTATION STEP

Reconstruct the lost AI-01 foundation first from the supplied implementation specification and the detailed recovery notes above: add `CharacterService` authoring/persistence commands, the registered Character Inspector and focused authoring/Undo/Redo tests; compile `CharacterService.cpp`, run its source-contract test, update this handoff with exact results, commit as `AI-01: add governed character authoring foundation`, and push `feature/character-ai-programme` before beginning AI-02 reconstruction.
