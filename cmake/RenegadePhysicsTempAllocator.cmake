# Build a Renegade-owned physics wrapper without modifying the pinned Wicked submodule.
# Fail visibly if upstream changes the anchors; never silently ship the slow allocator.
if(NOT TARGET WickedEngine_common)
    message(FATAL_ERROR "WickedEngine_common must exist before installing the physics allocator overlay")
endif()
set(_renegade_wicked_physics "${PROJECT_SOURCE_DIR}/WickedEngine/WickedEngine/wiPhysics_Jolt.cpp")
file(READ "${_renegade_wicked_physics}" _renegade_physics_original)
string(REPLACE "\r\n" "\n" _renegade_physics_original "${_renegade_physics_original}")
set(_renegade_old_allocator "static TempAllocatorMalloc temp_allocator; // 10-100 MB was not enough for large simulation, I don't want to reserve more memory up front")
string(FIND "${_renegade_physics_original}" "ReusablePhysicsTempAllocator" _renegade_already_patched)
string(FIND "${_renegade_physics_original}" "${_renegade_old_allocator}" _renegade_original_found)
if(NOT _renegade_already_patched EQUAL -1 OR _renegade_original_found EQUAL -1)
    message(FATAL_ERROR "Expected unmodified pinned Wicked wiPhysics_Jolt.cpp; check its revision and local changes")
endif()
set(_renegade_physics_patched "${_renegade_physics_original}")
string(REPLACE "#include <thread>\n" "#include <thread>\n#include <array>\n#include <mutex>\n"
    _renegade_physics_patched "${_renegade_physics_patched}")
file(READ "${CMAKE_CURRENT_LIST_DIR}/RenegadeReusablePhysicsTempAllocator.inc" _renegade_allocator_class)
string(REPLACE "using namespace wi::scene;\n\n"
    "using namespace wi::scene;\n\n${_renegade_allocator_class}"
    _renegade_physics_patched "${_renegade_physics_patched}")
string(REPLACE "${_renegade_old_allocator}"
    "static ReusablePhysicsTempAllocator temp_allocator; // Reuse large buffers without reserving RAM up front or reducing capacity."
    _renegade_physics_patched "${_renegade_physics_patched}")
if(_renegade_physics_patched STREQUAL _renegade_physics_original)
    message(FATAL_ERROR "Physics allocator overlay did not change the original wrapper")
endif()
string(FIND "${_renegade_physics_patched}" "static ReusablePhysicsTempAllocator temp_allocator;" _renegade_installed)
if(_renegade_installed EQUAL -1)
    message(FATAL_ERROR "Physics allocator overlay replacement failed")
endif()
# Keep rendered terrain and physics triangles identical, including inside cells.
# This edits only the generated copy. See the explicit core compatibility decision.
function(renegade_physics_replace_once anchor replacement)
    string(REPLACE "${anchor}" "" _stripped "${_renegade_physics_patched}")
    string(LENGTH "${_renegade_physics_patched}" _length)
    string(LENGTH "${_stripped}" _stripped_length)
    string(LENGTH "${anchor}" _anchor_length)
    math(EXPR _removed "${_length} - ${_stripped_length}")
    if(NOT _removed EQUAL _anchor_length)
        message(FATAL_ERROR "Pinned physics anchor changed or is ambiguous: ${anchor}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _replacement "${_renegade_physics_patched}")
    set(_renegade_physics_patched "${_replacement}" PARENT_SCOPE)
endfunction()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/RenegadeTerrainCollisionShape.inc")
file(READ "${CMAKE_CURRENT_LIST_DIR}/RenegadeTerrainCollisionShape.inc" _renegade_terrain_collision)
string(STRIP "${_renegade_terrain_collision}" _renegade_terrain_collision)
renegade_physics_replace_once(
    "Vec3 bottom_offset = Vec3::sZero();\n\n\t\tswitch (physicscomponent.shape)"
    "${_renegade_terrain_collision}")
renegade_physics_replace_once(
    "mesh->GetLODSubsetRange(physicscomponent.mesh_lod, first_subset, last_subset);"
    "mesh->GetLODSubsetRange(renegade_exact_heightfield_mesh ? 0 : physicscomponent.mesh_lod, first_subset, last_subset);")

# P3 composes owner-thread static-body/query hooks with the existing wrapper.
# The upstream source and public headers are not edited.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/EngineBridge/include/renegade/bridge/TerrainPropPhysicsAdapter.h"
    "${CMAKE_CURRENT_LIST_DIR}/RenegadeTerrainPropPhysics.inc"
    "${CMAKE_CURRENT_LIST_DIR}/RenegadeTerrainPropQueries.inc"
    "${PROJECT_SOURCE_DIR}/EngineBridge/include/renegade/bridge/TerrainPropTerrainPhysics.h"
    "${CMAKE_CURRENT_LIST_DIR}/RenegadeTerrainPropTerrainPhysics.inc")
