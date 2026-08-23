# Story Flow Gate 8D — Packaged Owner Audit

This package is the owner-acceptance candidate for Gate 8D creator controls. Green CI is necessary but is **not** visual/editor acceptance.

## Before testing

1. Launch `Run-RenegadeStudio-DX12.cmd`.
2. Create/open a project and enter Story Flow.
3. Open a governed Screen node into the Screen Editor.
4. Stop immediately at the first mismatch and report the exact step.

## A. Creator controls

1. Confirm visible controls for `+ TEXT`, `+ BUTTON`, `+ IMAGE`, `+ BACKGROUND`, Duplicate, Delete, Back and Front.
2. Create each element type. Confirm the hierarchy and exact shared Runtime preview update immediately.
3. Duplicate a Button. Confirm the duplicate is independently selectable/editable.
4. Move overlapping elements Back and Front. Confirm visible layer order changes.
5. Create an Image before assigning a resource and hide/show it. Both states must be valid.

## B. Basic vs Advanced Inspector

1. Select an element and confirm the basic Inspector provides name/text/visibility/enabled editing; geometry is authored directly in the preview rather than through X/Y/W/H fields.
2. Click **ADVANCED**.
3. Confirm only the Advanced Inspector is interactive in the right-hand Inspector area. There must be no visible bleed-through or double Apply/input behaviour from the basic Inspector underneath.
4. Click `< BASIC` and confirm the basic Inspector returns normally.

## C. STYLE

1. Open Advanced → STYLE.
2. Edit values across Normal, Hover, Pressed, Focused and Disabled states.
3. Test background, foreground, image tint, border colour/width, corner radius and opacity.
4. Use a `PICK` button and change RGBA with the visual channel picker; Apply it.
5. Also edit the corresponding 8-digit RGBA precision field and Apply.
6. Confirm preview/runtime interaction uses the authored state values and one field does not reset another.

## D. TEXT / FONT

1. Select Text or Button and open TEXT / FONT.
2. Open the Font selector. It must be a direct scrollable list, not a click-to-cycle control.
3. Built-in Liberation Sans must remain selectable. If the project has imported governed fonts, they must also appear.
4. Edit font size, character spacing, line spacing, softness, bolden, shadow RGBA/X/Y/softness/bolden, horizontal alignment, vertical alignment and wrap.
5. Change several pending values before Apply; none may silently reset another.
6. Confirm the shared preview updates and persists the authored typography.

## E. Governed images/resources

1. Select an Image and open BIND / LAYOUT → Image selector.
2. If the project has governed imported textures, confirm they appear directly in the list with project paths; arbitrary external filesystem paths must not be offered.
3. Assign a governed image and Apply.
4. Select a Button and assign a governed per-state image on STYLE where available.
5. Reopen a resource selector after importing another governed resource during the session; it must refresh rather than require closing the Screen Editor.

## F. Actions

1. Select a Button and open BIND / LAYOUT.
2. Open the direct Action selector.
3. Enter a new symbolic ID such as `open_audio_settings` and click `+ ACTION`.
4. Bind the selected Button to it and Apply.
5. Rename that action to another valid symbolic ID. Confirm every Button reference remains valid after rename.
6. Attempt to delete an action still referenced by a Button. It must be refused.
7. Rebind away from an unused custom action, then delete the now-unreferenced action successfully.
8. No Story Flow destination/path should appear inside the Screen action editor; Gate 8D authors symbolic IDs only.

## G. Focus order

1. Ensure the Screen has at least two Buttons.
2. Select a Button and use Focus Up / Focus Down.
3. Confirm keyboard/controller traversal follows the authored saved order independently of visual layer order.

## H. Parent/layout

1. Reparent an element under another element using the direct Parent selector.
2. Confirm it does not jump on the design canvas.
3. Switch Absolute ↔ Anchored and confirm resolved visual geometry remains stable.
4. Edit anchor Min X/Y and Max X/Y and Apply.
5. Confirm invalid self/descendant parenting cannot be authored.

## I. Presets and reusable components

1. Create `+ PANEL` and `+ HEADING`.
2. Confirm all resulting values remain normal editable Screen properties; they must not behave as locked/hardcoded UI.
3. Parent Text and/or Button children under a root element.
4. Select the root and click `DUP COMPONENT`.
5. Confirm the entire subtree is copied, is independently selectable/editable and preserves Button action/focus behaviour without sharing identities with the original.

## J. Undo / Redo / Save / reopen

1. Undo and Redo representative style, typography, binding, action, focus and component operations.
2. Confirm hierarchy, preview and selection match the corresponding validated snapshot.
3. Save.
4. Return to Story Flow.
5. Reopen the Screen.
6. Confirm all creator elements, style states, typography, resource choices, actions, focus order, parent/layout state and duplicated component structure persist.

## K. Regression / parity

1. Confirm Hub → Story Flow → Screen still works.
2. Confirm Story Flow → Level still opens the 3D editor normally.
3. Confirm the Screen Editor preview uses the same Runtime Screen renderer and the 1280×720 authored design still scales correctly to the accepted output size.
4. Confirm no stock Wicked Editor windows appear.

Gate 8D is accepted only when this exact packaged Release passes the full owner audit.

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
