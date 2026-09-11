add_executable(RenegadePhase7Gate7AAnimationTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase7AnimationServiceTests.cpp
)
target_link_libraries(RenegadePhase7Gate7AAnimationTests
    PRIVATE
        Renegade::EngineBridge
)
target_compile_definitions(RenegadePhase7Gate7AAnimationTests
    PRIVATE
        UNICODE
        _UNICODE
)
target_compile_options(RenegadePhase7Gate7AAnimationTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(RenegadePhase7Gate7AAnimationTests PROPERTIES
    FOLDER "Renegade/Tests"
)
add_test(
    NAME RenegadePhase7Gate7AAnimation
    COMMAND RenegadePhase7Gate7AAnimationTests
)

add_test(
    NAME RenegadePhase7Gate7ASourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/Phase7Gate7ASourceContract.cmake
)
