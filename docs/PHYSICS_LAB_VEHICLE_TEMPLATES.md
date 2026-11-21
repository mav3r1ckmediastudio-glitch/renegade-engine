# Physics Lab and vehicle templates

The Physics Lab opens in the Studio Inspector dock and keeps the scene viewport visible. Select an entity with a physics component to see the Physics section in the ordinary Inspector. For a rigid body, it edits mass, friction, bounce, kinematic state and wheel radius where applicable. Open in Physics Lab jumps to the relevant page, including for VTOL, constraint, character, ragdoll, soft-body and secondary-collider selections. The shared scene and command stack handle these edits.

## Author a boardable vehicle

1. Select the chassis entity. Attach a dynamic rigid body in Rigid Body.
2. In Vehicle, select Car or Motorcycle for a ground vehicle. Assign the wheel entities and tune the chassis and suspension. Set Player Controls to Ground.
3. For a VTOL, use a dynamic hull with Vehicle Type set to None and set Player Controls to VTOL. The supplied template includes the original hull, landing pad and Player Start.
4. Save and use Test Game. Approach the chassis and press E. The ground vehicle uses W/S to drive, A/D to steer and Space to brake. The VTOL uses W/S for thrust, A/D for yaw, Space to climb and C to descend. Tab cycles Chase, Cockpit and Rear Quarter. E exits a stopped ground vehicle or a stopped and landed VTOL.

The local GroundVehicle and FlyingVehicle projects include standalone Runtime and Studio launchers. Brake lens masking, exhaust placement and driver camera fit are specific to the imported test van. Tyre skids and smoke sample mapped wheels. The VTOL uses a forward-positive-Z hull and a stabilization controller on a dynamic rigid body.

## Preview the wheel and suspension geometry

Select a car or motorcycle chassis and enable **Vehicle → Show Vehicle Physics**. Orange outlines the Box chassis collider. Cyan outlines the wheels at full suspension extension. Yellow marks the suspension travel and the selected ray or sphere contact geometry. In Cylinder mode the wheel outline shows the cylinder envelope.

Drag a geometry slider to preview the change with simulation paused. Releasing the slider commits the usual undoable edit. The preview is editing geometry and is available while the Vehicle page is open. The **World → Physics Visualizer** shows the live physics bodies for other chassis shapes.

## Animate the cockpit steering wheel

On a Car chassis, scroll to **Steering Wheel** and select a separate cockpit wheel mesh under the chassis. Set the mesh origin at the centre of the steering column before importing. Runtime rotates the mesh around its imported origin. The signed X/Y/Z settings are the axis in the mesh's local space. Reverse all three signs if the wheel turns in the opposite direction. **Turn to Lock** is the rotation in degrees from centre to one lock. At 180 degrees the full left-to-right travel is 360 degrees.

Board the car and press **Tab** to check the cockpit view. The visual wheel eases toward the mean steering angle of the two front wheels and back to neutral. Leave the slot empty for vehicles without a cockpit wheel mesh. The mapping and settings save on the chosen mesh. The existing player-driven car path animates them at runtime.

## Fly a VTOL from a project script

Copy `Examples/Vehicles/VTOLController.lua` into the project's `Content/Scripts` and attach it as a **Script** to the dynamic hull. In **Physics Lab → Vehicle**, leave **Vehicle Type** set to **None** and set **Player Controls** to **Scripted VTOL**. Save and board with **E** in Test Game. The same vehicle cameras and exit rules apply.

The script receives normalized thrust, yaw, and climb through `renegade.physics.vtol_pilot_input(self.entity)`. It returns nil when the hull is unboarded or the player controls are blocked. The script sends its chosen values to `renegade.physics.vtol_pilot_drive(self.entity, thrust, yaw, climb, previous_target)`. Save the returned altitude target for the next update. The API accepts one command per frame for the piloted scripted hull and supplies the frame time. The native VTOL controller still handles levelling, velocity and altitude corrections.

The sample scales thrust and yaw and keeps the last altitude target when the climb input returns to zero. Movement bindings come from the project input map. Space and C are the built-in climb and descend controls.

## Fly the fixed-wing trainer

Click the aircraft in the viewport to select its chassis, or choose **Kestrel T7 Chassis** in the Scene Hierarchy. The Vehicle page shows the aircraft's player mode, mass and damping. Use Test Game to fly it and check the landing gear.

