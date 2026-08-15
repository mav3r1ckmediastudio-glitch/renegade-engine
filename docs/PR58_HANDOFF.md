# PR #58 Continuation Handoff — Startup, Project Hub & Project Lifecycle Reliability

> **Purpose:** This is the continuation authority for PR #58. A fresh session must read this file and `docs/PR58_GATE1_PROJECT_LIFECYCLE_BASELINE.md`, then verify live GitHub state before changing code. SHAs and CI statuses below are snapshots only.

## 0. Mandatory first actions in a new chat

1. Inspect live PR #58 metadata and current head.
2. Inspect current `main` and confirm PR #58 still targets the merged PR #57 baseline.
3. Compare `main` to the live PR head and report the exact changed-file set before editing.
4. Check exact-head Renegade Studio and Windows baseline workflows.
5. Read `docs/PR58_GATE1_PROJECT_LIFECYCLE_BASELINE.md` in full; it contains the measured V3 Gate 1 result.
6. Preserve the PR #57 reusable-asset pipeline. Do not refactor working `.rasset` import, thumbnail, Asset Browser, drag/drop or texture persistence behavior without direct regression evidence.
7. Work gate-by-gate. Do not begin a later gate without owner acceptance of the current gate.

## 1. Repository / PR identity

- Repository: `mav3r1ckmediastudio-glitch/renegade-engine`
- PR: **#58 — Rebuild Project Hub and harden project lifecycle**
- PR branch: `agent/pr58-project-hub-lifecycle-reliability`
- PR state: **OPEN, DRAFT, UNMERGED**
- Base branch: `main`
- PR #57 merge baseline on `main`: `a7775f31c5ec1ff61463d495e7db6ac4a5d63258`
- Diagnostic head used for the V3 Gate 1 owner test: `03d5d55dfe029f808ac499a91bdf87a91ce7fa34`
- Pinned Wicked Engine commit: `3a800b7134aafe58461093c8abb2e274d4e64033`
- Always re-read the live PR head before work.

## 2. Owner / working rules

Renegade Engine is a Wicked-based engine with its own UX and project/asset architecture. Wicked is the technical foundation, not the visible editor identity.

Critical rules:

- Work gate-by-gate; do not create another uncontrolled multi-issue repair blob.
- Diagnose before fixing. Compilation is not behavioral proof.
- GitHub CI is authoritative compile/test evidence because the owner's local CPU has known instability/damaged cores.
- Owner testing of the **Release artifact** is authoritative for editor behavior.
- PR #58 remains **Draft** until Gate 8 acceptance.
- After each visual/behavioral gate, stop for owner acceptance.
- No force-pushes to the PR branch.
- Preserve the owner's local modification to `Tools/Windows-Build.Common.ps1` and existing untracked LP04 patch files; never instruct destructive cleanup of them.
- Do not claim a workflow is fixed merely because CI is green.

## 3. PR #57 behavior that is locked/frozen

PR #57 established the reusable asset workflow and took substantial stabilization work. Treat these as protected unless direct evidence proves regression:

- governed `.rasset` model import
- governed texture assets
- thumbnail capture and Asset Browser display
- imported assets appearing in Asset Browser
- repeat drag/drop from Asset Browser
- repeated placement without second-drag crash
- live textures/materials on freshly dragged instances
- saved scene texture/material persistence after close/reopen
- the accepted Release did not reproduce the earlier automatic Runtime launch problem

PR #58 intentionally moves away from importer work. Creator Importer UX/reliability belongs to planned **PR #59**.

## 4. PR #58 scope and explicit non-goals

PR #58 owns three connected areas:

1. startup experience
2. Project Hub redesign
3. project / scene / save / load lifecycle reliability and performance

Do **not** absorb into PR #58:

- Creator Importer editable asset-name repair
- importer false post-commit verification warning
- importer ~20-second processing optimization
- importer progress/percentage UI
- new model formats
- terrain redesign
- Inspector redesign
- new gameplay/runtime systems
- changes to PR #57 reusable-asset architecture unless a proven regression requires a minimal compatibility correction

## 5. Gate 1 — measured result and closure state

### Objective

Gate 1 was **Baseline, lifecycle map & diagnostics**. It deliberately changed no Hub/startup UX and introduced no reload optimization.

The owner tested exact diagnostic head `03d5d55dfe029f808ac499a91bdf87a91ce7fa34` using the real V3 project after:

- Windows baseline #920: **Debug SUCCESS / Release SUCCESS**
- Renegade Studio #497: **Debug SUCCESS / Release SUCCESS**

