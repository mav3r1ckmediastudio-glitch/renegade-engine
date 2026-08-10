add_executable(RenegadePackageIntegrityTests
    "${CMAKE_CURRENT_LIST_DIR}/PackageIntegrityTests.cpp"
)
target_link_libraries(
    RenegadePackageIntegrityTests
    PRIVATE
        Renegade::EngineBridge
)
target_compile_definitions(
    RenegadePackageIntegrityTests
    PRIVATE
        UNICODE
        _UNICODE
)
target_compile_options(
    RenegadePackageIntegrityTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(RenegadePackageIntegrityTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_executable(RenegadeStandalonePackageTests
    "${CMAKE_CURRENT_LIST_DIR}/StandalonePackageTests.cpp"
)
target_link_libraries(
    RenegadeStandalonePackageTests
    PRIVATE
        Renegade::RuntimeBootstrap
)
target_compile_definitions(
    RenegadeStandalonePackageTests
    PRIVATE
        UNICODE
        _UNICODE
)
target_compile_options(
    RenegadeStandalonePackageTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(RenegadeStandalonePackageTests PROPERTIES
    FOLDER "Renegade/Tests"
)
add_dependencies(
    RenegadeStandalonePackageTests
    RenegadeRuntime
)

# Studio CI explicitly builds RenegadeBridgeTests before running CTest. Keep
# both Gate 4 proofs in that authoritative graph; the standalone test then
# launches the real configuration-matched RenegadeRuntime target it depends on.
add_dependencies(
    RenegadeBridgeTests
    RenegadePackageIntegrityTests
    RenegadeStandalonePackageTests
)

add_test(
    NAME RenegadePackageIntegrityTests
    COMMAND RenegadePackageIntegrityTests
)
add_test(
    NAME RenegadeStandalonePackageTests
    COMMAND RenegadeStandalonePackageTests
        "$<TARGET_FILE:RenegadeRuntime>"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/dxcompiler.dll"
        "${PROJECT_SOURCE_DIR}/Runtime/fixtures/LP03/Valid Screen"
        "${PROJECT_SOURCE_DIR}/WickedEngine/Content/models/cube.wiscene"
        "${CMAKE_CURRENT_LIST_DIR}/fixtures/lp06_gate2"
        "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(
    RenegadeStandalonePackageTests
    PROPERTIES
        TIMEOUT 180
        RUN_SERIAL TRUE
)