foreach(_p3_anchor "struct PhysicsScene" "void AddRigidBody(" "void CreateRigidBodyShape(" "cMaxBodies = 65536")
    string(FIND "${_renegade_physics_original}" "${_p3_anchor}" _p3_found)
    if(_p3_found EQUAL -1)
        message(FATAL_ERROR "Pinned terrain-prop physics anchor changed: ${_p3_anchor}")
    endif()
endforeach()
string(APPEND _renegade_physics_patched
    "\n#include \"${PROJECT_SOURCE_DIR}/EngineBridge/include/renegade/bridge/TerrainPropPhysicsAdapter.h\"\n"
    "#include \"${CMAKE_CURRENT_LIST_DIR}/RenegadeTerrainPropPhysics.inc\"\n"
    "#include \"${CMAKE_CURRENT_LIST_DIR}/RenegadeTerrainPropQueries.inc\"\n"
    "#include \"${PROJECT_SOURCE_DIR}/EngineBridge/include/renegade/bridge/TerrainPropTerrainPhysics.h\"\n"
    "#include \"${CMAKE_CURRENT_LIST_DIR}/RenegadeTerrainPropTerrainPhysics.inc\"\n")

# Gameplay filters and callbacks live in the same generated Jolt translation unit.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/EngineBridge/include/renegade/bridge/PhysicsGameplayService.h"
    "${CMAKE_CURRENT_LIST_DIR}/RenegadeBodyCollisionFilter.inc"
    "${CMAKE_CURRENT_LIST_DIR}/RenegadeGameplayContactListener.inc"
    "${CMAKE_CURRENT_LIST_DIR}/RenegadeGameplayContactEvents.inc"
    "${CMAKE_CURRENT_LIST_DIR}/RenegadeDestructibles.inc")
string(PREPEND _renegade_physics_patched
    "#include <atomic>\n#include <mutex>\n"
    "#include \"${PROJECT_SOURCE_DIR}/EngineBridge/include/renegade/bridge/PhysicsGameplayService.h\"\n"
    "#include \"${CMAKE_CURRENT_LIST_DIR}/RenegadeGameplayContactListener.inc\"\n")
renegade_physics_replace_once(
    "// All Jolt symbols are in the JPH namespace"
    "#include \"${CMAKE_CURRENT_LIST_DIR}/RenegadeBodyCollisionFilter.inc\"\n\n// All Jolt symbols are in the JPH namespace")
renegade_physics_replace_once(
    "switch (inObject1)"
    "if (!CollisionMasksAllow(inObject1, inObject2))\n\t\t\t\t\treturn false;\n\t\t\t\tinObject2 = MotionLayer(inObject2);\n\t\t\t\tswitch (MotionLayer(inObject1))")
renegade_physics_replace_once("inLayer == Layers::MOVING" "MotionLayer(inLayer) == Layers::MOVING")
renegade_physics_replace_once("switch (inLayer1)" "switch (MotionLayer(inLayer1))")
renegade_physics_replace_once(
    "\t\t\t\tif (physicscomponent.IsLocked2D())"
    "\t\t\t\tif (renegade::bridge::SupportsBodyCollisionFilter(physicscomponent))\n\t\t\t\t\tsettings.mObjectLayer = EncodeCollisionLayer(settings.mObjectLayer, renegade::bridge::GetBodyCollisionFilter(scene, entity));\n\t\t\t\tif (physicscomponent.IsLocked2D())")
