add_executable(RenegadeS4BScriptAuthoringTests
    ${CMAKE_CURRENT_LIST_DIR}/S4BScriptAuthoringTests.cpp
)

target_link_libraries(
    RenegadeS4BScriptAuthoringTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeS4BScriptAuthoringTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeS4BScriptAuthoringTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeS4BScriptAuthoringTests
)

add_test(
    NAME RenegadeS4BScriptAuthoringTests
    COMMAND RenegadeS4BScriptAuthoringTests
)

add_test(
    NAME RenegadeS4BScriptAuthoringSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/S4BScriptAuthoringSourceContract.cmake
)
