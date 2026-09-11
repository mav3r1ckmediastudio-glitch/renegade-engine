add_executable(RenegadePhase7Gate7BTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase7Gate7BHumanoidRetargetTests.cpp
)
target_link_libraries(RenegadePhase7Gate7BTests PRIVATE Renegade::EngineBridge)
target_compile_features(RenegadePhase7Gate7BTests PRIVATE cxx_std_17)
set_target_properties(RenegadePhase7Gate7BTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_test(
    NAME RenegadePhase7Gate7BHumanoidRetargetTests
    COMMAND RenegadePhase7Gate7BTests
)
add_test(
    NAME RenegadePhase7Gate7BSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/Phase7Gate7BSourceContract.cmake
)
