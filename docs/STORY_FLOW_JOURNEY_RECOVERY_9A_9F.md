# Story Flow Journey Recovery — Gates 9A through 9F

**Status:** bounded interim closeout candidate; pre-release visual revisit required.

**Approved visual reference:** `storyflow concept(3).png` supplied by the
project owner on 2026-08-24.

## Locked scope

This recovery changes Journey Flow only. Graph Flow remains synchronized with
the same authoritative Story Flow document but is frozen and is not a visual or
interaction redesign target for this sequence. The existing Graph editor remains
available through the restored Journey/Graph view switch.

The reference image is an acceptance target for hierarchy, density, card
language and fixed chrome. Its handwritten blue notes and arrows are design
annotations, not controls. Journey renders no node wires.

## Gate sequence

### 9A — Native Journey UI Foundation

- fixed 70-pixel application bar;
- fixed 96-pixel left navigation rail;
- bounded 280-336-pixel Inspector;
- Journey-default project home at 1680x945, 1920x1080 and 1280x720;
- real Select, Arrange, Filter, Search, Preview, Validate, save state,
  Undo/Redo and project controls;
- Hub, Story Flow, Levels, Screens, Assets and Test Play route through existing
  governed application/lifecycle seams;
- unavailable Variables, Settings and Main Menu surfaces are visibly disabled,
  never decorative active controls.

### 9B — Real Journey Reel, Cards and Thumbnails

- main cards are 164x214 logical pixels at 100% zoom;
- cards are rounded, shadowed, image-led surfaces;
- all unselected card borders are neutral;
- blue is selection only; validation is a small state mark;
- Level and Screen thumbnails use the governed thumbnail slot, with the largest
  visible Runtime Screen image as a read-only fallback;
- images use cover-cropping inside the card rather than scaling outside it.

### 9C — Branch Lanes and Inspector Routing

- the main lane remains neutral;
- alternate roles own the row accent and subtle row surface: purple for
  options/settings, turquoise for load/save, and red for failure/death;
- the same role classifier drives Inspector exit bullets;
- each action destination is edited directly in the Inspector;
- rewiring preserves the existing stable route ID and shared Undo history;
- adding an action uses `StoryFlowAuthoringSession`; Journey never opens Graph
  merely to configure an exit.

### 9D — Navigation, Semantic Zoom and Story Overview

- Journey zoom is constrained to 82-118%; obsolete tiny persisted zoom migrates
  to 100%;
- wheel, plus/minus and the visible slider all change the Journey canvas;
- Search focuses Journey cards, Filter hides detached tracks, Arrange fits the
  reel and Start focuses Game Start;
- the lower-left navigation host and lower-right Story Overview are fixed in
  screen space; their frames never grow or shrink with canvas zoom;
- only the overview contents and viewport indicator represent canvas state.

### 9E — Advanced Nonlinear Journey Authoring

- branch rows collapse and expand;
- Inspector route creation and stable-ID destination rewiring are functional;
- the deterministic Journey projection represents merges, loops, hubs and
  returns without duplicating semantic nodes;
- screen actions remain governed by the Runtime Screen action list;
- unsupported semantic mutations fail closed and leave the Inspector synced to
  the authoritative document.

Presentation-only chapters and automatic insertion/splicing remain separate
follow-up authoring operations; they must not be simulated by decorative UI or
by mutating Runtime semantics implicitly.

### 9F — UX Hardening, Diagnostics and Gate 9 Acceptance

- source contracts reject legacy panels, type-coloured card frames, unreadable
  zoom, zoom-scaled overview chrome and Journey-to-Graph exit routing;
- fixed-layout tests cover the approved and compact resolutions;
- project Preview saves Flow first and launches the governed project descriptor
  through the supervised Runtime process without an LP04 snapshot;
- Debug and Release GitHub CI must pass at the exact candidate head;
- owner screenshots at 1920x1080 and 1280x720 must be compared with the
  approved concept;
- green CI is not visual acceptance and the pull request remains draft until the
  owner accepts the packaged Release artifact.

## Non-negotiable visual rules

1. No coloured Level or Screen card frames.
2. No empty-box or letter-placeholder navigation icons.
3. No Journey wires.
4. No zoom-scaled minimap frame or navigation controls.
5. No legacy Level/Screen management rows beneath the Journey shell.
6. No surfaced control that is neither functional nor visibly disabled.
7. A failed owner screenshot overrides all automated build results.

## Interim closeout boundary

The 2026-08-24 owner review reduced the immediate blocking acceptance scope to:

- Journey cards and lanes are hard-clipped at the Inspector boundary;
- the fixed bottom-right Story Overview renders its miniature journey content
  and viewport indicator after card rendering;
- the existing synchronized Graph editor is reachable through a real view
  switch.

Remaining concept-fidelity work is deliberately deferred, not accepted as
complete. It must be reopened and owner-reviewed before a release candidate is
declared. CI compilation and startup remain necessary evidence, but do not
constitute that future visual acceptance.
