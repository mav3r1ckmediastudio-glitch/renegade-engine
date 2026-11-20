# Jolt gameplay and authoring coverage

Renegade's goal is to make each applicable Jolt physics capability usable from Studio where it has an authoring or diagnostic workflow, and available through the public C++ EngineBridge and renegade.physics Lua namespace. The reference is the vendored Jolt 5.6.0 source in the pinned Wicked revision, 82557d439a41d104f559b0d3134d160b7bf6154e. Retail availability of optional features must be checked against that build's configuration.

## Coverage inventory

| Capability | Renegade baseline | Remaining exposure |
|---|---|---|
| World and solver | Gravity, simulation enable, fixed rate, accuracy, interpolation, debug controls, paused single stepping, CPU time, active body, contact and constraint counts and buffered rigid contact begin/end events | Advanced solver and activation controls, runtime diagnostic capture and replay |
| Rigid bodies | Creation, primitive and mesh-backed shapes, material response, movement, forces, torque, impulses and editable sensor state, eight simulation layers with per-body masks and authored impact destructibles, and compound box, sphere and capsule parts | Decorated shapes, tapered shapes and plane, collision groups and specialized-body filtering, motion quality and degrees of freedom, inertia and centre of mass, advanced material and buoyancy controls |
| Queries | Scene raycast with body, ragdoll and soft-body hit data, plus native-body raycasts with scene layer, entity subtree and sensor filters | Multi-hit raycasts, sweeps, shape and point overlap, cast collectors and subshape information |
| Constraints | Fixed, point, distance, hinge, cone, six degrees of freedom, swing-twist and slider, with viewport pivot and axis authoring | Gear, rack and pinion, pulley and path; remaining limits, spring and motor settings, reactions and event reporting |
| Characters | Capsule movement, ground query and resizing | Remaining physical and virtual character controls, contact callbacks and interactions with other characters |
| Vehicles | Car and motorcycle controls, wheel mapping and suspension, tracked drive and starter airframes | Remaining wheel and contact data, transmission, differentials, anti-roll bars and advanced tuning |
| Ragdolls | Humanoid setup, activation, ghosting and bone impulses | Remaining rig and constraint authoring, pose control and body-level queries |
| Soft bodies and cloth | Mesh weights, mass, friction, restitution, pressure, radius, wind, wake and reset | Remaining native soft-body settings and constraints, contact events, live deformation checks and a general vertex-painting workflow |
| Hair | Existing engine hair authoring is separate | The Release Jolt project includes Hair sources. Verify whether the compiled module is active and integrated before exposing its controls |
| Liquids and secondary collision | Wicked particle emitters already have SPH controls and secondary colliders | Validate liquid scenes independently. SPH is a Wicked particle feature and is outside Jolt parity. |

The implementation boundary for advanced native features is the physics adapter. Keep scene and body ownership in the engine and expose stable entity-based commands, queries and events. Engine-internal locks, allocator internals and raw world pointers have no creator-facing authoring function. Record exact backend configuration, source coverage and executable proof for each feature instead of equating headers with shipped support.

## Inspect a level in the Physics Lab

Open **World** in the Lab while the level is open in Studio. **Physics CPU** measures the most recent update that ran one or more fixed ticks, using the same interval as the Physics profiler row. **Contacts** counts manifold callbacks for the latest sampled tick. **Awake** shows awake rigid and soft bodies together against the total body count. **Constraints** shows the current native constraint count.

Clear **Simulation Enabled**, then press **Step Once**. A click queues one fixed tick for that scene. The viewport shows the resulting pose while the world stays paused. Use the debug visualiser alongside the counts when investigating a body or contact. These controls apply to the Studio scene. Use Test Game separately to verify gameplay scripts and player-driven behaviour.

## Fit a compound collider

Create an ordinary rigid body, open **Rigid Body**, pick a box, sphere or capsule under **Collider Part Shape**, then press **Add Collider Part**. Studio selects the new child. Use Move, Rotate and Scale in the viewport. The collider preview highlights the selected part in orange. For a capsule, X/Z scale sets its radius and Y sets the half-length between the round ends.

