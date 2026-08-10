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

# Build the Gate 4 scene serializer as a normal test fixture executable, but do
# not run it as an MSBuild custom command. Run-time fixture generation belongs
# under CTest so the serializer is bounded and reports a normal test failure
# instead of being able to strand the entire build step until the workflow's
# outer timeout.
add_executable(RenegadeGate4SceneFixture
    "${CMAKE_CURRENT_LIST_DIR}/Gate4SceneFixture.cpp"
)
target_link_libraries(
    RenegadeGate4SceneFixture
    PRIVATE
        Renegade::EngineBridge
)
target_compile_options(
    RenegadeGate4SceneFixture
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(RenegadeGate4SceneFixture PROPERTIES
    FOLDER "Renegade/TestFixtures"
)
set(RENEGADE_GATE4_SCENE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/lp06-gate4-scene"
)
set(RENEGADE_GATE4_SCENE
    "${RENEGADE_GATE4_SCENE_DIR}/Gate4SelfContained.wiscene"
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
# all Gate 4 executables in that authoritative graph, but keep scene generation
# itself exclusively in the bounded CTest phase below.
add_dependencies(
    RenegadeBridgeTests
    RenegadePackageIntegrityTests
    RenegadeGate4SceneFixture
    RenegadeStandalonePackageTests
)

add_test(
    NAME RenegadePackageIntegrityTests
    COMMAND RenegadePackageIntegrityTests
)
add_test(
    NAME RenegadeGate4SceneFixtureTests
    COMMAND RenegadeGate4SceneFixture
        "${RENEGADE_GATE4_SCENE_DIR}"
)
set_tests_properties(
    RenegadeGate4SceneFixtureTests
    PROPERTIES
        TIMEOUT 30
        FIXTURES_SETUP RenegadeGate4Scene
)
add_test(
    NAME RenegadeStandalonePackageTests
    COMMAND RenegadeStandalonePackageTests
        "$<TARGET_FILE:RenegadeRuntime>"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/dxcompiler.dll"
        "${PROJECT_SOURCE_DIR}/Runtime/fixtures/LP03/Valid Screen"
        "${RENEGADE_GATE4_SCENE}"
        "${CMAKE_CURRENT_LIST_DIR}/fixtures/lp06_gate2"
        "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(
    RenegadeStandalonePackageTests
    PROPERTIES
        TIMEOUT 180
        RUN_SERIAL TRUE
        FIXTURES_REQUIRED RenegadeGate4Scene
)
