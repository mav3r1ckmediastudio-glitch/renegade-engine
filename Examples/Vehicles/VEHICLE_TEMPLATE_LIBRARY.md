# Renegade vehicle template library

Run **New Vehicle Project.cmd** and choose a template, project name, and destination. On DESIGN, launch **Open Updated Studio.cmd**. Open the resulting **.renegade** project, select **Main Level**, and click **Open Editor**. Edit and save the scene, then choose **Test Game**. Each project gets a fresh project ID so multiple experiments can coexist.

| Template | Starting point | Main controls |
|---|---|---|
| Van | Original Workshop Van with steering-wheel animation, brake lamps, tyre marks, and smoke | E enter, W/S drive, A/D steer, Space brake, Tab camera |
| BasicVTOL | Original boardable VTOL and landing pad | E board, W/S thrust, A/D yaw, Space rise, C descend, Tab camera |
| LuaVTOL | VTOL with an editable Lua controller in Content/Scripts | Same VTOL controls |
| Kestrel | Fixed-wing trainer, runway and wheel bindings | E board, W/S power, mouse fly, RMB or Space brake, Tab camera |
| Vesper | Force-based VTOL with linked exhaust plumes | Same VTOL controls |
| Bastion | Tracked vehicle with independent turret, sight and shot effects | E board, WASD drive, Space brake, mouse aim, LMB fire, RMB sight, Tab camera |
| Merlin | Helicopter with animated rotors, helipad and a flight course | E board, W/S cyclic, A/D yaw, Space/C collective, E exit on the ground, Tab camera |
| Oriole | Zero-gravity spacecraft with a departure deck and three navigation frames | E board, W/S thrust, Z/X strafe, Space/C translate up/down, Up/Down pitch, A/D yaw, Q/R roll, Tab camera |

## Cockpit camera

Select a driveable vehicle's chassis or one of its visual parts and expand **Cockpit Camera** in the Scene Inspector. Choose **Preview Cockpit** to see the driver's view and adjust the local left/right, up/down and fore/aft position, pitch, yaw, roll and vertical field of view. **Advanced Lens** shows the equivalent focal length on a 36 × 24 mm reference frame. The angle and focal length edit the same setting.

To position the camera by flying the editor view, exit the preview, line up the perspective viewport and choose **Set From View**. Use **Reset to Default** to return to the template's original cockpit or marker. Changes support Undo/Redo and save with the chassis. In Test Game, board the vehicle and press **Tab** to reach the cockpit.

## Workshop Van source

The Workshop Van uses originally authored geometry and material colours. The template includes its editable Blender source and GLB in **SourceAssets**. The active scene has its own four animated wheel bindings, cockpit steering pivot and emissive brake lenses.

Build the source model with Blender 4.3 or later, then build and run the Windows Release scene author from a Renegade app folder so it can load its shaders.

    blender -b --python-exit-code 1 --python Examples/Vehicles/BuildWorkshopVan.py -- <output-folder>
    cmake --build BUILD/clean-release --config Release --target RenegadeAuthorWorkshopVan
    RenegadeAuthorWorkshopVan.exe <WorkshopVan.glb> <Main.wiscene>

The installer takes this freshly authored project through the **OriginalVanProject** parameter. Use the same descriptor and gameplay sidecars as the installed Workshop Van template.
## Tracked vehicle tuning

Select the Bastion chassis or one of its model parts and open **Physics Lab > Vehicle**. **Player Controls > Tracked** uses a dynamic rigid body with native Vehicle Type set to **None**. Save changes before starting Test Game. Each session reads the saved values before registering its Jolt constraint.

The main controls cover layout, wheel radius, suspension, grip and braking. **Advanced** exposes drivetrain torque, final drive, track inertia and damping, pivot power, steering reduction and chassis tilt. Use Rigid Body to edit the hull collision shape. The current tracked rig uses nine contact stations per side with Ray, Sphere and Cylinder contact tests. Wheel width tunes the contact shape in Sphere and Cylinder modes.

The stock layout supplies missing anchor positions. Custom rigs can supply descendant transforms named REF_JoltWheel_L_00 through REF_JoltWheel_L_08, with equivalent R names. Chassis local +Z points forwards. The optional Bastion wheel and track-link visual bindings follow the naming in its source model. Tread links circulate around the authored visual loops.

## Helicopter and spacecraft rigs

Select the aerodynamic hull in Physics Lab and choose **Helicopter** or **Spacecraft** under **Vehicle > Player Controls**. Use a dynamic rigid body with native Vehicle Type set to **None**. The supplied projects link their GLB as named nodes under the hull. Duplicate the project to experiment, or import the included GLBs and follow the rig notes in **SourceAssets**.

Merlin's collective controls climb and descent. With neutral collective it holds altitude. Cyclic tilts rotor lift forward or back. The forces interact with the pad and the course's solid pylons. Use the skid landing area before disembarking.

Oriole's thrusters act at their marker positions. Release the controls to coast, or use opposing thrust to slow yourself. The dock and navigation frames are solid. The starter begins on a deck for boarding. Leaving the pilot seat during flight requires a separate docking or EVA system.

In C++, use **CommandHelicopter**, **IsHelicopterGrounded**, and **CommandSpacecraft** from FlightVehicleService.h. The equivalent Lua functions live under **renegade.physics**.

    helicopter_command(entity, cyclic, yaw, collective, enabled)
    helicopter_grounded(entity)
    spacecraft_command(entity, forward, lateral, vertical, pitch, yaw, roll, enabled)

Commands use normalized inputs from -1 to 1. The optional enabled argument defaults to true. Use these calls for a scripted or automated pilot when the built-in player is out of the seat. The default pilot submits commands while the player is aboard.
