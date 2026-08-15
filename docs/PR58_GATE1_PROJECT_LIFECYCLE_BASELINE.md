# PR58 Gate 1 — Project Lifecycle Baseline and Diagnostic Scope

## Baseline

- Repository: `mav3r1ckmediastudio-glitch/renegade-engine`
- Baseline branch: `main`
- Exact baseline: `a7775f31c5ec1ff61463d495e7db6ac4a5d63258`
- Baseline commit: `Restore guided creator asset workflow and textured model placement (#57)`
- Gate branch: `agent/pr58-gate1-project-lifecycle-baseline`

PR58 is the Startup + Project Hub + Project Lifecycle Reliability programme. Gate 1 is deliberately non-visual and diagnostic. It must not redesign the Hub or change the accepted PR57 reusable-asset/texture placement architecture.

## Current startup path

Current Studio startup is:

1. `StudioApplication::Initialize()` initializes `ProjectService` state from `Saved/RenegadeStudio.ini`.
2. `PrepareProvingGround()` loads `startupScene_` (`Content/ProvingGround.wiscene`) when present, otherwise creates and saves a proving-ground scene.
3. Studio creates/loads the normal `RenderPath3D` editor.
4. `StudioRenderPath::Load()` builds both workspace chrome and Project Hub widgets, refreshes them, and then sets the Project Hub visible.
5. Project Hub visibility hides most editor chrome, but the same active 3D render path remains behind the Hub.

This explains why the current Hub is an overlay/presentation state rather than a distinct startup state. Startup/identity/reveal redesign is explicitly Gate 2+, not Gate 1.

## Current project create/open path

### Create Project

`StudioRenderPath::CreateProject()`:

1. requests a destination parent folder;
2. calls `ProjectService::CreateProject()` at a Wicked thread-safe point;
3. `ProjectService` creates the project directory structure, copies the starter WISCENE, writes the `.renegade` descriptor, ensures the LC01 registry, makes the project current, and writes the recent-project entry;
4. Studio then synchronously calls `session_->LoadScene(project.StartupScenePath())`;
5. Studio synchronously restores governed material textures;
6. Studio refreshes Hub state and hides the Hub.

### Open Project / Recent Project

`StudioRenderPath::OpenProjectDescriptor()`:

1. calls `ProjectService::OpenProject()`;
2. project transaction recovery runs;
3. descriptor/default directories/project ID/LC01 registry are validated or repaired;
4. `ProjectService` makes the project current and updates recents;
5. Studio synchronously calls `session_->LoadScene(project.StartupScenePath())`;
6. Studio synchronously restores governed material textures;
7. Studio refreshes Hub state and hides the Hub.

### Reliability hazard recorded for Gate 5

Project identity is committed before the startup scene has successfully opened. `SceneDocumentService::Open()` preserves the old active scene when preparation fails. Therefore a failed startup-scene load can leave the new project current while the previous scene remains active. Gate 5 must make project+scene adoption transactional/fail-closed.

## Current scene open/reopen path

Explicit Open Scene and normal Reopen use the prepared scene path:

1. `RequestSceneReplacement()` resolves dirty state first;
2. `BeginOpenScene()` marks the open in progress and schedules low-priority Wicked job work;
3. `SceneDocumentService::PrepareOpen()` / `PrepareWickedSceneOpen()` reads and deserializes the WISCENE away from the Studio thread-safe commit point;
4. at `EVENT_THREAD_SAFE_POINT`, `CompleteOpenScene()` commits the prepared scene;
5. camera state is adopted;
6. governed material textures are restored synchronously;
7. environment/terrain workspace state and hierarchy/Inspector/status are rebuilt;
8. Hub is hidden.

`SceneDocumentService::Reload()` itself is a synchronous `Open(CurrentPath())` fallback, but the normal Studio Reopen button reaches `BeginOpenScene()` when a saved path exists.

## Current save path

`SaveSceneAfterTransientCleanup()` first removes any transient creator drag preview, then calls `SceneDocumentService::Save()`.

The save service currently performs a deliberately defensive sequence:

