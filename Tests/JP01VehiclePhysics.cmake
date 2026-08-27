add_executable(RenegadeVehiclePhysicsTests
    VehiclePhysicsTests.cpp
)

target_link_libraries(RenegadeVehiclePhysicsTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeVehiclePhysicsTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_dependencies(RenegadeBridgeTests RenegadeVehiclePhysicsTests)

add_test(
    NAME RenegadeVehiclePhysicsTests
    COMMAND RenegadeVehiclePhysicsTests
)
