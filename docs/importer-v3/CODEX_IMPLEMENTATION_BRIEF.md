# Renegade Importer v3 — local Codex implementation brief

**Mission:** Make a substantial, working start on the approved native Studio importer, not another HTML prototype or planning-only report. Work locally in this dedicated Git worktree. ChatGPT will continue the same branch through Desktop Commander if Codex usage runs out.

## 0. First actions (do these before editing)

1. Read repository `AGENTS.md`, `README.md`, `docs/PROJECT_CHARTER.md`, `docs/ARCHITECTURE.md`, `docs/ROADMAP.md`, `HANDOFF.md`, and relevant `docs/FEATURE_MATRIX.csv` rows.
2. Open `docs/importer-v3/reference/Renegade_Importer_Interactive_Concept_v3.html` in a browser **and inspect its HTML/CSS/JS**. The user approved this v3 visual/interaction design. It is a simulation, not an importer. Do not replace it with the older UI.
3. Read `docs/importer-v3/reference/RENEGADE_NEXT_CHAT_HANDOFF_2026-09-16.md` in full, but verify any older status against current source and Git. Read `docs/importer-v3/IMPORTER_V3_HANDOFF.md` for the current checkpoint.
4. Confirm `git status --short --branch`, `git log -1`, `git worktree list`, pinned submodules, available Windows toolchain and whether Studio is running. Do not change another worktree or running program.
5. Review current importer implementation and GitHub PRs #162 (mixed recovery; do not merge as-is), #166 (A1 failure diagnostics), #168 (A2 persisted import/weight normalization), #158 (AI) and #170 (Jolt FPS). #164 already merged initial navigation tile. Do not assume PR code is in this `main`-based worktree.

## 1. Non-negotiable workspace and source boundaries

- This branch starts from `origin/main` at `5732d8d460610045275a9149252e47f0a3d75a39`, **not** A1/A2, AI, CW-05 or the FPS-fix branches. Verify if `main` moves; do not rebase or merge without the user deciding.
- All implementation commits stay on `feature/importer-v3-local-handoff` in this worktree. Do not cherry-pick an entire mixed PR; reuse only necessary, inspected code and report dependencies.
- The approved mockup and older handoff live in the `reference/` folder, intentionally ignored by Git. They may include the user's screenshot. **Never stage, commit, push, post in PRs, or upload those files or embedded imagery.** Do not copy private Mutant FBX, external animation FBXs, `.rasset`, project scenes, logs, secrets, screenshots or local paths into remote repo/CI.
- Do not reset, clean, rebase, force-push, delete branches, change submodule revisions, merge PRs, or modify the user's original checkout. **No GitHub push, PR or CI run without new explicit user approval.** Ordinary local checkpoint commits are authorized for recovery.
- Do not close/restart Studio, overwrite binaries used by a running process, change the user's authored scene, or replace a working Runtime. Build outputs must remain under this worktree's own `BUILD/` directory.
- Scope this phase to the importer. Do not repair Physics Lab UI, AI combat, navigation, Jolt allocator, or general editor features incidentally.

## 2. Approved UX contract (the mockup is design authority)

- Dedicated native importer workspace, large isolated 3D preview and **resizable right inspector**. No narrow modal, overlapping editor widgets or duplicate left-side stage rail.
- The **six inspector section headings are the only stage navigation**: Asset Setup, Transform, Materials, Rig & Retargeting, Animations, Review & Import. Only the selected stage's controls are visible; headings stay accessible. Sections retain their selections/queued files; the inspector scrolls internally without stretching the viewport.
- Prominent mutually exclusive **Model / Character** choice. Model skips Rig/Animations; Character enables them. Do not add a redundant later 'IMPORT AS' selector.
- Preview scene, camera, lighting and fixed **1.82 m reference mannequin** must be isolated from the user's editor scene; reference height must not change with model scaling, moving or rotation. Preserve original imported source units/scale by default; unit conversion and transform adjustments must be explicit.
- Materials/textures inspection; editable, persistent transform controls; actual source mesh viewport; camera controls. Avoid using the mockup's static image or CSS-sway animation as a native substitute.
- Character stage: actual skeleton/bone mapping and missing-mapping diagnostics; external file picker, multi-file queue, clip selection, name, duration, include/exclude/remove, source and mapping status; selecting Preview must drive **real playback on the imported character**, with accurate success/failure reporting.
- Review must display actual destination and transaction outcomes. **Never report success until a physical `.rasset` has been committed, reopened successfully and revealed in the Asset Browser.** Model: `Content/Models`; Character: `Content/Characters`. Keep original scene and other assets intact on failure.
- HTML's twelve clips, 37 bones, mannequin graphic and success modal are illustrative. Do not claim those counts or rig results in real Studio without measuring them.

## 3. Implementation priority: meaningful code, not a mock-up-only PR

