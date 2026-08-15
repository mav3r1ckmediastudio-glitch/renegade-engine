# PR #58 Continuation Handoff — Startup, Project Hub & Project Lifecycle Reliability

> **Purpose:** This file exists so a fresh ChatGPT/Claude session can continue PR #58 without context loss or design drift. Read this file **before changing code**. Then verify live GitHub state because SHAs/CI statuses can move after this handoff was written.

## 0. First actions for a new chat — mandatory

1. Inspect live PR #58 metadata and current head. Do **not** trust the SHA in this document if GitHub has advanced.
2. Inspect current `main` and confirm PR #58 still targets the merged PR #57 baseline.
3. Compare `main` to the live PR #58 head and report the exact changed-file set before editing anything.
4. Check exact-head Renegade Studio and Windows baseline workflows. CI status below is only a snapshot from handoff creation.
5. Read `docs/PR58_GATE1_PROJECT_LIFECYCLE_BASELINE.md` in full.
6. Do **not** start Gate 2 until Gate 1 is completed and the owner explicitly agrees to advance.
7. Preserve the PR #57 reusable-asset pipeline. Do not refactor working `.rasset` import/thumbnail/browser/drag/texture behavior as part of PR #58.

## 1. Repository / PR identity

- Repository: `mav3r1ckmediastudio-glitch/renegade-engine`
- PR: **#58 — Rebuild Project Hub and harden project lifecycle**
- PR branch: `agent/pr58-project-hub-lifecycle-reliability`
- PR state at handoff: **OPEN, DRAFT, UNMERGED**
- Base branch: `main`
- PR #57 merge baseline on `main`: `a7775f31c5ec1ff61463d495e7db6ac4a5d63258`
- Code-bearing PR #58 head immediately before this handoff document was committed: `e928596f5dc2815da032f2b35e757f9c18a212bc`
- This documentation commit advances the branch beyond that SHA. **Always re-read live PR metadata before work.**
- Pinned Wicked Engine commit: `3a800b7134aafe58461093c8abb2e274d4e64033`

### CI snapshot at handoff creation

For pre-handoff code head `e928596f...`:

- Windows baseline #919: **Debug SUCCESS / Release SUCCESS**
- Renegade Studio #496: **Debug BUILDING / Release BUILDING** at the moment this handoff was written

A docs-only handoff commit will trigger newer exact-head runs. A new chat must recheck them.

## 2. Owner / working rules

The owner is building Renegade Engine as a Wicked-based engine with its own UX and project/asset architecture.

Critical process rules:

- Work gate-by-gate. Do not turn PR #58 into another uncontrolled multi-issue repair blob.
- Diagnose before fixing. Compilation is not proof of behavior.
- GitHub CI is authoritative compile/test evidence because the owner's local CPU has known instability/damaged cores.
- Owner testing of the **Release artifact** is authoritative for editor behavior.
- PR #58 remains **Draft** until final Gate 8 acceptance.
- After each visual/behavioral gate, stop and get owner acceptance before continuing.
- No force-pushes to the PR branch. Prefer clean fast-forward changes. If risky patch generation is needed, use scratch isolation and verify containment first.
- Preserve the owner's local modification to `Tools/Windows-Build.Common.ps1` and existing untracked LP04 patch files; do not instruct destructive cleanup of them.
- Do not claim a workflow is fixed merely because CI is green.

## 3. Why PR #58 exists

PR #57 successfully established the reusable asset workflow but took roughly three days to stabilize. The following behavior is considered **locked/frozen** unless direct evidence proves a regression:

- governed `.rasset` model import
- governed texture assets
- thumbnail capture and Asset Browser display
- imported assets appearing in the Asset Browser
- repeat drag/drop from Asset Browser
- repeated placement without second-drag crash
- live textures/materials on freshly dragged instances
- saved scene texture/material persistence after close/reopen

PR #58 intentionally moves away from importer work. Importer UX/reliability is planned for **PR #59**.

PR #58 owns three connected areas:

1. startup experience
2. Project Hub redesign
3. project / scene / save / load lifecycle reliability and performance

## 4. Explicit non-goals for PR #58

Do **not** expand PR #58 into:

