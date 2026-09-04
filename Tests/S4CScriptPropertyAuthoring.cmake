add_executable(RenegadeS4CScriptPropertyAuthoringTests
    ${CMAKE_CURRENT_LIST_DIR}/S4CScriptPropertyAuthoringTests.cpp
)

target_link_libraries(
    RenegadeS4CScriptPropertyAuthoringTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeS4CScriptPropertyAuthoringTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeS4CScriptPropertyAuthoringTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeS4CScriptPropertyAuthoringTests
)

add_test(
    NAME RenegadeS4CScriptPropertyAuthoringTests
    COMMAND RenegadeS4CScriptPropertyAuthoringTests
)

add_test(
    NAME RenegadeS4CScriptPropertyAuthoringSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/S4CScriptPropertyAuthoringSourceContract.cmake
)