Use up to eight parts. The child transforms are saved with the scene and all parts share the root rigid body, material response and collision filters. Delete a part through the hierarchy and use Undo to restore it. The base shape is used when the body has no parts. Author and tune shape changes with simulation paused.

## Place and orient a constraint in the viewport

Select a constraint marker and open **Constraint**. The overlay shows its pivot, axes for constraints that use them, and links to its bound bodies. Move the marker to position the pivot. Rotate it to aim the hinge's local Y axis or the slider, cone and swing-twist local X axis. Six DOF shows the complete frame. The existing Move and Rotate commands rebuild the native constraint when committed and when undone or redone.

## Cloth pin authoring

Select a mesh with a soft body in the Physics Lab. **Pin Top Edge** adds the highest local-Y mesh vertices to the current pins. **Clear Pins** unpins all vertices. Both are Undo/Redo commands. For a differently oriented mesh or a custom set of anchors, use Lua or the C++ command.

- get_soft_body_settings and set_soft_body_settings still provide the component-wide settings.
- soft_body_get_pinned_vertices(entity) returns sorted, zero-based mesh vertex indices in a Lua array, or nil if the soft body is absent.
- soft_body_set_pinned_vertices(entity, {0, 3, 6}) replaces the pinned set. An empty array clears it. It returns false for a no-op, missing component or invalid input. A call with a valid changed set rebuilds the native soft body so the pin weights take effect.
- The C++ equivalents are GetSoftBodyPinnedVertices and SetSoftBodyPinsCommand in SoftBodyPhysicsService.h.
- Weights use the existing WISCENE soft-body component format. Unpinned vertices retain their existing nonzero weights. A vertex unpinned from a native zero weight gets the default weight.

The Studio authoring bench records a hanging mesh pinned on its top edge, followed by a Studio save/reopen and DX12 Test Game run. Cloth seams and general vertex painting remain advanced authoring work.

## Reproduce the native cloth proof

Build the Windows Release target RenegadeVerifyClothPins, then run it from the Runtime Release working directory with a writable .wiscene output path. The proof creates a mesh, pins one edge through the same C++ command as the Lab and Lua, saves it, reopens its native archive and steps Jolt for 90 simulated frames. It fails if a pinned sample moves more than 5 mm or the free sample drops less than 35 cm. The DESIGN run held the pin at 4.000 m and dropped the free sample to 3.538 m.

## Body sensors, contacts and filtered rays

A Sensor checkbox on the Rigid Body page enables or disables a Jolt sensor on an ordinary rigid body. Studio records the edit in Undo/Redo and saves the flag with entity metadata in WISCENE. At the next physics update, the existing rigid-body refresh path rebuilds the body with the authored flag. The standard terrain-aware ray and native gameplay collision queries pass through sensors. Character and vehicle bodies use their own native adapters.

Lua under renegade.physics provides:

- body_is_sensor(entity) reads the saved sensor setting or returns nil for a missing rigid body.
- body_set_sensor(entity, enabled) returns whether the authored setting changed.
- enable_contact_events(enabled) subscribes the current scene to native rigid-body contacts.
- drain_contact_events() returns events and a dropped count. Each event has body_a, body_b, subshape_a, subshape_b, sensor and phase. phase is begin or end. The queue is consumed when drained and belongs to the current scene. Contact end follows Jolt contact removal, which can include sleep as well as separation. Body IDs in the event are scene entity IDs. Subshape numbers are native identifiers for the lifetime of the shapes.
- raycast_bodies(from_x, from_y, from_z, to_x, to_y, to_z, layer_mask, excluded_entity, include_sensors) returns the nearest native hit with entity, fraction, position and normal. The scene mask, excluded entity subtree and sensor opt-in are optional. A miss returns nil. Invalid arguments return nil and an error string. Streamed terrain props are queried through the existing raycast API.

