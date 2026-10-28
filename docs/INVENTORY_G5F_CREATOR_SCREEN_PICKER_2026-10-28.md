# Inventory G5F: Project Inventory Screen selector (2026-10-28)

Status: isolated feature branch implementation; retain owner-approved G5E packaged proof and do not merge before approval.

## Developer workflow
1. Create or open an authored inventory grid Screen through the normal Screen Editor. A suitable Screen has Usage = Overlay, Enabled in Game = OFF (the Runtime enables its gameplay copy), and native inventory slot actions. Do not turn it into a normal Story Flow menu.
2. In the Screen Editor, switch the toolbox to **Globals**. Expand **Project**, then choose **Inventory Screen**.
3. The compact picker lists all available authored Overlay Screens with Enabled in Game OFF, including Screens not linked to Story Flow. Choose the desired inventory Screen.
4. The project-level assignment is journaled to Content/Gameplay/Inventory.renegade-ui using a stable document identity and safe project-relative Screen path. The assigned Screen, its graphics and the shared item catalogue are included in production Build Game dependency closure.
5. To use a different look, select another authored Overlay in the same picker. To return to the basic Runtime inventory, select **Use basic inventory (no custom Screen)**. Clearing the assignment does not delete the designer's Screen or any inventory data.

## Technical invariants
- The selected Screen must exist inside Content/UI, have an exact stable project/Screen ID, be a disabled Overlay, and contain native inventory slot actions. Validation is repeated by Studio, the package dependency collector and Runtime. Missing or invalid choices fail without destroying a previous valid assignment.
- Uses ProjectDocumentTransaction's journaling/rollback and project boundary for writes/deletions.
- No replacement or mutation of any authored Screen, WISCENE, inventory companion or live game inventory. Existing .rinvui previews remain development-only.
- G5F Reader/Writer tests include create/change, invalid input preservation, clear, repeated clear, document identity, usage and path checks.
- Real in-game potion use after a damaging live gameplay event remains a manual acceptance item; the production combat-damage-to-use regression passed in G5E.
