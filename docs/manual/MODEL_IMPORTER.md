# Renegade Studio model importer

## Your first character in a level

### Open the importer

From the Level Editor, choose **Add > Import Model...** and open an FBX, GLB or OBJ. With a character, use an FBX or GLB that includes the skinned mesh and skeleton. Choose **Character** above the viewport. If Character is unavailable, check that the mesh and its armature were both included in the export.

### Put the character on its feet

In **Model**, enter the rotation in degrees and confirm the field. Orbit with the right mouse button, pan with the middle button and zoom with the wheel. Hold Shift for finer movement. Check the soles against the floor from the side. Model rotation changes the imported character; orbit only moves your view. Set the scale before fitting anything to a hand.

### Give it something to do

Open **Actions** and click a source clip. It plays on the character. For a first test, choose a standing loop and set its entry to **Idle**. A model with a single attack and no idle can stand still when the level starts. If the animation comes from another skeleton, use the retargeting walkthrough in “Retarget a clip from another rig”.

### Save, place and reopen

Give the asset a recognisable name. Check **Folder** (normally **Content/Characters**) and choose **Add to Library**. This saves the configuration. Drag the new asset from the Asset Browser onto the level floor, save the level and run **Test Game**. Reopen the project and check the same instance for facing direction, foot height, material appearance and idle playback.

### Choosing a different workflow

**Static Model** is for a fixed mesh. **Animated Model** is for an object with moving parts or a deforming mesh (see “Static and animated objects”). **Player Arms / Weapon** classifies first-person equipment. To give an NPC something to hold, stay in **Character** and use **Held item** (see “Actions, IK and held items”).

## Materials, maps and glass

### Start with one material

In **Materials**, select the surface you want to change. A character body and its equipment can use different materials. Hover over a map thumbnail for a larger preview. Use the filename button to choose a replacement or the adjacent remove control to clear that slot.

### Choose how the surface data is stored

Use **Separate maps** when roughness, metalness and ambient occlusion are individual images. Choose **Packed surface map** for a combined texture. Check its channel layout against the source art. A metallic-only image in the packed slot can make an entire model look wet.

### Adjust the response to light

Use each slider for a quick comparison, then enter a number for precision. Start with **Roughness** on an overly glossy surface. Check **Metalness** on skin, cloth and painted areas before reducing reflectance across everything. **Normal strength** changes surface relief; **Emissive intensity** changes how strongly the emissive colour or map shows.

### Set up a windscreen

Select the glass material. Use **Blended** for graded transparency and adjust **Opacity** while looking through the glass at the seats or dashboard. Use **Cutout** and **Alpha cutoff** for a hard-edged mask such as leaves or a grille. **Double-sided** shows a thin surface from either side. Orbit around overlapping panes and check them again in Test Game, where the lighting and background differ.

## Retarget a clip from another rig

### Put the two rigs side by side

Import the character that will receive the movement. Open **Character > Retargeting > Open source rig and clips...**, then choose the source clip. The orange rig on the left supplies the movement. The cyan rig on the right belongs to your character. An animation-only file can show a skeleton without a source mesh.

### Match the body before the fingers

Each row pairs a **SOURCE** joint with a **TARGET** joint. Search for a target such as the right forearm, select its row and inspect the highlights on both rigs. Correct the source selection if it points to the opposite limb or an IK helper. Clear the search to return to the full list. **Unresolved sources** helps review joints with no decision.

### Deal with different anatomy

On a three-fingered hand, choose the source finger that best represents each target finger. Map its segments in order from knuckle to tip. Extra source fingers can be ignored. For a target-only tail or wing, use **Hold unmatched branch** to preserve its reference shape while its parent moves.

### Align the reference poses and bake

If the whole source faces the wrong way, use **Apply source rig rotation**. For a local mismatch, select the joint and use **Rotate source joint** or **Rotate target joint**. Check shoulders, elbows and wrists from the front and side. Choose **Bake clip to Actions**, then watch the resulting clip on the target. Scrub the start, the widest movement and the end. A high match count alone cannot tell you whether a wrist twists.

## Actions, IK and held items

### Make entries you can recognise

