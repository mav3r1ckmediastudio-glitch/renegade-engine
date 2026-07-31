add_executable(RenegadeBridgeTests
    BridgeCommandTests.cpp
)

target_link_libraries(RenegadeBridgeTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeBridgeTests PROPERTIES
    FOLDER "Renegade"
)

add_test(NAME RenegadeBridgeTests COMMAND RenegadeBridgeTests)

add_executable(RenegadePrecipitationTests
    PrecipitationTests.cpp
)

target_link_libraries(RenegadePrecipitationTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadePrecipitationTests PROPERTIES
    FOLDER "Renegade"
)

# The Windows Studio CI builds RenegadeBridgeTests explicitly before running
# the complete CTest suite. Keep the precipitation executable in that build
# chain so CTest never registers a test binary that the targeted build omitted.
add_dependencies(RenegadeBridgeTests RenegadePrecipitationTests)

add_test(NAME RenegadePrecipitationTests COMMAND RenegadePrecipitationTests)
