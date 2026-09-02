if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(service_header "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/GameplayScriptService.h")
set(service_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/GameplayScriptService.cpp")
set(runtime_source "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeApplication.cpp")
set(snapshot_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/TestLevelSnapshotService.cpp")
set(studio_source "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")
set(chrome_source "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioChrome.cpp")

foreach(path IN ITEMS "${service_header}" "${service_source}"
    "${runtime_source}" "${snapshot_source}" "${studio_source}" "${chrome_source}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Phase 6 Gate 4 missing required source: ${path}")
    endif()
endforeach()

file(READ "${snapshot_source}" snapshot_text)
foreach(token IN ITEMS
    "StageGameplayScripts"
    "ResolveGameplayScriptPath"
    "snapshotRoot")
    string(FIND "${snapshot_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 4 Test Level staging missing ${token}")
    endif()
endforeach()

file(READ "${service_header}" header_text)
foreach(token IN ITEMS
    "GameplayScriptRuntime"
    "CreateGameplayScriptCommand"
    "GameplayScriptDiagnostic"
    "renegade.gameplay.script"
    "renegade.gameplay.script_path"
    "ImportGameplayScript"
    "ValidateGameplayScriptSyntax"
    "ResolveGameplayScriptPath")
    string(FIND "${header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 4 bridge contract missing ${token}")
    endif()
endforeach()

file(READ "${service_source}" service_text)
foreach(token IN ITEMS
    "Content/Scripts"
    "PrepareGameplayScriptsForRuntime"
    "luaL_loadbuffer"
    "luaL_ref"
    "on_start"
    "on_update"
    "on_pause"
    "on_resume"
    "on_reset"
    "on_stop"
    "contract_version"
    "native_id"
    "AssignNewPersistentEntityId"
    "IsRenegadeSoundSource"
    "GetPhysicsPosition")
    string(FIND "${service_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 4 lifecycle/API mapping missing ${token}")
    endif()
endforeach()

file(READ "${runtime_source}" runtime_text)
foreach(token IN ITEMS
    "SyncGameplayScriptsForScene"
    "PrepareGameplayScriptsForRuntime"
    "gameplayScripts_.Update"
    "gameplayScripts_.Pause"
    "gameplayScripts_.Resume"
    "gameplayScripts_.Reset"
    "ReportGameplayScriptDiagnostics")
    string(FIND "${runtime_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 4 Runtime lifecycle missing ${token}")
    endif()
endforeach()

# Wicked executes native playing ScriptComponents inside Application::Update.
# Renegade must disable that path first on every Runtime frame.
string(FIND "${runtime_text}" "PrepareGameplayScriptsForRuntime" prepare_position)
string(FIND "${runtime_text}" "wi::Application::Update" wicked_update_position)
if(prepare_position EQUAL -1 OR wicked_update_position EQUAL -1 OR
    prepare_position GREATER wicked_update_position)
    message(FATAL_ERROR
        "Phase 6 Gate 4 did not suppress native script execution before Wicked update")
endif()

file(READ "${studio_source}" studio_text)
file(READ "${chrome_source}" chrome_text)
foreach(token IN ITEMS
    "ChooseGameplayScript"
    "ImportGameplayScript"
    "CreateGameplayScriptCommand"
    "SetGameplayScriptEnabledCommand"
    "GAMEPLAY SCRIPT // LUA LIFECYCLE")
    string(FIND "${studio_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 4 Studio workflow missing ${token}")
    endif()
endforeach()
string(FIND "${chrome_text}" "GAMEPLAY SCRIPT..." add_menu)
if(add_menu EQUAL -1)
    message(FATAL_ERROR "Phase 6 Gate 4 ADD menu is missing GAMEPLAY SCRIPT...")
endif()

# Renegade owns script execution. A governed attachment must never opt into
# Wicked's independent whole-file-every-frame ScriptComponent lifecycle.
string(FIND "${service_source}" "nativeScript.Play" competing_play)
if(NOT competing_play EQUAL -1)
    message(FATAL_ERROR "Phase 6 Gate 4 enabled a competing Wicked script owner")
endif()
string(FIND "${service_source}" "luaL_newstate" second_vm)
if(NOT second_vm EQUAL -1)
    message(FATAL_ERROR "Phase 6 Gate 4 created a second Lua VM")
endif()
string(FIND "${service_source}" "native_entity" raw_context_handle)
if(NOT raw_context_handle EQUAL -1)
    message(FATAL_ERROR "Phase 6 Gate 4 leaked a native handle into callback context")
endif()
