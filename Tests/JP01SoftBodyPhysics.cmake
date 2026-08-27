add_executable(RenegadeSoftBodyPhysicsTests
    SoftBodyPhysicsTests.cpp
)

target_link_libraries(RenegadeSoftBodyPhysicsTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeSoftBodyPhysicsTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_dependencies(RenegadeBridgeTests RenegadeSoftBodyPhysicsTests)

add_test(
    NAME RenegadeSoftBodyPhysicsTests
    COMMAND RenegadeSoftBodyPhysicsTests
)
