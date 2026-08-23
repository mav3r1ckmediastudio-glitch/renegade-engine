# Story Flow Gate 8D — Creator Controls

## Outcome

Gate 8D turns the accepted Gate 8C Screen Editor shell into the creator-facing Screen authoring surface. The Screen document remains the semantic source of truth, `ScreenAuthoringSession` remains the mutation/history/persistence authority, and the shared Gate 8B Screen renderer remains the only Runtime/editor presentation path.

Gate 8D does **not** create a Gate 8F and does not redesign Story Flow. Gate 8E remains Story Flow outcome + packaged standalone parity closeout. Journey View UI/UX remains Gate 9.

## Complete Gate 8D boundary

Gate 8D owns:

- create/delete Text, Button, Image and Background elements;
- duplicate, authored Back/Front layer ordering and stable selection identity;
- governed project Image and Font selection;
- broader creator controls implemented as editable Screen primitives/presets rather than new hidden Runtime widget types;
- reusable components by duplicating authored Screen subtrees with complete stable-ID remapping;
- complete Normal/Hover/Pressed/Focused/Disabled visual-state editing;
- editable background/foreground/image tint, state image, border colour/width, corner radius and opacity;
- editable font identity, size, spacing, softness, bolden, shadow, alignment and wrapping;
- symbolic Screen action identity authoring and typed Button action selection;
- creator-authored Button focus order;
- direct preview manipulation: drag elements to move and drag corner handles to resize;
- parent selection plus Absolute/Anchored layout and all four anchor coordinates;
- the same Screen-specific Undo/Redo, transactional Save/Open and dirty-return guarantees established by Gate 8C.

Every user-facing Screen value remains serialized and editable. Runtime styling, fonts, actions or layout defaults must never silently override authored values.

## Creator-control architecture

### One mutation authority

Every persistent operation builds a complete candidate `ScreenDocument`, validates it, then enters the existing bounded Screen history through `ScreenAuthoringSession`. Studio does not mutate Screen documents directly.

This includes:

- basic name/text/visible/enabled edits plus direct preview geometry manipulation;
- create/delete/duplicate/layer transactions;
- full advanced style/state/typography edits;
- resource/action/parent/layout bindings;
- symbolic action add/rename/delete;
- focus-order moves;
- reusable subtree duplication.

### Exact Runtime preview

The Screen Editor continues to preview the live authoring document through the shared `screen::ScreenRenderer`. Gate 8D introduces no editor-only approximation and does not expose Wicked Editor windows.

Runtime keeps the renderer's GUI surfaces interactive. Screen Editor switches only renderer input ownership off while authoring so the shared surfaces remain the exact presentation source without competing with Studio for pointer selection, drag or resize. This applies uniformly to seeded/template widgets and creator-added widgets.

### Basic and Advanced Inspector coexistence

Gate 8C's basic Inspector remains available for fast name/text/visibility/enabled editing. Gate 8D moves normal geometry authoring into direct preview manipulation rather than X/Y/W/H entry. Gate 8D also adds an **ADVANCED** surface for style, typography and binding controls. While Advanced is open, basic Inspector mutation controls are suppressed so covered controls cannot receive the same creator action or generate a second history entry. Closing Advanced returns directly to the basic workflow.

### Governed resources

Creator Image/Font choices are projected from the accepted LC01/LP08 governed-resource state:

- LC01 imported-product provenance supplies stable source/product identity;
- LP08 resource metadata proves the product class is Texture or Font;
- the `.rasset` product and `wicked.resourcemanager` provenance must remain current and available;
- no arbitrary absolute filesystem picker is introduced.

`ScreenCreatorResourceChoice` carries the stable LP08 product `assetId` and governed `.rasset` product path. Current Screen schema v2 still serializes the renderer-compatible project-contained source path, so the path is treated by the creator catalogue as a governed resolution hint rather than as permission to browse arbitrary files. A later persistence migration to stable resource references must remain backward-compatible with existing schema-v2 Screens.

The creator picker refreshes the governed catalogue when opened, so resources imported after the Screen Editor was opened become selectable without closing the Screen.

### Symbolic actions remain separate from Story Flow routing

Gate 8D authors **symbolic Screen action IDs only**. It does not place Flow destinations, level paths or journey routing semantics in widgets.

Creators can:

- add an action ID;
- rename an action ID, atomically updating every Button reference;
- delete an unreferenced action ID;
- select which valid action a Button invokes.

Deleting an action still referenced by a Button is refused. Gate 8E remains responsible for Story Flow route/outcome parity.

### Focus order

Every Button must remain in `focusOrder` exactly once. Gate 8D allows the creator to move the selected Button earlier/later in that order independently of visual layer ordering. This is the authored keyboard/controller traversal contract.

