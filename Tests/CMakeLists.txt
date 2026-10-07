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

add_executable(RenegadeDiagnosticServiceTests DiagnosticServiceTests.cpp)
target_link_libraries(RenegadeDiagnosticServiceTests PRIVATE Renegade::EngineBridge)
set_target_properties(RenegadeDiagnosticServiceTests PROPERTIES FOLDER "Renegade/Tests")
add_test(NAME RenegadeDiagnosticServiceTests COMMAND RenegadeDiagnosticServiceTests)
if(WIN32)
    target_link_libraries(RenegadeDiagnosticServiceTests PRIVATE ws2_32)
endif()
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME RenegadeDiagnosticReaderTests
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/ReadRenegadeDiagnosticsTests.py")
endif()


add_executable(RenegadeDependencyTests
    DependencyTests.cpp
)
target_link_libraries(RenegadeDependencyTests PRIVATE Renegade::EngineBridge)
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


add_executable(RenegadeCharacterAiAnimationTests
    CharacterAiAnimationTests.cpp
)
target_link_libraries(
    RenegadeCharacterAiAnimationTests
    PRIVATE
        Renegade::EngineBridge
)
target_include_directories(
    RenegadeCharacterAiAnimationTests
    PRIVATE
        "${CMAKE_SOURCE_DIR}/Runtime/src"
)
set_target_properties(RenegadeCharacterAiAnimationTests PROPERTIES
    FOLDER "Renegade/Tests"
)
# The Studio CI builds RenegadeBridgeTests before running the full CTest
# registry, so this executable must be part of that aggregate dependency graph.
add_dependencies(RenegadeBridgeTests RenegadeCharacterAiAnimationTests)
add_test(
    NAME RenegadeCharacterAiAnimationTests
    COMMAND RenegadeCharacterAiAnimationTests
)

# Alpha Playability P1 - first-person Player View Rig.
add_executable(RenegadePlayerViewRigTests
    PlayerViewRigTests.cpp
)
target_link_libraries(
    RenegadePlayerViewRigTests
    PRIVATE
        Renegade::EngineBridge
)
target_include_directories(
    RenegadePlayerViewRigTests
    PRIVATE
        "${CMAKE_SOURCE_DIR}/Runtime/src"
)
target_compile_options(RenegadePlayerViewRigTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
        "$<$<CXX_COMPILER_ID:MSVC>:/bigobj>"
)
set_target_properties(RenegadePlayerViewRigTests PROPERTIES
    FOLDER "Renegade/Tests"
)
# Studio CI builds RenegadeBridgeTests before the complete CTest registry.
add_dependencies(RenegadeBridgeTests RenegadePlayerViewRigTests)
add_test(
    NAME RenegadePlayerViewRigTests
    COMMAND RenegadePlayerViewRigTests
)
add_test(
    NAME RenegadePlayerViewRigSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/PlayerViewRigSourceContract.cmake
)

