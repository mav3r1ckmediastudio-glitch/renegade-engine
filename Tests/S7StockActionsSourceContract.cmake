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
string(FIND "${S7_MANIFEST_TEXT}" "\"package_version\": \"1.1.0\"" S7_PACKAGE_VERSION)
if(S7_PACKAGE_VERSION EQUAL -1)
    message(FATAL_ERROR "S7 repaired stock package must publish a new immutable version")
endif()

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
    "UiShowPromptLua"
    "show_prompt"
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
foreach(S7_SWITCH_TOKEN "was_pressed(\"interact\")" "renegade.ui.show_prompt")
    string(FIND "${S7_SWITCH}" "${S7_SWITCH_TOKEN}" S7_SWITCH_INTERACT)
    if(S7_SWITCH_INTERACT EQUAL -1)
        message(FATAL_ERROR "S7 Switch is missing interactive UX: ${S7_SWITCH_TOKEN}")
    endif()
endforeach()

file(READ "${S7_LIBRARY_DIR}/Door.lua" S7_DOOR)
foreach(S7_DOOR_TOKEN "was_pressed(\"interact\")" "renegade.ui.show_prompt" "auto_close")
    string(FIND "${S7_DOOR}" "${S7_DOOR_TOKEN}" S7_DOOR_INTERACTION)
    if(S7_DOOR_INTERACTION EQUAL -1)
        message(FATAL_ERROR "S7 Door is missing direct interaction UX: ${S7_DOOR_TOKEN}")
    endif()
endforeach()

file(READ "${S7_LIBRARY_DIR}/PlaySound.lua" S7_AUDIO_ACTION)
foreach(S7_AUDIO_TOKEN "renegade.audio.play" "renegade.audio.stop")
    string(FIND "${S7_AUDIO_ACTION}" "${S7_AUDIO_TOKEN}" S7_AUDIO_INDEX)
    if(S7_AUDIO_INDEX EQUAL -1)
        message(FATAL_ERROR "S7 Audio Action is missing ${S7_AUDIO_TOKEN}")
    endif()
endforeach()

file(READ
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/ScriptLibraryService.cpp"
    S7_LIBRARY_SERVICE)
string(FIND "${S7_LIBRARY_SERVICE}" "GetExecutablePath" S7_EXECUTABLE_ROOT)
if(S7_EXECUTABLE_ROOT EQUAL -1)
    message(FATAL_ERROR "S7 built-in library discovery is not executable-relative")
endif()

file(READ
    "${RENEGADE_SOURCE_DIR}/Studio/src/S4BScriptAttachmentInspector.cpp"
    S7_ACTION_INSPECTOR)
foreach(S7_INSPECTOR_TOKEN
    "EnsureEntityOwnerIdentity"
    "identity will be assigned on ADD"
    "controls.addSource.SetSize(XMFLOAT2(width, 28.0f))"
)
    string(FIND "${S7_ACTION_INSPECTOR}" "${S7_INSPECTOR_TOKEN}" S7_INSPECTOR_INDEX)
    if(S7_INSPECTOR_INDEX EQUAL -1)
        message(FATAL_ERROR "S7 Action Inspector repair is missing: ${S7_INSPECTOR_TOKEN}")
    endif()
endforeach()

file(READ
    "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioChrome.cpp"
    S7_STUDIO_CHROME)
string(FIND "${S7_STUDIO_CHROME}" "ElideText" S7_PICKER_ELISION)
if(S7_PICKER_ELISION EQUAL -1)
    message(FATAL_ERROR "S7 source picker labels are not bounded to their controls")
endif()

file(READ "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeApplication.cpp" S7_RUNTIME_APP)
string(FIND "${S7_RUNTIME_APP}" "SetInteractionPrompt" S7_PROMPT_RENDER)
if(S7_PROMPT_RENDER EQUAL -1)
    message(FATAL_ERROR "S7 Runtime does not render creator interaction prompts")
endif()

message(STATUS "S7 six-Action stock package source contract passed")
