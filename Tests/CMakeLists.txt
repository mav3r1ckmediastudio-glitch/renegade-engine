add_executable(RenegadeBridgeTests
    BridgeCommandTests.cpp
)

target_link_libraries(RenegadeBridgeTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeBridgeTests PROPERTIES
    FOLDER "Renegade"
)

add_test(NAME RenegadeBridgeTests COMMAND RenegadeBridgeTests)

add_executable(RenegadeDependencyTests
    DependencyTests.cpp
)
target_link_libraries(RenegadeDependencyTests PRIVATE Renegade::EngineBridge)
target_compile_definitions(RenegadeDependencyTests
    PRIVATE
        RENEGADE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
set_target_properties(RenegadeDependencyTests PROPERTIES FOLDER "Renegade/Tests")
add_dependencies(RenegadeBridgeTests RenegadeDependencyTests)
add_test(NAME RenegadeDependencyTests COMMAND RenegadeDependencyTests)

add_executable(RenegadeAssetRegistryTests
    AssetRegistryTests.cpp
)
target_link_libraries(
    RenegadeAssetRegistryTests
    PRIVATE
        Renegade::EngineBridge
)
set_target_properties(RenegadeAssetRegistryTests PROPERTIES
    FOLDER "Renegade/Tests"
)
add_dependencies(RenegadeBridgeTests RenegadeAssetRegistryTests)
add_test(NAME RenegadeAssetRegistryTests COMMAND RenegadeAssetRegistryTests)

add_executable(RenegadeAssetRegistryPersistenceTests
    AssetRegistryPersistenceTests.cpp
)
target_link_libraries(
    RenegadeAssetRegistryPersistenceTests
    PRIVATE
        Renegade::EngineBridge
)
set_target_properties(RenegadeAssetRegistryPersistenceTests PROPERTIES
    FOLDER "Renegade/Tests"
)
add_dependencies(RenegadeBridgeTests RenegadeAssetRegistryPersistenceTests)
add_test(
    NAME RenegadeAssetRegistryPersistenceTests
    COMMAND RenegadeAssetRegistryPersistenceTests
)

# LP05 Gate 8 packaged proof fixture. The Windows build script copies this
# console executable and its fixed mini-project into the Studio package, then
# launches it twice as separate processes and compares both JSON outputs plus
# every project-file hash. It is not a standalone CTest because the package
# layout is part of the acceptance boundary.
add_executable(RenegadeDependencyProcessFixture
    DependencyProcessFixture.cpp
)
target_link_libraries(
    RenegadeDependencyProcessFixture
    PRIVATE
        Renegade::EngineBridge
)
target_compile_options(RenegadeDependencyProcessFixture
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(RenegadeDependencyProcessFixture PROPERTIES
    FOLDER "Renegade/TestFixtures"
)
add_dependencies(RenegadeBridgeTests RenegadeDependencyProcessFixture)

# LC01 Gate 5 packaged cross-process source-update/reopen proof. The Windows
# build copies this executable and its immutable fixture into the assembled
# Studio package, runs each lifecycle phase in a disposable working copy, and
# compares canonical registry evidence.
add_executable(RenegadeAssetRegistryProcessFixture
    AssetRegistryProcessFixture.cpp
)
target_link_libraries(
    RenegadeAssetRegistryProcessFixture
    PRIVATE
        Renegade::EngineBridge
)
target_compile_options(RenegadeAssetRegistryProcessFixture
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(RenegadeAssetRegistryProcessFixture PROPERTIES
    FOLDER "Renegade/TestFixtures"
)
add_dependencies(RenegadeBridgeTests RenegadeAssetRegistryProcessFixture)

add_executable(RenegadePrecipitationTests
    PrecipitationTests.cpp
)

target_link_libraries(RenegadePrecipitationTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadePrecipitationTests PROPERTIES
    FOLDER "Renegade"
)

# The Windows Studio CI builds RenegadeBridgeTests explicitly before running
# the complete CTest suite. Keep the precipitation executable in that build
# chain so CTest never registers a test binary that the targeted build omitted.
add_dependencies(RenegadeBridgeTests RenegadePrecipitationTests)

add_test(NAME RenegadePrecipitationTests COMMAND RenegadePrecipitationTests)

add_executable(RenegadeSunTests
    SunTests.cpp
)

target_link_libraries(RenegadeSunTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeSunTests PROPERTIES
    FOLDER "Renegade"
)

# Windows Studio CI builds the bridge target explicitly before CTest.
add_dependencies(RenegadeBridgeTests RenegadeSunTests)

add_test(NAME RenegadeSunTests COMMAND RenegadeSunTests)

add_executable(RenegadeOceanTests
    OceanTests.cpp
)

target_link_libraries(RenegadeOceanTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeOceanTests PROPERTIES
    FOLDER "Renegade"
)

# Windows Studio CI builds the bridge target explicitly before CTest.
add_dependencies(RenegadeBridgeTests RenegadeOceanTests)

add_test(NAME RenegadeOceanTests COMMAND RenegadeOceanTests)

add_executable(RenegadeTerrainTests
    TerrainTests.cpp
)

target_link_libraries(RenegadeTerrainTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeTerrainTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeTerrainTests)

add_test(NAME RenegadeTerrainTests COMMAND RenegadeTerrainTests)

add_executable(RenegadeLightTests
    LightTests.cpp
)

target_link_libraries(RenegadeLightTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeLightTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeLightTests)

add_test(NAME RenegadeLightTests COMMAND RenegadeLightTests)

add_executable(RenegadeMaterialTests
    MaterialTests.cpp
)

target_link_libraries(RenegadeMaterialTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeMaterialTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeMaterialTests)

add_test(NAME RenegadeMaterialTests COMMAND RenegadeMaterialTests)

add_executable(RenegadeImportTests
    ImportTests.cpp
)

target_link_libraries(RenegadeImportTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeImportTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeImportTests)

add_test(NAME RenegadeImportTests COMMAND RenegadeImportTests)

add_executable(RenegadeCollisionTests
    CollisionTests.cpp
)

target_link_libraries(RenegadeCollisionTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeCollisionTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeCollisionTests)

add_test(NAME RenegadeCollisionTests COMMAND RenegadeCollisionTests)

add_executable(RenegadeAssetBrowserTests
    AssetBrowserTests.cpp
)

target_link_libraries(RenegadeAssetBrowserTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeAssetBrowserTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeAssetBrowserTests)

add_test(NAME RenegadeAssetBrowserTests COMMAND RenegadeAssetBrowserTests)

add_executable(RenegadeProjectStructureTests
    ProjectStructureTests.cpp
)

target_link_libraries(RenegadeProjectStructureTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeProjectStructureTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeProjectStructureTests)

add_test(
    NAME RenegadeProjectStructureTests
    COMMAND RenegadeProjectStructureTests
)

add_executable(RenegadeIdentityTests
    IdentityTests.cpp
)

target_link_libraries(RenegadeIdentityTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeIdentityTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeIdentityTests)

add_test(
    NAME RenegadeIdentityTests
    COMMAND RenegadeIdentityTests
)


add_executable(RenegadeRuntimeBootstrapTests
    RuntimeBootstrapTests.cpp
)

target_link_libraries(RenegadeRuntimeBootstrapTests
    PRIVATE
        Renegade::RuntimeBootstrap
)

target_compile_options(RenegadeRuntimeBootstrapTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeRuntimeBootstrapTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeRuntimeBootstrapTests)

add_test(
    NAME RenegadeRuntimeBootstrapTests
    COMMAND RenegadeRuntimeBootstrapTests
)

add_executable(RenegadeStoryFlowGate7ProjectHomeLifecycleTests
    StoryFlowGate7ProjectHomeLifecycleTests.cpp
)

target_link_libraries(
    RenegadeStoryFlowGate7ProjectHomeLifecycleTests
    PRIVATE
        Renegade::EngineBridge
        Renegade::RuntimeBootstrap
)

target_compile_options(RenegadeStoryFlowGate7ProjectHomeLifecycleTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeStoryFlowGate7ProjectHomeLifecycleTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeStoryFlowGate7ProjectHomeLifecycleTests
)

add_test(
    NAME RenegadeStoryFlowGate7ProjectHomeLifecycleTests
    COMMAND RenegadeStoryFlowGate7ProjectHomeLifecycleTests
)

add_executable(RenegadeFlowTests
    FlowTests.cpp
)

target_link_libraries(RenegadeFlowTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeFlowTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeFlowTests)

add_test(
    NAME RenegadeFlowTests
    COMMAND RenegadeFlowTests
)

add_executable(RenegadeStoryFlowPresentationTests
    StoryFlowPresentationTests.cpp
)

target_link_libraries(RenegadeStoryFlowPresentationTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeStoryFlowPresentationTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeStoryFlowPresentationTests)

add_test(
    NAME RenegadeStoryFlowPresentationTests
    COMMAND RenegadeStoryFlowPresentationTests
)

add_executable(RenegadeRuntimeFlowTests
    RuntimeFlowTests.cpp
)

target_link_libraries(RenegadeRuntimeFlowTests
    PRIVATE
        Renegade::RuntimeBootstrap
)

target_compile_options(RenegadeRuntimeFlowTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeRuntimeFlowTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeRuntimeFlowTests)

add_test(
    NAME RenegadeRuntimeFlowTests
    COMMAND RenegadeRuntimeFlowTests
)

add_executable(RenegadeScreenTests
    ScreenTests.cpp
)

target_link_libraries(RenegadeScreenTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeScreenTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeScreenTests)

add_test(
    NAME RenegadeScreenTests
    COMMAND RenegadeScreenTests
)

add_executable(RenegadeRuntimeScreenTests
    RuntimeScreenTests.cpp
)

target_link_libraries(RenegadeRuntimeScreenTests
    PRIVATE
        Renegade::RuntimeBootstrap
)

target_compile_options(RenegadeRuntimeScreenTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeRuntimeScreenTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeRuntimeScreenTests)

add_test(
    NAME RenegadeRuntimeScreenTests
    COMMAND RenegadeRuntimeScreenTests
)

add_executable(RenegadeScreenRendererTests
    ScreenRendererTests.cpp
)

target_link_libraries(RenegadeScreenRendererTests
    PRIVATE
        Renegade::ScreenRenderer
)

target_compile_options(RenegadeScreenRendererTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

target_compile_definitions(RenegadeScreenRendererTests
    PRIVATE
        RENEGADE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

set_target_properties(RenegadeScreenRendererTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeScreenRendererTests)

add_test(
    NAME RenegadeScreenRendererTests
    COMMAND RenegadeScreenRendererTests
)

add_executable(RenegadeScreenAuthoringSessionTests
    ScreenAuthoringSessionTests.cpp
)

target_link_libraries(RenegadeScreenAuthoringSessionTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(RenegadeScreenAuthoringSessionTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeScreenAuthoringSessionTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeScreenAuthoringSessionTests
)

add_test(
    NAME RenegadeScreenAuthoringSessionTests
    COMMAND RenegadeScreenAuthoringSessionTests
)

add_executable(RenegadeProjectDocumentTransactionTests
    ProjectDocumentTransactionTests.cpp
)

target_link_libraries(RenegadeProjectDocumentTransactionTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeProjectDocumentTransactionTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeProjectDocumentTransactionTests
)

add_test(
    NAME RenegadeProjectDocumentTransactionTests
    COMMAND RenegadeProjectDocumentTransactionTests
)

add_executable(RenegadeProjectServiceTransactionTests
    ProjectServiceTransactionTests.cpp
)

target_link_libraries(RenegadeProjectServiceTransactionTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeProjectServiceTransactionTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeProjectServiceTransactionTests
)

add_test(
    NAME RenegadeProjectServiceTransactionTests
    COMMAND RenegadeProjectServiceTransactionTests
)

add_executable(RenegadeTestLevelSnapshotTests
    TestLevelSnapshotTests.cpp
)

target_link_libraries(RenegadeTestLevelSnapshotTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeTestLevelSnapshotTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeTestLevelSnapshotTests)

add_test(
    NAME RenegadeTestLevelSnapshotTests
    COMMAND RenegadeTestLevelSnapshotTests
)


add_executable(RenegadeTestLevelSnapshotRuntimeTests
    TestLevelSnapshotRuntimeTests.cpp
)

target_link_libraries(RenegadeTestLevelSnapshotRuntimeTests
    PRIVATE
        Renegade::RuntimeBootstrap
)

target_compile_options(RenegadeTestLevelSnapshotRuntimeTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeTestLevelSnapshotRuntimeTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeTestLevelSnapshotRuntimeTests
)

add_test(
    NAME RenegadeTestLevelSnapshotRuntimeTests
    COMMAND RenegadeTestLevelSnapshotRuntimeTests
)

add_executable(RenegadeTestLevelProcessFixture
    TestLevelProcessFixture.cpp
)

target_compile_options(RenegadeTestLevelProcessFixture
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeTestLevelProcessFixture PROPERTIES
    FOLDER "Renegade/TestFixtures"
)

add_executable(RenegadeTestLevelRuntimeProcessTests
    TestLevelRuntimeProcessTests.cpp
)

target_link_libraries(RenegadeTestLevelRuntimeProcessTests
    PRIVATE
        Renegade::TestLevelRuntimeProcess
)

target_compile_options(RenegadeTestLevelRuntimeProcessTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeTestLevelRuntimeProcessTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(
    RenegadeTestLevelRuntimeProcessTests
    RenegadeTestLevelProcessFixture
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeTestLevelRuntimeProcessTests
)

add_test(
    NAME RenegadeTestLevelRuntimeProcessTests
    COMMAND RenegadeTestLevelRuntimeProcessTests
        "$<TARGET_FILE:RenegadeTestLevelProcessFixture>"
)

# LP04 Gate 3B2a - real Runtime handshake proof. Deliberately EXCLUDE_FROM_ALL
# and NOT registered with add_test: this launches the real windowed
# RenegadeRuntime.exe end to end, which has not yet been proven stable on
# the GitHub Actions Windows runner's graphics adapter. Build and run this
# target explicitly and locally first (Gate 3B2a); only promote it into
# CTest/CI (Gate 3B2b) once real, observed behavior confirms it is stable.
add_executable(RenegadeTestLevelRuntimeHandshakeTests EXCLUDE_FROM_ALL
    TestLevelRuntimeHandshakeTests.cpp
)

target_link_libraries(RenegadeTestLevelRuntimeHandshakeTests
    PRIVATE
        Renegade::TestLevelRuntimeProcess
)

target_compile_options(RenegadeTestLevelRuntimeHandshakeTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeTestLevelRuntimeHandshakeTests PROPERTIES
    FOLDER "Renegade/Manual"
)

add_dependencies(
    RenegadeTestLevelRuntimeHandshakeTests
    RenegadeRuntime
)
