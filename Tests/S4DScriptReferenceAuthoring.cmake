add_executable(RenegadeS4DScriptReferenceAuthoringTests
    ${CMAKE_CURRENT_LIST_DIR}/S4DScriptReferenceAuthoringTests.cpp
)

target_link_libraries(
    RenegadeS4DScriptReferenceAuthoringTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeS4DScriptReferenceAuthoringTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeS4DScriptReferenceAuthoringTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeS4DScriptReferenceAuthoringTests
)

add_test(
    NAME RenegadeS4DScriptReferenceAuthoringTests
    COMMAND RenegadeS4DScriptReferenceAuthoringTests
)

add_test(
    NAME RenegadeS4DScriptReferenceAuthoringSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/S4DScriptReferenceAuthoringSourceContract.cmake
)
