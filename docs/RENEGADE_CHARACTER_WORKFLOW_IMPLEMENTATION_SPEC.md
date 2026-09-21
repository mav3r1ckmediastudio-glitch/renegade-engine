# Renegade Character Workflow & AI-06 Implementation Specification

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`  
**Implementation authority:** This document becomes authoritative once the owner signs it off and it is committed to the repository.  
**Primary implementation agent:** Codex  
**Continuation/recovery agent:** ChatGPT must be able to continue directly from GitHub at any point without access to Codex's private workspace.  
**Status:** Design specification for owner review. Do **not** begin implementation until the owner explicitly approves this document.

---

## 1. Purpose

This specification defines the creator-facing Character workflow that Renegade must implement on top of the existing Wicked-native character, humanoid, animation, AI, navigation and combat systems.

The goal is not to replace Wicked's native animation, humanoid, IK, look-at, character controller, navigation or scene systems. The goal is to turn those existing lower-level capabilities into a coherent Renegade creator workflow.

The normal user experience must be:

> **Import a character once, prepare its animations during import, see it immediately as a Character asset, drag it into a level, configure what that instance does, and play.**

A creator must not be required to understand Wicked ECS entity IDs, raw stable IDs, manual humanoid retarget commands, native `AnimationComponent` ownership, source animation indices, or other implementation details simply to get a character into a playable level.

Advanced Wicked-native tooling remains available. It is not deleted.

---

## 2. Product Principles

Renegade's intended character workflow is **simple first, depth underneath**.

The system must preserve these principles:

1. A character should become a usable **Character Asset during import**, not after a chain of scene-level repair/setup operations.
2. Animation preparation is primarily an **asset-authoring concern**.
3. Gameplay behaviour is primarily a **scene-instance concern**.
4. A configured scene instance can be duplicated repeatedly and must retain its authored gameplay configuration while each duplicate receives its own identity/runtime state.
5. Dragging the original Character Asset from the Asset Browser again creates a fresh independently configurable instance, not a copy of an existing configured scene instance.
6. A configured Character instance can optionally be saved as a reusable **Character Prefab**.
7. The normal workflow must not require typing raw Stable IDs.
8. Existing advanced/native tools remain accessible for users who want them.
9. Do not impose a fixed Renegade armature or a mandatory Renegade animation library.
10. Do not create competing skeletal, navigation, event, humanoid, IK, look-at or character-controller systems. Renegade must continue to use Wicked-native ownership underneath.

---

## 3. Current Accepted Architecture That Must Be Preserved

Before changing code, inspect the current repository and verify the actual branch state. Do not rely solely on this document's snapshot.

At the time this specification was authored:

- Active AI programme PR: `#158`
- Programme branch: `feature/character-ai-programme`
- Current head observed: `cc31ef318ec55db5d115b48a0242665ce571c561`
- Accepted Phase 7 baseline beneath the programme: `d36918878776d0d91e0c39f88f6764a1926a6534`
- Windows baseline CI for `cc31ef...` had passed.
- Renegade Studio CI for `cc31ef...` was still running.
- Do not assume those facts are still current when implementation begins. Re-check GitHub first.

The existing architecture already includes:

- `EngineBridge/include/renegade/bridge/CharacterService.h`
  - Character metadata and schema
  - `CharacterAuthoringSettings`
  - `MakeCharacterCommand`
  - `RemoveCharacterCommand`
  - `SetCharacterSettingsCommand`
  - existing `animationSetId`, patrol, squad, weapon, combat, skill, awareness and behaviour settings

- `EngineBridge/include/renegade/bridge/HumanoidRetargetService.h`
  - Wicked `HumanoidComponent` mapping
  - auto-map support
  - supported external animation formats:
    - WISCENE
    - FBX
    - GLTF
    - GLB
    - VRM
    - VRMA
  - `RetargetHumanoidAnimationsCommand`
  - command-owned baked retarget snapshots for deterministic Undo/Redo

- `EngineBridge/include/renegade/bridge/CreatorModelImportRecipe.h`
  - current model import recipe
  - embedded/source animation recipe entries:
    - `sourceAnimationIndex`
    - `name`
    - `start`
    - `end`
    - `enabled`

- `Studio/src/StudioApplication.cpp`
  - current guided model importer
  - current embedded animation clip editing workflow

- `Studio/src/Phase7Gate7BHumanoidRetargetInspector.cpp`
  - current post-import external animation retarget UI

