add_executable(RenegadePhase5Gate7RenderSettingsTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase5Gate7RenderSettingsTests.cpp
)

target_link_libraries(RenegadePhase5Gate7RenderSettingsTests
    PRIVATE Renegade::EngineBridge
)

target_compile_options(RenegadePhase5Gate7RenderSettingsTests
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadePhase5Gate7RenderSettingsTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_dependencies(RenegadeBridgeTests RenegadePhase5Gate7RenderSettingsTests)
add_test(NAME RenegadePhase5Gate7RenderSettingsTests COMMAND RenegadePhase5Gate7RenderSettingsTests)
add_test(
    NAME RenegadePhase5Gate7SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/Tests/Phase5Gate7SourceContract.cmake
)