- Creator Importer editable asset-name repair
- importer false post-commit verification warning
- importer ~20-second processing optimization
- importer progress/percentage UI
- new model formats
- terrain redesign
- Inspector redesign
- new gameplay/runtime systems
- changes to PR #57 reusable-asset architecture unless a proven regression requires a minimal compatibility correction

Those importer items belong in PR #59.

## 5. Current Gate 1 — exact scope

### Gate 1 objective

**Baseline, lifecycle map & diagnostics.** No Hub redesign and no startup animation implementation yet.

Gate 1 exists to understand current project/scene behavior and identify the cause of the owner's reported symptom:

> Reopen/Reload appears frozen for about **30 seconds**, then the scene finishes loading correctly with textures intact.

No optimization is accepted before measurement.

### Existing Gate 1 files

At the pre-handoff head the intended production diff from merged PR #57 was:

- `docs/PR58_GATE1_PROJECT_LIFECYCLE_BASELINE.md`
- `EngineBridge/include/renegade/bridge/ProjectLifecycleDiagnostics.h`
- `EngineBridge/src/MaterialTextureAssetService.cpp`

The handoff document itself adds `docs/PR58_HANDOFF.md`.

There must be **no temporary workflow/patcher residue** in the final PR diff.

### Diagnostic output

`ProjectLifecycleDiagnostics.h` writes `[PR58-GATE1]` timing information to Wicked backlog and also appends to the known file:

`Saved/Diagnostics/PR58Gate1Lifecycle.log`

Logging must fail open: inability to write diagnostics must never block scene/project behavior.

### Current texture-restore probe records

`RestoreMaterialTextureBindings()` reports:

- total persisted texture bindings
- unique texture Stable IDs
- bindings already live
- bindings restored
- failures
- binding inspection time
- cumulative governed texture preparation time
- cumulative apply/Wicked decode time
- total restore time

No cache/dedup optimization has been introduced yet.

## 6. Gate 1 lifecycle findings already proven by source review

### Normal Reopen path

For a saved scene, Studio's Reopen button currently follows:

`ReopenScene()`
→ dirty-state protection through `RequestSceneReplacement()`
→ `BeginOpenScene(scenePath)`
→ Wicked job system performs `SceneDocumentService::PrepareOpen()` / WISCENE deserialization in the background
→ `EVENT_THREAD_SAFE_POINT`
→ `CompleteOpenScene()`
→ `CommitPreparedOpen()`
→ `AdoptOpenedSceneCamera()`
→ `RestoreGovernedMaterialTextures()`
→ environment/terrain workspace reset
→ hierarchy / Inspector / status refresh

**Important correction:** normal Reopen does **not** simply perform the whole WISCENE load synchronously on the UI path. The reported ~30-second apparent freeze is therefore likely in a post-prepare/post-commit phase or in work triggered around that phase. Measure it; do not guess.

### Project open/create path differs

`OpenProjectDescriptor()` and current Create Project adoption call `session_->LoadScene(session_->Projects().StartupScenePath())`, which uses the synchronous `SceneDocumentService::Open()` boundary. Project opening therefore has different responsiveness characteristics from explicit Open Scene/Reopen and should be addressed later in lifecycle hardening.

### Strong performance hypothesis — NOT yet proven

Current governed texture restore does this per persisted material texture binding:

`PrepareMaterialTextureAsset()`
→ reads/validates LC01 asset registry
→ finds texture asset/provenance
→ canonicalizes paths
→ reads governed texture `.rasset`
→ extracts payload / builds resource cache identity
→ loads/applies the resource

In a scene containing many repeated instances of the same asset, many material bindings can reference the same Stable IDs. The current loop can therefore repeat governed preparation for the same texture asset many times.

This is a **hypothesis** for the long stall, not a conclusion. The owner's V3 project log must prove how much time is actually spent there.

### Reliability hazard 1: project/scene split-brain on failed adoption

Current project open/create can commit `ProjectService`'s current project and recents **before** the startup scene is proven loadable. If startup-scene loading fails, Renegade can theoretically hold:

- new project identity/context
- old scene still in memory

This must be fixed transactionally in **Gate 5**, not smuggled into Gate 1.

