add_executable(RenegadePhase5Gate1SceneComponentTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase5Gate1SceneComponentTests.cpp
)

target_link_libraries(RenegadePhase5Gate1SceneComponentTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(RenegadePhase5Gate1SceneComponentTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadePhase5Gate1SceneComponentTests PROPERTIES
    FOLDER "Renegade/Tests"
)

# Studio CI builds RenegadeBridgeTests explicitly before CTest. Keep the Gate 1
# executable in that dependency chain so the owner regression cannot be skipped.
add_dependencies(
    RenegadeBridgeTests
    RenegadePhase5Gate1SceneComponentTests
)

add_test(
    NAME RenegadePhase5Gate1SceneComponentTests
    COMMAND RenegadePhase5Gate1SceneComponentTests
)

add_test(
    NAME RenegadePhase5Gate1SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/Phase5Gate1SourceContract.cmake
)