### Owner V3 measurement

Owner wall-clock observation: approximately **2 minutes 3 seconds** from pressing Reopen until the editor unlocked and full controls returned.

The Reopen-aligned diagnostic pass reported:

- `156` persisted material/texture bindings
- `27` unique governed texture Stable IDs
- `0` already live
- `156` restored
- `0` failures
- `18.08 ms` binding inspection
- `122924.55 ms` governed preparation
- `6551.67 ms` Wicked resource apply/load
- `129653.87 ms` total governed texture restoration

The scene contains **27 asset instances**, each normally using approximately **4–6 PBR texture maps**. `156 / 27 = 5.78` bindings per instance on average, consistent with that workload.

Governed preparation consumed approximately **94.8%** of the measured texture-restore pass.

### Proven Gate 1 diagnosis

`RestoreMaterialTextureBindings()` walks persisted material/slot bindings. For each non-live binding it calls `PrepareMaterialTextureAsset()` before applying the resource. The measured workload contains only 27 unique governed texture Stable IDs but 156 bindings, so expensive governed preparation is repeated during one restoration pass.

**The Reopen stall is therefore dominated by governed texture preparation performed at binding granularity.** This is no longer a hypothesis.

Do not fix it in Gate 1. The likely Gate 7 seam is to prepare/reuse each unique governed texture identity once per restore pass, or an equivalent measured design, while preserving `.rasset` governance, StableId authority and project portability.

### Gate 1 lifecycle findings retained for later gates

Normal Reopen uses the prepared asynchronous scene-open path, then synchronously restores governed textures after commit. Project Open/Create currently differs and uses a synchronous `session_->LoadScene(...)` boundary.

Two reliability hazards are already identified:

1. **Project/scene split-brain on failed adoption — Gate 5.** Project identity/recents can become current before the startup scene is proven loadable, potentially leaving new project context with the old scene still active.
2. **Asset Browser project-switch isolation — Gate 5.** Project adoption/switch does not make Asset Browser refresh an explicit lifecycle boundary, allowing stale project presentation until another refresh path runs.

The existing save service has substantial safety architecture: temporary serialization, validation, destination protection, atomic replacement, final validation, restore-on-failure and backups. Do not casually replace it. Gate 6 tests and hardens user-facing semantics.

### Gate 1 closure requirements

Performance diagnosis is complete. To mark Gate 1 fully **CLOSED**:

1. the final docs-only closure head must pass exact-head Renegade Studio Debug/Release and Windows baseline Debug/Release;
2. the owner must explicitly confirm the post-Reopen scene/textures remained visually correct.

Then stop and obtain explicit owner approval before Gate 2.

## 6. Full PR #58 gate plan

### Gate 1 — Baseline, lifecycle map & diagnostics

Status: **diagnosis complete; closure-head CI + explicit visual correctness confirmation are the remaining closure checks.**

No reload optimization belongs here.

### Gate 2 — Startup identity experience

Gate 2 is the cinematic/startup state and has four mini-stages.

#### Gate 2A — Logo reveal

Use the owner's actual file: **`renegade logo reveal V2.mp4`**.

Known properties from the source chat:

- approximately 9.77 seconds
- 1280×720
- H.264 video
- 30 fps
- AAC audio

The file was supplied previously but is not committed to the repo. When Gate 2 begins, obtain/re-upload the real asset rather than inventing a substitute.

Requirements:

- full-screen/viewport presentation
- preserve aspect ratio appropriately
- audio plays
- no editor or Hub flash/bleed underneath
- clean fade to black at the end
- missing/corrupt reveal fails safe into startup rather than preventing Studio from opening
- do not use the reveal to hide unrelated project loading

#### Gate 2B — First-run developer identity prompt

After the reveal:

- fade to black
- blinking cursor
- typewriter text: conceptually/exactly `Welcome developer, what is your name?`
- user enters preferred display identity; the owner's example is `MAV3R!CK`
- punctuation such as `!` is allowed because this is a display identity, not a filename
- Backspace/editing works
- Enter confirms
- empty identity cannot confirm
- identity persists in **user/editor preferences**, not project metadata or WISCENE

This is first-run setup, not a prompt on every launch.

#### Gate 2C — Identity Handshake

Use the owner's supplied concept image as the visual authority:

- dark technical Renegade UI
- large central mechanical/cybernetic iris
- horizontal red scan/laser through the centre
- diagnostic/status details around it
- left-side identity-handshake language
- dynamic `WELCOME, <chosen name>`
- `RENEGADE // IDENTITY ACCEPTED` belongs here, not permanently on the Hub
- **ENTER HUB** control
- do not force automatic entry; owner concept requires a click

Subsequent launches should be:

`logo reveal → handshake using saved identity → ENTER HUB`

Do not ask for the name again unless identity preferences are cleared/changed.

A future skip option may be desirable, but skip policy was **not locked**. Do not invent it without owner discussion.

#### Gate 2D — Iris-to-Hub transition

On **ENTER HUB**:

- central iris unlocks/splits vertically into left/right halves
- halves move away from centre
- this recreates the semicircular side machinery seen in the early Hub concept
- Project Hub is revealed between/behind the halves

Critical history: the giant semicircles in the early Hub concept were the split iris halves, not arbitrary permanent Hub decoration. The owner later asked to ignore/remove them from the everyday permanent Hub. Preserve the transition concept without permanently dominating the Hub with them.

Gate 2 acceptance:

- first run: reveal → typed identity prompt → handshake → ENTER HUB → iris opens → Hub
- later run: reveal → persisted-identity handshake → ENTER HUB → Hub
- no editor/Hub bleed before intended reveal
- no hardcoded `MAVERICK`; identity is user-selected
- owner accepts the actual sequence before Gate 3

### Gate 3 — Full Project Hub redesign

The current Hub is **not** to be cosmetically patched. Replace the prototype overlay experience with the approved Renegade design language.

Known current defects:

- editor/content bleed visible behind/below Hub
- feels like an overlay rather than a true startup/home state
- wrong colour treatment and typography
- disconnected from Renegade Studio visual language
- poor sizing/layout and clipped labels such as `NAME:`
- bland/dead areas
- recent projects too filesystem/text-like
- needs project imagery for rapid visual identification

Visual authorities/references from the source chat:

1. current Renegade Hub screenshot — defect evidence, not target
2. GameGuru MAX Hub — information-architecture reference only; do **not** copy its visual styling/blue scheme
3. original Renegade Project Hub concept — authoritative design-language target, except giant permanent side semicircles are removed/ignored
4. Identity Handshake / iris concept — authority for Gate 2 transition

Before serious Gate 2/3 visual implementation, re-upload the reveal and reference images if absent from the active conversation. Do not improvise a materially different design from memory.

Renegade Hub design language to preserve:

- dark graphite / near-black base
- fine technical/circuit/frame detailing
- subtle cyan/blue luminous borders and edges
- restrained orange micro-accents
- white/grey technical typography
- Renegade branding top-left
- strong `PROJECT HUB` heading
- welcome/system status top-right
- left action rail
- central Recent Projects driven by artwork
- right selected-project details panel
- strong primary `OPEN PROJECT`
- restrained bottom status strip using real useful information, not fake decorative metrics

Hub must own the full presentation state: no editor bleed, no panel peeking, no partially constructed editor visible, responsive layout, no clipping or overlaps.

Target interaction hierarchy:

- **New Project**
- **Open Project**
- visual Recent Projects cards/grid
- selected-project details + `OPEN PROJECT`
- **BACK TO EDITOR** only when an active editor session exists

`IMPORT PROJECT` belongs only if a real import workflow exists. Current recommendation is to remove `OPEN SCENE...` from the primary startup Hub because `.wiscene` should belong to project context; discuss before hard-locking that UX.

Gate 3 ends only after owner approval of the actual visual design.

### Gate 4 — Project identity, artwork & Recent Projects

Project cards become authoritative, not decorative. Support:

- project name
- descriptor/root path where useful
- recent/last-opened metadata where available
- selected state
- moved/missing/unavailable state that does not crash
- persistent **16:9 project artwork**

Artwork precedence:

1. user-provided/custom image
2. Renegade automatic screenshot/capture
3. branded fallback placeholder

Store artwork reference in project/editor metadata as appropriate, not WISCENE scene content.

Acceptance includes restart persistence, correct selected-project details and safe missing/moved states.

### Gate 5 — Create / Open / Continue / Switch reliability

Harden transactionally:

- Create Project
- Open Project
- Recent Project
- Continue last/recent where defined
- Return to Hub
- Back to Editor
- Project A → Project B switching
- cancel paths
- malformed descriptor
- missing project folder
- missing startup scene

Fix project/scene split-brain: new project identity must not become authoritative until startup scene preparation/adoption succeeds, or equivalent rollback must guarantee consistency.