### Reliability hazard 2: Asset Browser refresh on project switch

Project switching/adoption does not currently make Asset Browser refresh an explicit lifecycle boundary. Stale browser presentation from Project A can therefore remain until another refresh path runs. Treat this as a Gate 5 reliability issue.

### Save system facts observed in source

`SceneDocumentService::Save()` already contains substantial safety architecture:

- validates destination
- ensures persistent entity identities
- serializes to temporary archive
- reopens temporary archive to validate it
- protects previous destination
- atomically replaces destination
- validates final destination
- restores previous file if final validation fails
- creates `.bak.wiscene` / automatic rolling backups
- marks command stack saved only after success

Do not casually replace this architecture. Gate 6 is to test and harden user-facing semantics around it, especially Save As / cancel / dirty transitions.

## 7. Gate 1 owner acceptance procedure

Once exact-head Studio Debug/Release and Windows baseline are green:

1. Fetch the **exact-head Renegade Studio Release artifact**.
2. Owner launches that Release build.
3. Open the real V3 project used during PR #57 acceptance.
4. Confirm the scene is correct/textured before the test.
5. Use **Reopen/Reload once** and allow it to finish.
6. Confirm scene and textures are still correct after it completes.
7. Send back:
   - `Saved/Diagnostics/PR58Gate1Lifecycle.log`
   - ideally a rough wall-clock observation of how long the apparent freeze lasted
8. Analyze measured restore totals against the observed stall.
9. If the texture restore probe does not account for most of the stall, add the **next smallest timing seam** rather than changing behavior.
10. Gate 1 passes only when we can identify where the time is actually going with evidence sufficient for later optimization.

Do not start Gate 2 just because the diagnostic build compiles.

## 8. Full PR #58 gate plan

### Gate 1 — Baseline, lifecycle map & diagnostics

Scope:

- map startup / Hub / project / scene / save / reopen paths
- measure the ~30-second Reopen stall
- document hazards without fixing later-gate items early

Acceptance:

- exact-head Studio Debug/Release green
- exact-head Windows baseline Debug/Release green
- owner Release test on real V3 project
- measured evidence identifies the expensive phase(s)
- no PR #57 regression

### Gate 2 — Startup identity experience

Gate 2 is the cinematic/startup state. It has four logical mini-stages:

#### Gate 2A — Logo reveal

Use the owner's actual file: **`renegade logo reveal V2.mp4`**.

Known media properties from inspection in the source chat:

- approximately 9.77 seconds
- 1280×720
- H.264 video
- 30 fps
- AAC audio

The file was supplied in the original chat but **was not committed to the repo at handoff time**. When Gate 2 implementation begins, obtain/re-upload the actual asset rather than inventing/replacing it.

Requirements:

- full-screen/viewport presentation
- preserve aspect ratio appropriately
- audio plays
- no editor or Hub flash/bleed underneath
- clean fade to black at the end
- missing/corrupt reveal must fail safe into startup rather than preventing Studio from opening
- do not use the reveal to hide unrelated project loading

#### Gate 2B — First-run developer identity prompt

Original intended design:

After logo reveal:

- screen fades to black
- blinking cursor appears
- typewriter text types exactly/conceptually: **`Welcome developer, what is your name?`**
- user can type a preferred display identity, e.g. owner uses **`MAV3R!CK`**
- punctuation like `!` must be allowed because this is a display identity, not a filename
- Backspace/editing works
- Enter confirms
- empty identity cannot confirm
- chosen identity persists in **user/editor preferences**, not project metadata or WISCENE

This is intended as **first-run setup**, not a prompt on every launch.

#### Gate 2C — Identity Handshake

After identity exists, show the dedicated Identity Handshake screen based on the owner's supplied concept image:

- dark technical Renegade UI
- large central mechanical/cybernetic iris
- horizontal red scan/laser through the centre
- diagnostic/status details around it
- left-side identity-handshake language
- dynamic text **`WELCOME, <chosen name>`**, e.g. `WELCOME, MAV3R!CK`
- `RENEGADE // IDENTITY ACCEPTED` language belongs here, **not** permanently on the Hub
- display an **ENTER HUB** control
- do not automatically force entry; original concept requires user click

