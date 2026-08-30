add_executable(RenegadePhase5Gate5RenderSettingsTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase5Gate5RenderSettingsTests.cpp
)

target_link_libraries(RenegadePhase5Gate5RenderSettingsTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(RenegadePhase5Gate5RenderSettingsTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadePhase5Gate5RenderSettingsTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadePhase5Gate5RenderSettingsTests
)

add_test(
    NAME RenegadePhase5Gate5RenderSettingsTests
    COMMAND RenegadePhase5Gate5RenderSettingsTests
)

add_executable(RenegadePhase5Gate5LutTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase5Gate5LutTests.cpp
)

target_link_libraries(RenegadePhase5Gate5LutTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(RenegadePhase5Gate5LutTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadePhase5Gate5LutTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadePhase5Gate5LutTests
)

add_test(
    NAME RenegadePhase5Gate5LutTests
    COMMAND RenegadePhase5Gate5LutTests
)

add_test(
    NAME RenegadePhase5Gate5SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/Tests/Phase5Gate5SourceContract.cmake
)
