if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

function(read_required relative output)
    set(path "${RENEGADE_SOURCE_DIR}/${relative}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "S5A source contract missing ${relative}")
    endif()
    file(READ "${path}" text)
    set(${output} "${text}" PARENT_SCOPE)
endfunction()

function(require_text text_var needle description)
    string(FIND "${${text_var}}" "${needle}" index)
    if(index EQUAL -1)
        message(FATAL_ERROR "S5A source contract missing ${description}: ${needle}")
    endif()
endfunction()

function(forbid_text text_var needle description)
    string(FIND "${${text_var}}" "${needle}" index)
    if(NOT index EQUAL -1)
        message(FATAL_ERROR "S5A source contract forbids ${description}: ${needle}")
    endif()
endfunction()

read_required("CMakeLists.txt" root_cmake)
read_required("Runtime/src/RuntimeScriptEntityApi.h" api_header)
read_required("Runtime/src/RuntimeScriptEntityApi.cpp" api_source)
read_required("Runtime/src/RuntimeScriptRuntime.cpp" runtime_source)
read_required("Studio/src/S4BScriptAttachmentInspector.cpp" inspector_source)
read_required("Tests/S4BScriptAuthoringTests.cpp" authoring_test)
read_required("Tests/S5CoreGameplayApi.cmake" test_cmake)
read_required("Tests/S5CoreGameplayApiTests.cpp" api_test)
read_required("Tests/S5CoreGameplayLuaTests.cpp" lua_test)
read_required("Tests/S5BGameplayLifecycleTests.cpp" s5b_test)

require_text(root_cmake "include(Runtime/S5CoreGameplayApi.cmake)" "Runtime S5A build registration")
require_text(root_cmake "include(Tests/S5CoreGameplayApi.cmake)" "S5A acceptance registration")

foreach(required IN ITEMS
    "RuntimeScriptEntityReference"
    "RuntimeScriptEntityApi"
    "std::uint64_t generation"
    "GetName"
    "GetLocalPosition"
    "SetLocalPosition"
    "TranslateLocal")
    require_text(api_header "${required}" "core entity/transform API seam")
endforeach()

foreach(required IN ITEMS
    "PersistentEntityId"
    "reference.generation != generation_"
    "Transform position must contain finite numbers."
    "Transform delta must contain finite numbers."
    "transform->SetDirty()"
    "transform->UpdateTransform()")
    require_text(api_source "${required}" "generation/liveness/finite-value/transform propagation hardening")
endforeach()

foreach(required IN ITEMS
    "#include \"RuntimeScriptEntityApi.h\""
    "EntityGetNameLua"
    "TransformGetLocalPositionLua"
    "TransformSetLocalPositionLua"
    "TransformTranslateLocalLua"
    "\"get_name\""
    "\"get_local_position\""
    "\"set_local_position\""
    "\"translate_local\""
    "\"transform\"")
    require_text(runtime_source "${required}" "governed renegade.* Lua registration")
endforeach()

require_text(runtime_source "RuntimeScriptEntityApi api" "Lua callbacks routed through S5A safety service")
require_text(runtime_source "reference.generation = payload->generation" "opaque S3 generation projection")
require_text(runtime_source "lua_rawget" "vector input read without creator metamethod execution")

require_text(test_cmake "RenegadeS5CoreGameplayLuaTests" "governed Lua behavioural target")
require_text(api_test "GetPosition()" "C++ acceptance checks propagated Wicked world position")
require_text(lua_test "renegade.entity.get_name" "creator-safe entity name owner path")
require_text(lua_test "renegade.transform.set_local_position" "creator transform write owner path")
require_text(lua_test "renegade.transform.translate_local" "creator transform translation owner path")
require_text(lua_test "GetPosition()" "Lua acceptance checks visible Wicked world position")
require_text(lua_test "move the barrel visibly" "visible barrel-movement acceptance")

foreach(required IN ITEMS
    "PlayerIsPresentLua"
    "PlayerGetPositionLua"
    "PlayerGetForwardLua"
    "InputGetAxisLua"
    "InputIsDownLua"
    "InputWasPressedLua"
    "\"player\""
    "\"input\"")
    require_text(runtime_source "${required}" "S5B governed gameplay registration")
endforeach()

require_text(s5b_test "renegade.player.get_position" "player state acceptance")
require_text(s5b_test "renegade.input.was_pressed" "input edge acceptance")
require_text(s5b_test "runtime.Pause" "lifecycle pause/resume acceptance")

# S5A owner testing exposed a confusing S4B source chooser reset. The chooser
# must retain its selected source across Scene/Story Flow refreshes, while the
# actual attachment persistence remains S2 sourceId/sourcePath authoritative.
require_text(inspector_source "selectedSourcePath" "SCRIPT source chooser refresh continuity")
require_text(authoring_test "Story Flow-style reopen" "real Level reopen persistence regression")
require_text(authoring_test "sourcePath == scriptPath" "exact SCRIPT source path persistence")
require_text(authoring_test "sourceId == scriptSourceId" "exact SCRIPT source identity persistence")

foreach(forbidden IN ITEMS
    "native_id"
    "GetEntity()"
    "wi::lua::")
    forbid_text(api_header "${forbidden}" "raw/Wicked authority in S5A header")
    forbid_text(api_source "${forbidden}" "raw/Wicked authority in S5A service")
endforeach()

forbid_text(runtime_source "referenceId.c_str" "raw stable reference ID presentation")

message(STATUS "S5A core entity/transform API source contract passed")
