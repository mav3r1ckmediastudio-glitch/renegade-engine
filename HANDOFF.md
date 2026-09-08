# Renegade Engine — Current Handoff

**Date:** 8 September 2026
**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`
**Merged baseline:** S6 — Library Adoption & Package Closure in PR #143
**Active candidate:** S7 — Stock Actions in PR #144 on
`scripting/s7-stock-actions-wave-a`
**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Current status

Renegade is in **Phase 6 — Playable Core**. The old Phase 6 Gate 3/audio handoff
is historical: Gate 3 merged in PR #125. Since then the scripting programme has
advanced through S1-S6 and the diagnostics/runtime bridge has been substantially
expanded.

The rebased S7 baseline `3e679ec` passed all four Windows Debug/Release checks.
Owner testing then exposed three connected completion defects: packaged builds
could report no scripts after working-directory changes; imported entities could
block Action authoring because they lacked Renegade identity; and the Action
picker crowded or overlapped its REFRESH/ADD buttons. Those defects and the
missing in-game interaction prompt are repaired in the current working
candidate. One final exact-head Windows matrix and packaged owner pass remain.

## S7 completion candidate

- exactly six representative vanilla Lua Actions remain in scope: Sliding Door,
  Interaction Switch, Player Trigger Zone, Proximity Pickup, Activation Relay
  and Play Sound;
- built-in library discovery is executable-relative and the Windows package
  build now rejects a missing manifest or a count other than six Lua Actions;
- the Action catalogue loads even when the selected imported object has no ID;
  pressing ADD assigns every missing Scene identity through shared Undo/Redo;
- the picker has a full-width row and its labels elide instead of drawing over
  controls;
- Runtime exposes one bounded `renegade.ui.show_prompt` seam; Door and Switch
  show nearby E prompts and Pickup can optionally require E;
- Sliding Door supports direct interaction and optional Auto Close while
  retaining open/close/toggle event control; and
- the real shipped Door source is included in the Runtime integration proof.

Setup and acceptance:
[`docs/SCRIPTING_S7_STOCK_ACTIONS_OWNER_TEST.md`](docs/SCRIPTING_S7_STOCK_ACTIONS_OWNER_TEST.md)

## What is now on main

### Gameplay foundation

- Player Start and first-person Runtime possession.
- Wicked/Jolt player capsule, movement, mouse look, sprint and jump.
- Governed gameplay input map plus Pause/Resume and deterministic Reset.
- Native global/2D and positional 3D audio authoring and Runtime lifecycle.
- JP01 Jolt physics authoring/runtime foundation.

### Scripting S1-S6

- extensible Inspector section/provider architecture;
- durable `.rscripts` document and script-source model;
- governed Runtime-owned Lua lifecycle;
- restricted metadata evaluation;
- ACTION and SCRIPT attachment authoring;
- generated typed script properties;
- governed entity references;
- GLOBAL SCRIPT authoring;
- generation-safe entity/transform gameplay API;
- governed gameplay lifecycle;
- bounded cross-script event dispatch;
- structured scripting diagnostics;
- Studio/Test Level IPC and handshake diagnostics; and
- installed Creator Library packages with deterministic dependency closure,
  transactional first-use adoption, update/conflict protection and Build Game
  closure.

### Live diagnostics

PR #141 integrated the built-in live diagnostic path for running Studio and
Runtime, including script/audio state and bounded local transport. PR #142 then
closed the structured Studio-Test Level IPC/handshake seam. Diagnostics are meant
to be visible from Renegade's Diagnostics surface; the external local reader is
an additional engineering access path, not the product UI.

## S6 creator contract

Installed packages are immutable inputs. Selecting a compatible Creator Library
row and pressing **ADD** adopts the selected entry's complete deterministic
closure into the project under:

`Content/Scripts/Library/<package-id>/...`

After adoption the project copy is authoritative. Test Level and Build Game use
that same project-owned closure. Runtime never searches or executes directly
from the installed Library.

Creator-edited adopted files are preserved. A conflicting installed-package
update reports a structured conflict instead of overwriting creator bytes.

Canonical contract:
[`docs/SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE.md`](docs/SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE.md)

## Immediate owner-visible verification

Use the final PR #144 Studio artifact and follow the S7 owner guide. At minimum:

1. Open the same project/imported crate that previously showed no persistent
   identity. Expand ACTION and confirm six stock rows appear.
2. Exercise the dropdown plus REFRESH and ADD without overlap or dead controls.
3. Attach Sliding Door directly to any nearby object. In Test Level confirm the
   prompt appears and E opens/closes it.
4. Save/reopen and repeat Test Level.
5. Run the packaged Studio after an Open/Save dialog and confirm the six stock
   Actions remain discoverable.
6. Run packaged Build Game and confirm the same adopted Action behavior.

Do not infer creator acceptance from compilation alone.

## Next engineering work

Once S7 exact-head CI and visible acceptance are recorded, the next bounded
Playable Core sequence is:

1. **Objective and interaction vertical slice** — one reusable scripted objective
   loop using the governed script properties/references/events/gameplay APIs.
2. **Navigation and actor path queries** — stable Renegade access over Wicked
   voxel/pathfinding and one Runtime navigation proof.
3. **Integrated Phase 6 acceptance** — reopen, Test Level and independently
   packaged playable slice with player, collisions, audio and scripted objective.

A viewport-placeable trigger/volume system should only be introduced as one
shared ZoneService for objectives, audio, weather and later systems. Do not
reintroduce the abandoned audio-only zone implementation.

## Known deferred boundaries

- Shared ZoneService / reusable trigger volumes.
- Creator-facing VSync control.
- Physical controller owner evidence where no controller hardware is available.
- Player arms, weapons, combat and production enemy AI.
- Advanced animation/simulation work governed by later phases.
- Commercial redistribution/release packaging clearance.

## Canonical references

- [`README.md`](README.md) — product/build entry point.
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — current programme sequence.
- [`docs/MASTER_PLAN.md`](docs/MASTER_PLAN.md) — long-range programme.
- [`docs/SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE.md`](docs/SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE.md) — current scripting closure and acceptance.
- [`docs/SCRIPTING_S7_STOCK_ACTIONS_OWNER_TEST.md`](docs/SCRIPTING_S7_STOCK_ACTIONS_OWNER_TEST.md) — S7 creator setup and exact owner acceptance.
- [`docs/LIVE_DIAGNOSTIC_ACCESS.md`](docs/LIVE_DIAGNOSTIC_ACCESS.md) — live diagnostics architecture/access.
- [`docs/FEATURE_MATRIX.csv`](docs/FEATURE_MATRIX.csv) — capability evidence ledger.
- [`docs/AI_WORKFLOW.md`](docs/AI_WORKFLOW.md) — implementation/handover rules.

Historical gate narratives belong in their gate/contract documents and Git
history. This file intentionally records the **current** handoff only.
