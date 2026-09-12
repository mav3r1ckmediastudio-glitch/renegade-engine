add_executable(RenegadePhase7Gate7FTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase7Gate7FMeshBlendTests.cpp
)
target_link_libraries(RenegadePhase7Gate7FTests PRIVATE Renegade::EngineBridge)
target_compile_features(RenegadePhase7Gate7FTests PRIVATE cxx_std_17)
set_target_properties(RenegadePhase7Gate7FTests PROPERTIES
    FOLDER "Renegade/Tests"
)

# Studio CI builds RenegadeBridgeTests explicitly before running the complete
# CTest suite. Keep 7F in that dependency chain so its executable is present.
add_dependencies(RenegadeBridgeTests RenegadePhase7Gate7FTests)

add_test(
    NAME RenegadePhase7Gate7FMeshBlendTests
    COMMAND RenegadePhase7Gate7FTests
)
add_test(
    NAME RenegadePhase7Gate7FSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/Phase7Gate7FSourceContract.cmake
)
