add_executable(RenegadePhase5Gate4MaterialTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase5Gate4MaterialTests.cpp
)

target_link_libraries(RenegadePhase5Gate4MaterialTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(RenegadePhase5Gate4MaterialTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadePhase5Gate4MaterialTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadePhase5Gate4MaterialTests
)

add_test(
    NAME RenegadePhase5Gate4MaterialTests
    COMMAND RenegadePhase5Gate4MaterialTests
)

add_test(
    NAME RenegadePhase5Gate4SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/Phase5Gate4SourceContract.cmake
)
