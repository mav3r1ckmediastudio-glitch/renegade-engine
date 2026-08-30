add_executable(RenegadePhase5Gate8LightmapBakeTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase5Gate8LightmapBakeTests.cpp
)

target_link_libraries(RenegadePhase5Gate8LightmapBakeTests
    PRIVATE Renegade::EngineBridge
)

target_compile_options(RenegadePhase5Gate8LightmapBakeTests
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadePhase5Gate8LightmapBakeTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_dependencies(RenegadeBridgeTests RenegadePhase5Gate8LightmapBakeTests)
add_test(
    NAME RenegadePhase5Gate8LightmapBakeTests
    COMMAND RenegadePhase5Gate8LightmapBakeTests
)
add_test(
    NAME RenegadePhase5Gate8SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/Tests/Phase5Gate8SourceContract.cmake
)
