# LP03 — Minimal Runtime Screen and Stable Actions

## Status

Accepted implementation on branch `poc/lp03-minimal-runtime-screen`, draft
PR #19.

- Branch base: `f578c849896edc4048fa931ac658c339c6efb3e7`
- Accepted code commit: `288dc91bba8e0e184dd8ac4fbc3e2ba224ad3f53`
- Wicked pin: `3a800b7134aafe58461093c8abb2e274d4e64033`
- GitHub Actions: run #92, Debug PASS and Release PASS
- Release artifact: `renegade-studio-windows-x64-Release-288dc91bba8e0e184dd8ac4fbc3e2ba224ad3f53`
- Release artifact SHA-256: `23ea9b8657fb93ef01f2a79031190792d55b6f6530c8d206f1f329de918b9c2b`

Physical gamepad input is deferred because no controller was available.
Automated tests cover gamepad-labelled requests entering the same stable action
dispatcher. The hardware route is therefore recorded as untested, not passed.

## Purpose

LP03 proves that a Renegade project can own a serialized startup screen that
appears before the existing LP02 Story Flow, accepts common input routes and
dispatches stable Runtime actions without embedding the Wicked Editor or
inventing a parallel application lifecycle.

This is deliberately smaller than a UI Designer. It establishes the document,
resolution, presentation, focus and action boundaries that later Runtime UI
authoring can build upon.

## Serialized document

The `runtime-screen` document uses the Renegade document envelope and contains:

- stable document and owning-project IDs;
- mutable project-relative path hint;
- design width and height;
- bounded action records;
- bounded image, text and button widgets;
- stable widget IDs;
- authored back-to-front order;
- deterministic focus order.

LP03 permits exactly the stable actions `play` and `quit`. The accepted proof
requires one background image, at least one text widget and exactly one Play and
one Quit button.

Image resources must remain inside the owning project's `Content` tree and must
exist. Malformed documents, duplicate action/widget IDs, unknown actions,
invalid focus targets, missing required controls, missing resources, unsafe
paths and ambiguous stable-ID resolution fail closed.

## Project reference

The project descriptor may contain the optional paired fields:

- `startup_screen_id`
- `startup_screen`

The stable ID is authoritative. The path is a project-relative discovery and
diagnostic hint that may change when content is moved.

Projects without a startup-screen pair retain the pre-LP03 LP01/LP02 immediate
startup behaviour.

## Runtime presentation

Runtime uses its existing Wicked application and `RenderPath3D`/`RenderPath2D`
seam. No second window and no stock Wicked Editor are introduced.

Renegade owns:

- current focus and focus order;
- next/previous navigation;
- initial Play focus;
- hidden and disabled control skipping;
- pointer focus;
- confirm/debounce;
- input-source evidence;
- stable action dispatch.

Wicked `wiGUI` renders its widget storage in reverse. Runtime inserts the
authored back-to-front document in reverse so the background is drawn first and
foreground title/buttons remain visible.

## Stable actions

All activation routes create `RuntimeActionRequest` and enter the same
`RuntimeActionDispatcher`.

### `play`

`play` calls the existing LP02 `LoadRuntimeProjectFlow()` route. The menu is
removed only after LP02 successfully enters the startup flow. A failed Play
attempt leaves the menu available.

### `quit`

`quit` sets the Runtime-owned shutdown request. The Win32 host destroys the
normal application window, reaches `WM_DESTROY`, posts the quit message and
shuts down through the existing lifecycle.

## Automated evidence

The accepted GitHub Actions run compiled and tested both Debug and Release.

Dedicated coverage:

- `RenegadeScreenTests`
- `RenegadeRuntimeScreenTests`
- existing `RenegadeRuntimeFlowTests`

Coverage includes serialization, validation failures, stable-ID resolution,
resource containment, deterministic focus, hidden/disabled skipping,
keyboard/mouse/gamepad-labelled action requests, duplicate action registration
and unknown-action structured failure.

## Packaged evidence

The project owner tested the Release artifact built from `288dc91bba8e0e184dd8ac4fbc3e2ba224ad3f53` on DX12.

### Visual startup

PASS:

- dark Renegade background visible;
- `RENEGADE RUNTIME` title visible;
- Play and Quit visible;
- Play initially focused in cyan;
- startup cube withheld until Play.

### Mouse

PASS:

- Play entered LP02 Level One;
- Quit requested normal shutdown;
- exit code 0;
- Runtime log recorded `last_action_input=mouse`.

### Keyboard

PASS:

- focus navigation reached both actions;
- Play entered LP02 Level One;
- Quit requested normal shutdown;
- exit code 0;
- Runtime log recorded `last_action_input=keyboard`.

### Gamepad

Automated dispatcher coverage: PASS.

Physical controller mapping: DEFERRED because no gamepad hardware was available.
A future hardware check should verify D-pad/left-stick navigation and primary
confirm activation without changing the accepted action boundary.

## Scope limits

LP03 does not implement:

- Studio screen authoring;
- a general UI Designer;
- responsive anchors or layout rules;
- HUDs;
- custom font schema;
- scripting or data binding;
- video;
- cooking;
- standalone-game packaging;
- LF02 transactional project saves.

Those belong to later lifecycle slices.
