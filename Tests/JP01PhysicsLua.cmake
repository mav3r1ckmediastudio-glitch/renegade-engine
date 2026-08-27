add_executable(RenegadePhysicsLuaTests
    PhysicsLuaTests.cpp
)

target_link_libraries(RenegadePhysicsLuaTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadePhysicsLuaTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_dependencies(RenegadeBridgeTests RenegadePhysicsLuaTests)

add_test(
    NAME RenegadePhysicsLuaTests
    COMMAND RenegadePhysicsLuaTests
)
