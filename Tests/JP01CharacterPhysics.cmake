add_executable(RenegadeCharacterPhysicsTests
    CharacterPhysicsTests.cpp
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
