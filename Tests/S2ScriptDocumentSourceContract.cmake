set(header "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/ScriptDocumentService.h")
set(service "${RENEGADE_SOURCE_DIR}/EngineBridge/src/ScriptDocumentService.cpp")
set(edits "${RENEGADE_SOURCE_DIR}/EngineBridge/src/ScriptDocumentEdits.cpp")
set(bridge_cmake "${RENEGADE_SOURCE_DIR}/EngineBridge/S2ScriptDocument.cmake")
set(tests "${RENEGADE_SOURCE_DIR}/Tests/S2ScriptDocumentTests.cpp")

foreach(path IN ITEMS "${header}" "${service}" "${edits}" "${bridge_cmake}" "${tests}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "S2 source contract file is missing: ${path}")
    endif()
endforeach()

file(READ "${header}" header_text)
file(READ "${service}" service_text)
file(READ "${edits}" edits_text)
file(READ "${bridge_cmake}" bridge_cmake_text)

foreach(token IN ITEMS
    "ScriptDocumentSuffix = \".rscripts\""
    "ScriptInstanceId"
    "ScriptScope"
    "ScriptPresentation"
    "ScriptPropertyType"
    "EntityReference"
    "AssetReference"
    "Animation"
    "Audio"
    "GlobalScript"
    "DuplicateEntityScriptAttachments"
    "MakeAddScriptAttachmentCommand"
    "MakeReplaceScriptSourceCommand")
    string(FIND "${header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "S2 header contract is missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "return \"ACTION\";"
    "return \"SCRIPT\";"
    "return \"GLOBAL SCRIPT\";"
    "Game script scope is reserved but has no schema-v1 semantics."
    "WriteTransactionalDocument("
    "ValidateProjectScriptSource("
    "Script sources may not traverse symbolic links."
    "sourcePath must remain under Content/Scripts"
    "Entity-reference properties intentionally are not resolved here.")
    string(FIND "${service_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "S2 persistence/source contract is missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "NormalizeScriptAttachmentOrder"
    "DuplicateScriptInstance"
    "GenerateStableId()"
    "ScriptDocumentMutationCommand"
    "MakeSetScriptEnabledCommand"
    "MakeMoveScriptAttachmentCommand"
    "MakeSetScriptPropertyCommand")
    string(FIND "${edits_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "S2 edit/command contract is missing: ${token}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
    "ScriptComponent"
    "native_id"
    "lua_State"
    "luaL_"
    "renegade.entity.native_id")
    string(FIND "${header_text}${service_text}${edits_text}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "S2 must not introduce Runtime/Wicked Lua authority: ${forbidden}")
    endif()
endforeach()

string(FIND "${header_text}${service_text}${edits_text}" ".rmeta" rmeta_found)
if(NOT rmeta_found EQUAL -1)
    message(FATAL_ERROR "S2 creator scripting semantics must not be stored in .rmeta")
endif()

foreach(token IN ITEMS
    "ScriptDocumentService.h"
    "ScriptDocumentService.cpp"
    "ScriptDocumentEdits.cpp")
    string(FIND "${bridge_cmake_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "S2 bridge source registration is missing: ${token}")
    endif()
endforeach()