1. validate path/destination;
2. ensure persistent entity identities;
3. serialize the active scene to a temporary WISCENE;
4. fully deserialize/validate that temporary WISCENE;
5. protect the prior destination;
6. atomically replace the destination;
7. fully deserialize/validate the final destination a second time;
8. promote a previous-version backup;
9. create/rotate automatic backups;
10. update the active scene path and mark commands saved.

This is safe but potentially expensive. Gate 1 must measure it before Gate 6/7 considers any optimization. Validation must never be removed merely to improve timings.

## Dirty-state path

`RequestSceneReplacement()` currently:

- commits an active sun preview first;
- continues immediately when the command stack is clean;
- otherwise presents Yes / No / Cancel;
- No continues without saving;
- Cancel leaves the current scene in place;
- Yes saves the current path, or invokes Save As when no current path exists, and only continues after successful save.

Gate 6 will deliberately try to break this contract across project switching, Hub return, scene replacement and application exit.

## Asset Browser project-switch hazard recorded for Gate 5

`SetProjectHubVisible(false)` refreshes hierarchy, Inspector and status, but does not explicitly refresh the Asset Browser. `OpenProjectDescriptor()` and `CreateProject()` likewise do not perform an explicit browser refresh after project adoption. The browser is refreshed on initial Studio load, drawer-tab activation and folder selection.

This creates a plausible stale-project browser state after project switches and must be behaviorally tested/fixed in Gate 5. Gate 1 does not alter it.

## Reload performance: confirmed timing seams

Owner observation on the accepted PR57 Release build: Reopen/Reload can appear frozen for roughly 30 seconds and then complete correctly with textures preserved.

No root cause is claimed yet.

Gate 1 must distinguish at least these phases:

1. **WISCENE prepare** — archive open + full scene deserialization;
2. **scene commit** — clear/merge, terrain rebinding, selection/command reset;
3. **governed texture binding discovery**;
4. **governed texture preparation**;
5. **governed Wicked resource decode/load/apply**;
6. **post-open camera/workspace/hierarchy/Inspector/status refresh**;
7. **whole Reopen wall time**.

## High-risk texture-restoration pattern requiring measurement

`RestoreMaterialTextureBindings()` enumerates every persisted material texture binding. For each binding that is not already live, it calls `PrepareMaterialTextureAsset()` independently.

`PrepareMaterialTextureAsset()` currently rereads the LC01 asset registry, resolves/canonicalizes product paths, reads the governed `.rasset` document/payload and rebuilds its cache identity.

Therefore repeated scene instances that reference the same governed texture can repeat registry/product preparation work for the same stable texture ID. This is a strong performance hypothesis, not an accepted diagnosis. Gate 1 timing/counters must prove or reject it before optimization.

Instrumentation must record:

- total persisted bindings discovered;
- number already live/skipped;
- number actually restored;
- number of unique governed texture stable IDs;
- cumulative preparation time;
- cumulative resource load/apply time;
- total restore time;
- failures and first failure without changing existing success/failure semantics.

## Gate 1 diagnostic-only implementation rule

Instrumentation may add timing/counter logging, but it must not:

- cache or deduplicate texture work yet;
- change scene load ordering;
- change project adoption ordering;
- change Save/Save As semantics;
- change Hub presentation;
- change PR57 `.rasset`, StableId, thumbnail, Asset Browser placement or texture-resource handoff behavior.

Any performance/reliability fix discovered by the diagnostics belongs to its later gate.

## Gate 1 acceptance

Gate 1 is complete only when an exact-head Release build can produce a timing report for the owner's V3 project that lets us state, with measured evidence, where Reopen spends its time.

Minimum report:

- WISCENE prepare ms;
- scene commit ms;
- governed texture restore ms with binding/unique-ID counts;
- post-open Studio refresh ms;
- total Reopen ms.

Also record Create/Open/Save timings sufficiently to establish later Gate 5–7 baselines.

CI remains authoritative for compilation/tests. Owner Release behavior/timing remains authoritative for the real V3 project.
