add_executable(RenegadePhase6NativeNavigationTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase6NativeNavigationTests.cpp
)

target_link_libraries(
    RenegadePhase6NativeNavigationTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadePhase6NativeNavigationTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadePhase6NativeNavigationTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadePhase6NativeNavigationTests
)

add_test(
    NAME RenegadePhase6NativeNavigationTests
    COMMAND RenegadePhase6NativeNavigationTests
)

set_tests_properties(
    RenegadePhase6NativeNavigationTests
    PROPERTIES
        LABELS "Phase6;Navigation"
)

add_test(
    NAME RenegadePhase6NativeNavigationSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/Phase6NativeNavigationSourceContract.cmake
)

set_tests_properties(
    RenegadePhase6NativeNavigationSourceContract
    PROPERTIES
        LABELS "Phase6;Navigation;SourceContract"
)