renegade_physics_replace_once(
    "\t\t\t\tif (currentMotionType == EMotionType::Static)\n\t\t\t\t{\n\t\t\t\t\tbody_interface.SetObjectLayer(physicsobject.bodyID, Layers::NON_MOVING);\n\t\t\t\t}\n\t\t\t\telse\n\t\t\t\t{\n\t\t\t\t\tbody_interface.SetObjectLayer(physicsobject.bodyID, Layers::MOVING);\n\t\t\t\t}"
    "\t\t\t\tconst ObjectLayer currentLayer = body_interface.GetObjectLayer(physicsobject.bodyID);\n\t\t\t\tif (currentLayer != cObjectLayerInvalid && MotionLayer(currentLayer) != Layers::GHOST)\n\t\t\t\t{\n\t\t\t\t\tconst ObjectLayer motion = currentMotionType == EMotionType::Static ? Layers::NON_MOVING : Layers::MOVING;\n\t\t\t\t\tbody_interface.SetObjectLayer(physicsobject.bodyID, WithMotionLayer(currentLayer, motion));\n\t\t\t\t}")
renegade_physics_replace_once(
    "ObjectLayer layer = value ? Layers::GHOST : (motionType == EMotionType::Static ? Layers::NON_MOVING : Layers::MOVING);\n\t\tbody_interface.SetObjectLayer(physicsobject.bodyID, layer);"
    "const ObjectLayer layer = body_interface.GetObjectLayer(physicsobject.bodyID);\n\t\tif (layer == cObjectLayerInvalid)\n\t\t\treturn;\n\t\tconst ObjectLayer motion = value ? Layers::GHOST : (motionType == EMotionType::Static ? Layers::NON_MOVING : Layers::MOVING);\n\t\tbody_interface.SetObjectLayer(physicsobject.bodyID, WithMotionLayer(layer, motion));")
renegade_physics_replace_once(
    "struct PhysicsScene\n\t\t{\n\t\t\tPhysicsSystem physics_system;"
    "struct PhysicsScene\n\t\t{\n\t\t\tGameplayContactListener gameplay_contacts;\n\t\t\tbool destructibles_prepared = false;\n\t\t\tstd::unordered_map<Entity, DestructibleRattle> destructible_rattles;\n\t\t\tstd::vector<DestructibleDebris> destructible_debris;\n\t\t\trenegade::bridge::PhysicsDiagnostics diagnostics;\n\t\t\tbool diagnostics_requested = false;\n\t\t\tbool single_step_requested = false;\n\t\t\tPhysicsSystem physics_system;")
renegade_physics_replace_once(
    "\t\t\t\tsettings.mMotionQuality = cMotionQuality;"
    "\t\t\t\tsettings.mIsSensor = renegade::bridge::SupportsBodySensor(physicscomponent) && renegade::bridge::IsBodySensor(scene, entity);\n\t\t\t\tsettings.mMotionQuality = settings.mIsSensor ? EMotionQuality::Discrete : cMotionQuality;")
renegade_physics_replace_once(
    "\t\t\tconst Body& body = lock.GetBody();\n\t\t\tconst uint64_t userdata = body.GetUserData();\n\t\t\tif (userdata == 0)\n\t\t\t\treturn;\n\n\t\t\tif (body.IsRigidBody())"
    "\t\t\tconst Body& body = lock.GetBody();\n\t\t\tconst uint64_t userdata = body.GetUserData();\n\t\t\tif (userdata == 0 || body.IsSensor())\n\t\t\t\treturn;\n\n\t\t\tif (body.IsRigidBody())")
string(APPEND _renegade_physics_patched
    "#include \"${CMAKE_CURRENT_LIST_DIR}/RenegadeGameplayContactEvents.inc\"\n"
    "#include \"${CMAKE_CURRENT_LIST_DIR}/RenegadeDestructibles.inc\"\n"
    "#include \"${PROJECT_SOURCE_DIR}/EngineBridge/include/renegade/bridge/VehicleSteeringService.h\"\n"
    "#include \"${CMAKE_CURRENT_LIST_DIR}/RenegadeVehicleSteering.inc\"\n")

# Update Renegade aircraft immediately before each native Jolt step.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/EngineBridge/include/renegade/bridge/FixedWingService.h"
    "${CMAKE_CURRENT_LIST_DIR}/RenegadeFixedWing.inc")
string(PREPEND _renegade_physics_patched
    "#include \"${PROJECT_SOURCE_DIR}/EngineBridge/include/renegade/bridge/FixedWingService.h\"\n"
    "#include \"${PROJECT_SOURCE_DIR}/EngineBridge/include/renegade/bridge/FlightVehicleService.h\"\n")
