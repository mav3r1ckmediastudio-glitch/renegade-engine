add_executable(RenegadePhase7Gate7CTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase7Gate7CCharacterControlsTests.cpp
)
target_link_libraries(RenegadePhase7Gate7CTests PRIVATE Renegade::EngineBridge)
target_compile_features(RenegadePhase7Gate7CTests PRIVATE cxx_std_17)
set_target_properties(RenegadePhase7Gate7CTests PROPERTIES
    FOLDER "Renegade/Tests"
)

# Studio CI builds RenegadeBridgeTests explicitly before running the complete
# CTest suite. Keep the 7C executable in that build chain so CTest never
# registers a binary that the targeted build omitted.
add_dependencies(RenegadeBridgeTests RenegadePhase7Gate7CTests)

add_test(
    NAME RenegadePhase7Gate7CCharacterControlsTests
    COMMAND RenegadePhase7Gate7CTests
)
add_test(
    NAME RenegadePhase7Gate7CSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/Phase7Gate7CSourceContract.cmake
)