- `Studio/src/Phase7Gate7AAnimationInspector.cpp`
  - native Wicked clip playback/editing

- `Studio/src/Phase7Gate7CCharacterControlsInspector.cpp`
  - native IK / look-at / expressions controls

- `Studio/src/Phase7Gate7DNativeTimelineInspector.cpp`
  - native timeline/keyframe authoring

- `Studio/src/AICharacterInspector.cpp`
  - current Character and Advanced AI authoring UI

- `EngineBridge/include/renegade/bridge/AssetBrowserService.h`
  - already has `AssetType::Character`
  - already has `AssetType::Prefab`

- project structure already includes `Content/Characters`

The implementation must reuse and extend these systems. Do not independently replace them.

---

## 4. The Three-Layer Character Model

Renegade must clearly distinguish three concepts.

### 4.1 Character Asset

A Character Asset answers:

> **What is this actor physically and how is it animated?**

It owns or durably references:

- mesh
- materials
- skeleton/armature
- Wicked humanoid mapping where applicable
- prepared native Wicked animation clips
- durable animation-source/reimport provenance
- internal character-animation role/semantic mapping required by runtime
- Character asset classification/metadata

It does **not** own the scene identity of a placed actor.

It should not normally contain a specific faction, patrol route, combat role, or other scene gameplay setup unless those values are explicitly stored as a prefab/template layer.

### 4.2 Character Instance

A Character Instance answers:

> **What does this particular actor do in this level?**

It owns authored instance configuration such as:

- Character type
- role
- personality
- faction
- skill
- awareness
- combat style
- weapon/reference configuration
- squad configuration
- patrol configuration
- autonomous/flee/surrender/cover/communication flags
- actions/scripts
- advanced AI overrides
- future Character gameplay settings

Every placed Character instance has its own persistent identity and its own transient runtime state.

### 4.3 Character Prefab

A Character Prefab answers:

> **Give me another actor already configured like this one.**

A Character Prefab is created deliberately by the user from a configured scene Character.

It should reference the base Character Asset rather than duplicating mesh/skeleton/animation payload unnecessarily, while storing the reusable authored gameplay configuration.

Placing a Character Prefab must create a new independent scene Character with:

- a fresh persistent identity
- copied prefab authoring settings
- reset transient runtime state

---

## 5. Approved Creator Workflow

### 5.1 Start Import

The existing guided model importer must gain a clear designation:

**IMPORT AS**

- Model
- Character

The Character choice must be available before final import and remain visible throughout the workflow.

Choosing **Character** changes the import target and enables Character-specific preparation.

It must not merely add a cosmetic tag after a normal model import.

### 5.2 Character Rig Preparation

When importing as Character:

1. Detect the skeleton/armature.
2. Detect an existing Wicked humanoid mapping if one exists.
3. Attempt existing Renegade/Wicked humanoid auto-mapping where appropriate.
4. Validate the mapping.
5. Present concise status to the creator.
6. Allow an advanced/review route for manual mapping if auto-map is incomplete or incorrect.

Do not require manual humanoid mapping in the normal success path.

Do not invent a Renegade-only skeletal runtime.

### 5.3 Embedded Animation Clips

If the main imported mesh contains actions/animations:

- detect them automatically
- show them in the Character import animation list
- allow preview
- allow rename
- allow include/exclude
- preserve current useful start/end split/trim functionality
- retain deterministic import-recipe behaviour

Existing embedded animation functionality should be evolved rather than discarded.

### 5.4 External Animation Files

The Character importer must contain an obvious control such as:

**+ ADD ANIMATIONS...**

Requirements:

- accept the formats already supported by the Wicked/Renegade retarget service
- support selecting multiple local animation files in one operation where the underlying file-dialog API permits it
- if the current native dialog does not support multi-select, implement the closest robust queued/add-many experience possible rather than silently pretending multi-select exists
- a one-action-per-file workflow such as Mixamo must require no manual timeline slicing
- if one external file contains multiple actions/takes, expose them individually
- default a single-action external clip name sensibly from its action name or filename
- show source filename/provenance
- show clip name
- show duration/range
- show include/exclude state
- show compatibility/retarget status
- allow preview on the Character being imported

Example:

