if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(runtime_source
    "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeScriptRuntime.cpp")
set(runtime_header
    "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeScriptRuntime.h")
set(application_source
    "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeApplication.cpp")
set(application_header
    "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeApplication.h")
set(runtime_main
    "${RENEGADE_SOURCE_DIR}/Runtime/src/main_Windows.cpp")

file(READ "${runtime_source}" source)
file(READ "${runtime_header}" header)
file(READ "${application_source}" application)
file(READ "${application_header}" application_h)
file(READ "${runtime_main}" main_source)
set(core_text "${header}\n${source}")
set(integration_text "${application_h}\n${application}\n${main_source}")

foreach(required
    "lua_newstate"
    "LuaAllocate"
    "luaL_requiref"
    "luaopen_base"
    "luaopen_table"
    "luaopen_string"
    "luaopen_math"
    "luaopen_utf8"
    "luaL_loadbufferx"
    "LUA_MASKCOUNT"
    "lua_sethook"
    "activeInstructionBudget"
    "lua_pcall"
    "ScriptInstanceId"
    "scriptInstanceId"
    "environmentReference"
    "contextReference"
    "StartSceneFromCompanion"
    "ScriptDocumentPathForScene"
    "EntityRef"
    "generation"
    "renegade"
    "api_version"
    "on_start"
    "on_pause"
    "on_resume"
    "on_reset"
    "on_stop"
    "on_update"
    "hasOnUpdate"
    "Governed require()"
    "Advanced/Unsafe Lua execution is not enabled by S3"
)
    string(FIND "${core_text}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "S3 governed Lua Runtime contract is missing '${required}'")
    endif()
endforeach()

foreach(required
    "RuntimeScriptRuntime creatorScripts_"
    "SyncCreatorScriptsForScene"
    "StartSceneFromCompanion"
    "creatorScripts_.Update"
    "creatorScripts_.Pause"
    "creatorScripts_.Resume"
    "creatorScripts_.ResetScene"
    "StopCreatorScripts"
    "ReportCreatorScriptDiagnostics"
    "ShutdownForProcessExit"
    "application.ShutdownForProcessExit()"
)
    string(FIND "${integration_text}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "S3 Runtime lifecycle integration is missing '${required}'")
    endif()
endforeach()

foreach(forbidden
    "wi::lua::GetLuaState"
    "wi::lua::RunFile"
    "wi::lua::RunText"
    "luaL_openlibs"
    "luaopen_package"
    "luaopen_io"
    "luaopen_os"
    "luaopen_debug"
    "ScriptComponent"
    "native_id"
    "InstructionBudgetGuard"
)
    string(FIND "${core_text}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "S3 governed Lua Runtime must not contain '${forbidden}'")
    endif()
endforeach()

message(STATUS "S3 governed Lua Runtime source contract passed")
