if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(COLLISION_HEADER "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/CollisionService.h")
set(COLLISION_SOURCE "${RENEGADE_SOURCE_DIR}/EngineBridge/src/CollisionService.cpp")
set(SOFTBODY_HEADER "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/SoftBodyPhysicsService.h")
set(PHYSICS_LUA_SOURCE "${RENEGADE_SOURCE_DIR}/EngineBridge/src/PhysicsLuaService.cpp")
set(REUSABLE_INSTANCE_SOURCE "${RENEGADE_SOURCE_DIR}/EngineBridge/src/ReusableAssetInstanceService.cpp")
set(PHYSICS_CHROME_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadePhysicsLabStudioChrome.cpp")
set(REUSABLE_TEST "${RENEGADE_SOURCE_DIR}/Tests/JP01ReusableAssetPhysicsTests.cpp")
set(SOFTBODY_TEST "${RENEGADE_SOURCE_DIR}/Tests/SoftBodyPhysicsTests.cpp")

foreach(path IN ITEMS
    "${COLLISION_HEADER}"
    "${COLLISION_SOURCE}"
    "${SOFTBODY_HEADER}"
    "${PHYSICS_LUA_SOURCE}"
    "${REUSABLE_INSTANCE_SOURCE}"
    "${PHYSICS_CHROME_SOURCE}"
    "${REUSABLE_TEST}"
    "${SOFTBODY_TEST}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "JP01 hardening contract input is missing: ${path}")
    endif()
endforeach()

file(READ "${COLLISION_HEADER}" collision_header)
file(READ "${COLLISION_SOURCE}" collision_source)
file(READ "${SOFTBODY_HEADER}" softbody_header)
file(READ "${PHYSICS_LUA_SOURCE}" physics_lua_source)
file(READ "${REUSABLE_INSTANCE_SOURCE}" reusable_instance_source)
file(READ "${PHYSICS_CHROME_SOURCE}" physics_chrome_source)
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

# Reusable assets are one creator-facing rigid body at their stable root.
require_text(collision_header
    "FitPrimitiveCollisionStateToTarget("
    "primitive asset-root auto-fit API")
require_text(collision_header
    "RequestCollisionShapeRefresh("
    "scale-driven shape refresh API")
require_text(collision_source
    "ResolveCollisionAuthoringTarget(scene, entity)"
    "root resolution before primitive fit")
require_text(collision_source
    "RelativeMatrixToTarget("
    "target-local descendant geometry measurement")
require_text(collision_source
    "if (current == target)"
    "root transform exclusion from fitted geometry")
require_text(collision_source
    "rigidbody->SetRefreshParametersNeeded(true);"
    "backend-owned live shape refresh")
require_text(collision_source
    "if (autoFitPrimitive_)"
    "creator primitive auto-fit default path")
forbid_text(collision_source
    "JPH::PhysicsSystem"
    "raw or second Jolt world ownership")

# Physics Lab must promote imported children to the whole asset before authoring
# and observe root scale so the existing backend body is rebuilt after scaling.
require_text(physics_chrome_source
    "if (target != selected &&"
    "unconditional child-to-root selection promotion")
require_text(physics_chrome_source
    "session->Selection().Select(target);"
    "asset-root selection transfer")
require_text(physics_chrome_source
    "RefreshSelectedCollisionScale();"
    "root scale watcher")
require_text(physics_chrome_source
    "bridge::RequestCollisionShapeRefresh(scene, target)"
    "scale-change body refresh request")
forbid_text(physics_chrome_source
    "!scene.rigidbodies.Contains(selected) &&"
    "old body-dependent selection promotion gate")

# Hierarchy root naming comes from creator-facing payload content, not the
# implementation wrapper label. Stable metadata remains the identity authority.
require_text(reusable_instance_source
    "std::string DeriveReusableAssetName("
    "creator-facing reusable root name derivation")
require_text(reusable_instance_source
    "ApplyReusableAssetName(*scene_, entity_, payloadRoot_);"
    "placement root renaming")
require_text(reusable_instance_source
    "ReusableAssetInstanceIdMetadataKey"
    "stable reusable identity metadata")
forbid_text(reusable_instance_source
    "name->name = \"Reusable Asset Instance\""
    "implementation wrapper label assignment")

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

# Headless regressions lock the behavioural contracts, not only source shape.
require_text(reusable_test
    "root scale was baked into fitted collider dimensions"
    "no-double-scale regression")
require_text(reusable_test
    "root-owned rigid body"
    "root-only rigid-body regression")
require_text(reusable_test
    "reusable root retained an implementation-facing name"
    "creator-facing root-name regression")
require_text(softbody_test
    "soft-body removal left stale unrigged bone streams"
    "soft-body removal cleanup regression")
require_text(softbody_test
    "soft-body Remove Undo did not restore mesh bone streams"
    "soft-body mesh Undo regression")

message(STATUS "JP01 hardening source contract passed")
