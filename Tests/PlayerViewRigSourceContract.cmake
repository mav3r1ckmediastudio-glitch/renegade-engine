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

set(view_rig "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/PlayerViewRig.h")
set(view_asset "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/PlayerViewAsset.h")
set(view_animation "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/PlayerViewAnimation.h")
set(runtime "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeApplication.cpp")
set(player_service "${RENEGADE_SOURCE_DIR}/EngineBridge/src/PlayerService.cpp")
set(reusable_dependency "${RENEGADE_SOURCE_DIR}/EngineBridge/src/ReusableAssetDependencyService.cpp")
set(reusable_runtime "${RENEGADE_SOURCE_DIR}/EngineBridge/src/ReusableAssetRuntimeService.cpp")
set(test_level_snapshot "${RENEGADE_SOURCE_DIR}/EngineBridge/src/TestLevelSnapshotService.cpp")
set(studio "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")

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
require_text("${view_animation}" "InitializeRuntimePlayerViewAnimations" "native view-model animation binding")
require_text("${view_animation}" "RequestRuntimePlayerViewAnimation" "semantic native animation request")
require_text("${view_animation}" "MatchingPlayerViewAnimationChannels" "native animation crossfade guard")
require_text("${view_animation}" "RootMotionOff" "view-model root-motion suppression")
require_text(
    "${view_rig}"
    "Component_Attach(created.root, created.playerEntity, true)"
    "authoritative Player parent")
require_text("${runtime}" "SpawnRuntimePlayerViewRig" "Runtime View Rig spawn")
require_text("${runtime}" "PoseRuntimePlayerViewRig" "Runtime View Rig pose")
require_text("${runtime}" "UpdateRuntimePlayerViewRigPresentation" "Runtime movement presentation")
require_text("${runtime}" "UpdateRuntimePlayerViewAnimations" "Runtime native view-model animation update")
require_text("${runtime}" "InitializeRuntimePlayerViewAnimations" "Runtime native view-model animation setup")
require_text("${runtime}" "DespawnRuntimePlayerViewRig" "Runtime View Rig cleanup")
require_text("${runtime}" "LoadRuntimePlayerViewAsset" "editor/Test Level governed arms load")
require_text("${runtime}" "LoadPackagedRuntimePlayerViewAsset" "packaged governed arms load")
require_text("${view_asset}" "CommitRuntimePlayerViewAsset" "real imported View Rig asset commit")
require_text("${view_asset}" "SanitizeRuntimePlayerViewModelHierarchy" "view-model hierarchy sanitization")
require_text("${view_asset}" "scene.characters.Remove(entity)" "second Character controller stripping")
require_text("${view_asset}" "scene.rigidbodies.Remove(entity)" "view-model rigid body stripping")
require_text("${view_asset}" "scene.colliders.Remove(entity)" "view-model collider stripping")
require_text("${view_asset}" "RemoveRuntimePlayerViewRigProofGeometry" "proxy replacement on real asset load")
require_text("${player_service}" "renegade.player.first_person_arms_asset_id" "persistent Player arms identity")
require_text("${reusable_dependency}" "p1.player_first_person_arms:" "packaging dependency provenance")
require_text("${reusable_runtime}" "PreparePackagedReusableAsset" "package manifest arms resolver")
require_text("${test_level_snapshot}" "SnapshotGovernedPlayerViewInputs" "Test Level arms snapshot")
require_text("${studio}" "Player First Person Arms" "creator-facing Player arms selector")
require_text("${studio}" "CommitSelectedPlayerArmsAsset" "Undo/Redo-backed Player arms authoring")

file(READ "${runtime}" runtime_text)
string(FIND "${runtime_text}" "PoseRuntimePlayerViewRig" rig_pose_pos)
string(FIND "${runtime_text}" "UpdateRuntimePlayerViewRigPresentation" rig_presentation_pos)
string(FIND "${runtime_text}" "UpdateRuntimePlayerViewAnimations" rig_animation_pos)
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

if(rig_animation_pos EQUAL -1 OR
   NOT rig_animation_pos LESS scene_update_pos)
    message(FATAL_ERROR
        "P1 Player View native animation must update before Wicked Scene update")
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
