if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

function(require_text path text label)
    file(READ "${path}" contents)
    string(FIND "${contents}" "${text}" index)
    if(index EQUAL -1)
        message(FATAL_ERROR "P1 Player View Rig missing ${label}: ${text}")
    endif()
endfunction()

function(forbid_text path text label)
    file(READ "${path}" contents)
    string(FIND "${contents}" "${text}" index)
    if(NOT index EQUAL -1)
        message(FATAL_ERROR "P1 Player View Rig contains forbidden ${label}: ${text}")
    endif()
endfunction()

set(view_rig "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimePlayerViewRig.h")
set(runtime "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeApplication.cpp")

require_text("${view_rig}" "primaryHandSocket" "primary hand socket")
require_text("${view_rig}" "offHandSocket" "off-hand socket")
require_text("${view_rig}" "twoHandSupportSocket" "two-hand support socket")
require_text("${view_rig}" "presentationRoot" "procedural presentation layer")
require_text("${view_rig}" "SetForeground(true)" "Wicked foreground rendering")
require_text("${view_rig}" "SetCastShadow(false)" "view-model shadow separation")
require_text("${view_rig}" "SetNotVisibleInReflections(true)" "reflection separation")
require_text("${view_rig}" "ConfigureRuntimeViewModelHierarchy" "foreground hierarchy policy")
require_text("${view_rig}" "AttachRuntimeViewModelHierarchy" "real asset attachment seam")
require_text("${view_rig}" "ResolvePlayerViewAction" "semantic Idle/Walk/Sprint resolver")
require_text("${view_rig}" "UpdateRuntimePlayerViewRigPresentation" "semantic movement presentation")
require_text(
    "${view_rig}"
    "Component_Attach(created.root, created.playerEntity, true)"
    "authoritative Player parent")
require_text("${runtime}" "SpawnRuntimePlayerViewRig" "Runtime View Rig spawn")
require_text("${runtime}" "PoseRuntimePlayerViewRig" "Runtime View Rig pose")
require_text("${runtime}" "UpdateRuntimePlayerViewRigPresentation" "Runtime movement presentation")
require_text("${runtime}" "DespawnRuntimePlayerViewRig" "Runtime View Rig cleanup")

file(READ "${runtime}" runtime_text)
string(FIND "${runtime_text}" "PoseRuntimePlayerViewRig" rig_pose_pos)
string(FIND "${runtime_text}" "UpdateRuntimePlayerViewRigPresentation" rig_presentation_pos)
string(FIND "${runtime_text}" "wi::Application::Update(paused_ ? 0.0f : dt);" scene_update_pos)
string(FIND "${runtime_text}" "bridge::ApplyRuntimePlayerCamera" camera_pos)

if(rig_pose_pos EQUAL -1 OR scene_update_pos EQUAL -1 OR
   NOT rig_pose_pos LESS scene_update_pos)
    message(FATAL_ERROR
        "P1 Player View Rig look pose must be authored before Wicked Scene update")
endif()

if(rig_presentation_pos EQUAL -1 OR
   NOT rig_presentation_pos LESS scene_update_pos)
    message(FATAL_ERROR
        "P1 Player View Rig presentation must update before Wicked Scene update")
endif()

if(camera_pos EQUAL -1 OR NOT scene_update_pos LESS camera_pos)
    message(FATAL_ERROR
        "P1 Player camera must sample the post-physics Player position")
endif()

# P1 presentation follows the accepted Player hierarchy and must not become a
# second physics/controller/render-camera implementation.
forbid_text(
    "${view_rig}"
    "RigidBodyPhysicsComponent"
    "second player rigid body")
forbid_text(
    "${view_rig}"
    "MovePhysicsCharacter"
    "second player movement controller")
forbid_text(
    "${view_rig}"
    "CharacterComponent"
    "second player Character controller")
forbid_text(
    "${view_rig}"
    "CameraComponent"
    "camera-owned View Rig world transform")

message(STATUS "PASS: P1 parented Player View Rig source contract")
