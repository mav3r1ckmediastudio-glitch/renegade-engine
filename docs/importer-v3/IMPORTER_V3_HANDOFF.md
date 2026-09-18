# Importer v3 — live local engineering handoff

**Status at setup (18 September 2026): PREPARED ONLY; no importer implementation has been started in this branch.** This file must be updated by Codex after every meaningful implementation checkpoint. A different assistant should read this file, inspect `git log`/`git status` and source, and continue the existing work—not restart it.

## Local branch and starting evidence

- Branch: `feature/importer-v3-local-handoff` in its own worktree (`renegade-importer-v3` under the same repos directory as `renegade-engine`). The original `main`, A2-performance and FPS-fix worktrees are separate.
- Base: `origin/main` commit `5732d8d460610045275a9149252e47f0a3d75a39` (merged #164 initial Story Flow navigation tile). Record the actual **latest commit** with `git rev-parse HEAD`; do not confuse this known base with later work.
- Pinned `WickedEngine` commit `3a800b7134aafe58461093c8abb2e274d4e64033`; pinned `ThirdParty/imnodes` commit `eb36902c892548ef94f88f51ad7e7c9c7058a71c`. Both checked out in this worktree during setup.
- Full approved v3 design: `docs/importer-v3/reference/Renegade_Importer_Interactive_Concept_v3.html`, SHA-256 `6CD5EBF25DB158B40F1FC2A98B6A91043891E5CCC81239D0CC47D9F3588189A3` (local only; ignored by Git).
- Historical handoff: `docs/importer-v3/reference/RENEGADE_NEXT_CHAT_HANDOFF_2026-09-16.md`, SHA-256 `8F65163336A8CE490BCFCF23A52EE63F5A5485799724F72BBC42ACD942D4CD00` (local only; older facts require fresh verification).
- Mission and working contract: `docs/importer-v3/CODEX_IMPLEMENTATION_BRIEF.md`; starting user instructions: root `CODEX_IMPORTER_V3_START.md`; repository instructions: root `AGENTS.md`.
- VS 18 Build Tools 2026 CMake configuration **PASSED** on this new worktree: `cmake -S . -B BUILD/renegade -G "Visual Studio 18 2026" -A x64 -DRENEGADE_EMBED_SHADERS=ON` (using verified VS18 CMake executable). The new Studio and Runtime `.vcxproj` files exist. Release compilation/link and importer acceptance are **NOT RUN**. CMake noted Gate 2C startup media absent in this checkout; that pre-existing packaging caveat is not importer validation.

## Work done / current implementation

- Local worktree created from `origin/main`, isolated from existing active branches.
- Local approved HTML and historical handoff copied byte-identically into an ignored reference directory; **not** part of the Git commit or remote repo.
- Submodules initialized at their pinned commits. Studio was observed running from the **A2 performance worktree** during setup; do not close or replace its executable without user permission.
- New importer code, a current-head Studio build, real Model `.rasset` persistence and Character retarget/playback: **NOT STARTED / NOT TESTED** at setup.

## Active priorities / first concrete action

1. Read approved v3 HTML and the full implementation brief; inspect actual `Studio/src/CreatorImportPreviewWindow.h` and `CreatorAssetWorkflowService` / `ReusableAssetService` persistence and Asset Browser reveal paths on this branch.
2. Identify how to expose the actual import failure reason in the native Studio UI and build a reproducible failure/commit test using non-private assets or approved local assets. Plain Model import was previously reported to fail to reveal `.rasset`; cause remains **unknown**.
3. Implement meaningful C++ vertical-slice progress: native v3 workspace with sole right-side stage headings and Model import wired through a real commit/reopen/reveal path. Do not stop after a plan or a static UI shell.
4. Run focused tests and build with VS18 only when safe; record actual results. Later stage: rig diagnostics and external clip playback/retarget on the model, not a CSS animation.

## Latest checkpoint (Codex must keep this section current)

- Latest completed **implementation** commit: none at setup.
- Latest handoff/setup commit: inspect `git rev-parse HEAD` after the initial documentation commit; do not invent a self-referential SHA.
- Changed implementation files: none.
- Implementation builds/tests: none. Setup-only VS18 CMake configure passed (exit 0); targeted native Studio/Runtime builds and owner importer tests have not run.
- Observed owner-acceptance result on **this importer branch**: none.
- Known failure/limitation: full importer and animation functionality have not been implemented or retested on this branch. #166/#168 contain staged importer diagnostics/roundtrip work but are separate unmerged PRs; #162 is mixed and duplicates #164 navigation work; #170 is the separate FPS change.
- Exact next command: from this worktree, run `git status --short --branch; git log -1 --oneline; git submodule status`, then read `docs/importer-v3/CODEX_IMPLEMENTATION_BRIEF.md` and follow its first vertical-slice instructions.

**Every future handoff:** replace these checkpoint entries with actual source/commit evidence and the next precise action. Commit the updates locally; record the handoff-documentation commit separately from the last tested implementation commit. No unauthorized push, CI or branch deletion.