On subsequent launches the intended flow is:

`logo reveal → handshake using saved identity → ENTER HUB`

Do not ask for the name again unless user identity preferences have been cleared or changed.

A later skip option may be desirable after novelty wears off, but exact skip policy was **not locked yet**. Do not invent it without owner discussion.

#### Gate 2D — Iris-to-Hub transition

On **ENTER HUB**:

- central iris unlocks/splits vertically into left and right halves
- halves move away from the centre
- this recreates the semicircular side machinery seen in the owner's early Hub concept
- the Project Hub is revealed between/behind the separating halves

Critical design history:

The huge semicircles on the early Hub concept were **not arbitrary permanent Hub decoration**. They originated as the two halves of the Identity Handshake iris after it split open.

The owner later said to ignore/remove the semicircles from the permanent Hub layout. Preserve the iris-opening transition concept, but do not make the final everyday Hub permanently dominated by giant side machinery unless the owner explicitly reverses that decision.

Acceptance for Gate 2:

- first run: reveal → black typed identity prompt → handshake with chosen identity → ENTER HUB → iris opens → Hub
- subsequent run: reveal → handshake with persisted identity → ENTER HUB → Hub
- no editor/Hub bleed before intended reveal
- no hardcoded `MAVERICK`; identity is user-selected
- owner accepts visual/behavioral sequence before Gate 3

### Gate 3 — Full Project Hub redesign

The current Hub is **not** to be cosmetically patched. Replace the prototype overlay experience with the approved Renegade design language.

#### Current Hub defects reported by owner

- unclear content visible behind/below Hub; editor appears to bleed around it
- current Hub feels like an overlay rather than a true startup/home state
- owner hates current colour treatment
- font feels wrong and disconnected from Renegade Studio
- does not feel like part of the same platform
- sizing/layout poor
- label such as `NAME:` visibly cut off at left edge
- too bland
- large dead areas without deliberate composition
- recent projects represented too much like text/filesystem entries
- needs project imagery so projects are visually identifiable

#### Visual references

The source chat included three important screenshots/concepts:

1. **Current Renegade Hub screenshot** — evidence of current defects; not a design target.
2. **GameGuru MAX Hub screenshot** — reference for useful information architecture only: visual project cards, browseable projects, selected-project preview/details, obvious actions. **Do not copy GGMAX visual styling/blue colour scheme.**
3. **Original Renegade Project Hub concept** — authoritative design-language target, with giant side semicircles ignored/removed from the permanent Hub as owner requested.
4. **Identity Handshake / iris concept** — source of those semicircles and authoritative reference for Gate 2 handshake transition.

A new chat does not automatically have those source images. Before serious Gate 2/3 visual implementation, ask the owner to re-upload the reveal and visual reference images if they are not available in the active conversation. Do not improvise a materially different design from memory.

#### Original Renegade Hub design language to preserve

Approximate structure from approved concept:

- dark graphite / near-black base
- fine technical/circuit/frame detailing
- subtle cyan/blue luminous borders and UI edges
- restrained orange micro-accents
- white/grey technical typography
- Renegade Engine logo/branding top-left
- `PROJECT HUB` strong top/central heading
- welcome/system status area top-right
- left action rail
- central Recent Projects presentation driven by artwork
- right selected-project details panel
- strong primary `OPEN PROJECT` action
- restrained bottom system/status strip using **real** useful information rather than fake decorative metrics

It should feel premium, futuristic, deliberate and recognisably Renegade, while still being practical for repeated daily use.

#### Hub full-screen ownership

The Hub must be a dedicated presentation/state:

- no editor bleed-through
- no hidden editor panel peeking out at bottom or edges
- no partially constructed editor visible behind it
- responsive layout at supported window sizes
- no clipped labels
- no control overlaps

#### Hub project layout

Target interaction hierarchy:

- **New Project**
- **Open Project**
- Recent Projects visual card/grid area
- selected-project details and clear `OPEN PROJECT` action
- `BACK TO EDITOR` when Hub is invoked while a project/editor session is already active

There was discussion of `IMPORT PROJECT`; include only if a real, defined project-import workflow exists. Do not invent a meaningless button just because it appeared in concept art.

