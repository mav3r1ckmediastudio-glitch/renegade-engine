# Dialogue editor candidate — 15 November 2026

Base: main `7093cf4`. Isolated branch: `feature/dialogue-editor-20261115`.
This is an unmerged implementation candidate. Windows Release, native editor,
Test Game dialogue and standalone save/load evidence is recorded for code
`4122820` in [the native review receipt](evidence/dialogue-editor/README.md).
Independent acceptance remains open; the receipt records one Test Game save issue.

## Creator workflow

Open **Add > Dialogue Editor**. The first conversation is the Gate Guard example.
Write the NPC line, add player replies, and select a reply before using **+ NPC
Line** to write its response. Use **Continue** to reconnect to a named section.
Save the conversation, select an NPC/object, and choose **Assign to selected NPC**.
Save the Level to persist its native metadata binding. No script is needed for
ordinary lines and replies. Assignment uses normal scene Undo/Redo. Conversation
edits use their own bounded snapshot Undo/Redo and explicit Save/Discard.

**Advanced** reveals outcomes, conditions, actions and one-time replies in the
same outline. Turning it off preserves the data and leaves rule-count badges.
Conditions support flag equality, minimum values and player inventory item counts.
An outcome can match all or any conditions. The final unconditional Otherwise is
mandatory. Outcomes are evaluated in order. Add Outcome inserts before Otherwise.
Flags and values are project-session dialogue memory; flags use integer 0/1.
An event action sends `dialogue.action` with a `key:value` payload through the
existing governed creator event queue. This is the script integration point;
there is no claim of built-in quest, gate, item-giving or reputation services.
Runtime accepts one event per outcome to preserve atomic queue admission.

Preview uses the same conversation evaluator and **ScreenRenderer** as Runtime.
The permit checkbox changes `has_permit` in isolated preview memory. Preview
events are simulated without changing the live scene. Preview starts when
the editor opens; **Restart preview** resets it to the current document.

The workspace follows the native Light/Particle picker styling: fully opaque
charcoal surfaces, pale teal actions and replies, legible native text fields,
and separate conversation, properties and preview areas. It does not inherit
the older translucent Studio chrome. Section fills are drawn before controls,
so decorative surfaces cannot intercept input or cover the preview. New in-game
panels default to fully opaque. Text size is a basic presentation setting;
optional in-game opacity is under Advanced and never changes the editor shell.

## In-game behaviour

An assigned object within three metres offers an Interact talk prompt. Interact
uses the project's existing binding. The dialogue owns movement/look/combat input
until it closes; Escape closes it without also opening Pause. Mouse replies,
arrow/D-pad navigation and Enter/controller activation use shared native Screen
surfaces. Four replies appear per page, with Previous/Next for longer lists.
The current implementation uses proximity, not a line-of-sight raycast.

`.rdialogue` assets are project-owned, versioned and transactionally written in
`Content/Dialogue`. Save registers each asset in the project's existing explicit
packaging roots. Test Game stages validated conversation assets into its shadow
project. Bindings live in WISCENE metadata and survive native save/reopen.

Dialogue memory is an additional bounded payload in quick-save format 2; format-1
quick saves remain readable. Inventory-free direct-scene and simple single-level games can quick-save
using an empty native inventory snapshot. Scene identity remains mandatory. The only supported Flow shape is an
unconditional Game Start -> one Level -> Complete Game route, with no startup
menu. Branched or multi-level Story Flow resume still requires a separate journal. Quick load closes the conversation and restores remembered choices;
resuming in the middle of a spoken line is not implemented.

## Current limits and acceptance

Voice audio, portraits, cinematic camera tracks, gestures, timed replies, custom
Screen layout assignment, global UI themes, a graph view and general Lua dialogue
memory APIs remain future work. The current Screen is generated through native
Screen widgets with authored font size/opacity, not an independently editable
Screen asset. No unsupported controls are advertised as working.

Required native evidence: editor visual/layout and interaction; Save/Discard and
Undo/Redo; assignment save/reopen; both permit paths; input/cursor restoration;
Test Game; dependency-closed standalone package; quick-save/quick-load with and
without inventory. Automated checks alone do not establish these acceptances.

The current script-free Test Game snapshot omits the scene identity companion,
so quick-save there correctly fails. Packaged single-level save/load passed with
dialogue memory restored. See the receipt before making broader save claims.

For local Windows review, configure with the normal embedded-shader setting and
build `RenegadeDialogueReview` in Release. This aggregate builds Studio, Runtime,
the four focused tests and the disposable project author in one MSBuild graph.
The author accepts only a fresh directory named `dialogue-proof-20261115`.
