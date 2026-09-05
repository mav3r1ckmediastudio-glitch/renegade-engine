# LP08 Gate 4 — explicit governed resource reimport and lifecycle recovery.
# Headless proof runs in both Debug and Release; no external fixtures required.
add_executable(RenegadeResourceAssetReimportTests
    ${CMAKE_CURRENT_LIST_DIR}/ResourceAssetReimportTests.cpp
)

target_link_libraries(
    RenegadeResourceAssetReimportTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeResourceAssetReimportTests
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(
    RenegadeResourceAssetReimportTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeResourceAssetReimportTests
)

add_test(
    NAME RenegadeResourceAssetReimportTests
    COMMAND RenegadeResourceAssetReimportTests
)
set_tests_properties(
    RenegadeResourceAssetReimportTests
    PROPERTIES TIMEOUT 90
)
