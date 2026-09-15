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

# CW-05 owner-validation repair: TestGame must create a missing native grid,
# reuse the persistent cache when navigation geometry is unchanged, and rebuild
# it when relevant geometry changes.
add_executable(RenegadeCW05NavigationCacheTests
    ${CMAKE_CURRENT_LIST_DIR}/CW05NavigationCacheTests.cpp
)

target_link_libraries(
    RenegadeCW05NavigationCacheTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeCW05NavigationCacheTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeCW05NavigationCacheTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeCW05NavigationCacheTests
)

add_test(
    NAME RenegadeCW05NavigationCacheTests
    COMMAND RenegadeCW05NavigationCacheTests
)

set_tests_properties(
    RenegadeCW05NavigationCacheTests
    PROPERTIES
        LABELS "CW05;Navigation;TestGame"
)

# Owner-level regression over the real project-aware TestLevelSnapshotService:
# authored grids must survive unchanged while the disposable automatic grid has
# a stable identity and is selected by Character AI's default-grid resolver.
add_executable(RenegadeCW05NavigationOwnershipTests
    ${CMAKE_CURRENT_LIST_DIR}/CW05NavigationOwnershipTests.cpp
)

target_link_libraries(
    RenegadeCW05NavigationOwnershipTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeCW05NavigationOwnershipTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeCW05NavigationOwnershipTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeCW05NavigationOwnershipTests
)

add_test(
    NAME RenegadeCW05NavigationOwnershipTests
    COMMAND RenegadeCW05NavigationOwnershipTests
)

set_tests_properties(
    RenegadeCW05NavigationOwnershipTests
    PROPERTIES
        LABELS "CW05;Navigation;TestGame;Ownership"
)

add_test(
    NAME RenegadeCW05NavigationCacheSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/CW05NavigationCacheSourceContract.cmake
)

set_tests_properties(
    RenegadeCW05NavigationCacheSourceContract
    PROPERTIES
        LABELS "CW05;Navigation;TestGame;SourceContract"
)
