if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(api_header "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeScriptEntityApi.h")
set(api_source "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeScriptEntityApi.cpp")
set(runtime_source "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeScriptRuntime.cpp")

foreach(path IN ITEMS "${api_header}" "${api_source}" "${runtime_source}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "S5 core gameplay API source contract is missing: ${path}")
    endif()
endforeach()

file(READ "${api_header}" header)
file(READ "${api_source}" source)
file(READ "${runtime_source}" runtime)

foreach(required IN ITEMS
    "RuntimeScriptEntityReference"
    "RuntimeScriptEntityApi"
    "std::uint64_t generation"
    "GetLocalPosition"
    "SetLocalPosition"
    "TranslateLocal")
    string(FIND "${header}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "S5 core gameplay API header is missing required seam: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
    "PersistentEntityId"
    "reference.generation != generation_"
    "Transform position must contain finite numbers."
    "Transform delta must contain finite numbers."
    "transform->SetDirty()")
    string(FIND "${source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "S5 core gameplay API source is missing required hardening: ${required}")
    endif()
endforeach()

# S5 may consume the existing opaque S3 userdata, but it must not regress the
# creator boundary by exposing engine/native identity or borrowing Wicked's
# global Lua VM.
foreach(forbidden IN ITEMS
    "native_id"
    "GetEntity()"
    "wi::lua::")
    string(FIND "${header}${source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "S5 core gameplay API exposes forbidden authority: ${forbidden}")
    endif()
endforeach()

string(FIND "${runtime}" "Renegade.EntityRef" entity_ref)
string(FIND "${runtime}" "generation" generation)
string(FIND "${runtime}" "PersistentEntityId" persistent)
if(entity_ref EQUAL -1 OR generation EQUAL -1 OR persistent EQUAL -1)
    message(FATAL_ERROR "S5 prep no longer sits on S3's opaque generation-aware EntityRef authority")
endif()

message(STATUS "S5 core gameplay API source contract passed")
