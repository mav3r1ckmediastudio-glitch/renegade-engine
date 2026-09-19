# START HERE — Codex importer v3 (local)

Open **this `renegade-importer-v3` folder** as the workspace in Codex on your PC. Paste the instruction below into Codex. This file and `docs/importer-v3/CODEX_IMPLEMENTATION_BRIEF.md` are the task; the approved v3 HTML and historical handoff are supplied locally in `docs/importer-v3/reference/` and intentionally ignored by Git.

---

**Codex task:** Implement a substantial first native C++ version of the **approved Renegade importer v3** in this exact local worktree. First read `AGENTS.md`, `docs/importer-v3/CODEX_IMPLEMENTATION_BRIEF.md`, `docs/importer-v3/IMPORTER_V3_HANDOFF.md`, and both local files in `docs/importer-v3/reference/`. Open the approved HTML to understand its visual design. Follow the brief and start making real C++/bridge changes promptly, prioritising verified Model import/save/reopen/Asset Browser reveal and a working v3 right-inspector workspace rather than only planning or painting a mock UI. Continue as far as practical into Character rig and real external-animation preview/retargeting.

Use VS 18 Build Tools 2026 for focused local builds/tests in this worktree. Keep other worktrees, current Studio, private assets, the pinned Wicked submodule and existing PRs untouched. Never stage/upload the ignored reference files or private FBXs. Make frequent **local commits**; update `docs/importer-v3/IMPORTER_V3_HANDOFF.md` and repository `HANDOFF.md` as required with actual SHAs, completed behaviour, tests, blockers and the very next coding action. If time/usage is nearly exhausted, save and locally commit an honestly labelled WIP checkpoint so ChatGPT can continue through Desktop Commander. **Do not push, create a PR, trigger CI, merge, reset, rebase, overwrite active binaries or delete anything without asking me.** Finish by stating exactly what was implemented and tested; never claim an unverified importer succeeded.

---

When you return to ChatGPT, say: **"Continue the local importer v3 worktree using its IMPORTER_V3_HANDOFF.md and Git history; audit the latest Codex commits before changing code."**
