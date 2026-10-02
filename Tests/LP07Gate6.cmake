# LP07 Gate 6 reusable-instance/runtime coverage retained after the clean
# importer rebuild. The original package acceptance created/reimported models
# through the retired LP07 importer API, so that obsolete acceptance executable
# is no longer registered. Current import/placement/reopen proof lives in
# ModelImporterRebuild.cmake; package infrastructure is covered independently.

add_executable(RenegadeReusableAssetInstanceTests
    ${CMAKE_CURRENT_LIST_DIR}/ReusableAssetInstanceTests.cpp
)
target_link_libraries(
    RenegadeReusableAssetInstanceTests
    PRIVATE Renegade::EngineBridge
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
    PRIVATE Renegade::EngineBridge
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

message(STATUS
    "LP07 Gate 6 legacy importer package acceptance retired; reusable instance/runtime proofs retained."
)
