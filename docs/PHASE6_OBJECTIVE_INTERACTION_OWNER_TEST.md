# Phase 6 Objective + Interaction Vertical Slice — Owner Test

## Purpose

This pass proves that Renegade can author a small reusable gameplay objective
entirely through the governed creator scripting stack already merged on `main`.
It must not depend on a new special-case objective runtime.

The reference loop is deliberately simple and visible:

1. press **E** at an Interaction Switch to start the objective;
2. collect three objects with **E**;
3. see objective progress after each pickup; and
4. unlock/open a Sliding Door when the required count is reached.

The objective itself is the reusable **Objective Counter** Action from the
`Renegade Objective Actions` Creator Library package. The switch, pickups and
door are the already accepted stock Actions.

## Setup

Use a fresh level or a disposable copy of an existing test level.

Create or place:

- one object to act as the **Objective Start Switch**;
- one ordinary object to host the **Objective Counter** (this is only a logic
  host and can be placed out of sight if desired);
- three visible collectible objects; and
- one object to act as the **Exit Door**.

A Player Start must already exist.

### Objective Counter

On the logic host, add **Objective Counter** from the Creator Library. Confirm
that first use adopts the package into the project rather than executing from the
installed library.

Set:

- **Start Active:** off
- **Start Event:** `start_objective`
- **Count Event:** `pickup`
- **Required Count:** `3`
- **Completion Target:** the Exit Door
- **Completion Event:** `open`
- **Progress Text:** `Crystals`
- **Completion Text:** `Exit unlocked`
- **One Shot:** on

Leave **Message Seconds** at its default unless a longer owner-test display is
helpful.

### Start switch

On the start-switch object, add **Interaction Switch** and set:

- **Target:** the Objective Counter host
- **Event:** `start_objective`
- **Prompt Text:** `Press E to start objective`
- **One Shot:** on

### Pickups

On each of the three collectible objects, add **Proximity Pickup** and set:

- **Target:** the Objective Counter host
- **Pickup Event:** `pickup`
- **Require Interact:** on
- **Prompt Text:** `Press E to collect crystal`
- **Hide On Pickup:** on

Keep each pickup far enough apart that only one is within pickup range at a time.

### Exit door

On the Exit Door object, add **Sliding Door** and set:

- **Direct Interaction:** off
- **Open Event:** `open`

Choose a visible **Open Offset** appropriate for the test object.

## Test Level acceptance

Save the level, close it, reopen it, and confirm all Action assignments,
properties and entity references remain intact before running Test Level.

In Test Level:

1. Approach the start switch. `Press E to start objective` must appear.
2. Press **E**. The objective is now active; the door must remain closed.
3. Approach the first pickup. `Press E to collect crystal` must appear. Press
   **E**; the object must disappear and `Crystals 1/3` must be shown.
4. Repeat with the second pickup; `Crystals 2/3` must be shown.
5. Collect the third pickup; `Exit unlocked` must be shown.
6. The Exit Door must begin moving to its authored open position without direct
   player interaction.
7. No scripting/runtime diagnostic should report a disabled instance, invalid
   reference, missing source or dropped objective event.

A compile-only pass is not owner acceptance.

## Save/reopen and Reset behaviour

Exit Test Level, save, reopen the project/level, and repeat the loop. Authored
references must still resolve to the same creator objects.

A fresh Test Level run must start with the switch, counter, pickups and door in
their authored initial state. The one-shot state is runtime state only; it must
not leak between runs.

## Packaged Runtime acceptance

Run **Build Game** from the same saved project and launch the independently
packaged Windows Runtime.

Repeat the same switch -> three pickups -> door sequence. Prompts, event counts,
pickup hiding and door completion must agree with Test Level.

The packaged Studio artifact must contain:

`Content/ScriptLibrary/RenegadeObjectiveActions/renegade-script-package.json`

and exactly one Lua entry in that package for this slice:

`ObjectiveCounter.lua`

## Exit criteria

The slice passes only when all of the following are true:

- Objective Counter is discoverable from Creator Library and adopts into the
  project through the existing S6 workflow;
- the objective can be started by a normal player interaction;
- three governed pickup events advance one reusable objective counter;
- objective completion targets an authored entity reference and opens the stock
  Sliding Door through the normal event API;
- project save/reopen retains every authored property/reference;
- Test Level and packaged Runtime agree; and
- CI passes the real shipped-script Runtime integration test.

No shared ZoneService is required for this reference loop. Do not introduce an
audio-only or objective-only trigger-volume implementation as part of this gate.
