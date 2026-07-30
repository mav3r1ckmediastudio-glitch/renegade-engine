RENEGADE PHASE 3 EDITOR USABILITY CHECK
=======================================

This is the first production-direction Studio increment. It is not the final
Identity Handshake animation or finished editor.

1. Double-click Run-RenegadeStudio-DX12.cmd.
2. Confirm the title ends with [DX12].
3. Confirm Studio opens on the holographic PROJECT HUB over a live scene.
4. Enter a project name, click CREATE PROJECT, and choose an existing parent
   folder. Renegade creates a new folder inside it.
5. Confirm the holographic workspace opens with:
   - the Proving Ground in the viewport;
   - WORLD // HIERARCHY on the left;
   - TRANSFORM inspector on the right; and
   - CONTENT // PROJECT ASSETS at the bottom.
6. Click a visible object directly in the 3D viewport. Confirm:
   - the object receives a cyan silhouette outline;
   - the matching hierarchy row becomes selected;
   - the inspector identifies the same entity; and
   - the translation gizmo moves to that object.
7. Select a different object in the hierarchy. Confirm the cyan outline and
   gizmo move to it. Click empty viewport space and confirm selection clears.
8. Hold the right mouse button over the viewport and move the mouse to look.
   Keep holding it and use W/A/S/D to move and Q/E to descend/ascend. Hold Shift
   to move faster. Use the wheel over the viewport to change movement speed.
   Confirm these controls do not activate over the editor panels.
9. Confirm generated grid-line internals do not flood the hierarchy and the
   command bar is no longer covered by diagnostic text.
10. Edit Position, Rotation, and Scale in the Inspector. Use W/E/R to switch
    Move/Rotate/Scale gizmos and confirm the selected entity follows each edit.
11. Select Environment in the hierarchy. Confirm the Inspector changes to SKY,
    FOG, and VOLUMETRIC CLOUDS controls.
12. Apply SCATTERED, OVERCAST, and STORM. Confirm the viewport changes
    immediately, including cloud shadows where visible. Undo and redo each
    preset.
13. Edit cloud coverage, base height, thickness, fog density, and sky exposure.
    Press Ctrl+S, reopen the scene, and confirm the values persist.
14. Press F to frame several differently sized objects.
15. Duplicate with Ctrl+D and the button. Delete the copy with Delete and the
    button. Undo and redo both operations.
16. Undo and redo position, rotation, and scale edits.
17. Press Ctrl+S, close Studio, reopen the project, and confirm the saved
    transform persists. Ctrl+Shift+S must still open Save As.
18. Confirm shortcuts do not trigger while typing in a text field.
19. Click PROJECTS. Confirm the new project appears as a recent-project card.
20. Select the card, click LAUNCH PROJECT, and confirm its scene reopens.
21. Close Studio, reopen DX12, and confirm the recent-project card persists.
22. Close Studio, run Run-RenegadeStudio-Vulkan.cmd, and repeat steps 6-18 and
    20.

The generated project must contain:

  <Project Name>.renegade
  Content\Scenes\Main.wiscene
  Saved\

Report the first failed numbered step, or report:

DX12 EDITING PASS / HIERARCHY PASS / TRANSFORM PASS / ENVIRONMENT PASS /
CLOUDS PASS / CLOUD SHADOWS PASS / FOCUS PASS / DUPLICATE-DELETE PASS /
HISTORY PASS / SAVE PASS / SHORTCUTS PASS / RECENTS PASS /
VULKAN EDITING PASS
