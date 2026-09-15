add_executable(RenegadeCW05CharacterPrefabTests
    CW05CharacterPrefabTests.cpp
)

target_link_libraries(
    RenegadeCW05CharacterPrefabTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeCW05CharacterPrefabTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeCW05CharacterPrefabTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_executable(RenegadeCW05CommandCompanionTests
    CW05CommandCompanionTests.cpp
)

target_link_libraries(
    RenegadeCW05CommandCompanionTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeCW05CommandCompanionTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeCW05CommandCompanionTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeCW05CharacterPrefabTests
    RenegadeCW05CommandCompanionTests
)

add_test(
    NAME RenegadeCW05CharacterPrefabTests
    COMMAND RenegadeCW05CharacterPrefabTests
)

add_test(
    NAME RenegadeCW05CommandCompanionTests
    COMMAND RenegadeCW05CommandCompanionTests
)

add_test(
    NAME RenegadeCW05CharacterPrefabSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/Tests/CW05CharacterPrefabSourceContract.cmake
)
