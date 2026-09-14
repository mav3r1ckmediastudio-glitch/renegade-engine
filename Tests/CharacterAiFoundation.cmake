add_executable(RenegadeCharacterAiFoundationTests
    ${CMAKE_CURRENT_LIST_DIR}/CharacterAiFoundationTests.cpp
)
target_link_libraries(RenegadeCharacterAiFoundationTests PRIVATE Renegade::EngineBridge)
target_compile_features(RenegadeCharacterAiFoundationTests PRIVATE cxx_std_17)
set_target_properties(RenegadeCharacterAiFoundationTests PROPERTIES
    FOLDER "Renegade/Tests"
)

# Studio CI explicitly builds RenegadeBridgeTests before the complete CTest
# pass. Keep AI-01's executable in that dependency chain so registered tests
# are always present when CTest starts.
add_dependencies(RenegadeBridgeTests RenegadeCharacterAiFoundationTests)

add_test(
    NAME RenegadeCharacterAiFoundationTests
    COMMAND RenegadeCharacterAiFoundationTests
)
add_test(
    NAME RenegadeCharacterAiSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/CharacterAiSourceContract.cmake
)
