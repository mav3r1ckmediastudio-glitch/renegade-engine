add_executable(RenegadePhase6Gate3AudioTests
    "${CMAKE_CURRENT_LIST_DIR}/Phase6Gate3AudioTests.cpp"
)
target_link_libraries(
    RenegadePhase6Gate3AudioTests
    PRIVATE
        Renegade::EngineBridge
)
set_target_properties(
    RenegadePhase6Gate3AudioTests
    PROPERTIES FOLDER "Renegade/Tests"
)
add_dependencies(RenegadeBridgeTests RenegadePhase6Gate3AudioTests)
add_test(
    NAME RenegadePhase6Gate3AudioTests
    COMMAND RenegadePhase6Gate3AudioTests
)

add_test(
    NAME RenegadePhase6Gate3SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/Phase6Gate3SourceContract.cmake
)