Select the dynamic rigid body that owns the imported aircraft. The selected body is the chassis. In **Vehicle → Advanced**, set thrust, thrust fade speed, wing area and incidence, drag area, stall angle, mouse sensitivity, or landing gear response. Thrust fade speed sets where available thrust falls to zero. Actual top speed is determined by the balance of thrust and drag.

Open **Part Bindings** to map the model's parts. The nose and two main positions each need a contact marker or a bound wheel. A marker supplies the tyre contact position directly. With a wheel alone, Runtime places the contact below its axle using the configured tyre radius. Assigning an optional visual part enables its animation. Force-point bindings place the wing, tail, fin, and thrust loads relative to the chassis. The cockpit binding sets the pilot eye position.

Use an unscaled dynamic chassis with local +Z forward and +Y up. Parent the visual rig and markers underneath it. Keep steering and wheel spin on separate pivots for the nose gear, and orient the visual hinge axes as in the Kestrel rig. The named Kestrel rig automatically resolves when its parts have no explicit binding. Settings live in the scene and apply on the next Test Game launch.

In Physics Lab, choose **Fixed Wing** under **Vehicle → Player Controls** on an ordinary dynamic rigid body. Keep **Vehicle Type** set to **None**. The prototype binds the named child transforms below. Use the Kestrel T7 runway scene to start with the bindings in place.

| Keys | Control |
|---|---|
| E | Board nearby, or exit with wheel contact below 1 m/s |
| W / S | Increase / decrease the held throttle setting |
| Mouse up / down | Increase / decrease the pitch target. The setting holds when the mouse stops |
| Mouse left / right | Steer on the runway or bank gradually in flight. Releasing the turn command brings the wings toward level |
| Right mouse button or Space | Main-wheel brakes |
| Tab | Chase, cockpit and rear-quarter view |

The chassis uses local +Z forward and +Y up. The Kestrel GLB's imported visual root is rotated 180 degrees about up to align it with the chassis. Place the three transform-only contact markers at the fully extended tyre contact points. Name them `REF_WheelContact_Nose`, `REF_WheelContact_Main_L` and `REF_WheelContact_Main_R`. The cockpit uses `REF_Eye_Pilot`. The authored ailerons, elevator, rudder, propeller, nose steering and wheels keep their imported hinge or axle pivots.

Studio keeps an unregistered fixed-wing aircraft at its authored transform. Test Game registers its runtime rig, wakes the dynamic body and takes over with the spring-supported landing gear. Place the tyre contacts at runway height in the editor before starting Test Game.

The trainer computes air-relative loads separately for each wing, the tail and the fin. Jolt applies those loads and the propeller thrust at the corresponding locations before each physics step. Gear uses surface ray contacts with spring, damping, grip and brakes. `FixedWingService.h` provides the C++ registration, command and telemetry API. Atmospheric tuning is available through the C++ API.

## C++ and Lua

See [Jolt gameplay coverage](JOLT_GAMEPLAY_COVERAGE.md) for cloth pinning and pending native Jolt exposure.

| Lab page | Public C++ bridge | Lua surface and verification |
|---|---|---|
| World | PhysicsService | World settings, gravity, broadphase and raycast. Binding contract and baseline tests passed. |
| Rigid Body | CollisionService, PhysicsService | Live transforms, forces, impulses, activation and ghosting. Shape, dimensions and surface settings can be read and edited from Lua. Ground and VTOL exercises cover dynamic bodies. |
| Constraint | ConstraintService | Broken-state read/write. Constraints can be created, paired, tuned and removed in Lua. Baseline tests passed. |
| Character | CharacterPhysicsService | Movement, ground query and capsule resizing. The settings table also exposes the character defaults. Baseline tests passed. |
| Vehicle | VehiclePhysicsService, FlightVehicleService | Ground drive, speed and wheel transforms. VTOL readiness and drive. Lua settings include wheel mappings, chassis dimensions and suspension. Ground Test Game and standalone flight were exercised on DESIGN. |
| Ragdoll | RagdollPhysicsService | Runtime impulses, ghosting and activation. Lua settings include proportions and the ragdoll options shown in the Lab. Baseline tests passed. |
| Soft Body | SoftBodyPhysicsService | Runtime node positions, activation and reset. Lua can create and edit a soft body on a mesh. Baseline tests passed. |
| Collider | WickedColliderService | Lua supports creating, editing and removing secondary colliders. CPU/GPU secondary-system behavior still needs native interactive coverage. |

