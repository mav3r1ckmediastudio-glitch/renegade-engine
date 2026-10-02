# LP08 Gate 5 governed resource package/runtime coverage.
# The former final acceptance executable reused the retired LP07 model
# import/reimport harness. Keep the independent stable-ID package/runtime and
# cache-identity proofs; standalone packaging is covered by the active package
# suites and current model lifecycle by ModelImporterRebuild.cmake.

add_executable(RenegadeResourceAssetPackageRuntimeTests
    ${CMAKE_CURRENT_LIST_DIR}/ResourceAssetPackageRuntimeTests.cpp
)
target_link_libraries(
    RenegadeResourceAssetPackageRuntimeTests
    PRIVATE Renegade::EngineBridge
)
target_compile_options(
    RenegadeResourceAssetPackageRuntimeTests
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(
    RenegadeResourceAssetPackageRuntimeTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_executable(RenegadeResourceAssetCacheIdentityTests
    ${CMAKE_CURRENT_LIST_DIR}/ResourceAssetCacheIdentityTests.cpp
)
target_link_libraries(
    RenegadeResourceAssetCacheIdentityTests
    PRIVATE Renegade::EngineBridge
)
target_compile_options(
    RenegadeResourceAssetCacheIdentityTests
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(
    RenegadeResourceAssetCacheIdentityTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeResourceAssetPackageRuntimeTests
    RenegadeResourceAssetCacheIdentityTests
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

add_test(
    NAME RenegadeResourceAssetCacheIdentityTests
    COMMAND RenegadeResourceAssetCacheIdentityTests
)
set_tests_properties(
    RenegadeResourceAssetCacheIdentityTests
    PROPERTIES TIMEOUT 60
)

message(STATUS
    "LP08 Gate 5 legacy LP07-coupled package acceptance retired; resource runtime/cache proofs retained."
)