```text
ANIMATIONS

EMBEDDED
✓ Idle                2.10s
✓ Death               1.65s

ADDED FILES
✓ Walking.fbx   → Walking       1.20s   READY
✓ Running.fbx   → Running       0.82s   READY
✓ RifleAim.fbx  → Rifle Aim     2.00s   READY
✓ RifleFire.fbx → Rifle Fire    0.43s   READY

[ + ADD ANIMATIONS... ]   [ PLAY ] [ STOP ] [ LOOP ]
```

### 5.5 Retarget External Animations During Import

External animation sources must be retargeted/prepared **before the Character Asset is finalized**.

Reuse the existing Wicked-native retarget path and `RetargetHumanoidAnimationsCommand` behaviour.

Do not create a competing retargeter.

Embedded animations already targeting the imported character's skeleton do not need unnecessary retargeting.

External animations from another humanoid skeleton should be retargeted through the existing Wicked humanoid mapping/retarget system.

By the time the Character Asset is available in the Asset Browser, its included animations must be usable against that Character.

### 5.6 Animation Meaning / Runtime Mapping

The runtime must know which prepared clip corresponds to the actions it requests.

This mapping must be established as part of Character Asset preparation, not as a raw Stable-ID field the scene creator must type later.

Important constraints:

- Do **not** require a fixed Renegade armature.
- Do **not** require a standard Renegade animation library.
- Do **not** force specific filenames.
- Do **not** expose raw source animation indices or Stable IDs in the normal workflow.

The importer may auto-suggest animation roles from names and allow the creator to confirm/change them.

Internally, the existing AI-06 semantic animation contract may remain the runtime API, but it is an implementation detail.

Examples of internal semantic requests may include:

- locomotion idle/walk/run
- combat aim/fire/reload
- hit reaction
- death
- interaction/cower where supported

The prepared Character Asset should own or automatically reference the resulting mapping.

The current scene-level `animationSetId` text box must not remain the normal creator workflow.

If the implementation retains an Animation Set as a governed internal resource, it must be created/assigned automatically through the Character Asset workflow and exposed to advanced users through a proper picker/editor rather than raw ID entry.

### 5.7 Final Import Result

Pressing the final Character import action must produce a real reusable Character Asset.

Expected result:

```text
ASSET BROWSER

Characters
  Soldier
  Zombie
  Scientist
```

The Character should be classified as `AssetType::Character` and appear under the Character asset workflow immediately.

Do not leave it appearing merely as a generic Model that the user must later convert.

The Character Asset must be reimportable from its durable recipe/provenance.

---

## 6. Reimport and Provenance Requirements

External animation sources must become part of the durable Character import recipe.

Do not rely on ephemeral editor state.

Do not make arbitrary absolute filesystem paths the long-term authoring authority.

Use the existing governed import/resource provenance architecture wherever possible.

If source files originate outside the project:

- use the project's existing source/provenance conventions
- preserve enough governed/project-relative information for deterministic reimport
- do not invent an isolated second provenance system

The durable recipe must be capable of reproducing:

- main character source
- transform/material import choices
- embedded clip choices
- external animation source list
- source actions/takes where applicable
- clip renames
- include/exclude choices
- trim/range choices
- humanoid mapping/retarget decisions required for deterministic output
- runtime animation-role mapping

Reimport must not silently destroy user-authored Character configuration.

Schema changes must be versioned and backward-compatible where practical.

---

## 7. Dragging a Character Asset Into the Scene

Dragging a prepared Character Asset from the Asset Browser into a level must create a usable Character instance.

The placement path must:

1. instantiate the Character asset
2. create/assign a new persistent scene identity
3. establish Renegade Character ownership using the existing Character service/controller architecture
4. preserve the asset's prepared rig/animations
5. start with fresh/default gameplay authoring settings unless the source is a Character Prefab
6. contain no stale transient AI memory, perception, combat or runtime state

The user should not need to press **MAKE CHARACTER** after placing a properly prepared Character Asset.

The existing `MakeCharacterCommand` remains useful for:

- backward compatibility
- converting suitable generic models
- advanced/manual workflows

It should not be the mandatory normal path for a prepared Character Asset.

---

## 8. Character Selection and Inspector Behaviour

When a placed Character instance is selected, Renegade should automatically surface the Character authoring workflow.

At minimum:

- `CHARACTER` must be visible and expanded appropriately
- normal gameplay settings should be immediately accessible
- selecting a Character should not first present the user with generic model-only setup

The normal Character Inspector should focus on:

