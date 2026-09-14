add_executable(RenegadeCharacterAiFoundationTests
    ${CMAKE_CURRENT_LIST_DIR}/CharacterAiFoundationTests.cpp
)
target_link_libraries(RenegadeCharacterAiFoundationTests PRIVATE Renegade::EngineBridge)
target_compile_features(RenegadeCharacterAiFoundationTests PRIVATE cxx_std_17)
set_target_properties(RenegadeCharacterAiFoundationTests PROPERTIES
    FOLDER "Renegade/Tests"
)

# Studio CI explicitly builds RenegadeBridgeTests before the complete CTest
# pass. Keep AI executables in that dependency chain so registered tests are
# always present when CTest starts.
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

add_executable(RenegadeCharacterAiProfilesTests
    ${CMAKE_CURRENT_LIST_DIR}/CharacterAiProfilesTests.cpp
)
target_link_libraries(RenegadeCharacterAiProfilesTests PRIVATE Renegade::EngineBridge)
target_include_directories(RenegadeCharacterAiProfilesTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Runtime/src
)
target_compile_features(RenegadeCharacterAiProfilesTests PRIVATE cxx_std_17)
set_target_properties(RenegadeCharacterAiProfilesTests PROPERTIES
    FOLDER "Renegade/Tests"
)
add_dependencies(RenegadeBridgeTests RenegadeCharacterAiProfilesTests)

add_test(
    NAME RenegadeCharacterAiProfilesTests
    COMMAND RenegadeCharacterAiProfilesTests
)

add_executable(RenegadeCharacterAiProfilesAuditTests
    ${CMAKE_CURRENT_LIST_DIR}/CharacterAiProfilesAuditTests.cpp
)
target_link_libraries(RenegadeCharacterAiProfilesAuditTests PRIVATE Renegade::EngineBridge)
target_include_directories(RenegadeCharacterAiProfilesAuditTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Runtime/src
)
target_compile_features(RenegadeCharacterAiProfilesAuditTests PRIVATE cxx_std_17)
set_target_properties(RenegadeCharacterAiProfilesAuditTests PROPERTIES
    FOLDER "Renegade/Tests"
)
add_dependencies(RenegadeBridgeTests RenegadeCharacterAiProfilesAuditTests)

add_test(
    NAME RenegadeCharacterAiProfilesAuditTests
    COMMAND RenegadeCharacterAiProfilesAuditTests
)
add_test(
    NAME RenegadeCharacterAiProfilesSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/CharacterAiProfilesSourceContract.cmake
)

add_executable(RenegadeCharacterAiPerceptionTests
    ${CMAKE_CURRENT_LIST_DIR}/CharacterAiPerceptionTests.cpp
)
target_link_libraries(RenegadeCharacterAiPerceptionTests PRIVATE Renegade::EngineBridge)
target_include_directories(RenegadeCharacterAiPerceptionTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Runtime/src
)
target_compile_features(RenegadeCharacterAiPerceptionTests PRIVATE cxx_std_17)
set_target_properties(RenegadeCharacterAiPerceptionTests PROPERTIES
    FOLDER "Renegade/Tests"
)
add_dependencies(RenegadeBridgeTests RenegadeCharacterAiPerceptionTests)

add_test(
    NAME RenegadeCharacterAiPerceptionTests
    COMMAND RenegadeCharacterAiPerceptionTests
)
add_test(
    NAME RenegadeCharacterAiPerceptionSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/CharacterAiPerceptionSourceContract.cmake
)

add_executable(RenegadeCharacterAiDecisionTests
    ${CMAKE_CURRENT_LIST_DIR}/CharacterAiDecisionTests.cpp
)
target_link_libraries(RenegadeCharacterAiDecisionTests PRIVATE Renegade::EngineBridge)
target_include_directories(RenegadeCharacterAiDecisionTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Runtime/src
)
target_compile_features(RenegadeCharacterAiDecisionTests PRIVATE cxx_std_17)
set_target_properties(RenegadeCharacterAiDecisionTests PROPERTIES
    FOLDER "Renegade/Tests"
)
add_dependencies(RenegadeBridgeTests RenegadeCharacterAiDecisionTests)

add_test(
    NAME RenegadeCharacterAiDecisionTests
    COMMAND RenegadeCharacterAiDecisionTests
)
add_test(
    NAME RenegadeCharacterAiDecisionSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/CharacterAiDecisionSourceContract.cmake
)
