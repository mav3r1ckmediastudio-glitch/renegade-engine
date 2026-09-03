if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(metadata_header
    "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/ScriptMetadataService.h")
set(metadata_source
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/ScriptMetadataService.cpp")

file(READ "${metadata_header}" header)
file(READ "${metadata_source}" source)
set(text "${header}\n${source}")

foreach(required
    "ScriptMetadataSchemaVersion"
    "ScriptMetadataDiagnostic"
    "ScriptMetadataPropertyDescriptor"
    "ScriptMetadataDescriptor"
    "EvaluateScriptMetadata"
    "ApplyScriptMetadataDefaults"
    "lua_newstate"
    "MetadataAllocate"
    "luaL_loadbufferx"
    "MetadataCapturedSentinel"
    "LUA_MASKCOUNT"
    "lua_sethook"
    "activeMetadataInstructionBudget"
    "CaptureMetadataLua"
    "renegade"
    "metadata"
    "schema_version"
    "ACTION"
    "SCRIPT"
    "GLOBAL SCRIPT"
    "reference_default_forbidden"
    "ValidateProjectScriptSource"
)
    string(FIND "${text}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "S4A metadata contract is missing '${required}'")
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
)
    string(FIND "${text}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "S4A metadata evaluator must not contain '${forbidden}'")
    endif()
endforeach()

message(STATUS "S4A metadata evaluator source contract passed")
