if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

function(require_text path text label)
    file(READ "${path}" contents)
    string(FIND "${contents}" "${text}" index)
    if(index EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 1 missing ${label}: ${text}")
    endif()
endfunction()

set(player_header "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/PlayerService.h")
set(player_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/PlayerService.cpp")
set(runtime_source "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeApplication.cpp")
set(studio_source "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")
set(chrome_source "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioChrome.cpp")

require_text("${player_header}" "PlayerInputFrame" "action-shaped input boundary")
require_text("${player_header}" "CreatePlayerStartCommand" "command-backed Player Start")
require_text("${player_header}" "SetPlayerControllerSettingsCommand" "command-backed Player Start Inspector settings")
require_text("${player_source}" "MetadataComponent::Preset::Player" "native Player classification")
require_text("${player_source}" "MovePhysicsCharacter" "JP01 character controller reuse")
# Gate 1 owns the action-shaped player service and Runtime possession, not the
# raw device polling implementation. Gate 2 intentionally moves raw polling
# behind GameplayInputService, so this historical contract must not pin the
# removed RuntimeApplication::CapturePlayerInput helper.
require_text("${runtime_source}" "UpdateRuntimePlayer" "Runtime player update")
require_text("${runtime_source}" "SpawnRuntimePlayer" "Runtime possession")
require_text("${runtime_source}" "ApplyRuntimePlayerCamera" "Runtime camera ownership")
require_text("${studio_source}" "CreatePlayerStartFromView" "Studio authoring workflow")
require_text("${studio_source}" "HandlePlayerStartSceneIcon" "visible selectable Player Start arrow")
require_text("${studio_source}" "PLAYER START // FIRST PERSON" "dedicated Player Start Inspector")
require_text("${chrome_source}" "PLAYER START" "Renegade-owned Add menu entry")

message(STATUS "PASS: Phase 6 Gate 1 source contract")
