# Terrain props P3: collision and gameplay queries

Implementation baseline: merged main `38196e9869fabd36c9255c64cd248d1000b68285` (P2 PR #198). This record describes the implementation under validation. Acceptance requires the frozen-commit receipts in the handover. P3 follows the collision, gameplay-query and navigation sections of the written terrain-props integration plan. Asset, geometry and texture streaming remain later stages.

## Creator workflow

Each terrain-prop mix row has **COLLISION / SCALE...** controls. Choose None, trunk capsule or rock box, fit its local center and dimensions, and set friction and impact surface. Auto surface resolves to Wood for trunks and Stone for boxes. Bushes and small decoration can use None. Collision uses the same uniform placement scale as rendering; the scale range supports boulder-sized rocks.

The orange wireframe preview uses a cached example placement. **FOCUS COLLISION PREVIEW** moves the editor camera to that example. Apply settings publishes an ordinary undoable document edit, preserving placement/profile IDs. Generate remains a separate explicit authoring operation. Save/reopen preserves the settings. The box is a simple convex primitive; automatic mesh-hull cooking and arbitrary mesh collision are deferred.

The editor keeps the authoritative CPU collision index without eagerly allocating all-map native bodies. Runtime creates only the required native prop bodies. Ordinary visual projection remains shared-resource based and transient.

## Ownership and query index

`SceneService` owns one `TerrainPropCollisionWorld` per loaded scene. Its constructor stays inert before native initialization. Scene replacement detaches the old controller before clearing native components. Captured immutable index snapshots remain usable by workers; live world mutation and gameplay facade queries require the scene owner thread. Wrong-thread access fails closed.

The CPU index is built transactionally from validated placement documents. It stores compact stable layer/profile/placement references, collision transforms and a median-split BVH. It uses actual collision bounds, including placements outside legacy page cells. Default limits are one million placements, 128 MiB index storage, 65,536 visited nodes and 16,384 tested shapes per segment query. Exceeding a limit preserves the caller's output and reports unavailable query state.

Capsule and box profiles share immutable native Jolt shapes. Indexed segment and swept-sphere queries transform into the same shapes used by resident physics. Uniform scale, yaw, origin-inside hits, segment endpoints and long segments use that common boundary. Candidate-bound clipping avoids overflow from the public ray's maximum-float range. Equal-distance results use stable layer/placement ordering.

Document edits stage the collision revision alongside native visual projection and immutable file products. Failed publication, cancellation, stale scene/history or late revision conflict retains the previous index, display and history. Recipe-only edits preserve equivalent collision and native IDs. Real visual replacement refreshes hit anchors at the document commit; frame queries use indexed anchors and native component membership.

## Native boundary decision

The pinned public physics API cannot synchronously confirm a newly allocated body's live native ID, share its immutable shape with all-map queries, or report actual allocated body capacity. A component count cannot establish readiness or reclamation.

Narrow hooks compose with the existing generated `RenegadePhysicsTempAllocator.cmake` adapter. They reuse pinned `CreateRigidBodyShape`, `GetPhysicsScene`, `AddRigidBody`, Jolt native queries and `RigidBody::Delete`. The public Renegade header exposes opaque shared shapes, capacity counters, live-body checks, synchronous static preparation, ordinary-world casts and native prop contacts. Raw Jolt IDs remain private.

Generated build-tree wrappers also extend pinned Scene queries and the existing terrain generation pump. Native CharacterComponent capsule and foot-ray queries include resident prop collision. Terrain generation accepts bounded actor/destination chunk interests, prioritizes those through its existing queue, retains requested source chunks and publishes chunk readiness at its existing owner stage. Runtime can keep that same terrain pump running while simulation time is held at zero. A final pre-physics terrain check prevents a chunk invalidated during generation from admitting simulation.

No second physics world, scene update, physics step, terrain generator or streaming pump is introduced. Gitlinks and the preexisting generated Wicked overlay bytes remain unchanged. Anchor-checked configuration fails if the pinned native integration points change. Future upstream review must check the shape/body lifecycle, Scene query/update ordering, CharacterComponent query masks and terrain generation ownership hooks.

## Residency and admission

Runtime collects player, native NPC, vehicle, ordinary dynamic/sleeping-support and scripted interests. Native rigid-body pose and velocity take precedence over a render transform that has not yet caught up after a teleport. Predicted movement extends the requested envelope. Sleeping bodies retain their local supporting props independently of the player camera.

Defaults:

| Bound | Default |
| --- | --- |
| Player/NPC/vehicle request / release distance | 100 / 150 m |
| Ordinary-body support request / release distance | 4 / 10 m, plus body radius |
| Prediction / release delay | 1 / 1 s |
| Resident prop bodies / changes per frame | 4,096 / 128 |
| Reserved ordinary native IDs | 4,096 of native 65,536 capacity |
| Aggregate frame query work | 262,144 |
| Requested terrain chunks / native preparations per frame | 512 / 16 |
| Frame / scripted / pending-destination interests | 1,024 each |

Cold eviction removes native components and destroys their IDs. Warm retention yields under capacity pressure. Missing, invalid or refreshed native bodies are repaired within the same bounded change budget. Required-set overflow, unavailable shape, native allocation failure and unfinished terrain preparation hold simulation at its completed pose.

Teleport facades return false while preparing a target and retain the source body's pose/support. Callers retry the request; cancellation explicitly removes it. Target terrain and required props must both have live native collision before admission. Runtime restore/quickload uses the same readiness boundary. The existing Runtime update publishes interests before its single native step; input, scripts, projectiles and AI advancement are held while required collision is unavailable.

## Gameplay and navigation

Physics rays merge ordinary native hits with all-map indexed hits. Projected foliage is excluded from duplicate ordinary mesh results. Results include query readiness, effective surface, friction and stable layer/placement/profile identity, including nonresident props. Lua exposes the u64 placement ID as a decimal string to avoid signed-integer truncation.

Projectile queries merge the nearest ordinary world and prop result while excluding their owner/visual roots. Physical projectile radius uses a swept sphere. Unknown state blocks movement or line of sight. Existing grenade visibility and NPC perception use this boundary. Native NPC physical contacts use the same resident Jolt shapes through the Scene adapter.

Navigation bakes authoritative collision independently of camera or native residency. Trunks contribute capsules; None and canopy visuals contribute no obstacles. Rock boxes use conservative yawed-box AABB voxel occupancy; physical/ray collision retains the exact oriented box. A geometry digest excludes recipes, friction, surface and residency. Real collision edits invalidate navigation queries and clear stale native character paths; explicit rebuild remains required. Ordinary body discovery uses native component membership rather than a body-by-all-objects inventory pass.

Lua additions under `renegade.physics` are `set_terrain_interest`, `clear_terrain_interest`, `cancel_terrain_destination` and `terrain_collision_state`. Existing bool teleport facades keep their return contract. A ray with unavailable state raises an error; nil continues to mean a completed query found no hit.

## Storage compatibility

New manifests use `RPROPS 3`, adding friction and surface to collision profiles. Readers accept manifests v1, v2 and v3, supplying friction 0.5 and Auto surface for older profiles. Placement-page schema remains v2; stable IDs and allocation high-water marks are unchanged. Canonical validation includes finite dimensions, friction in [0,1], known surface values and complete profile identity.

## Validation status

Focused tests cover shared native shapes, actual 65,536-ID exhaustion/reuse, cold reclamation, sleeping support, bounded interest work, all-map hits, nearest-hit merging, swept projectiles, wrong-thread failure, lifecycle, terrain admission, profile publication rollback, Undo/Redo, save/reopen and v1/v2 migration.

Local evidence is recorded in [the P3 validation record](evidence/terrain-props-p3/README.md). Release passed 344/344 in one unfiltered run. Debug passed 328/329 initially; the sole unchanged 250 ms launch-timing case passed ten targeted rechecks with byte-identical fixtures on fast scratch. Both configurations completed package and LP05/LC01 subprocess proofs. WARP passed 4/4; renderer overlay retained its original 60-second timeout. Both native inspector widths and the exact-scene standalone player passed and closed normally; real captures were inspected. Constructor startup smoke and matched hosted-depth proofs passed.

An earlier intermittent Debug maximum-terrain access violation remains unexplained. It did not recur in the final full native terrain checks; test-only crash capture is enabled in Studio CI. These repeats do not establish a root-cause fix. Hosted CI, independent exact-commit verification and owner acceptance remain pending. The opt-in proof drivers use dedicated TerrainProof projects. Small fixtures establish correctness, not production-asset forest performance.
