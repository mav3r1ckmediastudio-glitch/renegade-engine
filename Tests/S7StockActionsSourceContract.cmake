if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(S7_LIBRARY_DIR "${RENEGADE_SOURCE_DIR}/Library/Scripts/RenegadeStockActions")
set(S7_ACTIONS
    Door.lua
    Switch.lua
    TriggerZone.lua
    Pickup.lua
    Relay.lua
    PlaySound.lua
)

set(S7_MANIFEST "${S7_LIBRARY_DIR}/renegade-script-package.json")
if(NOT EXISTS "${S7_MANIFEST}")
    message(FATAL_ERROR "S7 stock Action package manifest is missing")
endif()
file(READ "${S7_MANIFEST}" S7_MANIFEST_TEXT)

foreach(S7_ACTION IN LISTS S7_ACTIONS)
    set(S7_PATH "${S7_LIBRARY_DIR}/${S7_ACTION}")
    if(NOT EXISTS "${S7_PATH}")
        message(FATAL_ERROR "S7 stock Action is missing: ${S7_ACTION}")
    endif()
    string(FIND "${S7_MANIFEST_TEXT}" "\"${S7_ACTION}\"" S7_MANIFEST_INDEX)
    if(S7_MANIFEST_INDEX EQUAL -1)
        message(FATAL_ERROR "S7 manifest does not declare ${S7_ACTION}")
    endif()

    file(READ "${S7_PATH}" S7_SOURCE)
    string(FIND "${S7_SOURCE}" "role = \"ACTION\"" S7_ROLE_INDEX)
    if(S7_ROLE_INDEX EQUAL -1)
        message(FATAL_ERROR "S7 script is not presented as an ACTION: ${S7_ACTION}")
    endif()
    foreach(S7_FORBIDDEN "Wicked" "wi::" "g_Entity" "MAXLua" "GetEntity" "native_id")
        string(FIND "${S7_SOURCE}" "${S7_FORBIDDEN}" S7_FORBIDDEN_INDEX)
        if(NOT S7_FORBIDDEN_INDEX EQUAL -1)
            message(FATAL_ERROR
                "S7 Action leaks forbidden/native scripting surface '${S7_FORBIDDEN}': ${S7_ACTION}")
        endif()
    endforeach()
endforeach()

file(READ "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeScriptRuntime.cpp" S7_RUNTIME)
foreach(S7_RUNTIME_TOKEN
    "get_world_position"
    "get_local_scale"
    "set_local_scale"
    "AudioPlayLua"
    "AudioStopLua"
    "GameplayAction::Interact"
)
    string(FIND "${S7_RUNTIME}" "${S7_RUNTIME_TOKEN}" S7_RUNTIME_INDEX)
    if(S7_RUNTIME_INDEX EQUAL -1)
        message(FATAL_ERROR "S7 Runtime seam is missing: ${S7_RUNTIME_TOKEN}")
    endif()
endforeach()

file(READ
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/GameplayInputService.cpp"
    S7_INPUT)
string(FIND "${S7_INPUT}" "{GameplayAction::Interact, \"E\", \"\", \"\"}" S7_INTERACT_DEFAULT)
if(S7_INTERACT_DEFAULT EQUAL -1)
    message(FATAL_ERROR "S7 Interact action is not bound to E by default")
endif()
string(FIND "${S7_INPUT}" "if (!seen[interactIndex])" S7_LEGACY_INTERACT)
if(S7_LEGACY_INTERACT EQUAL -1)
    message(FATAL_ERROR "S7 Interact does not preserve pre-S7 version-1 input maps")
endif()

file(READ "${S7_LIBRARY_DIR}/Switch.lua" S7_SWITCH)
string(FIND "${S7_SWITCH}" "was_pressed(\"interact\")" S7_SWITCH_INTERACT)
if(S7_SWITCH_INTERACT EQUAL -1)
    message(FATAL_ERROR "S7 Switch does not use the creator-facing Interact action")
endif()

file(READ "${S7_LIBRARY_DIR}/PlaySound.lua" S7_AUDIO_ACTION)
foreach(S7_AUDIO_TOKEN "renegade.audio.play" "renegade.audio.stop")
    string(FIND "${S7_AUDIO_ACTION}" "${S7_AUDIO_TOKEN}" S7_AUDIO_INDEX)
    if(S7_AUDIO_INDEX EQUAL -1)
        message(FATAL_ERROR "S7 Audio Action is missing ${S7_AUDIO_TOKEN}")
    endif()
endforeach()

message(STATUS "S7 six-Action stock package source contract passed")
