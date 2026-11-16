# Custom dialogue displays

The conversation and its appearance are separate. The Dialogue Editor's display
selector chooses the **project-wide** game display used by every conversation,
including conversations started from Lua. Existing projects keep the built-in
display until a creator chooses an editable display.

## Creator workflow

1. Open **Add > Dialogue Editor**. Beside the conversation name, click
   **Customise display**. This creates a project-owned template and opens it in
   the existing UI editor. Choose **Create a new display...** in the selector to
   make another independent design.
2. Select the background, speaker name, dialogue text or a reply in Layers/the
   canvas. Use **STYLE / FONT** to change fonts, sizes, colours, borders, opacity
   and hover/pressed/focused states. Apply the typography/style edit in its
   Inspector page. Use **CHANGE GRAPHIC** for artwork. Move and
   resize elements with the normal canvas/Inspector controls. Font/image imports
   remain governed project Content resources.
3. Move/resize **Dialogue background** to position the whole dialogue group.
   Children retain their relative layout. The full design canvas determines its
   position in the game. The conversation preview uses a readable close-up of
   the display; the UI editor shows the complete artboard.
4. Save, or let the normal UI-editor autosave settle, then use Back. This returns
   to the Dialogue Editor with the unfinished conversation and its history still
   present. The preview reloads the saved display. **Edit display** reopens it.
5. Use **Test in game** as usual. The chosen layout, artwork and fonts travel
   into the isolated launch snapshot. Restart an already running Test Game to
   use newly saved changes. Choosing **Built-in display** restores the standard
   layout without deleting the custom design.

The display selector saves its project setting immediately. Conversation text
keeps its existing Save/Discard workflow. With a custom display selected,
Advanced's built-in text-size/opacity fields are disabled: edit those properties
in the UI editor so there is one clear source of appearance.

## Dialogue element roles

Template elements already have roles; changing names, displayed sample text,
artwork, fonts or geometry preserves those roles. Real conversation content
replaces sample content automatically. No code or text placeholders are needed.

| Role | Game behaviour |
| --- | --- |
| speaker | Current NPC speaker name. |
| text | Current NPC line. |
| reply1–reply4 | Up to four current player replies. |
| continue | Appears when the line has no player replies. |
| previous / next | Appears when another page of replies is available. |
| close | Closes the conversation. |

For a replacement Text/Button, use the basic Inspector's **ROLE** control.
Each role must occur exactly once; additional artwork and decorative Text may
have no role. The four reply slots page through longer reply lists. Unused slots
are hidden automatically. Dialogue controls use the authored focus order and the
existing mouse, keyboard/controller navigation and gesture isolation.

Missing/duplicate roles and hidden parent containers produce repair messages.
Save/navigation is blocked for an incomplete role-bearing template so an
accidental deletion cannot silently remove a required control. Undo restores it.
Dialogue-role elements cannot also carry conflicting attribute bindings, local
click actions or linked component behaviour. Ordinary local style/theme editing
is supported; dynamic portraits and unrelated game actions are outside this UI.

## Persistence and runtime

Templates are normal Screen v3 documents, with additive `dialogue_role` widget
metadata. The assignment is project-owned in
`Content/Gameplay/Dialogue.renegade-ui` and identifies the Screen by stable ID
plus a repairable path hint. UI creation/selection registers both the setting and
the Screen in explicit packaging roots. The existing Screen dependency provider
follows fonts, images, themes and component source Screens. Test Game explicitly
stages the same closure and rejects missing resources.

Runtime resolves the selected display once per scene lifecycle. It fills a copy
of the authored Screen for each conversation step, preserving its styles and
geometry. It uses the shared ScreenRenderer and Voice mixer; UI changes do not
change outcomes, dialogue memory or audio assignment. An invalid unavailable
display logs a diagnostic and keeps dialogue playable with the built-in display;
Test Game staging rejects an invalid authored display so creators can fix it.
