# PR58 Gate 1 — Project Lifecycle Baseline, Diagnostic Result and Closure

## Baseline

- Repository: `mav3r1ckmediastudio-glitch/renegade-engine`
- Baseline branch: `main`
- Exact baseline: `a7775f31c5ec1ff61463d495e7db6ac4a5d63258`
- Baseline commit: `Restore guided creator asset workflow and textured model placement (#57)`
- PR branch: `agent/pr58-project-hub-lifecycle-reliability`

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

This safety architecture must not be removed merely to improve timings. Gate 6 will test and harden user-facing save semantics; Gate 7 may optimize only from measured evidence.

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

## Reload performance — measured V3 owner evidence

The earlier owner observation described Reopen/Reload as an apparent stall of roughly 30 seconds. Gate 1 exact-head Release testing on the real V3 project measured a materially longer representative Reopen: approximately **2 minutes 3 seconds** from pressing Reopen until the editor unlocked and full controls returned.

The Gate 1 diagnostic log contained two governed texture-restore passes:

- `101753.24 ms` total: `156` bindings, `27` unique texture Stable IDs, `0` already live, `156` restored, `0` failures, `18.17 ms` inspection, `94542.57 ms` preparation, `7067.12 ms` apply/load.
- `129653.87 ms` total: `156` bindings, `27` unique texture Stable IDs, `0` already live, `156` restored, `0` failures, `18.08 ms` inspection, `122924.55 ms` preparation, `6551.67 ms` apply/load.

The owner identified the scene as containing **27 asset instances**, with each asset normally carrying approximately **4–6 PBR texture maps**. The observed `156` persisted material/texture bindings are therefore consistent with the scene content: `156 / 27 = 5.78` bindings per instance on average.

The approximately 2m03s owner wall-clock observation aligns closely enough with the `129653.87 ms` texture-restore pass to identify governed texture restoration as the dominant Reopen stall. The diagnostic timing is longer than the rough manual stopwatch observation by several seconds, so it should not be treated as an exact wall-clock equality; it is nevertheless of the same magnitude and directly captures the blocked phase.

## Proven Gate 1 diagnosis — governed texture preparation dominates Reopen

`RestoreMaterialTextureBindings()` enumerates every persisted material/slot binding. For each binding that is not already live, it calls `PrepareMaterialTextureAsset()` independently before applying the prepared resource.

`PrepareMaterialTextureAsset()` rereads/validates governed project state, resolves product identity/path information, reads the governed texture `.rasset` payload and builds the resource cache identity.

Gate 1 instrumentation computes unique texture identity from each binding's `textureAssetId`. In the measured V3 Reopen pass:

- persisted bindings: **156**
- distinct governed texture Stable IDs: **27**
- successful restores: **156 / 156**
- failures: **0**
- binding inspection: **18.08 ms**
- governed preparation: **122924.55 ms**
- Wicked resource apply/load: **6551.67 ms**
- total governed texture restore: **129653.87 ms**

Governed preparation therefore consumed approximately **94.8%** of the measured texture-restore pass. Binding inspection was negligible and resource apply/load was comparatively small.

This promotes the earlier performance hypothesis to a **proven Gate 1 finding**: the Reopen stall is dominated by repeated governed texture preparation performed at material-binding granularity. The workload contains far fewer unique governed texture Stable IDs than persisted bindings, so the same governed texture identity can be prepared repeatedly during a single restore pass.

This does **not** justify changing `.rasset`, StableId authority, project portability, or the accepted PR57 resource handoff architecture. The likely Gate 7 optimization seam is to avoid repeating expensive governed preparation for the same texture Stable ID within one restoration pass—for example by preparing each unique governed texture once and reusing the prepared/live resource for all matching bindings. That is a later-gate direction only, not a Gate 1 implementation.

## Gate 1 diagnostic-only implementation rule

Gate 1 instrumentation may record timing/counter evidence, but it must not:

- cache or deduplicate texture work yet;
- change scene load ordering;
- change project adoption ordering;
- change Save/Save As semantics;
- change Hub presentation;
- change PR57 `.rasset`, StableId, thumbnail, Asset Browser placement or texture-resource handoff behavior.

The measured performance optimization belongs to Gate 7. The project/scene split-brain and Asset Browser project-switch hazards belong to Gate 5. Save/dirty-state hardening belongs to Gate 6.

## Gate 1 acceptance and closure

Gate 1 performance-diagnostic evidence is now satisfied:

- exact-head Renegade Studio Debug/Release build passed before the V3 test;
- exact-head Windows baseline Debug/Release passed before the V3 test;
- owner tested the exact-head Release artifact on the real V3 project;
- Reopen completed with all `156` governed texture bindings restored and `0` failures;
- the expensive phase is identified with measured evidence;
- the diagnostic reported all `156` governed texture bindings restored with `0` failures.

The continuation chat has not separately recorded an explicit owner statement that the post-Reopen scene remained visually texture-correct. Preserve that as the final behavioral acceptance check rather than inferring it from the diagnostic alone.

This closure documentation is a docs-only branch change and must receive its own exact-head Renegade Studio and Windows baseline green status. Once that CI is green **and** the owner explicitly confirms the post-Reopen scene/textures remained correct, Gate 1 is closed. Do not begin Gate 2 until the owner explicitly approves advancement.
