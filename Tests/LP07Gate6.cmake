# LP07 Gate 6 — persistent reusable asset identity across the real scene/build
# boundary. The first proof is deliberately headless: it catches Wicked ECS,
# metadata, hierarchy and archive API regressions in Debug and Release before
# the later standalone package smoke adds graphics/runtime cost.
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

add_dependencies(
    RenegadeBridgeTests
    RenegadeReusableAssetInstanceTests
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