- Type
- Role
- Personality
- Faction
- Skill
- Awareness
- Combat
- Weapon
- Squad
- Patrol
- Actions/Scripts
- supported Character flags/settings

The user should be configuring **what this actor does**, not repairing the asset's rig.

---

## 9. Scene Duplication Rules

If the user configures a Character instance and duplicates it in the level:

```text
Soldier 01
Enemy / Patrol Guard / Aggressive / Rifle
        |
     Duplicate
        v
Soldier 02
Enemy / Patrol Guard / Aggressive / Rifle
```

the duplicate must copy authored gameplay configuration.

It must **not** copy:

- persistent character identity
- transient perception memory
- current target
- current suspicion
- current decision intent
- runtime ammo state unless authored as a starting value
- current health damage/runtime death state
- other transient runtime cognition

Each duplicate must be independently addressable at runtime.

Copy all appropriate authoring metadata and governed references using stable identity rules.

---

## 10. Dragging the Base Character Again

Dragging `Soldier` from the Asset Browser a second time is different from duplicating `Soldier 01`.

It must produce a fresh Character instance based on the base Character Asset.

Example:

```text
Soldier Asset
   |
   +--> Soldier 01 → Enemy / Patrol Guard
   |
   +--> Soldier 02 → Friendly / Companion
   |
   +--> Soldier 03 → Neutral / Guard
```

The base asset supplies physical/animation preparation.

The scene instance supplies gameplay configuration.

No previously configured scene instance should become accidental global defaults for future placements.

---

## 11. Save as Character Prefab

A configured Character scene instance must support an explicit action such as:

**SAVE AS PREFAB**

or

**SAVE CHARACTER PREFAB**

The resulting reusable prefab must appear as a Prefab asset and be placeable from the Asset Browser.

Example:

```text
Base Character Asset:
Soldier

Configured instance:
Faction      Enemy
Role         Patrol Guard
Personality  Veteran
Combat       Ranged
Weapon       Rifle
Can Cover    Yes
Can Talk     Yes

        ↓

SAVE AS PREFAB

        ↓

Enemy Rifle Soldier
```

Placement of that prefab creates a fresh scene identity while restoring the prefab's authored settings.

### 11.1 Prefab Reference Safety

Do not persist invalid scene-local raw ECS references into a portable prefab.

Stable, portable governed references may be preserved.

Scene-specific references such as a patrol route in one particular level must be handled deliberately:

- either excluded/cleared with clear UI,
- or included only if the referenced dependency is part of the prefab,
- or converted to an appropriate portable asset reference if the architecture supports it.

Never silently save a broken reference.

Initial prefab implementation should prefer correctness over pretending every scene-local dependency is portable.

---

## 12. Advanced Character Tools Must Remain Available

Do not remove the technical systems already built.

Advanced users must still be able to access, as appropriate:

- Humanoid / Retarget
- native Animation controls
- IK
- Look-at
- Expressions
- Timeline / Keyframes
- advanced AI overrides
- diagnostics
- manual clip/rig correction

These are advanced/repair/customisation tools.

They must no longer be mandatory steps in the normal Character workflow.

Do not duplicate them with separate competing implementations.

---

## 13. AI-06 Runtime Animation Integration

The existing Character AI programme through AI-05 already owns:

- character foundation
- profiles/factions
- perception/memory
- decision runtime
- patrol/navigation
- combat

AI-06 must connect those decisions to the Character Asset's prepared native Wicked animations.

### 13.1 Runtime Ownership

Add transient runtime animation state parallel to existing perception/decision/combat state.

Do not persist transient playback state as Character authoring authority.

### 13.2 Native Playback

Use the existing Wicked-native `AnimationComponent` playback path through Renegade's animation services.

Do not introduce a competing skeletal player.

### 13.3 Decision-to-Animation Mapping

AI intent/movement/combat should request semantic gameplay actions internally.

Examples:

- no movement / non-combat → idle
- locomotion → walk/run as appropriate
- attack → weapon/attack animation
- reload → reload
- hit → reaction where available
- death → death where available

Missing optional clips must fail safely.

An absent optional animation must not crash the AI or prevent the Character from functioning.

### 13.4 Look-at

Use existing native humanoid look-at support.

Runtime look-at must be transient and driven by valid AI state/perception.

It must not require the creator to manually wire a live target in the editor for ordinary AI.

Priority/clear behaviour must be deterministic.