#### Open Scene on Hub

Current recommendation: **remove `OPEN SCENE...` from the primary startup Hub** because a `.wiscene` should belong to project context. File → Open Scene can remain in the editor after a project is active.

This recommendation was not yet owner-tested as final UX; discuss before hard-locking Gate 3 behavior.

#### `RETURN TO CURRENT PROJECT`

Replace with clearer language such as **`BACK TO EDITOR`** when applicable. It should not appear on initial startup with no active editor session.

Acceptance:

- owner approves the actual visual design before Gate 4 begins
- several supported sizes tested
- zero clipping/overlap/editor bleed
- design feels like the same product as Renegade Studio

### Gate 4 — Project identity, artwork & Recent Projects

Project cards must become authoritative, not decorative.

Requirements:

- project name
- descriptor/root path information where useful
- last opened/recent metadata where available
- visual artwork
- selected state
- moved/missing/unavailable state that does not crash

#### Project artwork requirement

Each project should support a persistent **16:9 project image**.

Sources:

1. user-provided/custom image
2. Renegade automatic screenshot/capture
3. branded fallback placeholder if neither exists

User-provided custom artwork should take precedence over automatic imagery.

Store artwork reference as project/editor metadata as appropriate; do not embed it into WISCENE scene content.

Acceptance:

- multiple project cards survive restart
- correct details shown for selected project
- missing/moved project produces a clear unavailable state rather than silent wrong-project behavior

### Gate 5 — Create / Open / Continue / Switch reliability

Harden lifecycle transactionally:

- Create Project
- Open Project
- Recent Project
- Continue last/recent behavior where defined
- Return to Hub
- Back to Editor
- Project A → Project B switching
- cancel paths
- malformed project descriptor
- missing project folder
- missing startup scene

Fix the already identified project/scene split-brain hazard: do not make new project identity authoritative until required startup scene preparation/adoption has succeeded, or provide an equivalent transactional rollback that guarantees consistency.

Make project switch explicitly refresh/isolate project-scoped presentation including Asset Browser state.

Acceptance sequence should include repeated A/B switching and restart/recent-project launch with no stale scene/assets/state from the previous project.

### Gate 6 — Save / Save As / dirty-state safety

Test deliberately for data loss.

Cover:

- Save
- Save As
- cancel Save As
- scene replacement
- Reopen
- project switch
- return to Hub
- app exit
- unsaved scene with and without existing path

Desired user choice semantics where appropriate:

`SAVE / DON'T SAVE / CANCEL`

**CANCEL must leave active work and context untouched.**

Do not replace the existing atomic save architecture unless evidence requires it.

Acceptance: if owner/testing can trick Renegade into silently losing dirty work, Gate 6 fails.

### Gate 7 — Reload/Reopen performance

Only now optimize from Gate 1 evidence.

If repeated governed texture preparation is proven expensive, likely optimization direction is to read/validate shared project registry/context once and/or prepare/load each unique texture Stable ID once, then apply the already-prepared/live resource to all matching material bindings. But do **not** implement this merely because it sounds sensible; use measured evidence.

Other possible hotspots must be measured similarly if texture restoration is not dominant.

If unavoidable work remains lengthy:

- provide real loading state/progress
- expose real phases
- do not fake a timer over a blocked main thread

Acceptance:

- before/after timing evidence on same V3 project
- repeated Reload/Reopen remains correct
- textures/materials remain intact
- PR #57 asset placement/persistence does not regress

### Gate 8 — Full owner acceptance, audit & merge

Release-build owner acceptance across the entire PR:

`startup reveal → identity flow → Hub → create/open/recent/switch → edit → Save → Save As → Reopen → Hub → Back to Editor → close/restart/reopen`

Verify:

- startup sequence
- chosen identity persistence
- iris transition
- Hub visual quality
- no clipping/overlap/editor bleed
- project artwork
- correct project/scene identity
- dirty-state safety
- correct Save/Save As behavior
- Reload performance
- textures/materials intact
- Asset Browser intact
- no PR #57 regression
- exact-head Debug/Release Studio CI
- exact-head Windows baseline
- independent diff/audit before merge

