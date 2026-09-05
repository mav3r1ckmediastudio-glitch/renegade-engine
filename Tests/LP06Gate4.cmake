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

add_executable(RenegadeGate4AudioProbe
    "${CMAKE_CURRENT_LIST_DIR}/Gate4AudioProbe.cpp"
)
target_compile_definitions(
    RenegadeGate4AudioProbe
    PRIVATE
        UNICODE
        _UNICODE
)
target_compile_options(
    RenegadeGate4AudioProbe
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(RenegadeGate4AudioProbe PROPERTIES
    FOLDER "Renegade/TestFixtures"
)

add_executable(RenegadeStandalonePackageTests
    "${CMAKE_CURRENT_LIST_DIR}/StandalonePackageTests.cpp"
)
target_link_libraries(
    RenegadeStandalonePackageTests
    PRIVATE
        Renegade::RuntimeBootstrap
        d3d12
        dxgi
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
    RenegadeGate4AudioProbe
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
    COMMAND ${CMAKE_COMMAND}
        "-DCONFIGURATION=$<CONFIG>"
        "-DAUDIO_PROBE=$<TARGET_FILE:RenegadeGate4AudioProbe>"
        "-DSTANDALONE_TEST=$<TARGET_FILE:RenegadeStandalonePackageTests>"
        "-DRUNTIME_EXE=$<TARGET_FILE:RenegadeRuntime>"
        "-DDXCOMPILER_DLL=$<TARGET_FILE_DIR:RenegadeRuntime>/dxcompiler.dll"
        "-DSCREEN_FIXTURE=${PROJECT_SOURCE_DIR}/Runtime/fixtures/LP03/Valid Screen"
        "-DSCENE_FIXTURE=${RENEGADE_GATE4_SCENE}"
        "-DGATE2_FIXTURE=${CMAKE_CURRENT_LIST_DIR}/fixtures/lp06_gate2"
        "-DREPOSITORY_ROOT=${PROJECT_SOURCE_DIR}"
        "-DMANUAL_EXPORT_ROOT=${PROJECT_SOURCE_DIR}/artifacts/studio/ci-$<CONFIG>/gate4-manual"
        -P "${CMAKE_CURRENT_LIST_DIR}/RunGate4Standalone.cmake"
)
set_tests_properties(
    RenegadeStandalonePackageTests
    PROPERTIES
        TIMEOUT 420
        RUN_SERIAL TRUE
        FIXTURES_REQUIRED RenegadeGate4Scene
        SKIP_REGULAR_EXPRESSION "GATE4_DEBUG_AUDIO_ENDPOINT_UNAVAILABLE"
)