### 13.5 Diagnostics

Expose enough runtime diagnostics to prove:

- current animation semantic request
- resolved native clip
- missing/unresolved mapping
- active look-at state/target class
- safe fallback behaviour

Diagnostics belong inside Renegade's existing editor/runtime diagnostics architecture.

Do not create a browser sidecar or parallel diagnostic server.

---

## 14. Character Identity Rules

Persistent identity is critical.

Never use raw Wicked ECS entity IDs as durable authoring authority.

A reusable Character Asset must not cause multiple placed actors to share one scene Character identity.

On placement:

- generate/assign fresh scene identity

On duplication:

- clone authoring settings
- assign fresh identity

On prefab placement:

- restore prefab authoring settings
- assign fresh identity

On reset/Test Level restart:

- preserve authored identity/settings
- clear transient cognition/runtime state

All reference resolution must continue to use Renegade's stable identity architecture.

---

## 15. Existing Native Ownership Constraints

These rules are non-negotiable:

- navigation = Wicked `VoxelGrid` + `PathQuery`
- movement/controller = native Wicked/Jolt `CharacterComponent`
- animation = Wicked native `AnimationComponent`
- humanoid mapping = Wicked `HumanoidComponent`
- retargeting = existing Wicked/Renegade retarget path
- IK = Wicked native IK component/service
- look-at = Wicked native humanoid controls
- gameplay events = existing governed `GameplayEventService`
- no second event bus
- no competing navigation system
- no competing skeletal system
- no raw ECS entity IDs persisted as stable authoring references
- reuse governed Lua/actions infrastructure
- preserve AI-01 through AI-05 behaviour and acceptance

If an implementation appears easier only by bypassing these rules, do not do it.

---

## 16. Implementation Gates

Use these gates for implementation and recovery.

### CW-00 — Baseline, Branch and Recovery Scaffolding

Before editing implementation code:

- inspect current `main`
- inspect PR #158 and its merge/acceptance state
- inspect current `.github/workflows`
- inspect the relevant current source files named in this specification
- do not assume the snapshot SHA in this document is still head
- do not discard existing accepted work
- do not restart from an older main

Do not begin the Character workflow implementation until the owner has accepted the AI-05 baseline and explicitly authorized continuation.

Once the accepted starting point is clear, create/continue one long-lived remote branch for this programme, recommended:

`feature/character-workflow-programme`

Do not create a swarm of short-lived feature branches.

Create and commit a recovery/progress file:

`docs/CHARACTER_WORKFLOW_PROGRESS.md`

The progress file must contain:

- authoritative spec path
- remote branch name
- current baseline commit
- latest pushed commit
- current gate
- completed gates
- work currently in progress
- tests run and results
- known failures
- owner tests still required
- exact next implementation task

Update this file before every required remote checkpoint.

**Exit:** any engineer/ChatGPT instance can determine the exact implementation state using GitHub alone.

### CW-01 — Character Import Designation and Asset Classification

Implement:

- Import As: Model / Character
- Character-specific importer state
- Character destination/classification
- Character asset metadata
- Asset Browser shows finished asset under Characters

Do not yet require external animation support to pass this gate.

**Exit:** a model can be deliberately imported as a Character Asset and is classified/presented as such.

### CW-02 — Unified Animation Ingestion

Implement:

- embedded clip list retained/improved
- external Add Animations path
- add-many/multi-select where supported
- unified animation list
- preview
- include/exclude
- sensible naming
- external source compatibility/status
- durable provenance schema

**Exit:** a Character import can combine embedded animations and several separate external animation files in one import session.

### CW-03 — Humanoid Preparation and Retargeting Inside Import

Implement:

- auto humanoid mapping where appropriate
- mapping validation
- external animation retarget before final asset commit
- clear error/repair route
- deterministic import result
- deterministic reimport

Reuse current retarget backend.

**Exit:** a Mixamo-style character + separate animation files can be imported once and finalized as a prepared Character Asset.

### CW-04 — Prepared Character Placement and Inspector Routing

Implement:

- drag Character Asset into scene
- automatic Character instance creation/promotion
- fresh identity
- default gameplay settings
- appropriate Character Inspector surfaced automatically
- no mandatory MAKE CHARACTER step for prepared assets

**Exit:** user can drag imported Character into level and immediately configure faction/role/etc.

### CW-05 — Duplication and Character Prefabs

Implement:

- duplicate configured Character instance
- copy authoring configuration
- fresh identity/transient state
- Save as Character Prefab
- prefab placement
- safe handling of scene-local references

**Exit:** user can configure once, duplicate freely, or save/reuse the configuration as a prefab.

### CW-06 — AI-06 Native Animation Runtime

Implement:

- character asset animation mapping consumed by runtime
- semantic requests internally
- native clip playback
- safe fallback/missing-clip handling
- native look-at integration
- runtime diagnostics

**Exit:** existing AI decisions visibly drive the appropriate prepared native animations without requiring Lua.

### CW-07 — Advanced Tools Integration and Workflow Polish

Ensure:

- existing advanced tools remain functional
- normal Character path contains no raw Stable-ID requirement
- technical tools are available but not mandatory
- labels/tooltips distinguish asset preparation from scene behaviour
- no duplicated/competing subsystems
- import/reimport/prefab workflows are coherent

**Exit:** creator workflow is simple while advanced control remains available.

### CW-08 — Acceptance, Regression and Packaging

Complete:

- unit/source-contract tests
- persistence tests
- recipe round-trip/reimport tests
- duplication identity tests
- prefab identity/reference tests
- runtime animation tests
- safe missing-animation tests
- owner-facing manual acceptance
- final CI

**Exit:** owner can perform the end-to-end workflow described in Section 20.

---

## 17. Mandatory Codex Recovery Rules

This section is a hard requirement.

The previous programme lost substantial work because implementation existed only inside a private execution workspace. That must never happen again.

Assume the Codex session can terminate at any time.

### 17.1 GitHub Is the Recovery Authority

No implementation may be considered completed unless it has been committed and pushed to the agreed remote programme branch.

Local commits alone do not count as recoverable work.

### 17.2 Remote Checkpoints

Codex must push a recoverable checkpoint:

- at the end of every CW gate
- before starting a risky refactor
- before a long build/test operation if meaningful implementation has accumulated
- before stopping for usage/session limits
- whenever substantial work would otherwise exist only locally

A checkpoint must include the updated progress document.

### 17.3 Never Restart Blindly

On every new Codex session:

1. fetch remote
2. inspect the programme branch
3. inspect `docs/CHARACTER_WORKFLOW_PROGRESS.md`
4. inspect recent commits/diff
5. inspect the authoritative spec
6. continue from the remote state

Do not reset to `main` merely because a new session has started.

Do not rebuild already-completed gates from memory.

Do not discard remote work unless the owner explicitly authorizes it.

### 17.4 ChatGPT Continuation Requirement

At every remote checkpoint, another engineer or ChatGPT must be able to continue without:

- Codex chat history
- Codex local filesystem
- Codex scratch notes
- unpushed commits
- hidden implementation decisions

If that is not true, the checkpoint is incomplete.

---

## 18. CI and Build Discipline

Windows CI is slow and expensive. Recoverability must not force unnecessary Actions usage.

At the time of this specification:

- `Renegade Studio` pushes run on `main`; PRs targeting `main` run Studio CI.
- `Windows baseline` pushes run on `main` and `agent/**`; PRs targeting `main` run baseline CI.

Codex must **re-check current workflow YAML before relying on this behaviour**.

Recommended programme strategy after the accepted AI-05 baseline is merged:

1. create `feature/character-workflow-programme`
2. do **not** immediately open a PR
3. push frequent recovery checkpoints to that remote feature branch
4. perform local/configure/build/test validation at each gate
5. open the PR only at the planned acceptance/CI gate or when the owner explicitly requests it

This preserves remote recoverability without triggering full PR CI on every checkpoint, assuming workflow triggers remain as currently configured.

Do not rename the branch to `agent/**`, because current Windows baseline configuration push-triggers that namespace.

### 18.1 Local Validation Before CI

Use the repository's real build system and existing scripts.

Do not claim successful compilation unless it was actually compiled.

Do not claim runtime acceptance based solely on source inspection.

At each gate, run the narrowest useful tests locally first.

Do not use GitHub Actions as a compiler/debugging loop when local validation is available.

### 18.2 CI Gates

Do not trigger expensive CI casually.

Expected full CI points:

- when the owner requests a confidence build
- when the implementation reaches the agreed integrated acceptance gate
- final PR/merge acceptance

If a small documentation-only checkpoint can be pushed without CI, prefer that.

---

## 19. Testing Requirements

