add_executable(RenegadeWickedColliderTests
    ${CMAKE_CURRENT_LIST_DIR}/WickedColliderTests.cpp
)

target_link_libraries(RenegadeWickedColliderTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeWickedColliderTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_dependencies(RenegadeBridgeTests RenegadeWickedColliderTests)

add_test(
    NAME RenegadeWickedColliderTests
    COMMAND RenegadeWickedColliderTests
)