### Reusable components and broader catalogue

Gate 8D does not invent a second Screen/component file format. Existing semantic primitives remain Image, Text and Button.

- **Panel** is an editable Image primitive preset.
- **Heading** is an editable Text primitive preset.
- **Duplicate Component** clones the selected root plus all descendants, gives every clone a fresh stable ID, remaps cloned parent links and preserves Button action/focus identity.

Preset values are starting values only. The creator can edit every resulting property.

## Validation invariants

1. Every persistent mutation validates a complete candidate Screen before entering history.
2. Newly-created and duplicated widgets receive fresh valid stable IDs.
3. A widget cannot parent itself, a descendant, a missing widget or a cyclic parent graph.
4. Reparenting or changing Absolute/Anchored mode preserves resolved design-canvas geometry.
5. Every Button retains a valid symbolic action and appears in focus order exactly once.
6. Action rename updates all Button references in the same transaction.
7. Referenced actions cannot be deleted.
8. Component duplication remaps all internal parent links and keeps any external parent of the copied root.
9. Image visibility is authored state; validation does not force it true.
10. An Image may exist before a resource is chosen. A non-empty resource remains project-contained and must come from the governed creator catalogue for normal creator workflows.
11. Every persistent advanced edit participates in Undo/Redo and Save/Open.
12. Seeded/template and creator-added widgets use the same authoring transaction and direct-manipulation path; no template widget is locked merely because it came from a Screen preset.

## Creator UX

The Advanced Inspector has three pages:

### STYLE

- Normal / Hover / Pressed / Focused / Disabled state
- per-state image
- background RGBA
- foreground RGBA
- image tint RGBA
- border RGBA
- border width
- corner radius
- opacity

RGBA values retain editable hexadecimal precision fields and are paired with a direct four-channel colour picker/swatches. Field validation identifies the specific invalid field rather than returning one generic error.

### TEXT / FONT

- governed Font selection plus explicit built-in Liberation Sans identity
- font size
- character spacing
- line spacing
- softness
- bolden
- shadow RGBA/picker
- shadow X/Y
- shadow softness/bolden
- horizontal/vertical alignment
- wrap

Pending typography choices share one draft and do not silently reset one another.

### BIND / LAYOUT

- governed primary Image selection
- Button action selection
- action ID Add/Rename/Delete
- Button Focus Up/Down
- parent selection
- Absolute/Anchored mode
- anchor min X/Y and max X/Y
- Apply binding/layout
- Panel/Heading presets
- reusable component duplication

Image, Font, Action and Parent choices open direct scrollable creator lists with current-selection highlighting rather than requiring repeated click-cycling through the catalogue.

## Automated proof

`RenegadeScreenAuthoringSessionTests` covers the accepted Gate 8C baseline and Gate 8D creator/advanced transactions, including:

- basic absolute and anchored edits;
- invalid/no-op mutation history behaviour;
- Text/Button/Image creation;
- Button duplication and focus identity;
- Back/Front layer operations;
- hidden/unassigned Images;
- delete cleanup and creator Undo/Redo;
- authored values across visual states and typography;
- legal reparent + Absolute/Anchored geometry preservation;
- self/descendant parent rejection in the mutation authority;
- nested reusable component duplication with fresh IDs and cloned parent remapping;
- component Button action/focus preservation;
- component Undo/Redo;
- action Add/Rename/Delete reference integrity;
- creator focus-order moves;
- direct move/resize gesture coalescing into one Undo entry;
- renderer input mode defaults to Runtime-interactive and can be suppressed/restored for authoring;
- transactional Save/Open preserving advanced style, binding, component, action, focus and geometry state.

Windows Debug/Release CI remains authoritative for compilation and regression proof.

## Owner acceptance — complete Gate 8D

1. Open a governed Screen containing seeded/template controls such as `NEW GAME` and `LOAD GAME`.
2. Select a seeded/template element in the preview and drag it; it must follow the pointer live.
3. Use all four corner resize handles on that seeded/template element and confirm live resizing.
4. Create new Button, Image and Text elements and repeat move/resize. Seeded template elements and creator-added elements must behave identically.
5. Confirm normal X/Y/W/H coordinate-entry fields are gone.
6. Undo/Redo a move and resize; each full gesture is one Undo step.
7. Parent an element and switch Absolute/Anchored mode; direct movement must not jump.
8. Recheck Advanced style/state, colour, typography/font, governed resource, action, focus, parent/layout, presets and component controls.
9. Save, return to Story Flow, reopen and confirm geometry plus authored state persists.
10. Confirm Runtime preview parity and Hub → Story Flow → Screen/Level navigation remain intact.

Gate 8D is not merge-ready until exact-head CI and this owner audit pass.