Add regression coverage proportional to each gate.

Minimum automated coverage should include:

### Import/Recipe

- Model vs Character designation persists correctly
- Character classification is deterministic
- embedded animation recipe remains compatible
- external animation provenance round-trips
- multiple external sources round-trip
- reimport reproduces clip list/mapping
- malformed/missing source fails clearly, not silently

### Humanoid/Retarget

- valid mapping succeeds
- incompatible mapping fails clearly
- external retarget creates native destination clips
- redo does not depend on reopening the original source where command snapshots apply
- missing optional animation does not crash

### Placement/Identity

- Character Asset placement creates a Character
- each placement receives distinct persistent identity
- base asset is not mutated by instance authoring
- drag base asset again produces fresh default gameplay settings

### Duplication

- authored settings copy
- persistent identity does not copy
- runtime/transient cognition does not copy

### Prefab

- reusable authored Character configuration persists
- prefab placement gets fresh identity
- base Character Asset reference remains valid
- invalid scene-local references are not silently persisted

### AI-06 Animation

- intent resolves to expected semantic request
- semantic request resolves to prepared native clip
- fallback behaviour is deterministic
- missing optional slots fail safely
- runtime reset clears transient animation/look-at state
- look-at respects valid perception/target state
- no hidden-truth tracking is introduced by animation/look-at

### Regression

All AI-01 through AI-05 tests/source contracts must remain green unless an intentionally changed creator contract is updated with equivalent or stronger coverage.

Do not "fix" tests simply by weakening a required architectural invariant.

---

## 20. Owner Acceptance Scenario

The implementation is not accepted until the owner can perform this workflow in a real build.

### A. Import a Character

1. `ADD -> IMPORT MODEL...`
2. choose a humanoid character FBX/GLB/etc.
3. select **Import As: Character**
4. verify skeleton/humanoid status
5. if embedded actions exist, see them listed
6. use **ADD ANIMATIONS...**
7. select several separate animation files, e.g.:
   - Idle
   - Walk
   - Run
   - Aim
   - Fire
   - Reload
   - Death
8. preview the clips on the imported character
9. verify retarget/compatibility status
10. finalize import

Expected:

- Character appears under Asset Browser Characters
- it is not merely a generic Model
- no post-import retargeting ceremony is required for the clips just prepared

### B. Place and Configure

1. drag the Character asset into the level
2. select it
3. Character Inspector is immediately available
4. configure:
   - faction
   - role
   - personality
   - skill
   - awareness
   - combat
   - weapon
   - patrol/actions/scripts as relevant

Expected:

- no manual MAKE CHARACTER step
- no raw Animation Set ID entry
- no raw ECS/stable-ID plumbing in the normal path

### C. Duplicate

1. duplicate the configured Character several times

Expected:

- authored settings copied
- each is an independent Character
- identities differ
- runtime state is fresh

### D. Drag the Base Asset Again

1. drag the original Character Asset from the Asset Browser again
2. configure it differently

Expected:

- new instance starts from base/default gameplay setup
- previous scene Character configuration has not become global asset defaults

### E. Save Prefab

1. select a configured Character
2. Save as Character Prefab
3. place that prefab elsewhere

Expected:

- reusable gameplay configuration restored
- new identity created
- no stale runtime state
- no broken scene-local references silently carried across

### F. Runtime Animation

1. run Test Level/Game
2. observe the Character performing AI behaviour

Expected:

- movement/AI/combat drive the prepared native animations
- look-at behaves through native Wicked systems
- missing optional clips degrade safely
- no Lua is required merely to make the baseline AI animate

---

## 21. Explicit Non-Goals

This programme must **not**:

- force every character onto one fixed Renegade armature
- require a mandatory stock Renegade animation library
- replace Wicked's animation runtime
- replace Wicked humanoid retargeting
- replace native IK/look-at
- remove advanced animation/humanoid/timeline tools
- make scene users type stable IDs
- make animation import a repeated per-instance operation
- turn a configured scene instance into global defaults automatically
- copy transient AI state when duplicating
- persist raw ECS IDs
- introduce another gameplay-event bus
- redesign AI-01 through AI-05 without a demonstrated integration need
- silently break existing generic Model import

---

## 22. UI Direction

The exact final layout may adapt to existing Renegade widget constraints, but the conceptual separation is mandatory.

### Character Importer

