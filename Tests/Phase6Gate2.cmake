add_executable(RenegadePhase6Gate2InputTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase6Gate2InputTests.cpp
)

target_link_libraries(RenegadePhase6Gate2InputTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(RenegadePhase6Gate2InputTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadePhase6Gate2InputTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadePhase6Gate2InputTests
)

add_test(
    NAME RenegadePhase6Gate2InputTests
    COMMAND RenegadePhase6Gate2InputTests
)

add_test(
    NAME RenegadePhase6Gate2SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/Phase6Gate2SourceContract.cmake
)