renegade_physics_replace_once(
    "struct PhysicsScene\n\t\t{\n\t\t\tGameplayContactListener gameplay_contacts;"
    "struct FixedWingStepState\n\t\t{\n\t\t\trenegade::bridge::FixedWingAircraft aircraft;\n\t\t\trenegade::bridge::FixedWingInput input;\n\t\t\tfloat brakes = 1.f;\n\t\t\trenegade::bridge::FixedWingTelemetry telemetry;\n\t\t};\n\t\tstruct PhysicsScene\n\t\t{\n\t\t\twi::unordered_map<Entity, FixedWingStepState> fixed_wings;\n\t\t\tGameplayContactListener gameplay_contacts;")
renegade_physics_replace_once(
    "PhysicsScene& GetPhysicsScene(Scene& scene)"
    "void StepFixedWing(Scene& scene, PhysicsScene& physics, float dt);\n\t\tPhysicsScene& GetPhysicsScene(Scene& scene)")
renegade_physics_replace_once(
    "physics_scene.physics_system.Update(TIMESTEP, COLLISION_STEPS, &temp_allocator, &job_system);"
    "StepFixedWing(scene, physics_scene, TIMESTEP);\n\t\t\t\tphysics_scene.physics_system.Update(TIMESTEP, COLLISION_STEPS, &temp_allocator, &job_system);")
string(APPEND _renegade_physics_patched "#include \"${CMAKE_CURRENT_LIST_DIR}/RenegadeFixedWing.inc\"\n")


# Physical VTOL shares the aircraft step and ground query with fixed-wing flight.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/RenegadePhysicalVtol.inc")
renegade_physics_replace_once(
    "struct PhysicsScene\n\t\t{\n\t\t\twi::unordered_map<Entity, FixedWingStepState>"
    "struct PhysicalVtolState\n\t\t{\n\t\t\trenegade::bridge::FlightVehicleInput input;\n\t\t\trenegade::bridge::PlayerVehicleMode mode = renegade::bridge::PlayerVehicleMode::Disabled;\n\t\t\trenegade::bridge::SpacecraftInput spacecraft;\n\t\t\tstruct Nozzle { JPH::Vec3 position; JPH::Vec3 direction; };\n\t\t\tstd::array<Nozzle, 16> nozzles;\n\t\t\tJPH::Vec3 rotorHub = JPH::Vec3::sZero();\n\t\t\tdouble targetAltitude = 0.0;\n\t\t\tbool started = false;\n\t\t\tbool grounded = false;\n\t\t};\n\t\tstruct PhysicsScene\n\t\t{\n\t\t\twi::unordered_map<Entity, PhysicalVtolState> physical_vtols;\n\t\t\twi::unordered_map<Entity, FixedWingStepState>")
renegade_physics_replace_once(
    "void StepFixedWing(Scene& scene, PhysicsScene& physics, float dt);"
    "void StepPhysicalVtol(Scene& scene, PhysicsScene& physics, float dt);\n\t\tvoid StepFixedWing(Scene& scene, PhysicsScene& physics, float dt);")
renegade_physics_replace_once(
    "StepFixedWing(scene, physics_scene, TIMESTEP);"
    "StepFixedWing(scene, physics_scene, TIMESTEP);\n\t\t\t\tStepPhysicalVtol(scene, physics_scene, TIMESTEP);")
string(APPEND _renegade_physics_patched "#include \"${CMAKE_CURRENT_LIST_DIR}/RenegadePhysicalVtol.inc\"\n")

# Keep authored aircraft parked until the fixed-wing runtime rig is registered.
renegade_physics_replace_once(
    "void StepFixedWing(Scene& scene, PhysicsScene& physics, float dt);"
    "void StepFixedWing(Scene& scene, PhysicsScene& physics, float dt);\n\t\tbool IsParkedFixedWing(const Scene& scene, Entity entity);")
renegade_physics_replace_once(
    "const EMotionType motionType = physicscomponent.mass == 0 ? EMotionType::Static : (physicscomponent.IsKinematic() ? EMotionType::Kinematic : EMotionType::Dynamic);"
    "const EMotionType motionType = physicscomponent.mass == 0 ? EMotionType::Static : ((physicscomponent.IsKinematic() || IsParkedFixedWing(scene, entity)) ? EMotionType::Kinematic : EMotionType::Dynamic);")
