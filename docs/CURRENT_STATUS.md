# Current integrated Renegade status

**Reviewed:** 10 November 2026. **Source baseline:** main at
[618b231](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/commit/618b2310ec7d6a7889d490e93304e39043f7e5ff), the merge of PR #193.
**Wicked gitlink:** `82557d439a41d104f559b0d3134d160b7bf6154e`.
This page supersedes status claims in older implementation checkpoints; it does
not replace their evidence or declare every programme/release gate complete.

## Integrated milestones

| Merge | Date | Integrated scope |
|---|---|---|
| [#178](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/178) | 5 Oct | First-person arms and reusable Player prefabs; owner-accepted bounded workflow. |
| [#181](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/181) | 10 Oct | Combined equipment/actions, directional hand presentation, projectiles, Hitscan and surface impacts; includes the P2 stream. |
| [#182](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/182) | 10 Oct | Original impact artwork and fourteen owner-authored impact sounds. |
| [#183](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/183) | 17 Oct | Night sky, aurora and Milky Way; owner artistic acceptance recorded. |
| [#184](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/184) | 20 Oct | Consolidated UI, gameplay and assistant source; merge is not blanket UX acceptance. |
| [#185](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/185) | 23 Oct | Environment-probe creation safety and live reflections. |
| [#186](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/186) | 28 Oct | Approved spline rivers and placeable waterfalls. |
| [#187](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/187) | 28 Oct | Inventory G1–G5 integrated with river/waterfall main. |
| [#188](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/188) | 1 Nov | Accepted Weather V2 combined with world/inventory systems; current Wicked pin. |
| [#189](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/189) | 5 Nov | Terrain initialization at Y=0, maximum finite terrain/rivers and Player Start preview recovery. |
| [#190](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/190) | 5 Nov | Local riverbed rebuild/progress/focus and restored stock water normals after reopen. |
| [#191](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/191) | 7 Nov | Point-query performance guards, stable overlays, lighting input, VSync and responsive Test Game preparation. |
| [#192](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/192) | 10 Nov | Light picker, reusable behaviours and layered particles with textures, fields, lifetime, shapes, timing and Lua. |
| [#193](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/193) | 10 Nov | Multi-selection and functional scene prefabs, script remapping and preserved packaged instance edits. |

These PRs are merged. Their pre-merge descriptions can still contain historical
“draft”, “do not merge” or “CI pending” language. Merge records establish integration;
only scoped receipts establish the corresponding behavioural/owner acceptance.

## Latest validated feature candidate

PR #193 head `cb590f06267e3407633d4e8f68bbde9265dd6ec1` passed all four
pre-merge jobs: Studio Debug/Release and Windows baseline Debug/Release.
[Studio run](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/42857306536);
[baseline run](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/42857306652).
The Studio workflow builds Studio and Runtime and runs registered tests/startup
checks; baseline jobs build the pinned Wicked reference. They are not four
independent Runtime visual tests. This is pre-merge-head validation, not a claim
that separate post-merge-main jobs have finished.

The functional prefab candidate also passed Release builds, eleven focused
checks, sixty source contracts, the six earlier failing checks, the original
geometry proof and complete scripted-brazier proof. Actual Studio save/card drag/
Unpack/Save/Reopen and package-relative standalone rendering were checked.
Runtime reported four governed script instances started. See
[evidence](evidence/scene-prefab-functional/README.md).

## Current boundaries

- Windows x64/DX12 is the primary supported development/acceptance target.
  Vulkan checks are scoped; do not infer full backend parity from DX12 CI.
- General prefab copies are independent. Linked source-update propagation,
  a prefab-override management UI and arbitrary external script-target capture
  are not implemented by PR #193.
- Script targets outside a prefab selection must be included or cleared.
  There is no explicit prefab entity-count cap; large castle assemblies have
  not been stress-tested. Script/data safety budgets still apply.
- Existing material shader selections/resources survive prefabs. A third-party
  shader-editor integration and general plugin SDK/marketplace are proposals,
  not current product capabilities.
- Terrain is finite, through 33×33 / 1,089 chunks in the recovered path.
  The existing changed-sample, payload and composition budgets still apply;
  maximum extent is not permission to bypass riverbed limits.
- Native Test Game capture has a short synchronous phase. Activity indication
  and later background preparation do not imply zero startup latency.
- FPS figures in receipts describe their named hardware/scene/settings.
  No general minimum-FPS guarantee or empty-scene performance acceptance.
- Scoped owner acceptance and a merge do not automatically close independent
  verification, complete combat/AI/media parity or distribution/licensing gates.
- No new alpha, castle asset update, plugin implementation or distribution is
  authorized by this documentation reconciliation.

## Documentation authority

Use [README](../README.md), [creator guide](CREATOR_GUIDE.md),
[roadmap](ROADMAP.md), [architecture](ARCHITECTURE.md), [build guide](BUILD_WINDOWS.md)
and [feature ledger](FEATURE_MATRIX.csv) for current orientation.
[Documentation index](README.md) classifies every tracked docs/root Markdown file.
Dated plans, QA captures and evidence directories are historical/scoped records,
not automatic current instructions. Independent checks still use
[VERIFICATION_CHECKLIST](VERIFICATION_CHECKLIST.md).