Click a source clip to play it. **Preview reference pose** shows the rig at rest. A take named **Take 001** is worth previewing. Give its entry a descriptive name and set the gameplay action separately. Use **Add entry** for another range of the same take or another clip for the same action. Set each row's start frame, end frame and speed. **Undo** and **Redo** sit below the table.

### Choose when to edit a pose

Before baking, use **Edit Pose / IK** or the independent reference poses in Retargeting to align the rigs. For a baked retargeted clip, use **Actions > Edit baked pose / IK**. Choose **This frame** for a single correction, **Hold for whole clip** for a constant pose, or **Offset whole clip** to preserve movement with a correction throughout. Preview the result. Post-bake edits can be kept with the asset through **Add to Library**.

### Pose a bent limb

In **Edit Pose / IK**, select the end joint, such as a hand. The **Target** sets where you want that joint to reach. The **Pole** steers which way the elbow or knee bends. Set the target and pole, choose **Solve two-bone IK**, then orbit to check the bend from the side. Use **IK influence** for a partial correction. For a finger, select its joint, enter the local turn and choose **Apply rotation**.

### Pin a contact

For a baked clip, choose **Pin joint** and select the foot, hand or other joint you want to steady. Set **Start frame** and **End frame** around the contact. Choose an **Anchor** and lock position, rotation or both. **Parent bones** controls how far up the chain the solve can reach. **Blend frames** softens the transition. Choose **Preview pin**, scrub into and out of the range, then **Keep previewed pin**. Where available, **Find next source contact** suggests a range.

### When a pinned ankle is still underground

A pin holds the joint you selected. The sole, toes or body can extend below it. For a crawling, kneeling or sleeping clip, inspect the lowest visible part of the mesh from the side. Try **Preview floor lift** and scrub the whole clip. Choose **Keep floor lift** when the contact looks right.

### Put a club in the right hand

In **Model > Held item**, choose the item and select the character's right-hand bone. Use **Move** to seat the handle in the palm, then **Rotate** to point the club head in the intended direction. Check from the side and play the attack. Tick **Equipped at spawn** if it should appear immediately in Test Game.

## Static and animated objects

### Inspect the parts

Choose **Static Model** or **Animated Model**, then **Advanced > Part pivot**. Click a mesh or select a transform from the list. The coloured axes show its pivot and the panel identifies its parent. On an aircraft, select the propeller and check that the axes meet at the hub. This is a read-only inspection.

### Add motion to an object

In **Animated Model > Actions > Add animation clips...**, choose an FBX or GLB made for that object's named parts and hierarchy. A skinned object needs a matching skeleton and bind pose. Click the source clip to play it. For a hinged panel, watch whether the free edge swings around the hinge before saving.

### Decide what happens at level start

Select an entry and set **On level start** to **Wait**, **Loop selected clip**, or **Play once and hold**. Loop suits a continuously turning propeller. Wait suits a door that should stay closed until something triggers it. Player interaction still needs a script or gameplay connection. An object's visible animation alone does not establish moving-platform physics.

### Keep one frame as a prop

Click a clip, scrub to the pose you want and use the frame-step buttons for precision. Choose **Advanced > Export current pose as static GLB...** and enter a new filename. Open that GLB through **Add > Import Model... > Static Model**. Inspect it from a second angle and choose **Add to Library**. The animated original is still available.


## Weapon Editor

From the Level Editor choose **Add > Weapon Editor**. In **Add > Import Model > Player Arms / Weapon**, set the asset role to **Weapon** and choose **Open Weapon Editor** to use the selected source model. With the **Player arms** role, **Use these arms in Weapon Editor** opens the selected rig for fitting. The weapon workspace uses the same Studio window.

- **Weapon only** opens marker placement. Open a model, place main and support grip markers, add one PSP per barrel, and place the casing marker. Drag on the surface to preview a marker and release to place it.
- **Use my arms model** accepts first-person GLB, glTF or FBX rigs. Direct fitting expects hand_l and hand_r. Elbow and individual finger controls require the matching chains. The editor lists animation clips from the supplied rig.
- **Save setup** defaults to the project's Content/Player/Weapons folder. The .weapon file references its source models. Keep those files with the project when sharing it. **Open saved setup** in the launch menu restores the saved arms choice.
- **Back to importer** returns to the previous Studio workspace. **Resume last edit** returns to the in-memory edit during the same Studio session.