renegade_physics_replace_once(
    "if (motionType == EMotionType::Dynamic)\n\t\t\t\t{\n\t\t\t\t\tsettings.mMassPropertiesOverride.mMass = physicscomponent.mass;"
    "if (physicscomponent.mass > 0.f && !physicscomponent.IsKinematic())\n\t\t\t\t{\n\t\t\t\t\tsettings.mMassPropertiesOverride.mMass = physicscomponent.mass;")
renegade_physics_replace_once(
    "const EMotionType currentMotionType = physicscomponent.mass == 0 ? EMotionType::Static : (physicscomponent.IsKinematic() ? EMotionType::Kinematic : EMotionType::Dynamic);"
    "const EMotionType currentMotionType = physicscomponent.mass == 0 ? EMotionType::Static : ((physicscomponent.IsKinematic() || IsParkedFixedWing(scene, entity)) ? EMotionType::Kinematic : EMotionType::Dynamic);")

renegade_physics_replace_once(
    "body_interface.SetMotionType(physicsobject.bodyID, currentMotionType, EActivation::DontActivate);"
    "const bool startingFlight = prevMotionType == EMotionType::Kinematic && currentMotionType == EMotionType::Dynamic && physics_scene.fixed_wings.find(entity) != physics_scene.fixed_wings.end();\n\t\t\t\tbody_interface.SetMotionType(physicsobject.bodyID, currentMotionType, startingFlight ? EActivation::Activate : EActivation::DontActivate);")


# Bind native tracked constraints to the lifetime of their rigid bodies.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/RenegadeTrackedVehicle.inc")
string(PREPEND _renegade_physics_patched
    "#include \"${PROJECT_SOURCE_DIR}/EngineBridge/include/renegade/bridge/TrackedVehicleTuning.h\"\n")
renegade_physics_replace_once(
    "struct PhysicsScene\n\t\t{\n\t\t\twi::unordered_map<Entity, PhysicalVtolState>"
    "struct TrackedVehicleSetup\n\t\t{\n\t\t\trenegade::bridge::TrackedWheelAnchors anchors;\n\t\t\trenegade::bridge::TrackedVehicleTuning tuning;\n\t\t\tfloat forward = 0, turn = 0, brake = 1;\n\t\t};\n\t\tstruct PhysicsScene\n\t\t{\n\t\t\twi::unordered_map<Entity, TrackedVehicleSetup> tracked_vehicles;\n\t\t\twi::unordered_map<Entity, PhysicalVtolState>")
renegade_physics_replace_once(
    "#include <Jolt/Physics/Vehicle/MotorcycleController.h>"
    "#include <Jolt/Physics/Vehicle/MotorcycleController.h>\n#include <Jolt/Physics/Vehicle/TrackedVehicleController.h>")
renegade_physics_replace_once(
    "VehicleConstraint* vehicle_constraint = nullptr;"
    "VehicleConstraint* vehicle_constraint = nullptr;\n\t\t\tVehicleConstraint* tracked_constraint = nullptr;\n\n\t\t\tvoid RemoveTrackedVehicle(PhysicsSystem& system)\n\t\t\t{\n\t\t\t\tif (!tracked_constraint) return;\n\t\t\t\tsystem.RemoveStepListener(tracked_constraint);\n\t\t\t\tsystem.RemoveConstraint(tracked_constraint);\n\t\t\t\ttracked_constraint = nullptr;\n\t\t\t}")
renegade_physics_replace_once(
    "body_interface.RemoveBody(bodyID);\n\t\t\t\tif (character != nullptr)"
    "RemoveTrackedVehicle(jolt_physics_scene->physics_system);\n\t\t\t\tbody_interface.RemoveBody(bodyID);\n\t\t\t\tif (character != nullptr)")
renegade_physics_replace_once(
    "void StepPhysicalVtol(Scene& scene, PhysicsScene& physics, float dt);"
    "void StepTrackedVehicles(Scene& scene, PhysicsScene& physics);\n\t\tvoid StepPhysicalVtol(Scene& scene, PhysicsScene& physics, float dt);")
renegade_physics_replace_once(
    "StepPhysicalVtol(scene, physics_scene, TIMESTEP);"
    "StepPhysicalVtol(scene, physics_scene, TIMESTEP);\n\t\t\t\tStepTrackedVehicles(scene, physics_scene);")
string(APPEND _renegade_physics_patched "#include \"${CMAKE_CURRENT_LIST_DIR}/RenegadeTrackedVehicle.inc\"\n")

