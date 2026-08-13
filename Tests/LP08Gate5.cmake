# LP08 Gate 5 — governed resource dependency/package/Runtime acceptance.
# Headless stable-ID closure and package resolver proof runs Debug + Release.
add_executable(RenegadeResourceAssetPackageRuntimeTests
    ${CMAKE_CURRENT_LIST_DIR}/ResourceAssetPackageRuntimeTests.cpp
)

target_link_libraries(
    RenegadeResourceAssetPackageRuntimeTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeResourceAssetPackageRuntimeTests
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(
    RenegadeResourceAssetPackageRuntimeTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeResourceAssetPackageRuntimeTests
)

add_test(
    NAME RenegadeResourceAssetPackageRuntimeTests
    COMMAND RenegadeResourceAssetPackageRuntimeTests
        "${CMAKE_CURRENT_BINARY_DIR}/lp08-gate5-package-runtime"
)
set_tests_properties(
    RenegadeResourceAssetPackageRuntimeTests
    PROPERTIES TIMEOUT 120
)
