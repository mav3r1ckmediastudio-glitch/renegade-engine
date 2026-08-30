add_executable(RenegadePhase5Gate6RenderSettingsTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase5Gate6RenderSettingsTests.cpp
)

target_link_libraries(RenegadePhase5Gate6RenderSettingsTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(RenegadePhase5Gate6RenderSettingsTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadePhase5Gate6RenderSettingsTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadePhase5Gate6RenderSettingsTests
)

add_test(
    NAME RenegadePhase5Gate6RenderSettingsTests
    COMMAND RenegadePhase5Gate6RenderSettingsTests
)

add_test(
    NAME RenegadePhase5Gate6SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/Tests/Phase5Gate6SourceContract.cmake
)
