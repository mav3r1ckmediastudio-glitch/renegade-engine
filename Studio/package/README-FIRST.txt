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
6. Select an entity, move it, and confirm Undo, Redo, Save As, and Reopen still
   work.
7. Click PROJECTS. Confirm the new project appears as a recent-project card.
8. Select the card, click LAUNCH PROJECT, and confirm its scene reopens.
9. Close Studio, reopen DX12, and confirm the recent-project card persists.
10. Close Studio, run Run-RenegadeStudio-Vulkan.cmd, and repeat steps 7-8.

The generated project must contain:

  <Project Name>.renegade
  Content\Scenes\Main.wiscene
  Saved\

Report the first failed numbered step, or report:

DX12 HUB PASS / CREATE PASS / RECENTS PASS / REOPEN PASS / VULKAN PASS