The authoring API lives under renegade.physics. Use get_<component>_settings(entity) to get a table and set_<component>_settings(entity, patch) to apply fields from a table. Supported component names are rigid_body, constraint, character, vehicle, ragdoll, soft_body and collider. Rigid bodies, constraints, soft bodies and secondary colliders also support add_<component>(entity, initial_settings) and remove_<component>(entity). Initial settings are optional. Component settings use snake_case keys and the same units as the C++ fields. Angles are radians, vectors use x/y/z and linked entities use their numeric IDs. Use the C++ state definitions for enum values.

A patch can update a single field or a vector coordinate. For example, set_rigid_body_settings(entity, { mass = 12, box_half_extents = { y = 2 } }) edits mass and only the Y half-extent. A missing component reads as nil. Set, add and remove return whether the scene changed. Scripts apply these operations directly to their bound scene.

FlightVehicleService.h exposes GetPlayerVehicleMode, SetPlayerVehicleMode, HasLiveFlightVehicle and DriveFlightVehicle. To pilot a spare VTOL with Lua, call renegade.physics.has_flight_vehicle(entity), then renegade.physics.flight_vehicle_drive(entity, thrust, yaw, climb, dt, previous_target). Pass inputs between -1 and 1. Keep the returned altitude target per craft and feed it back on the next update. Omit the first target to initialize from the current height. The drive call returns nil if the body is unavailable or the time step is invalid.

## Physical VTOL

Open the **Vesper V9 VTOL** demo and use **Test Game**. Press **E** to board, **Space** to rise, **W/S** to pitch for forward and reverse travel, **A/D** to yaw, and **C** to descend. Release the climb controls to hold altitude. **Tab** cycles chase, cockpit and rear-quarter views. Press **E** again after the craft is on the ground and has stopped.

For a custom craft, put its visuals under a dynamic rigid-body chassis, point the chassis's local +Z forward and +Y up, and leave the ground vehicle type at **None**. Select **Physical VTOL** under **Physics Lab > Vehicle > Player Controls** and set the chassis collision in **Rigid Body**. Place the collider so that the craft can rest on the ground with its gear visibly touching. An optional child named REF_Pilot_Eye places the cockpit camera. Name the exhaust markers REF_Nacelle_Exhaust_L and REF_Nacelle_Exhaust_R to add the Runtime plumes. The imported markers point along their local -Z axes. The plumes strengthen during climb and fade when the craft settles.

The Vesper demo uses a simple box chassis collider and nacelles posed for hover. The controller applies lift along the chassis's up axis. With forward speed, yaw input also banks the craft and tilts lift into the turn. Yaw in a stationary hover pivots around the vertical axis.

Use CommandPhysicalVtol and IsPhysicalVtolGrounded from FlightVehicleService.h in C++. In Lua, use renegade.physics.physical_vtol_command(entity, thrust, yaw, climb) and renegade.physics.physical_vtol_grounded(entity). Inputs range from -1 to 1 and persist until replaced. Send zero inputs for hover, or pass false as the fifth argument to stop the controller. Use one pilot per craft. A player occupying the craft supplies the same command from the Runtime input map.

To regenerate the demo scene on Windows, build the RenegadeAuthorVesperVtol target and run its executable with the Vesper GLB path and a writable scene path. The authoring tool reopens the scene and runs the native flight checks.

## Verification

- Focused Windows x64 Release compilation and linking use existing local dependencies. Clean Studio and Runtime Debug and Release builds and hosted checks remain open.
- The updated Physics Lab source contract and rebuilt Lua binding test passed. Ten native Physics tests passed in the installed Release build. The Lua test covers component authoring and readback across the seven component pages, a rejected invalid patch and a tagged craft with no live Jolt body. The Runtime flight run exercises the controller with a live body.
- Studio visual checks covered the Physics Inspector, all eight docked pages, dock resizing, wheel picker and player-mode Undo. Both projects launched through Test Game. The flight authoring tool saved and reopened the VTOL data. Runtime checks exercised boarding, lift, forward flight, landing and exit.
- Representative authoring, save/reopen and live simulation coverage across all Lab pages remains open. Smoke has a bounded Runtime emitter path and needs close-up visual confirmation.

On Windows, build the scene authoring tool with cmake --build <configured-build> --config Release --target RenegadeAuthorFlightVehicleTemplate. Run it with one writable .wiscene output path and the native Runtime shader and content resources available to Wicked initialization.
