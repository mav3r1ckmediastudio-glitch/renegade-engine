# Asset Browser catalogue recovery must never let one stale LC01 tombstone
# blank every healthy creator asset. This is filesystem/registry-only and does
# not require a renderer or GPU.
add_executable(RenegadeAssetBrowserTombstoneRecoveryTests
    ${CMAKE_CURRENT_LIST_DIR}/AssetBrowserTombstoneRecoveryTests.cpp
)

target_link_libraries(
    RenegadeAssetBrowserTombstoneRecoveryTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_definitions(
    RenegadeAssetBrowserTombstoneRecoveryTests
    PRIVATE UNICODE _UNICODE
)
target_compile_options(
    RenegadeAssetBrowserTombstoneRecoveryTests
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(
    RenegadeAssetBrowserTombstoneRecoveryTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeAssetBrowserTombstoneRecoveryTests
)

add_test(
    NAME RenegadeAssetBrowserTombstoneRecoveryTests
    COMMAND RenegadeAssetBrowserTombstoneRecoveryTests
)
set_tests_properties(
    RenegadeAssetBrowserTombstoneRecoveryTests
    PROPERTIES TIMEOUT 60
)