**First vertical slice (highest priority):** Trace existing shared Model/Character import, persistence, registry and Asset Browser paths; reproduce or instrument the missing `.rasset` failure with an approved local/non-private fixture. Make detailed *native* failures visible, including stage, full reason, destination and whether an asset was actually committed. Fix the verified underlying defect if demonstrable. Then wire the v3 workspace shell and usable Model path to these real services. Prefer one end-to-end functioning Model import over six disconnected decorative pages.

**Next:** Native inspector navigation, isolated viewport, fixed reference and transform/material surfaces, wired to real state. Refactor rather than maintain two competing importer workflows; do not delete functioning old import paths before parity is established. Reuse bridge service boundaries; avoid scattering engine internals into UI.

**Then:** Character rig mapping and external animations, real preview/retarget on the imported character, persistence/reopen and placement transform parity. If full retargeting is not reached before quota ends, checkpoint the strongest compiled working slice and record the precise unimplemented behaviours.

Find and inspect likely starting points: `Studio/src/CreatorImportPreviewWindow.h`, `Studio/src/StudioApplication.cpp`, `EngineBridge/src/CreatorModelImportRecipe.cpp`, `EngineBridge/src/CreatorExternalAnimationImportService.cpp`, `CreatorAssetWorkflowService`, `ReusableAssetService`, and importer tests. Actual repository search and current types override these historical pointers.

## 4. Local build and acceptance (avoid repeated 55-minute CI)

- Windows x64 / DX12. Installed VS **18 / Build Tools 2026** with CMake and MSBuild; do not use the older VS2022 toolchain. Confirm executable paths with `Get-Command` or the installed VS18 Build Tools folders before running them.
- In **this** worktree configure with the VS18 CMake: `cmake -S . -B BUILD/renegade -G "Visual Studio 18 2026" -A x64 -DRENEGADE_EMBED_SHADERS=ON` (adjust executable path only after verifying it). Build the focused Studio target: `cmake --build BUILD/renegade --config Release --target RenegadeStudio --parallel 4`. Use focused source/contract tests between expensive builds; build matching Runtime only when a gameplay-facing change requires it.
- This new worktree begins with no compiled Studio/Runtime. **Do not borrow or overwrite binaries from the active A2 performance worktree** or interpret old copied Runtime as matching. Check running processes before every link and do not terminate them without asking the owner.
- Capture exit code, UTC timestamp, exact commit/build source, compiler errors, test names and results in the handoff. A compiler pass does not establish a working importer or UI.
- Verify on actual Studio, with consent if using private local test assets: source model preview appears; Model import writes a real `.rasset`; exact path exists; registry/Asset Browser reveals it; close/reopen preserves mesh/material; Character + external FBXs play **on the actual character**, preserve rig/animations across reopen, and placement retains transform. Record failed/untested cases explicitly.
- Owner has previously observed Mutant + 12 external animation FBXs apparently process but produce **no visible `.rasset`**; plain Model Mutant import also failed. This is evidence of a symptom, **not a proven root cause**. Do not repeatedly force the same owner test without improving diagnostics or implementation.
- Keep the Physics Lab UI issues, ragdoll FPS fix and standalone Runtime packaging separate. For tests of gameplay-facing importer changes, produce a matching Runtime rather than relying on a previously copied executable.

## 5. Recoverability / quota-exhaustion protocol

1. **Start implementation promptly** after a short code audit. Avoid spending the allowance on exhaustive planning or repeated full builds while no real code is changed.
2. After each meaningful, self-contained slice: `git diff --check`; run the smallest relevant build/tests; make an explicit **local commit** on the importer branch. Use `WIP:` in the commit title if incomplete/unverified and describe its limitations. Never silently claim it works.
3. Update `docs/importer-v3/IMPORTER_V3_HANDOFF.md` after every checkpoint with exact branch, known base, latest *implementation* commit SHA (`git rev-parse HEAD` at that moment), source paths, commands/results, unfinished work, and **the first command/file/action for the next assistant**. Update `HANDOFF.md` and `docs/FEATURE_MATRIX.csv` only as the repository instructions require when implementation changes capability exposure. The final handoff documentation may itself be a subsequent local commit; identify its SHA separately with `git rev-parse HEAD`.
4. Before running out of time: save all intentional edits; locally commit even an explicitly labelled partial slice if necessary, documenting build status and any compile blockers. Do not stash then forget, reset, delete or overwrite unknown changes. A handoff is invalid if it exists only in the Codex chat.
5. In final response, report the actual branch, latest commit, what truly works, what fails/was not tested, and point to the handoff file. Stop cleanly; do not push or start CI without the owner's request. ChatGPT can inspect this exact local worktree later through Desktop Commander.

**Completion standard for this Codex session:** significant native C++/bridge progress plus reproducible local checkpoints and honest acceptance evidence. A functioning first vertical slice is the target; full importer completion is not assumed or required before handoff.