Only then mark ready and merge PR #58.

## 9. Startup / Hub concept — concise authoritative flow

### First run

`Launch Renegade Studio`
→ play `renegade logo reveal V2.mp4`
→ fade to black
→ blinking cursor
→ typewriter: `Welcome developer, what is your name?`
→ user enters chosen display identity (example: `MAV3R!CK`)
→ persist identity
→ Identity Handshake screen
→ dynamic `WELCOME, MAV3R!CK`
→ user clicks **ENTER HUB**
→ iris splits into two halves
→ halves move left/right
→ Project Hub revealed

### Subsequent run

`Launch`
→ logo reveal
→ Identity Handshake using persisted chosen name
→ ENTER HUB
→ iris opens
→ Hub

### Hub after transition

Permanent Hub should **not** be dominated by the giant iris semicircles. The semicircles were transition remnants from the handshake concept. Use the cleaner original Renegade Hub concept as the everyday layout target.

## 10. Design philosophy / platform consistency

Renegade Studio owns its UX. Wicked is the technical foundation, not the visible editor identity.

Therefore:

- do not surface stock Wicked Editor windows as the product UI
- preserve Renegade terminology and visual language
- functionality may be backed by Wicked systems/services, but presentation is Renegade-owned
- GGMAX is a behavioral/information-architecture reference where useful, not a visual clone

The Hub must look and behave like part of the same Renegade platform as the editor.

## 11. Important PR #57 context to protect

PR #57 was merged before PR #58 began. The owner accepted the following Release behavior:

- a newly imported asset appears in Asset Browser
- thumbnails display
- repeated instances can be dragged into scene
- repeated drags no longer crash
- newly dragged instances are textured immediately
- save/close/reopen retains textures
- current tested Release did not reproduce the earlier automatic Runtime launch problem

Do not reopen these solved issues without evidence.

One architecture detail matters for PR #58 performance work:

Renegade governed material bindings use StableId metadata and live Wicked resources. On scene reload, `RestoreMaterialTextureBindings()` reconstructs those resources from governed project assets. Any performance optimization must preserve StableId authority and governed project portability; do not fall back to original external texture paths.

## 12. Current project portability principle

Imported external files are importer inputs, not intended runtime dependencies.

For textures the established flow is conceptually:

external source image
→ retained source copy under project `SourceAssets/Textures/...`
→ governed texture product under `Content/Textures/...rasset`
→ StableId binding in scene/material metadata
→ runtime/editor restoration from governed project asset

Do not “optimize” reload by reintroducing dependency on the user's original external source folders.

## 13. PR #59 reminder

After PR #58, planned **PR #59 — Creator Importer UX & Reliability** should handle at least:

- actually editable asset name
- proper duplicate/rename behavior
- false “committed but Asset Browser verification failed” warning
- real import stages/progress/percentage
- investigate/reduce ~20-second import processing
- portability acceptance
- GLB/GLTF/FBX, multi-material, skinned/animated broader acceptance

Do not absorb these into #58.

## 14. New-chat continuation command

A user can start a fresh chat with something as short as:

> Continue Renegade Engine PR #58. Read `docs/PR58_HANDOFF.md` and `docs/PR58_GATE1_PROJECT_LIFECYCLE_BASELINE.md`, then inspect live PR #58, current `main`, exact changed files and exact-head CI before doing anything. Continue the current gate only; do not advance gates without my acceptance.

That should be sufficient. The assistant should use GitHub live state to resolve anything that has changed since this handoff was written.

## 15. Immediate next action as of this handoff

Do **not** design Gate 2 yet.

1. Recheck the latest live PR #58 head after this handoff docs commit.
2. Wait for exact-head Renegade Studio Debug/Release and Windows baseline workflows to complete.
3. If exact-head Release succeeds, fetch the Release artifact.
4. Give owner the exact artifact and the small Gate 1 V3 Reopen test.
5. Owner returns `Saved/Diagnostics/PR58Gate1Lifecycle.log`.
6. Analyze measured evidence.
7. Complete Gate 1 only when the expensive phase is proven.
8. Ask owner for approval before beginning Gate 2.

**PR #58 remains Draft. Do not merge.**
