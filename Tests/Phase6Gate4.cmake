add_executable(RenegadePhase6Gate4GameplayScriptTests
    "${CMAKE_CURRENT_LIST_DIR}/Phase6Gate4GameplayScriptTests.cpp"
)
target_link_libraries(
    RenegadePhase6Gate4GameplayScriptTests
    PRIVATE
        Renegade::EngineBridge
)
set_target_properties(
    RenegadePhase6Gate4GameplayScriptTests
    PROPERTIES FOLDER "Renegade/Tests"
)
add_dependencies(RenegadeBridgeTests RenegadePhase6Gate4GameplayScriptTests)
add_test(
    NAME RenegadePhase6Gate4GameplayScriptTests
    COMMAND RenegadePhase6Gate4GameplayScriptTests
)

add_test(
    NAME RenegadePhase6Gate4SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/Phase6Gate4SourceContract.cmake
)