The C++ equivalents are in PhysicsGameplayService.h. Contact callbacks copy identifiers to a bounded queue after the caller opts in. Consume the queue at a scene safe point. Use the dropped count to detect incomplete event history if the queue fills. The query mask matches Wicked scene layers. Simulation filtering uses the independent collision layer and mask below.

The generated native adapter keeps all Jolt locks, contact keys and body pointers inside the physics translation unit. Sensor authoring uses existing entity archives and body recreation, so the upstream pin stays unchanged. The Release RenegadeVerifyGameplayPhysics proof on DESIGN passed save/reopen, sensor Undo/Redo, five native ray filter cases, a live Lua sensor-inclusive raycast and one contact begin and end while a dynamic body fell through a static sensor.


## Rigid-body simulation layers

An ordinary rigid body belongs to one of eight layers, numbered 0 through 7. Its mask lists which layers it accepts. Both bodies must accept the other's layer for Jolt to consider the pair. Existing bodies use layer 0 and accept all eight layers. The native static, moving and ghost rules still apply.

The Lab's **Rigid Body** page has a **Collision Layer** picker and one **Collide With** checkbox per layer. The asset Inspector exposes the layer and sensor fields and opens the same Lab page for mask edits. Both entry points use the Studio Undo/Redo stack.

- Lua body_get_collision_filter(entity) returns the layer and mask as two integers. It returns nil for a missing or unsupported body.
- Lua body_set_collision_filter(entity, layer, mask) returns true when it changes the setting. Pass 0 to reject every layer or 255 to allow them all. For example, 2 + 8 allows layers 1 and 3. Invalid values, a missing or unsupported body, and unchanged values return false.
- C++ uses GetBodyCollisionFilter, SetBodyCollisionFilter and SetBodyCollisionFilterCommand in PhysicsGameplayService.h. Apply changes at a scene safe point, after physics jobs finish.

Settings persist with the body entity in WISCENE metadata. Live edits update the native body in place and invalidate cached contacts. Wicked scene layer masks continue to select candidates for the public body raycast. Specialized vehicle, character, ragdoll, soft-body and streamed terrain bodies use the default simulation layer and mask. The Renegade VTOL uses an ordinary rigid body.


## Impact destructibles

See [Build breakable objects for Renegade](BREAKABLE_OBJECTS_GUIDE.md) for Blender export and Studio setup.
Put a static rigid body on a transform root. Place two to sixteen unskinned mesh objects below it as the visible pieces. Imported asset wrappers and transform groups may contain those pieces. Give the pieces a little clearance from one another and leave their rigid bodies absent. Use an unconstrained root and fit its intact collider to the assembly. On that root, set **Break Speed (m/s)** under Physics in the asset Inspector or on the Lab's Breakables page. Zero disables the destructible. Changes participate in Undo/Redo and persist in WISCENE.

During Test Game or standalone play, Jolt samples closing speed along the contact normal. A lighter impact above the rattle minimum briefly moves the visible pieces along the impact direction before they return to rest. The intact collider stays fixed. A qualifying break impact releases the authored pieces together at the next scene safe point and replaces the intact collider with fitted dynamic boxes. The first implementation uses a fixed piece mass, a bounded number of releases per frame and ordinary Jolt collision for debris. An ordinary scene reset reloads the intact authored assembly.

C++ uses GetDestructibleImpactSpeed, SetDestructibleImpactSpeed, RequestDestructibleBreak and DrainDestructibleBreakEvents in PhysicsGameplayService.h. Break events give the target, impactor, measured speed and piece count. Event draining consumes the scene queue. The engine physics Lua binding exposes set_destructible_impact_speed, break_destructible and drain_destructible_breaks. The creator Lua environment exposes renegade.physics.break_destructible(entityRef) with the normal Level-scoped entity reference.

Run RenegadeVerifyDestructibles from the Runtime Release working directory to check weak and strong impacts, passage clearance and an explicit Lua break. Pass an optional .wiscene output path to save the intact demo setup. The two drop weights in the playtest make the threshold difference visible immediately. Fracture authoring tools, creator Lua break-event delivery and whole-scene debris budgets are follow-up work.