```text
IMPORT AS
○ Model
● Character

CHARACTER RIG
Humanoid: READY
[ Review Mapping ]

ANIMATIONS
Embedded:
  ✓ Idle
  ✓ Death

Added:
  ✓ Walking.fbx → Walking
  ✓ Running.fbx → Running
  ✓ Fire.fbx    → Fire

[ + ADD ANIMATIONS... ]
[ PLAY ] [ STOP ] [ LOOP ]

Character animation roles/mapping:
  Idle       → Idle
  Walk       → Walking
  Run        → Running
  Fire       → Fire
  ...

[ IMPORT CHARACTER ]
```

### Scene Character Inspector

```text
CHARACTER

Type
Role
Personality
Faction
Skill
Awareness

Combat
Weapon
Squad
Patrol
Actions / Scripts

[ SAVE AS PREFAB ]

ADVANCED AI
...
```

The scene Character Inspector should not become the normal place to ingest/retarget the asset's animation files.

---

## 23. Source Review Checklist Before Coding

Codex must inspect the current versions of at least:

- `Studio/src/StudioApplication.cpp`
- `Studio/src/CreatorImportPreviewWindow.h`
- `Studio/src/AICharacterInspector.cpp`
- `Studio/src/Phase7Gate7BHumanoidRetargetInspector.cpp`
- `Studio/src/Phase7Gate7AAnimationInspector.cpp`
- `Studio/src/Phase7Gate7CCharacterControlsInspector.cpp`
- `Studio/src/Phase7Gate7DNativeTimelineInspector.cpp`
- `EngineBridge/include/renegade/bridge/CreatorModelImportRecipe.h`
- `EngineBridge/src/CreatorModelImportRecipe.cpp`
- `EngineBridge/include/renegade/bridge/HumanoidRetargetService.h`
- `EngineBridge/src/HumanoidRetargetService.cpp`
- `EngineBridge/include/renegade/bridge/AnimationService.h`
- `EngineBridge/include/renegade/bridge/CharacterAnimationControlService.h`
- `EngineBridge/include/renegade/bridge/CharacterService.h`
- `EngineBridge/src/CharacterService.cpp`
- `EngineBridge/include/renegade/bridge/AssetBrowserService.h`
- `EngineBridge/src/AssetBrowserService.cpp`
- `Runtime/src/RuntimeCharacterSystem.h`
- `Runtime/src/RuntimeCharacterDecision.h`
- `Runtime/src/RuntimeApplication.h`
- relevant prefab/placement/duplication services discovered in the current tree
- relevant import/reimport/provenance services discovered in the current tree
- relevant current tests/source contracts
- current `.github/workflows/*.yml`

If filenames have moved, find the current equivalents. Do not assume a file disappeared merely because a historical path no longer resolves.

---

## 24. Implementation Decision Rules

When a detail is not explicitly specified:

1. preserve existing Renegade architecture
2. reuse existing Wicked-native systems
3. prefer the shortest creator workflow
4. keep technical complexity out of the normal path
5. preserve advanced access
6. preserve deterministic Undo/Redo/persistence
7. preserve stable identity
8. preserve deterministic reimport
9. avoid duplicated subsystems
10. add tests around the chosen behaviour
11. record significant implementation decisions in the progress document

Do not independently redesign the approved Character workflow because another architecture appears more elegant.

If a genuine blocker forces a user-visible workflow change, stop at a recoverable remote checkpoint and report the blocker before proceeding.

---

## 25. Definition of Done

This Character programme is complete when:

- Character can be designated during import
- embedded and external animation sources can be prepared in the Character importer
- external animations are retargeted/prepared before asset finalization
- Character appears correctly in the Asset Browser
- Character can be dragged into a level already usable as a Character
- scene gameplay configuration remains instance-specific
- configured instances duplicate correctly
- base Character can be dragged again for a fresh independent setup
- configured Character can be saved/reused as a prefab
- AI-06 drives native prepared animations/look-at
- advanced Wicked/Renegade tools remain available
- raw Stable IDs are absent from the normal workflow
- deterministic persistence/reimport/identity rules are covered
- AI-01 through AI-05 regressions remain green
- automated tests pass
- required Windows CI passes
- the owner completes the end-to-end acceptance scenario

And throughout implementation:

> **GitHub must always contain enough pushed code, specification and progress state for ChatGPT to continue if Codex disappears.**

That recovery requirement is part of the build, not optional process documentation.
