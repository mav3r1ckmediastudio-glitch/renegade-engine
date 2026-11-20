# Build breakable objects for Renegade

Use this workflow for a barricade, crate, barrel, or similar prop that should rattle on a light hit and come apart on a harder hit. Build the pieces in the pose they occupy before the break. Renegade uses those same meshes for the debris.

## 1. Model the pieces in Blender

1. Set the scene to **Metric** and work at the size the prop should have in game. A good first model is a barricade about **2.1 m wide, 1.5 m tall and 0.2 m deep**.
2. Make each eventual fragment a **separate mesh object**. For the barricade, start with four thick boards. For a crate, try six individual panels. Use **Edit Mode → P → Selection** to split selected geometry when starting from a joined model. Name the pieces clearly, such as `Board_01` through `Board_04`.
3. Give each piece solid thickness and finish the faces exposed by the break. Check normals and materials on the back, edges and interior. Leave a small gap between neighbouring pieces where you can. Overlapping pieces can cause them to kick apart abruptly as soon as their collision turns on.
4. In Object Mode, apply **Rotation & Scale** to the mesh pieces before parenting. Keep their positions in the assembled pose. Use a convenient placement pivot at ground level on the assembly root. [Blender manual on applying transforms](https://docs.blender.org/manual/en/latest/scene_layout/object/editing/apply.html)
5. Add an **Empty** named `Barricade` at that placement pivot. Parent the boards to it with **Keep Transform**. Keep the assembly to **2–16 unskinned mesh objects**. Transform-only groups are supported. Keep decorative bits that should stay fixed outside the assembly. Export the assembled pieces only. A separate intact shell would also become a piece.
6. Select the Empty and its pieces. Use **File → Export → glTF 2.0**, choose **glTF Binary (.glb)**, and limit the export to **Selected Objects**. Preserve the object hierarchy. Apply mesh modifiers in Blender or use the export option if you want the evaluated geometry. Use standard glTF-compatible materials and check them in Renegade's import preview. [Blender glTF export manual](https://docs.blender.org/manual/en/latest/addons/import_export/scene_gltf2.html)

Use the same structure from other modelling software. Export a transform root and separate, assembled mesh nodes in one GLB. Give each piece usable thickness and a clean transform. Reserve rigging, animation and Blender rigid-body settings for a separate workflow. The break threshold and collision are authored in Renegade.

## 2. Set up the break in Renegade Studio

1. Use **Add → Import Model**, select the GLB, and confirm the static model import. Drag the asset from the **Asset Browser** into the level. Check the height against the floor before adding physics.
2. Select the **placed asset root** in the scene hierarchy. Renegade adds this wrapper when you place an imported model. Your Blender Empty and the mesh objects can remain beneath it. The collider goes on the wrapper, and the pieces are the renderable mesh objects under it.
3. Open **Physics Lab → Rigid Body** and add a **Box** rigid body to the wrapper. Set **Mass** to **0** so the intact prop is static. Keep **Sensor** off and leave the wrapper free of physics constraints. Check the collider dimensions and local offset so it blocks the intended opening. Leave the pieces free of their own rigid bodies.
4. Set **Break Speed (m/s)** on the wrapper in **Physics Lab → Breakables** or the asset Inspector's **Physics** section. Start with **6 m/s** for the barricade. A value of **0** disables the feature. Save the scene.

| Part of the assembly | What belongs there |
|---|---|
| Placed asset root | One static collider and the Break Speed setting |
| Blender Empty and optional groups | Transforms that organise the pieces |
| Mesh pieces | Renderable fragments in their assembled pose. Renegade creates their dynamic box colliders when the prop breaks. |

The impact threshold uses the relative speed toward the surface at contact, so a glancing hit can register less strongly than a direct hit. Debris uses fitted box colliders. Keep early pieces simple enough that a box is a reasonable fit.

## 3. Test and tune

- **Try a light hit.** The mesh pieces should rattle briefly and return to their starting pose. The intact collider should continue blocking the passage. Very small contacts below the rattle threshold should leave the prop still.
- **Try a harder hit.** The pieces should detach and move independently, the intact collider should be gone, and the space should open up subject to any debris still in the way. Walk through the gap in **Test Game**.
- **Reopen and repeat.** End Test Game and reopen the saved scene to check that the threshold and assembled state were saved.
- **Try a script-driven break.** Creator Lua can call `renegade.physics.break_destructible(entityRef)` on the placed root. C++ uses `RequestDestructibleBreak(scene, entity)`. Both queue the break for the next scene update.

### If the result looks wrong

| Symptom | Check |
|---|---|
| The prop stays rigid on every hit | Check that the root has a nonzero Break Speed and a static, solid body. Use a direct impact and see whether lowering the threshold helps. |
| Renegade reports invalid pieces | Verify there are 2–16 separate mesh objects under a transform-only root, with usable thickness and no skinning or existing piece bodies. Remove constraints targeting the intact body. |
| The whole prop falls over before breaking | Set the root's Mass to **0**. |
| The pieces fly apart too violently or snag on one another | Check for overlapping piece volumes, oversized box bounds, protruding details and pieces starting inside the ground. |
| The passage remains blocked | Check for a duplicate static collider or a separate intact mesh outside the assembly. Small fragments may still lie across the floor. |

The **Barricade Impact Range** playtest contains the same assembly with two falling weights. Use it as the baseline for tuning your first model.

[Physics feature coverage and scripting details](JOLT_GAMEPLAY_COVERAGE.md)

## Imported intact-swap trial

The separate Breakables Range on DESIGN demonstrates an intact crate and two barrels swapping to their supplied debris meshes. The demo author imports each prop as a static visual and its aligned hidden fragment group under a single static collider. It marks the intact visual through entity metadata and writes a native WISCENE scene. The Runtime displays the intact mesh up to the impact and then shows and releases the fragments with fitted convex hulls. For your own model, put one intact mesh and 2–16 separate fragment meshes below a transform-only parent with one solid static collider. Position both versions in the same local space. Select the intact mesh in the hierarchy, open **Physics Lab → Breakables**, and click **Set as Intact Mesh**. Studio shows the intact mesh and hides the fragments. Undo restores each mesh’s prior role and visibility. For an assembly made entirely of pieces, leave the intact role unset.

Build RenegadeVerifyDestructibles and invoke it as RenegadeVerifyDestructibles.exe --prop-swaps <asset-folder> <output-wiscene>. The trial folder requires Crate.glb, Barrel.glb, and ExplosiveBarrel.glb, each with an Intact Mesh node and a Broken Pieces group aligned to its prop origin. The runtime uses the saved role marker and the hierarchy. The fixture command uses these node names to assign the roles.
## Break rules for weapons and physics

Select the static prop root or one of its visible meshes in Physics Lab and open **Breakables**. The page shows which root owns the rules. Use **Hit Points** for cumulative damage from hitscan, travelling projectiles and the independent-hands melee strike. Use **Break Speed (m/s)** for physical contacts with a closing-speed threshold. A rule with a zero value is disabled. Both can be configured on the same prop. Hit points are transient in the play session, and restarting/reopening the level restores the authored total. Existing scenes with only Break Speed keep their impact rule.

The melee path probes the first contact within 2.25 m along the player's aim when the swing reaches its contact moment. Light and fully charged strikes apply 12 and 20 hit points respectively. Weapon and physical breaks release the same pre-authored pieces.

## Explosive props

Once the prop has a valid fragment setup and a break trigger, tick **Explode on Break** under **Inspector → Physics**. This applies the oil drum's 5.5 m/s blast preset and uses the prop's current break triggers and debris timing. Untick it to remove the blast. For finer tuning, set **Blast Speed (m/s)** above zero to launch the debris out and up and show a brief fire, smoke and light burst. The **Break Speed** or **Hit Points** rules still decide when it breaks. The blast starts at the intact collider offset. Use an intact visual for the full prop and author only the pieces you want to survive the blast. The oil drum in the Breakables Range uses 5.5 m/s. Leave Blast Speed at zero for ordinary breakables.

## Debris timing

**Debris Stay (s)** sets how long each released piece stays fully visible, measured from the break. Zero keeps the pieces for the play session. **Debris Fade (s)** controls how long the pieces take to disappear, with zero removing them as soon as their stay time expires. The colliders are removed with the visual pieces. The oil drum review holds its debris for 5 seconds and fades it over 1.5 seconds. Test Game or standalone **F8** reloads the authored intact scene.
