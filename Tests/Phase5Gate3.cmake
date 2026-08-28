add_executable(RenegadePhase5Gate3DecalProbeTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase5Gate3DecalProbeTests.cpp
)

target_link_libraries(RenegadePhase5Gate3DecalProbeTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(RenegadePhase5Gate3DecalProbeTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadePhase5Gate3DecalProbeTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadePhase5Gate3DecalProbeTests
)

add_test(
    NAME RenegadePhase5Gate3DecalProbeTests
    COMMAND RenegadePhase5Gate3DecalProbeTests
)

add_test(
    NAME RenegadePhase5Gate3SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/Phase5Gate3SourceContract.cmake
)
