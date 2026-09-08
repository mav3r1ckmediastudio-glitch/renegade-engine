# Renegade Engine — Current Handoff

**Date:** 7 September 2026  
**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`  
**Current implementation baseline:** S6 — Library Adoption & Package Closure,
merged as PR #143 (`708030996ea748e5251958a995e1bd69192ae10f`)  
**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Current status

Renegade is in **Phase 6 — Playable Core**. The old Phase 6 Gate 3/audio handoff
is historical: Gate 3 merged in PR #125. Since then the scripting programme has
advanced through S1-S6 and the diagnostics/runtime bridge has been substantially
expanded.

The final S6 branch was intentionally delivered as one end-to-end integration
rather than separate S6A-S6F PRs. Its final head passed:

- Windows baseline run **1688** — success; and
- Renegade Studio run **1102** — success.

It then merged to `main` as PR #143.

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

If S6 owner evidence has not already been recorded against the final artifact,
use the integrated acceptance checklist in the S6 contract and verify:

1. Creator Library rows appear beside the existing ACTION/SCRIPT/GLOBAL SCRIPT
   source workflow.
2. ADD performs first-use adoption and normal Undo/Redo-backed attachment.
3. Save/reopen preserves adopted state and project authority.
4. A clean installed-package update refreshes the adopted binding correctly.
5. A creator-modified adopted file is not overwritten by a package update.
6. Test Level runs the project-owned adopted script closure.
7. Build Game packages and runs the same project-owned closure without requiring
   the installed Library.
8. The Diagnostics surface remains usable while exercising the Runtime path.

Do not infer creator acceptance from compilation alone.

## Next engineering work

Once the S6 visible acceptance evidence is recorded where needed, the next
bounded Playable Core sequence is:

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
- [`docs/LIVE_DIAGNOSTIC_ACCESS.md`](docs/LIVE_DIAGNOSTIC_ACCESS.md) — live diagnostics architecture/access.
- [`docs/FEATURE_MATRIX.csv`](docs/FEATURE_MATRIX.csv) — capability evidence ledger.
- [`docs/AI_WORKFLOW.md`](docs/AI_WORKFLOW.md) — implementation/handover rules.

Historical gate narratives belong in their gate/contract documents and Git
history. This file intentionally records the **current** handoff only.
