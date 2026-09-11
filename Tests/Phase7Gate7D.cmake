add_executable(RenegadePhase7Gate7DTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase7Gate7DNativeTimelineTests.cpp
)
target_link_libraries(RenegadePhase7Gate7DTests PRIVATE Renegade::EngineBridge)
target_compile_features(RenegadePhase7Gate7DTests PRIVATE cxx_std_17)
set_target_properties(RenegadePhase7Gate7DTests PROPERTIES
    FOLDER "Renegade/Tests"
)

# Studio CI builds RenegadeBridgeTests explicitly before running the complete
# CTest suite. Keep 7D in the same targeted dependency chain used by 7B/7C.
add_dependencies(RenegadeBridgeTests RenegadePhase7Gate7DTests)

add_test(
    NAME RenegadePhase7Gate7DNativeTimelineTests
    COMMAND RenegadePhase7Gate7DTests
)
add_test(
    NAME RenegadePhase7Gate7DSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/Phase7Gate7DSourceContract.cmake
)
