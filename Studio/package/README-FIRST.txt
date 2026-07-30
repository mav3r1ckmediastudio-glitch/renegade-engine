RENEGADE PHASE 3 PROJECT HUB CHECK
==================================

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
9. Move a selected entity and confirm Undo, Redo, Save As, and Reopen still
   work. Reopen must not preserve a cyan material or stencil on the asset.
10. Click PROJECTS. Confirm the new project appears as a recent-project card.
11. Select the card, click LAUNCH PROJECT, and confirm its scene reopens.
12. Close Studio, reopen DX12, and confirm the recent-project card persists.
13. Close Studio, run Run-RenegadeStudio-Vulkan.cmd, and repeat steps 6-8 and
    10-11.

The generated project must contain:

  <Project Name>.renegade
  Content\Scenes\Main.wiscene
  Saved\

Report the first failed numbered step, or report:

DX12 HUB PASS / VIEWPORT SELECT PASS / OUTLINE PASS / CAMERA PASS /
RECENTS PASS / REOPEN PASS / VULKAN PASS
