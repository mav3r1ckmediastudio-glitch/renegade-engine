add_executable(RenegadePhase6Gate1PlayerTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase6Gate1PlayerTests.cpp
)

target_link_libraries(RenegadePhase6Gate1PlayerTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(RenegadePhase6Gate1PlayerTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadePhase6Gate1PlayerTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadePhase6Gate1PlayerTests
)

add_test(
    NAME RenegadePhase6Gate1PlayerTests
    COMMAND RenegadePhase6Gate1PlayerTests
)

add_test(
    NAME RenegadePhase6Gate1SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/Phase6Gate1SourceContract.cmake
)

