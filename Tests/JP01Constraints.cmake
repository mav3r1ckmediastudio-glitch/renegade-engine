add_executable(RenegadeConstraintTests
    ${CMAKE_CURRENT_LIST_DIR}/ConstraintTests.cpp
)

target_link_libraries(RenegadeConstraintTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeConstraintTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_dependencies(RenegadeBridgeTests RenegadeConstraintTests)

add_test(
    NAME RenegadeConstraintTests
    COMMAND RenegadeConstraintTests
)