Project switch must explicitly refresh/isolate project-scoped presentation including Asset Browser state.

Acceptance includes repeated A/B switching and restart/recent-project launch with no stale scene/assets/state.

### Gate 6 — Save / Save As / dirty-state safety

Deliberately test for data loss:

- Save
- Save As
- cancel Save As
- scene replacement
- Reopen
- project switch
- return to Hub
- app exit
- unsaved scene with/without existing path

Desired choice semantics where appropriate:

`SAVE / DON'T SAVE / CANCEL`

**CANCEL must leave active work and context untouched.**

Do not replace the existing atomic save architecture without evidence. If testing can trick Renegade into silently losing dirty work, Gate 6 fails.

### Gate 7 — Reload/Reopen performance

Only now optimize from Gate 1 evidence.

The measured hotspot is governed texture preparation. Likely direction: read/validate shared project registry/context once and/or prepare/load each unique texture Stable ID once, then reuse the prepared/live resource for matching material bindings. Preserve StableId authority, `.rasset` governance and project portability.

If unavoidable work remains lengthy, provide real loading state/progress and expose real phases; never fake a timer over a blocked thread.

Acceptance:

- before/after timing on same V3 project
- repeated Reopen remains correct
- textures/materials remain intact
- PR #57 asset placement/persistence does not regress

### Gate 8 — Full owner acceptance, audit & merge

Release owner acceptance across:

`startup reveal → identity flow → Hub → create/open/recent/switch → edit → Save → Save As → Reopen → Hub → Back to Editor → close/restart/reopen`

Verify startup, identity persistence, iris transition, Hub quality, no clipping/bleed, project artwork, correct project/scene identity, dirty-state safety, Save/Save As, Reload performance, textures/materials, Asset Browser, no PR #57 regression, exact-head Studio Debug/Release, Windows baseline, and an independent final diff/audit.

Only then mark ready and merge PR #58.

## 7. Startup / Hub authoritative flow

### First run

`Launch Renegade Studio`
→ play `renegade logo reveal V2.mp4`
→ fade to black
→ blinking cursor
→ `Welcome developer, what is your name?`
→ user enters chosen display identity
→ persist identity
→ Identity Handshake
→ dynamic `WELCOME, <chosen name>`
→ user clicks **ENTER HUB**
→ iris splits
→ halves move left/right
→ Project Hub revealed

### Subsequent run

`Launch`
→ logo reveal
→ handshake using persisted identity
→ ENTER HUB
→ iris opens
→ Hub

Permanent Hub should **not** be dominated by giant iris semicircles; they are transition remnants.

## 8. Platform consistency / portability principles

Renegade Studio owns its UX:

- do not surface stock Wicked Editor windows as product UI
- preserve Renegade terminology and visual language
- Wicked systems/services may back functionality, but presentation is Renegade-owned
- GGMAX is an information-architecture reference where useful, not a visual clone

Imported external files are importer inputs, not intended runtime dependencies. Texture flow is conceptually:

external image
→ retained source copy under `SourceAssets/Textures/...`
→ governed texture product under `Content/Textures/...rasset`
→ StableId scene/material metadata
→ runtime/editor restoration from governed project asset

Never "optimize" reload by falling back to the user's original external source paths.

## 9. PR #59 reminder

Planned **PR #59 — Creator Importer UX & Reliability** owns at least:

- actually editable asset name
- duplicate/rename behavior
- false post-commit Asset Browser verification warning
- real import stages/progress/percentage
- investigate/reduce ~20-second import processing
- portability acceptance
- broader GLB/GLTF/FBX, multi-material and skinned/animated acceptance

Do not absorb these into #58.

## 10. Immediate next action

1. Re-read the live PR #58 head after this handoff update.
2. Confirm the final diff remains exactly the intended Gate 1 diagnostic code plus PR #58 documentation, with no temporary residue.
3. Wait for exact-head Renegade Studio Debug/Release and Windows baseline Debug/Release on the final closure head.
4. Record all four jobs green before mechanically closing Gate 1.
5. Obtain/record the owner's explicit confirmation that the post-Reopen scene and textures remained visually correct.
6. Once both are satisfied, mark **Gate 1 CLOSED**.
7. Stop. Do **not** implement the measured reload optimization yet; it belongs to Gate 7.
8. Obtain explicit owner approval before Gate 2.
9. When Gate 2 is approved, re-upload the actual `renegade logo reveal V2.mp4` and relevant visual references if absent from the active conversation.

**PR #58 remains Draft. Do not merge.**