# Compound shapes use the exact local poses of the authored shape children.
renegade_physics_replace_once(
    "#include <Jolt/Physics/Collision/Shape/ScaledShape.h>"
    "#include <Jolt/Physics/Collision/Shape/ScaledShape.h>\n#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>\n#include <cstring>\n#include \"${PROJECT_SOURCE_DIR}/EngineBridge/include/renegade/bridge/CompoundColliderService.h\"")
renegade_physics_replace_once(
    "struct RigidBody\n\t\t{\n\t\t\twi::allocator::shared_ptr<void> physics_scene;"
    "struct RigidBody\n\t\t{\n\t\t\tuint64_t compound_signature = 0;\n\t\t\twi::allocator::shared_ptr<void> physics_scene;")
renegade_physics_replace_once(
    "\t\tvoid AddRigidBody(\n"
    "#include \"${CMAKE_CURRENT_LIST_DIR}/RenegadeCompoundCollider.inc\"\n\t\tvoid AddRigidBody(\n")
renegade_physics_replace_once(
    "\t\t\ttransform.ApplyTransform();\n\n\t\t\tif (mesh != nullptr && mesh->precomputed_rigidbody_physics_shape.physicsobject != nullptr)"
    "\t\t\ttransform.ApplyTransform();\n\n\t\t\tconst auto compound = ReadCompoundShape(scene, entity, physicscomponent, _transform);\n\t\t\tconst auto compound_shape = CreateCompoundShape(compound);\n\t\t\tconst bool uses_compound = compound_shape != nullptr;\n\t\t\tphysicsobject.compound_signature = uses_compound ? compound.signature : 0;\n\t\t\tif (uses_compound)\n\t\t\t\tphysicsobject.shape = compound_shape;\n\t\t\tif (!uses_compound && mesh != nullptr && mesh->precomputed_rigidbody_physics_shape.physicsobject != nullptr)")
renegade_physics_replace_once(
    "Vec3 local_offset = cast(physicscomponent.local_offset);"
    "Vec3 local_offset = uses_compound ? Vec3::sZero() : cast(physicscomponent.local_offset);")
renegade_physics_replace_once(
    "if ((physicscomponent.physicsobject == nullptr || physicscomponent.IsRefreshParametersNeeded()) && scene.transforms.Contains(entity))"
    "const auto* source_transform = scene.transforms.GetComponent(entity);\n\t\t\tbool compound_changed = false;\n\t\t\tif (source_transform && physicscomponent.physicsobject)\n\t\t\t{\n\t\t\t\tconst auto previous = GetRigidBody(physicscomponent).compound_signature;\n\t\t\t\tif (previous || renegade::bridge::HasCompoundCollider(scene, entity))\n\t\t\t\t\tcompound_changed = previous != ReadCompoundShape(scene, entity, physicscomponent, *source_transform).signature;\n\t\t\t}\n\t\t\tif (source_transform && (physicscomponent.physicsobject == nullptr || physicscomponent.IsRefreshParametersNeeded() || compound_changed))")

# Sample the native profiler interval while the Lab is reading diagnostics.
renegade_physics_replace_once(
    "PhysicsScene& physics_scene = GetPhysicsScene(scene);\n\t\tphysics_scene.physics_system.SetGravity"
    "PhysicsScene& physics_scene = GetPhysicsScene(scene);\n\t\tconst bool single_step = std::exchange(physics_scene.single_step_requested, false) && !IsSimulationEnabled();\n\t\tconst bool sample_diagnostics = std::exchange(physics_scene.diagnostics_requested, false) || single_step;\n\t\tconst auto diagnostics_begin = sample_diagnostics ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};\n\t\tuint32_t steps_this_update = 0;\n\t\tphysics_scene.physics_system.SetGravity")
renegade_physics_replace_once(
    "// Perform internal simulation step:\n\t\tif (IsSimulationEnabled())"
    "// Perform internal simulation step:\n\t\tif (IsSimulationEnabled() || single_step)")
renegade_physics_replace_once(
    "physics_scene.accumulator += dt;"
    "if (single_step)\n\t\t\t\tphysics_scene.accumulator = TIMESTEP;\n\t\t\telse\n\t\t\t\tphysics_scene.accumulator += dt;\n\t\t\tif (sample_diagnostics)\n\t\t\t{\n\t\t\t\tphysics_scene.gameplay_contacts.sample_diagnostics.store(true, std::memory_order_relaxed);\n\t\t\t\tphysics_scene.physics_system.SetContactListener(&physics_scene.gameplay_contacts);\n\t\t\t}")
