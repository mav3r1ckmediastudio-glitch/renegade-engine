# LP07 Gate 6 — persistent reusable asset identity across the real scene/build
# boundary. These first proofs are deliberately headless: they catch Wicked
# ECS/metadata/archive and package-refresh regressions in Debug and Release
# before the final standalone package smoke adds graphics/runtime cost.
add_executable(RenegadeReusableAssetInstanceTests
    ${CMAKE_CURRENT_LIST_DIR}/ReusableAssetInstanceTests.cpp
)

target_link_libraries(
    RenegadeReusableAssetInstanceTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_definitions(
    RenegadeReusableAssetInstanceTests
    PRIVATE UNICODE _UNICODE
)
target_compile_options(
    RenegadeReusableAssetInstanceTests
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(
    RenegadeReusableAssetInstanceTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_executable(RenegadeReusableAssetRuntimeTests
    ${CMAKE_CURRENT_LIST_DIR}/ReusableAssetRuntimeTests.cpp
)

target_link_libraries(
    RenegadeReusableAssetRuntimeTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_definitions(
    RenegadeReusableAssetRuntimeTests
    PRIVATE UNICODE _UNICODE
)
target_compile_options(
    RenegadeReusableAssetRuntimeTests
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(
    RenegadeReusableAssetRuntimeTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeReusableAssetInstanceTests
    RenegadeReusableAssetRuntimeTests
)

add_test(
    NAME RenegadeReusableAssetInstanceTests
    COMMAND RenegadeReusableAssetInstanceTests
        "${CMAKE_BINARY_DIR}/lp07-gate6-instance-proof-output"
)
set_tests_properties(
    RenegadeReusableAssetInstanceTests
    PROPERTIES TIMEOUT 60
)

add_test(
    NAME RenegadeReusableAssetRuntimeTests
    COMMAND RenegadeReusableAssetRuntimeTests
        "${CMAKE_BINARY_DIR}/lp07-gate6-runtime-refresh-output"
)
set_tests_properties(
    RenegadeReusableAssetRuntimeTests
    PROPERTIES TIMEOUT 60
)
