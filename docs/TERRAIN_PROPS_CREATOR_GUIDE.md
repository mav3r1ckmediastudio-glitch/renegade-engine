# Terrain props in Renegade Studio

Use terrain props to fill a finite terrain with repeated static models. A mix can contain trees, shrubs, rocks and small ground cover. Each entry has its own density and scale range. The project keeps the placements you have authored when you save and reopen the level.

## Make a woodland

1. Open a level containing a finite terrain. Import your static models into the project so they appear in the model picker.
2. In the **Terrain** workspace, expand **Terrain Props**. Choose a model and click **Add Model to Mix**. Repeat for the other species. Use **Refresh Models** if you imported a model while the palette was open.
3. Set **Density / m2** for each row. For example, enter `0.01` for roughly one candidate per 100 square metres before spacing and exclusions. **Minimum Spacing** limits how closely candidates can grow. Use the same **Seed** to repeat a layout with the same inputs.
4. Open **Prop Settings** on a tree. Choose **Trunk Capsule** and fit it to the trunk as described below. Choose **Rock / Boulder** for a solid rock. Small decorative plants can use **None**.
5. Click **Generate Apply** to populate the terrain. Use **Paint** to add props along a brush stroke or **Erase** to open a clearing. Set **Brush Radius** before dragging. A completed stroke is one Undo step.
6. Name the mix and click **Save Mix** to keep the recipe with the layer. Save the level to preserve your edits. Use **Test Game** to walk through the trees and check collision, viewing distance and shadows from Player Start.

## Fit a prop to its model

| Control | How it affects the level |
| --- | --- |
| **Trunk Capsule** | Set the local centre on the trunk. X extent is the radius. Y extent is half the straight section between the rounded ends. The canopy can overhang the player. |
| **Rock / Boulder** | Fit a box using local centre and half extents. Placement scale also scales the collision box. |
| **Focus Collision Preview** | Centres the view on a sample placement. Paint or generate a placement first. |
| **Minimum Scale / Maximum Scale** | Set the uniform scale range used when generating or painting placements. |
| **Draw Distance (m)** | Sets the maximum viewing distance at unit scale. A value of zero leaves the range unlimited. A larger placement can remain visible farther away. |
| **Apply Prop Settings** | Commits collision and draw-distance changes to existing placements. Generate applies a revised scale range to procedural placements. |

## Edit the layout

- To keep a path clear, erase its footprint. Future Generate and Paint operations respect the clearing. Undo the erase to restore that area.
- After sculpting the ground, use **Reproject to Terrain** to update placement heights. After expanding the terrain, use **Generate Apply** to populate the added area.
- To try a different mix, select a saved recipe and apply it with Generate or a brush stroke. Use **Cancel / Stop Brush** to discard a stroke or generation still in progress. Use Undo for a completed edit.

## Check the game build

Save the level. In Story Flow, connect Game Start to the woodland level and give it a route to Complete Game. For a single-level game, the level.complete outcome can lead directly to Complete Game. Build Windows Game checks that it can follow this route. Use the Release editor’s **Build Windows Game** command. Move the complete exported game folder to a separate location and launch its game executable. Walk past the trees and boulders, then turn back. For models with registered near and far versions, check the crown and the cast shadow during both passes. The supplied near and far meshes determine the two settled silhouettes.

For placement storage and authoring history, see [Terrain props P2](TERRAIN_PROP_P2.md). For collision fitting and gameplay queries, see [Terrain props P3](TERRAIN_PROP_P3.md).