renegade_physics_replace_once(
    "physics_scene.physics_system.Update(TIMESTEP, COLLISION_STEPS, &temp_allocator, &job_system);\n\t\t\t\tphysics_scene.accumulator = next_accumulator;"
    "if (sample_diagnostics)\n\t\t\t\t\tphysics_scene.gameplay_contacts.diagnostic_contacts.store(0, std::memory_order_relaxed);\n\t\t\t\tphysics_scene.physics_system.Update(TIMESTEP, COLLISION_STEPS, &temp_allocator, &job_system);\n\t\t\t\t++steps_this_update;\n\t\t\t\tphysics_scene.accumulator = next_accumulator;")
renegade_physics_replace_once(
    "physics_scene.alpha = physics_scene.accumulator / TIMESTEP;"
    "physics_scene.alpha = single_step ? 1.0f : physics_scene.accumulator / TIMESTEP;\n\t\t\tif (sample_diagnostics)\n\t\t\t{\n\t\t\t\tphysics_scene.gameplay_contacts.sample_diagnostics.store(false, std::memory_order_relaxed);\n\t\t\t\tif (!physics_scene.gameplay_contacts.NeedsListener())\n\t\t\t\t\tphysics_scene.physics_system.SetContactListener(nullptr);\n\t\t\t}")
renegade_physics_replace_once(
    "wi::profiler::EndRange(range);"
    "physics_scene.diagnostics.totalFixedSteps += steps_this_update;\n\t\tif (sample_diagnostics)\n\t\t{\n\t\t\tauto& stats = physics_scene.diagnostics;\n\t\t\tconst auto elapsed = std::chrono::steady_clock::now() - diagnostics_begin;\n\t\t\tstats.totalBodies = physics_scene.physics_system.GetNumBodies();\n\t\t\tstats.awakeRigidBodies = physics_scene.physics_system.GetNumActiveBodies(EBodyType::RigidBody);\n\t\t\tstats.awakeSoftBodies = physics_scene.physics_system.GetNumActiveBodies(EBodyType::SoftBody);\n\t\t\tstats.constraints = uint32_t(physics_scene.physics_system.GetConstraints().size());\n\t\t\tif (steps_this_update > 0)\n\t\t\t{\n\t\t\t\tstats.lastUpdateMilliseconds = std::chrono::duration<float, std::milli>(elapsed).count();\n\t\t\t\tstats.lastUpdateSteps = steps_this_update;\n\t\t\t\tstats.lastStepContactManifolds = physics_scene.gameplay_contacts.diagnostic_contacts.load(std::memory_order_relaxed);\n\t\t\t}\n\t\t}\n\t\twi::profiler::EndRange(range);")
string(APPEND _renegade_physics_patched "#include \"${CMAKE_CURRENT_LIST_DIR}/RenegadePhysicsDiagnostics.inc\"\n")

set(_renegade_physics_output "${CMAKE_CURRENT_BINARY_DIR}/renegade_physics_override/wiPhysics_Jolt.cpp")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/renegade_physics_override")
set(_renegade_physics_existing "")
if(EXISTS "${_renegade_physics_output}")
    file(READ "${_renegade_physics_output}" _renegade_physics_existing)
    string(REPLACE "\r\n" "\n" _renegade_physics_existing "${_renegade_physics_existing}")
endif()
if(NOT _renegade_physics_existing STREQUAL _renegade_physics_patched)
    file(WRITE "${_renegade_physics_output}" "${_renegade_physics_patched}")
endif()
get_target_property(_renegade_wicked_sources WickedEngine_common SOURCES)
list(FIND _renegade_wicked_sources "${_renegade_wicked_physics}" _renegade_wicked_index)
if(_renegade_wicked_index EQUAL -1)
    message(FATAL_ERROR "Pinned Wicked physics wrapper is not in WickedEngine_common sources")
endif()
list(REMOVE_ITEM _renegade_wicked_sources "${_renegade_wicked_physics}")
list(APPEND _renegade_wicked_sources "${_renegade_physics_output}")
set_property(TARGET WickedEngine_common PROPERTY SOURCES "${_renegade_wicked_sources}")
set_source_files_properties("${_renegade_physics_output}"
    TARGET_DIRECTORY WickedEngine_common PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE)
message(STATUS "Renegade physics: reusable-buffer wrapper generated without editing WickedEngine")
