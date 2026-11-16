# Dialogue editor candidate — 15 November 2026

Base: main `7093cf4`. Isolated branch: `feature/dialogue-editor-20261115`.
This is an unmerged implementation candidate. Windows Release, native editor,
Test Game dialogue and standalone save/load evidence is recorded for code
`4122820` in [the native review receipt](evidence/dialogue-editor/README.md).
Independent acceptance remains open. The Test Game save issue was fixed and
verified separately at `63cc73d`; the receipt preserves both revisions.

## Creator workflow

Open **Add > Dialogue Editor**. Three numbered sections show the complete workflow:

1. **Write the conversation**: select a line to edit it, add player replies, then
   select a reply and **Add response** to write and connect the NPC's answer.
   Reply rows show their destination or explicitly say that they end the conversation.
   New creates a single text-only opening line; the existing Gate Guard is an example.
   An unused NPC line is connected by selecting a reply and choosing that line in
   **Continue to**. The basic example's choices have fixed responses: no hidden
   permit flag changes the route of another sample reply.
2. **Try the replies**: click choices in the preview. An ended or stopped branch
   keeps a visible end card and Restart button instead of leaving an empty panel.
3. **Attach to an NPC**: choose an NPC or scene object in the editor's object list.
   **Save & attach** saves the conversation, assigns it and saves the current level.
   **Test in game** performs those same saves and starts the normal Test Game.
   Approach the chosen NPC within three metres and press Interact (E by default).
   The level must have been saved once before this launch action is enabled.

Tools and fields provide hover tooltips. Buttons have distinct hover, pressed
and disabled appearance; activation follows one mouse-release gesture. No script
is needed for ordinary lines and replies. Assignment uses normal scene Undo/Redo.
Conversation edits use their own bounded Undo/Redo and explicit Save/Discard.

**Advanced** reveals outcomes, conditions, actions and one-time replies in the
same script workspace. Turning it off preserves the data; conditional replies
retain a quiet summary and a Show in Advanced button.
Conditions support flag equality, minimum values and player inventory item counts.
An outcome can match all or any conditions. The final unconditional Otherwise is
mandatory. Outcomes are evaluated in order. Add Outcome inserts before Otherwise.
Flags and values are project-session dialogue memory; flags use integer 0/1.
An event action sends `dialogue.action` with a `key:value` payload through the
existing governed creator event queue. This is the script integration point;
there is no claim of built-in quest, gate, item-giving or reputation services.
Runtime accepts one event per outcome to preserve atomic queue admission.

Preview uses the same conversation evaluator and **ScreenRenderer** as Runtime.
The optional permit checkbox appears for authored has_permit rules and changes only isolated preview memory. Preview
events are simulated without changing the live scene. Preview starts when
the editor opens. Valid edits refresh the preview without Save, preserving the
current line, test memory and voice visit. Unfinished fields retain the last
valid preview and show a hint; deleting the displayed line returns to the opening.
Restart explicitly resets test progress. Saving and Test in game persist assets;
an already running Test Game uses its isolated launch snapshot.

The owner rejected the original turquoise outline UI. The revised workspace
uses Studio's opaque dark palette and orange accent, native default font family,
16-point headings, 14-point controls and 18-point script rows. The Advanced button
adds rules in place. Row drawing, picking and scrolling share one geometry;
section backgrounds are painted before controls. New in-game panels default to
opaque with orange accents. Presentation font size and optional in-game opacity
are under Advanced; neither changes the editor shell.

Select an NPC line and use **Add voice clip...** to browse local WAV/OGG recordings.
The recording is validated, copied into Content/Audio/Dialogue without overwriting
other clips and attached to that line. **Open voice folder** exposes the project's
recordings; after adding files externally, **Refresh clips** updates the list.
Choose **No voice (text only)** for silent lines. New lines are text-only, and
editing text does not generate or replace audio. **Play this line** auditions the
selected section; Restart returns to the opening. The same clip plays
in the editor and Runtime through the existing Voice mixer bus. Changing line or
closing the conversation stops the previous clip; pagination does not restart it.
Silent lines remain valid. Missing clips/audio devices leave text usable and log
the audio problem. Save registers clip dependencies for packaging; Test Game
copies them into its project snapshot. Dialogue format 2 adds optional voice paths
and retains format-1 reading. Voice references cannot leave project Content.

## In-game behaviour

An assigned NPC/object within three metres offers a prompt naming the project's
actual Interact key, for example **Press E to talk to Gate Guard**. Player Start
markers are excluded from NPC selection and Runtime dialogue targets. Interact
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

Portraits, cinematic camera tracks, gestures, timed replies, custom
Screen layout assignment, global UI themes, a graph view and general Lua dialogue
memory APIs remain future work. The current Screen is generated through native
Screen widgets with authored font size/opacity, not an independently editable
Screen asset. No unsupported controls are advertised as working.

Required native evidence: editor visual/layout and interaction; Save/Discard and
Undo/Redo; assignment save/reopen; both permit paths; input/cursor restoration;
Test Game; dependency-closed standalone package; quick-save/quick-load with and
without inventory. Automated checks alone do not establish these acceptances.

Test Game now stages saved scene identity independently of script attachments.
The script-free native F5/F9 dialogue-memory check passed at `63cc73d`. Unsaved
Levels without a scene identity remain playable, but cannot quick-save. Packaged
single-level save/load also passed; see the receipt for the tested revisions.

For local Windows review, configure with the normal embedded-shader setting and
build `RenegadeDialogueReview` in Release. This aggregate builds Studio, Runtime,
the four focused tests and the disposable project author in one MSBuild graph.
The author accepts only a fresh directory named `dialogue-proof-20261115`.
