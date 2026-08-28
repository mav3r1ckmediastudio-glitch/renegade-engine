add_executable(RenegadePhysicsLuaTests
    ${CMAKE_CURRENT_LIST_DIR}/PhysicsLuaTests.cpp
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

# A focused namespace contract must fail fast. It must never be allowed to
# consume the global 25-minute CTest timeout again if Lua lifecycle regresses.
set_tests_properties(RenegadePhysicsLuaTests PROPERTIES TIMEOUT 30)
