add_executable(RenegadePhase5Gate2CameraTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase5Gate2CameraTests.cpp
)

target_link_libraries(RenegadePhase5Gate2CameraTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(RenegadePhase5Gate2CameraTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadePhase5Gate2CameraTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadePhase5Gate2CameraTests
)

add_test(
    NAME RenegadePhase5Gate2CameraTests
    COMMAND RenegadePhase5Gate2CameraTests
)

add_test(
    NAME RenegadePhase5Gate2SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/Phase5Gate2SourceContract.cmake
)
