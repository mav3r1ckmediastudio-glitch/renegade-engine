# Renegade roadmap

**Reconciled 10 November 2026 against main `618b231`.**
[Current integrated status](CURRENT_STATUS.md) is the merge/evidence ledger.
This roadmap separates delivered source from remaining product work.

## Integrated foundations

| Area | Current baseline | Still to prove or extend |
|---|---|---|
| Studio/project lifecycle | Hub, Story Flow/Journey, Scenes/Screens, stable IDs, transactions and Undo/Redo are integrated. | Broader beginner usability and independent exact-version audits remain scoped tasks. |
| Player/combat | P1 arms/player prefabs and combined P2/P3 equipment, hand actions, projectiles/Hitscan and impacts merged through #178/#181/#182. | Full Alpha combat acceptance, reference weapon families and documented art/behaviour limits remain; do not treat every P1/P2/P3 gate as closed. |
| Character/importer | Native import/Character pipeline, assigned animations, crossfades and behaviour/navigation are integrated. | Expanded authoring, animation blending/crowd scale and complete importer-format parity are not implied. |
| Inventory | G1–G5 integration with rivers merged in #187, including item/container and Screen paths. | Wider UX/gameplay/save-game acceptance remains evidence-specific. |
| World/weather | Rivers/waterfalls #186, Weather V2 #188 and terrain/river recovery #189/#190 are merged. | Populate representative scenes, preserve budgets and validate release/package configurations; no unlimited terrain or global FPS guarantee. |
| Editor responsiveness | Point lookups, stable overlays, VSync, lighting fields and Test Game progress/background work merged in #191. | Capture still has a short synchronous stage; performance must be measured with real content. |
| Light/particle authoring | Complete picker/behaviour/texture/fields/lifetime/shapes/timing/Lua source merged in #192. | Track remaining owner/independent limits explicitly rather than calling the source a local-only candidate. |
| General assemblies | Multi-selection and functional prefabs merged in #193 after four green candidate checks. | Large assembly stress testing, broader independent acceptance and source-linked prefab updating are separate work. |

## Next work requires a bounded brief

The modular ruined castle kit can now use functional assemblies and individual
components. The prefab task did not modify castle assets. Castle/module/zoo work
must follow its own asset brief and preserve source assets.

A general plugin system, native developer SDK, commercial plugin distribution
and the proposed Wicked shader-editor integration have been discussed. They are
**design proposals**, with no implementation acceptance or V1 marketplace promise.
Any future SDK should distinguish Lua/content packages, native runtime modules,
editor extensions, shader/resource registration and standalone dependency closure.
Existing script Library packages are a foundation, not that complete SDK.

## Continuing product obligations

1. Keep creator flows simple; preserve native camera/tool gesture ownership,
   consistent Undo/Redo and clear save/error feedback.
2. Test populated real-world combinations: terrain, rivers, weather, lights,
   effects, gameplay and assemblies at matched camera/quality/VSync settings.
3. Preserve project data across updates and keep package/runtime behaviour
   aligned with Studio. No build distribution without owner approval.
4. Continue the Alpha Playability objective: a representative interactive game
   that can be authored, reopened and independently packaged. Integrated
   subsystems alone do not close that objective.
5. Maintain current summaries and feature statuses at each merge. Preserve failed
   and historical evidence; never promote a missing test to PASS.

The detailed combat design remains [PLAYER_ARMS_COMBAT_FRAMEWORK](PLAYER_ARMS_COMBAT_FRAMEWORK.md).
[MASTER_PLAN](MASTER_PLAN.md) is the July planning baseline, not today's progress.
The [previous roadmap snapshot](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/blob/618b2310ec7d6a7889d490e93304e39043f7e5ff/docs/ROADMAP.md) retains the
complete historical checkpoints and original obligations without presenting its
old “current main” header as current guidance.
