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

# Generate the Gate 4 level payload with Wicked's real scene serializer instead
# of borrowing an upstream sample scene that can carry unrelated external
# resource dependencies. The resulting archive is a genuine, self-contained
# WISCENE and is used by both levels in the disposable package.
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
add_custom_command(
    OUTPUT "${RENEGADE_GATE4_SCENE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RENEGADE_GATE4_SCENE_DIR}"
    COMMAND "$<TARGET_FILE:RenegadeGate4SceneFixture>"
            "${RENEGADE_GATE4_SCENE_DIR}"
    DEPENDS RenegadeGate4SceneFixture
    COMMENT "Generating LP06 Gate 4 self-contained Wicked scene"
    VERBATIM
)
add_custom_target(RenegadeGate4SceneData
    DEPENDS "${RENEGADE_GATE4_SCENE}"
)
set_target_properties(RenegadeGate4SceneData PROPERTIES
    FOLDER "Renegade/TestFixtures"
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
    RenegadeGate4SceneData
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
        "${RENEGADE_GATE4_SCENE}"
        "${CMAKE_CURRENT_LIST_DIR}/fixtures/lp06_gate2"
        "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(
    RenegadeStandalonePackageTests
    PROPERTIES
        TIMEOUT 180
        RUN_SERIAL TRUE
)
