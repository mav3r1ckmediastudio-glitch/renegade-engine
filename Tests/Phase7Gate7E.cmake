add_executable(RenegadePhase7Gate7ETests
    ${CMAKE_CURRENT_LIST_DIR}/Phase7Gate7ESpecialistTests.cpp
)
target_link_libraries(RenegadePhase7Gate7ETests PRIVATE Renegade::EngineBridge)
target_compile_features(RenegadePhase7Gate7ETests PRIVATE cxx_std_17)
set_target_properties(RenegadePhase7Gate7ETests PROPERTIES
    FOLDER "Renegade/Tests"
)

# Studio CI builds RenegadeBridgeTests explicitly before running the complete
# CTest suite. Keep 7E in that dependency chain so its executable is present.
add_dependencies(RenegadeBridgeTests RenegadePhase7Gate7ETests)

add_test(
    NAME RenegadePhase7Gate7ESpecialistTests
    COMMAND RenegadePhase7Gate7ETests
)
add_test(
    NAME RenegadePhase7Gate7ESourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/Phase7Gate7ESourceContract.cmake
)
