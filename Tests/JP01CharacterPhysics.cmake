add_executable(RenegadeCharacterPhysicsTests
    ${CMAKE_CURRENT_LIST_DIR}/CharacterPhysicsTests.cpp
)

target_link_libraries(RenegadeCharacterPhysicsTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeCharacterPhysicsTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_dependencies(RenegadeBridgeTests RenegadeCharacterPhysicsTests)

add_test(
    NAME RenegadeCharacterPhysicsTests
    COMMAND RenegadeCharacterPhysicsTests
)