# Manual Windows GPU fixture generator; not an automatic gameplay acceptance test.
# Writes a no-imported-arms scene for the standalone Runtime placeholder check.
if(WIN32)
    add_executable(RenegadePlayerViewRigFixture PlayerViewRigFixture.cpp)
    target_link_libraries(RenegadePlayerViewRigFixture PRIVATE Renegade::EngineBridge)
    target_include_directories(RenegadePlayerViewRigFixture PRIVATE
        "${CMAKE_SOURCE_DIR}/Runtime/src")
    target_compile_options(RenegadePlayerViewRigFixture PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    set_target_properties(RenegadePlayerViewRigFixture PROPERTIES
        FOLDER "Renegade/Tests")
endif()

# Manual GPU snapshot/staging proof for the owner-verified proxy scene.
if(WIN32)
    add_executable(RenegadePlayerViewRigParityFixture PlayerViewRigParityFixture.cpp)
    target_link_libraries(RenegadePlayerViewRigParityFixture PRIVATE Renegade::EngineBridge)
    target_compile_options(RenegadePlayerViewRigParityFixture PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    set_target_properties(RenegadePlayerViewRigParityFixture PROPERTIES FOLDER "Renegade/Tests")
endif()

# Manual DX12 GPU regression, intentionally excluded from headless CTest.
if(WIN32)
    add_executable(RenegadeStudioShaderDx12Proof StudioShaderDx12Proof.cpp)
    target_link_libraries(RenegadeStudioShaderDx12Proof PRIVATE Renegade::EngineBridge)
    set_target_properties(RenegadeStudioShaderDx12Proof PROPERTIES FOLDER "Renegade/Tests")
    add_custom_command(TARGET RenegadeStudioShaderDx12Proof POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/dxcompiler.dll"
            "$<TARGET_FILE_DIR:RenegadeStudioShaderDx12Proof>")
endif()

# Manual first-person assembly inspection. No third-party asset in source/CTest.
if(WIN32)
    add_executable(RenegadeFirstPersonAssemblyGraphicsProof
        FirstPersonAssemblyGraphicsProof.cpp
        ${CMAKE_SOURCE_DIR}/Studio/src/ModelImportPreview.cpp)
    target_link_libraries(RenegadeFirstPersonAssemblyGraphicsProof PRIVATE Renegade::EngineBridge)
    target_compile_definitions(RenegadeFirstPersonAssemblyGraphicsProof PRIVATE UNICODE _UNICODE)
    target_compile_options(RenegadeFirstPersonAssemblyGraphicsProof PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>" "$<$<CXX_COMPILER_ID:MSVC>:/bigobj>")
    set_target_properties(RenegadeFirstPersonAssemblyGraphicsProof PROPERTIES FOLDER "Renegade/Tests")
    add_custom_command(TARGET RenegadeFirstPersonAssemblyGraphicsProof POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/dxcompiler.dll"
            "$<TARGET_FILE_DIR:RenegadeFirstPersonAssemblyGraphicsProof>")
endif()

add_executable(RenegadeMatchingRigAnimationTests MatchingRigAnimationTests.cpp)
target_link_libraries(RenegadeMatchingRigAnimationTests PRIVATE Renegade::EngineBridge)
add_test(NAME MatchingRigAnimation COMMAND RenegadeMatchingRigAnimationTests)
set_target_properties(RenegadeMatchingRigAnimationTests PROPERTIES FOLDER "Renegade/Tests")

# Separate executable keeps the owner's accepted diagnostic preview running.
if(WIN32)
    add_executable(RenegadeFirstPersonAssemblyWorkflowProof
        FirstPersonAssemblyGraphicsProof.cpp
        ${CMAKE_SOURCE_DIR}/Studio/src/ModelImportPreview.cpp)
    target_link_libraries(RenegadeFirstPersonAssemblyWorkflowProof PRIVATE Renegade::EngineBridge)
    target_compile_definitions(RenegadeFirstPersonAssemblyWorkflowProof PRIVATE UNICODE _UNICODE)
    target_compile_options(RenegadeFirstPersonAssemblyWorkflowProof PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>" "$<$<CXX_COMPILER_ID:MSVC>:/bigobj>")
    set_target_properties(RenegadeFirstPersonAssemblyWorkflowProof PROPERTIES FOLDER "Renegade/Tests")
    add_custom_command(TARGET RenegadeFirstPersonAssemblyWorkflowProof POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/dxcompiler.dll"
            "$<TARGET_FILE_DIR:RenegadeFirstPersonAssemblyWorkflowProof>")
endif()

add_dependencies(RenegadeBridgeTests RenegadeMatchingRigAnimationTests)

add_executable(RenegadeFirstPersonAssemblySettingsTests FirstPersonAssemblySettingsTests.cpp)
target_link_libraries(RenegadeFirstPersonAssemblySettingsTests PRIVATE Renegade::EngineBridge)
add_test(NAME FirstPersonAssemblySettings COMMAND RenegadeFirstPersonAssemblySettingsTests)
set_target_properties(RenegadeFirstPersonAssemblySettingsTests PROPERTIES FOLDER "Renegade/Tests")
add_dependencies(RenegadeBridgeTests RenegadeFirstPersonAssemblySettingsTests)


add_executable(RenegadeGameplayInputTests Phase6Gate2InputTests.cpp)
target_link_libraries(RenegadeGameplayInputTests PRIVATE Renegade::EngineBridge)
target_compile_options(RenegadeGameplayInputTests PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
set_target_properties(RenegadeGameplayInputTests PROPERTIES FOLDER "Renegade/Tests")
add_test(NAME RenegadeGameplayInputTests COMMAND RenegadeGameplayInputTests)

add_dependencies(RenegadeBridgeTests RenegadeGameplayInputTests)

add_executable(RenegadePlayerPrefabTests PlayerPrefabTests.cpp)
target_link_libraries(RenegadePlayerPrefabTests PRIVATE Renegade::EngineBridge)
set_target_properties(RenegadePlayerPrefabTests PROPERTIES FOLDER "Renegade/Tests")
add_test(NAME RenegadePlayerPrefabTests COMMAND RenegadePlayerPrefabTests)
add_dependencies(RenegadeBridgeTests RenegadePlayerPrefabTests)

# Pure action ownership/state proof; no graphics device or native skeleton required.
add_executable(RenegadeEquipmentActionStateTests EquipmentActionStateTests.cpp)
target_include_directories(RenegadeEquipmentActionStateTests PRIVATE ${CMAKE_SOURCE_DIR}/EngineBridge/include)
target_compile_features(RenegadeEquipmentActionStateTests PRIVATE cxx_std_17)
add_test(NAME EquipmentActionState COMMAND RenegadeEquipmentActionStateTests)
set_target_properties(RenegadeEquipmentActionStateTests PROPERTIES FOLDER "Renegade/Tests")
add_dependencies(RenegadeBridgeTests RenegadeEquipmentActionStateTests)

add_executable(RenegadeEquipmentAssetTests EquipmentAssetTests.cpp)
target_link_libraries(RenegadeEquipmentAssetTests PRIVATE Renegade::EngineBridge)
set_target_properties(RenegadeEquipmentAssetTests PROPERTIES FOLDER "Renegade/Tests")
add_test(NAME EquipmentAsset COMMAND RenegadeEquipmentAssetTests)
add_dependencies(RenegadeBridgeTests RenegadeEquipmentAssetTests)
target_include_directories(RenegadeEquipmentAssetTests PRIVATE "${CMAKE_SOURCE_DIR}/WickedEngine/Editor")

add_executable(RenegadeLaunchSocketTests LaunchSocketTests.cpp)
target_link_libraries(RenegadeLaunchSocketTests PRIVATE Renegade::EngineBridge)
target_include_directories(RenegadeLaunchSocketTests PRIVATE "${CMAKE_SOURCE_DIR}/Runtime/src")
add_test(NAME LaunchSocket COMMAND RenegadeLaunchSocketTests)
set_target_properties(RenegadeLaunchSocketTests PROPERTIES FOLDER "Renegade/Tests")
add_dependencies(RenegadeBridgeTests RenegadeLaunchSocketTests)

if(WIN32)
    add_executable(RenegadeProjectilePlayground ../Tools/ProjectilePlayground.cpp)
    target_link_libraries(RenegadeProjectilePlayground PRIVATE Renegade::EngineBridge)
    set_target_properties(RenegadeProjectilePlayground PROPERTIES FOLDER "Renegade/Tools")
endif()
