add_executable(RenegadeRagdollPhysicsTests
    RagdollPhysicsTests.cpp
)

target_link_libraries(RenegadeRagdollPhysicsTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeRagdollPhysicsTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_dependencies(RenegadeBridgeTests RenegadeRagdollPhysicsTests)

add_test(
    NAME RenegadeRagdollPhysicsTests
    COMMAND RenegadeRagdollPhysicsTests
)
