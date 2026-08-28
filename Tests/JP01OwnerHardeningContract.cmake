if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(COLLISION_HEADER "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/CollisionService.h")
set(COLLISION_SOURCE "${RENEGADE_SOURCE_DIR}/EngineBridge/src/CollisionService.cpp")
set(COMMAND_SOURCE "${RENEGADE_SOURCE_DIR}/EngineBridge/src/CommandService.cpp")
set(REUSABLE_HEADER "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/ReusableAssetInstanceService.h")
set(REUSABLE_SOURCE "${RENEGADE_SOURCE_DIR}/EngineBridge/src/ReusableAssetInstanceService.cpp")
set(PHYSICS_CHROME_HEADER "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadePhysicsLabStudioChrome.h")
set(PHYSICS_CHROME_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadePhysicsLabStudioChrome.cpp")
set(PHYSICS_WORKSPACE_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadePhysicsLabWorkspace.cpp")
set(REUSABLE_TEST "${RENEGADE_SOURCE_DIR}/Tests/JP01ReusableAssetPhysicsTests.cpp")

foreach(path IN ITEMS
    "${COLLISION_HEADER}"
    "${COLLISION_SOURCE}"
    "${COMMAND_SOURCE}"
    "${REUSABLE_HEADER}"
    "${REUSABLE_SOURCE}"
    "${PHYSICS_CHROME_HEADER}"
    "${PHYSICS_CHROME_SOURCE}"
    "${PHYSICS_WORKSPACE_SOURCE}"
    "${REUSABLE_TEST}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "JP01 owner-hardening input missing: ${path}")
    endif()
endforeach()

file(READ "${COLLISION_HEADER}" collision_header)
file(READ "${COLLISION_SOURCE}" collision_source)
file(READ "${COMMAND_SOURCE}" command_source)
file(READ "${REUSABLE_HEADER}" reusable_header)
file(READ "${REUSABLE_SOURCE}" reusable_source)
file(READ "${PHYSICS_CHROME_HEADER}" physics_chrome_header)
file(READ "${PHYSICS_CHROME_SOURCE}" physics_chrome_source)
file(READ "${PHYSICS_WORKSPACE_SOURCE}" physics_workspace_source)
file(READ "${REUSABLE_TEST}" reusable_test)

function(require_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "JP01 owner hardening missing ${description}: ${needle}")
    endif()
endfunction()

function(forbid_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "JP01 owner hardening forbids ${description}: ${needle}")
    endif()
endfunction()

# Every rigid-body mutation route resolves imported descendants to the stable
# reusable root; no Create/Set/Remove command may author a nested body by accident.
require_text(collision_source
    "entity_(ResolveCollisionAuthoringTarget(scene, targetEntity))"
    "CreateCollisionCommand root resolution")
require_text(collision_source
    "entity_(ResolveCollisionAuthoringTarget(scene, entity))"
    "Set/Remove collision root resolution")
require_text(collision_source
    "FitPrimitiveCollisionStateToTarget(scene, entity_, fitted)"
    "default primitive auto-fit")
require_text(collision_header
    "bool autoFitPrimitive = true"
    "default auto-fit contract")

# Scale rebuild belongs to the central committed transform path so Execute,
# Undo and Redo all invalidate the same native body. The earlier single-selected
# polling workaround is explicitly forbidden.
require_text(command_source
    "IsMeaningfulScale(previousScale, transformState.scale)"
    "committed scale-change detection")
require_text(command_source
    "scene_->rigidbodies.Contains(entity_)"
    "actual body-owner scale boundary")
require_text(command_source
    "RequestCollisionShapeRefresh(*scene_, entity_)"
    "central native-shape refresh")
require_text(collision_source
    "rigidbody->physicsobject.reset();"
    "native body invalidation before recreation")
forbid_text(physics_chrome_header
    "observedPhysicsScale"
    "single selected-body scale tracker")
forbid_text(physics_chrome_source
    "RefreshSelectedCollisionScale"
    "per-frame scale polling")

# Rigid Body, Character and Vehicle share the body owner, but mesh/humanoid
# pages remain free to target their legitimate descendants.
require_text(physics_chrome_source
    "page != Page::RigidBody"
    "Rigid Body owner-page selection scope")
require_text(physics_chrome_source
    "page != Page::Character"
    "Character owner-page selection scope")
require_text(physics_chrome_source
    "page != Page::Vehicle"
    "Vehicle owner-page selection scope")
require_text(physics_chrome_source
    "Soft Body, Ragdoll and Secondary Collider"
    "non-rigidbody descendant-selection preservation")

# Repeated reusable instances have creator-facing, deterministic names and old
# anonymous wrappers are repaired without overwriting a creator rename.
require_text(reusable_header
    "RepairReusableAssetInstanceNames"
    "legacy wrapper-name repair API")
require_text(reusable_source
    "MakeUniqueReusableAssetName"
    "same-asset unique wrapper naming")
require_text(reusable_source
    "base + \" (\" + std::to_string(suffix) + \")\""
    "creator name suffixing")
require_text(reusable_source
    "Never overwrite an explicit creator-authored wrapper name."
    "custom wrapper-name preservation")
require_text(collision_source
    "RepairReusableAssetInstanceNames(scene)"
    "load/save naming canonicalization")

# The exact owner workflow is executable evidence, not just source topology.
require_text(reusable_test
    "std::array<ReusableFixture, 3> three"
    "three same-asset instance fixture")
require_text(reusable_test
    "\"crate002\", \"crate002 (2)\", \"crate002 (3)\""
    "three-instance creator names")
require_text(reusable_test
    "SetTransformCommand"
    "committed root-scale regression")
require_text(reusable_test
    "scale Undo did not rebuild"
    "Undo scale rebuild assertion")
require_text(reusable_test
    "scale Redo did not rebuild"
    "Redo scale rebuild assertion")
require_text(reusable_test
    "ambiguous multi-body reusable hierarchy"
    "ambiguous migration refusal")
require_text(reusable_test
    "Character authoring created or edited an unsafe nested body"
    "Character nested-body safety")
require_text(reusable_test
    "Vehicle authoring created or edited an unsafe nested body"
    "Vehicle nested-body safety")

# Creator must still be able to turn on the real backend collision visualizer to
# inspect the fitted green shape; do not replace it with a fake Renegade box.
require_text(physics_workspace_source
    "worldDebug.Create(\"PHYSICS VISUALIZER\")"
    "creator physics visualizer control")
require_text(reusable_test
    "visualizerOn.debugDrawEnabled = true"
    "visualizer backend regression")

# Creator-facing terminology stays Renegade-owned.
require_text(physics_workspace_source
    "\"SECONDARY COLLIDER\""
    "secondary collider label")
forbid_text(physics_workspace_source
    "WICKED EDITOR PARITY"
    "developer parity slogan in creator UI")
forbid_text(physics_workspace_source
    "JOLT-POWERED"
    "implementation branding in creator UI")
forbid_text(physics_workspace_source
    "WICKED COLLIDER // NOT JOLT"
    "developer collision-system banner in creator UI")

message(STATUS "JP01 owner hardening contract passed")
