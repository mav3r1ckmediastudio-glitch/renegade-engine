if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(COLLISION_HEADER "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/CollisionService.h")
set(COLLISION_SOURCE "${RENEGADE_SOURCE_DIR}/EngineBridge/src/CollisionService.cpp")
set(COMMAND_SOURCE "${RENEGADE_SOURCE_DIR}/EngineBridge/src/CommandService.cpp")
set(SOFTBODY_HEADER "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/SoftBodyPhysicsService.h")
set(PHYSICS_LUA_SOURCE "${RENEGADE_SOURCE_DIR}/EngineBridge/src/PhysicsLuaService.cpp")
set(REUSABLE_HEADER "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/ReusableAssetInstanceService.h")
set(REUSABLE_INSTANCE_SOURCE "${RENEGADE_SOURCE_DIR}/EngineBridge/src/ReusableAssetInstanceService.cpp")
set(PHYSICS_CHROME_HEADER "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadePhysicsLabStudioChrome.h")
set(PHYSICS_CHROME_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadePhysicsLabStudioChrome.cpp")
set(PHYSICS_WORKSPACE_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadePhysicsLabWorkspace.cpp")
set(REUSABLE_TEST "${RENEGADE_SOURCE_DIR}/Tests/JP01ReusableAssetPhysicsTests.cpp")
set(SOFTBODY_TEST "${RENEGADE_SOURCE_DIR}/Tests/SoftBodyPhysicsTests.cpp")

foreach(path IN ITEMS
    "${COLLISION_HEADER}"
    "${COLLISION_SOURCE}"
    "${COMMAND_SOURCE}"
    "${SOFTBODY_HEADER}"
    "${PHYSICS_LUA_SOURCE}"
    "${REUSABLE_HEADER}"
    "${REUSABLE_INSTANCE_SOURCE}"
    "${PHYSICS_CHROME_HEADER}"
    "${PHYSICS_CHROME_SOURCE}"
    "${PHYSICS_WORKSPACE_SOURCE}"
    "${REUSABLE_TEST}"
    "${SOFTBODY_TEST}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "JP01 hardening contract input is missing: ${path}")
    endif()
endforeach()

file(READ "${COLLISION_HEADER}" collision_header)
file(READ "${COLLISION_SOURCE}" collision_source)
file(READ "${COMMAND_SOURCE}" command_source)
file(READ "${SOFTBODY_HEADER}" softbody_header)
file(READ "${PHYSICS_LUA_SOURCE}" physics_lua_source)
file(READ "${REUSABLE_HEADER}" reusable_header)
file(READ "${REUSABLE_INSTANCE_SOURCE}" reusable_instance_source)
file(READ "${PHYSICS_CHROME_HEADER}" physics_chrome_header)
file(READ "${PHYSICS_CHROME_SOURCE}" physics_chrome_source)
file(READ "${PHYSICS_WORKSPACE_SOURCE}" physics_workspace_source)
file(READ "${REUSABLE_TEST}" reusable_test)
file(READ "${SOFTBODY_TEST}" softbody_test)

function(require_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "JP01 hardening contract missing ${description}: ${needle}")
    endif()
endfunction()

function(forbid_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "JP01 hardening contract forbids ${description}: ${needle}")
    endif()
endfunction()

# Reusable assets are one creator-facing rigid body at their stable root. Every
# normal collision mutation route must resolve the imported child selection.
require_text(collision_header
    "FitPrimitiveCollisionStateToTarget("
    "primitive asset-root auto-fit API")
require_text(collision_header
    "RequestCollisionShapeRefresh("
    "scale-driven native-shape refresh API")
require_text(collision_header
    "bool autoFitPrimitive = true"
    "creator primitive auto-fit default")
require_text(collision_source
    "RelativeMatrixToTarget("
    "target-local descendant geometry measurement")
require_text(collision_source
    "if (current == target)"
    "root transform exclusion from fitted geometry")
require_text(collision_source
    "entity_(ResolveCollisionAuthoringTarget(scene, targetEntity))"
    "CreateCollisionCommand root resolution")
require_text(collision_source
    "entity_(ResolveCollisionAuthoringTarget(scene, entity))"
    "Set/Remove collision root resolution")
require_text(collision_source
    "if (autoFitPrimitive_)"
    "creator primitive auto-fit path")
require_text(collision_source
    "rigidbody->physicsobject.reset();"
    "native body invalidation before scaled-shape recreation")
require_text(collision_source
    "RepairReusableAssetInstanceNames(scene)"
    "load/save wrapper-name canonicalization")
forbid_text(collision_source
    "JPH::PhysicsSystem"
    "raw or second Jolt world ownership")

# Scale rebuild belongs to the central committed transform command. Execute,
# Undo and Redo share Apply(); do not restore the one-selected-entity polling
# workaround that missed other reusable instances.
require_text(command_source
    "IsMeaningfulScale(previousScale, transformState.scale)"
    "committed root-scale change detection")
require_text(command_source
    "scene_->rigidbodies.Contains(entity_)"
    "actual rigid-body owner scale boundary")
require_text(command_source
    "RequestCollisionShapeRefresh(*scene_, entity_)"
    "central collision-shape rebuild request")
forbid_text(physics_chrome_header
    "observedPhysicsScale"
    "single-body scale observation state")
forbid_text(physics_chrome_source
    "RefreshSelectedCollisionScale"
    "per-frame selected-body scale watcher")

# Physics Lab promotes imported children only on the three pages which operate
# on RigidBodyPhysicsComponent. Mesh/humanoid secondary pages keep legitimate
# descendant selection.
require_text(physics_chrome_source
    "page != Page::RigidBody"
    "Rigid Body owner-page scope")
require_text(physics_chrome_source
    "page != Page::Character"
    "Character owner-page scope")
require_text(physics_chrome_source
    "page != Page::Vehicle"
    "Vehicle owner-page scope")
require_text(physics_chrome_source
    "session->Selection().Select(target);"
    "asset-root selection transfer")
require_text(physics_chrome_source
    "Soft Body, Ragdoll and Secondary Collider"
    "non-rigidbody descendant selection preservation")

# Hierarchy roots use deterministic creator-facing names. Anonymous wrappers in
# old scenes are repaired, same-asset instances are suffixed, and a creator's
# explicit rename is never overwritten.
require_text(reusable_header
    "RepairReusableAssetInstanceNames"
    "legacy wrapper-name repair API")
require_text(reusable_header
    "ReusableAssetInstanceDisplayNameMetadataKey"
    "persisted import/product display title")
require_text(reusable_instance_source
    "NormalizeReusableAssetDisplayName"
    "import/product title normalization")
require_text(reusable_instance_source
    "std::string DeriveReusableAssetName("
    "creator-facing reusable root name derivation")
require_text(reusable_instance_source
    "MakeUniqueReusableAssetName"
    "same-asset unique root naming")
require_text(reusable_instance_source
    "base + \" (\" + std::to_string(suffix) + \")\""
    "creator-facing duplicate suffixing")
require_text(reusable_instance_source
    "Never overwrite an explicit creator-authored wrapper name."
    "creator rename preservation")
require_text(reusable_instance_source
    "ReusableAssetInstanceIdMetadataKey"
    "stable reusable identity metadata")
forbid_text(reusable_instance_source
    "name->name = \"Reusable Asset Instance\""
    "implementation wrapper label assignment")

# Creator-facing Physics Lab language remains Renegade-owned, while the actual
# backend debug visualizer stays available for inspecting the green collision
# shape rather than drawing a fake Renegade approximation.
require_text(physics_workspace_source
    "worldDebug.Create(\"PHYSICS VISUALIZER\")"
    "physics visualizer creator control")
require_text(physics_workspace_source
    "\"SECONDARY COLLIDER\""
    "secondary collider creator label")
forbid_text(physics_workspace_source
    "WICKED EDITOR PARITY"
    "developer parity wording in creator UI")
forbid_text(physics_workspace_source
    "JOLT-POWERED"
    "implementation branding in creator UI")
forbid_text(physics_workspace_source
    "WICKED COLLIDER // NOT JOLT"
    "developer collision-system banner in creator UI")

# The pinned public getter currently reports native enabled state while its
# setter consumes broken=true by disabling the constraint. Renegade corrects
# the public Lua meaning locally instead of patching the pinned dependency.
require_text(physics_lua_source
    "PushBool(L, !wi::physics::IsConstraintBroken(*constraint));"
    "correct constraint_broken Lua semantics")
require_text(physics_lua_source
    "wi::physics::SetConstraintBroken("
    "public constraint broken setter")

# Soft-body removal matches the editor's unrigged mesh cleanup but retains
# Renegade Undo fidelity and keeps GPU-free unit tests GPU-free.
foreach(field IN ITEMS
    "vertex_boneindices"
    "vertex_boneweights"
    "vertex_boneindices2"
    "vertex_boneweights2")
    require_text(softbody_header
        "mesh->${field}.clear();"
        "soft-body removal cleanup ${field}")
    require_text(softbody_header
        "mesh->${field} = removed"
        "soft-body Undo restoration ${field}")
endforeach()
require_text(softbody_header
    "wi::graphics::GetDevice() != nullptr"
    "GPU-free soft-body cleanup guard")
require_text(softbody_header
    "mesh.CreateRenderData();"
    "live editor mesh render-data rebuild")

# Headless regressions lock the actual owner workflow rather than only source
# topology: three same-asset instances, unique names, independent scale rebuild,
# Undo/Redo, migration refusal, Character/Vehicle safety and visualizer access.
require_text(reusable_test
    "root scale was baked into fitted collider dimensions"
    "no-double-scale regression")
require_text(reusable_test
    "std::array<ReusableFixture, 3> three"
    "three same-asset instance regression")
require_text(reusable_test
    "\"Imported Crate\", \"Imported Crate (2)\", \"Imported Crate (3)\""
    "import-title creator root-name regression")
require_text(reusable_test
    "scale Undo did not rebuild"
    "scale Undo rebuild regression")
require_text(reusable_test
    "scale Redo did not rebuild"
    "scale Redo rebuild regression")
require_text(reusable_test
    "ambiguous multi-body reusable hierarchy"
    "ambiguous recovery refusal regression")
require_text(reusable_test
    "Character authoring created or edited an unsafe nested body"
    "Character nested-body safety regression")
require_text(reusable_test
    "Vehicle authoring created or edited an unsafe nested body"
    "Vehicle nested-body safety regression")
require_text(reusable_test
    "visualizerOn.debugDrawEnabled = true"
    "collision visualizer regression")
require_text(softbody_test
    "soft-body removal left stale unrigged bone streams"
    "soft-body removal cleanup regression")
require_text(softbody_test
    "soft-body Remove Undo did not restore mesh bone streams"
    "soft-body mesh Undo regression")

message(STATUS "JP01 hardening source contract passed")
