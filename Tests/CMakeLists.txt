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

add_executable(RenegadeRiverSplineTests
    RiverSplineTests.cpp
)
target_link_libraries(RenegadeRiverSplineTests PRIVATE Renegade::EngineBridge)
set_target_properties(RenegadeRiverSplineTests PROPERTIES FOLDER "Renegade")
add_test(NAME RenegadeRiverSplineTests COMMAND RenegadeRiverSplineTests)

if(WIN32)
    add_executable(RenegadeRiverSplineGraphicsProof RiverSplineGraphicsProof.cpp)
    target_link_libraries(RenegadeRiverSplineGraphicsProof PRIVATE Renegade::EngineBridge)
    # Disposable DX12 diagnostics: one native Wicked WATER rectangular plane
    # with identical material, varied orientation and absolute elevation.
    add_executable(RenegadeNativeWaterPlaneOrientationProof NativeWaterPlaneOrientationProof.cpp)
    target_link_libraries(RenegadeNativeWaterPlaneOrientationProof PRIVATE Renegade::EngineBridge)
    set_target_properties(RenegadeNativeWaterPlaneOrientationProof PROPERTIES FOLDER "Renegade")
    renegade_deploy_river_underwater_shader(RenegadeNativeWaterPlaneOrientationProof)
    add_custom_command(TARGET RenegadeNativeWaterPlaneOrientationProof POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:RenegadeNativeWaterPlaneOrientationProof>/Content/river"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/WickedEngine/Content/models/waterNormalMap.jpg"
            "$<TARGET_FILE_DIR:RenegadeNativeWaterPlaneOrientationProof>/Content/river/waterNormalMap.jpg"
        VERBATIM)
    set_target_properties(RenegadeRiverSplineGraphicsProof PROPERTIES FOLDER "Renegade")

    add_executable(RenegadeRiverTerrainFollowProof RiverTerrainFollowProof.cpp)
    target_link_libraries(RenegadeRiverTerrainFollowProof PRIVATE Renegade::EngineBridge)
    foreach(river_gpu_proof IN ITEMS RenegadeRiverSplineGraphicsProof RenegadeRiverTerrainFollowProof)
        renegade_deploy_river_underwater_shader(${river_gpu_proof})
        add_custom_command(TARGET ${river_gpu_proof} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${river_gpu_proof}>/Content/river"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/WickedEngine/Content/models/waterNormalMap.jpg"
                "$<TARGET_FILE_DIR:${river_gpu_proof}>/Content/river/waterNormalMap.jpg"
            VERBATIM)
    endforeach()
    if(TARGET RenegadeTerrainSurfacePacker)
        add_dependencies(RenegadeRiverTerrainFollowProof RenegadeTerrainSurfacePacker)
        add_custom_command(TARGET RenegadeRiverTerrainFollowProof POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:RenegadeRiverTerrainFollowProof>/Content/terrain/default_grass"
            COMMAND $<TARGET_FILE:RenegadeTerrainSurfacePacker>
                "${CMAKE_SOURCE_DIR}/Studio/assets/terrain/default_grass/default_grass_ao.tga"
                "${CMAKE_SOURCE_DIR}/Studio/assets/terrain/default_grass/default_grass_roughness.tga"
                "${CMAKE_SOURCE_DIR}/Studio/assets/terrain/default_grass/default_grass_basecolor.tga"
                "${CMAKE_SOURCE_DIR}/Studio/assets/terrain/default_grass/default_grass_normal.tga"
                "$<TARGET_FILE_DIR:RenegadeRiverTerrainFollowProof>/Content/terrain/default_grass/default_grass_basecolor.tga"
                "$<TARGET_FILE_DIR:RenegadeRiverTerrainFollowProof>/Content/terrain/default_grass/default_grass_normal.tga"
                "$<TARGET_FILE_DIR:RenegadeRiverTerrainFollowProof>/Content/terrain/default_grass/default_grass_surface.tga"
                "32"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/WickedEngine/Content/terrain/grass.wiscene"
                "$<TARGET_FILE_DIR:RenegadeRiverTerrainFollowProof>/Content/terrain/grass.wiscene"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/WickedEngine/Content/terrain/grassparticle.png"
                "$<TARGET_FILE_DIR:RenegadeRiverTerrainFollowProof>/Content/terrain/grassparticle.png"
            VERBATIM)
    endif()
    set_target_properties(RenegadeRiverTerrainFollowProof PROPERTIES FOLDER "Renegade")
endif()

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
set_tests_properties(LaunchSocket PROPERTIES TIMEOUT 20)
set_target_properties(RenegadeLaunchSocketTests PROPERTIES FOLDER "Renegade/Tests")
add_dependencies(RenegadeBridgeTests RenegadeLaunchSocketTests)

if(WIN32)
    add_executable(RenegadeProjectilePlayground ../Tools/ProjectilePlayground.cpp)
    target_link_libraries(RenegadeProjectilePlayground PRIVATE Renegade::EngineBridge)
    set_target_properties(RenegadeProjectilePlayground PROPERTIES FOLDER "Renegade/Tools")
endif()


# Night Sky feature bridge contract (isolated branch only).
add_executable(RenegadeNightSkyTests NightSkyStateTests.cpp)
target_link_libraries(RenegadeNightSkyTests PRIVATE Renegade::EngineBridge)
set_target_properties(RenegadeNightSkyTests PROPERTIES FOLDER "Renegade/Tests")
add_test(NAME RenegadeNightSkyTests COMMAND RenegadeNightSkyTests)
add_test(NAME RenegadeNightSkySourceContract COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/NightSkySourceContract.cmake")

# Editor camera proof is GPU-independent and remains active in Release.
add_executable(RenegadeScreenEditorCanvasCameraTests ScreenEditorCanvasCameraTests.cpp)
target_compile_features(RenegadeScreenEditorCanvasCameraTests PRIVATE cxx_std_17)
add_test(NAME RenegadeScreenEditorCanvasCameraTests COMMAND RenegadeScreenEditorCanvasCameraTests)
set_target_properties(RenegadeScreenEditorCanvasCameraTests PROPERTIES FOLDER "Renegade/Tests")

add_executable(RenegadeScreenUsageTests ScreenUsageTests.cpp)
target_link_libraries(RenegadeScreenUsageTests PRIVATE Renegade::RuntimeBootstrap)
add_test(NAME RenegadeScreenUsageTests COMMAND RenegadeScreenUsageTests)
set_tests_properties(RenegadeScreenUsageTests PROPERTIES TIMEOUT 30)
add_executable(RenegadeGameplayAttributeProfileTests GameplayAttributeProfileTests.cpp)
target_link_libraries(RenegadeGameplayAttributeProfileTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeGameplayAttributeProfileTests COMMAND RenegadeGameplayAttributeProfileTests)
set_tests_properties(RenegadeGameplayAttributeProfileTests PROPERTIES TIMEOUT 30)

add_executable(RenegadeScreenBindingTests ScreenBindingTests.cpp)
target_link_libraries(RenegadeScreenBindingTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeScreenBindingTests COMMAND RenegadeScreenBindingTests)
set_tests_properties(RenegadeScreenBindingTests PROPERTIES TIMEOUT 30)

add_executable(RenegadeScreenProgressTests ScreenProgressTests.cpp)
target_link_libraries(RenegadeScreenProgressTests PRIVATE Renegade::RuntimeBootstrap)
add_test(NAME RenegadeScreenProgressTests COMMAND RenegadeScreenProgressTests)
set_tests_properties(RenegadeScreenProgressTests PROPERTIES TIMEOUT 30)

add_executable(RenegadeScreenBindingFrameTests ScreenBindingFrameTests.cpp)
target_link_libraries(RenegadeScreenBindingFrameTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeScreenBindingFrameTests COMMAND RenegadeScreenBindingFrameTests)
set_tests_properties(RenegadeScreenBindingFrameTests PROPERTIES TIMEOUT 30)
add_executable(RenegadeRuntimeUiGameplaySnapshotTests RuntimeUiGameplaySnapshotTests.cpp)
target_link_libraries(RenegadeRuntimeUiGameplaySnapshotTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeRuntimeUiGameplaySnapshotTests COMMAND RenegadeRuntimeUiGameplaySnapshotTests)
set_tests_properties(RenegadeRuntimeUiGameplaySnapshotTests PROPERTIES TIMEOUT 30)

add_executable(RenegadeRuntimeScreenBindingTests RuntimeScreenBindingTests.cpp)
target_link_libraries(RenegadeRuntimeScreenBindingTests PRIVATE Renegade::RuntimeBootstrap)
add_test(NAME RenegadeRuntimeScreenBindingTests COMMAND RenegadeRuntimeScreenBindingTests)
set_tests_properties(RenegadeRuntimeScreenBindingTests PROPERTIES TIMEOUT 30)


add_executable(RenegadeScreenCompositionTests ScreenCompositionTests.cpp)
target_include_directories(RenegadeScreenCompositionTests PRIVATE ${CMAKE_SOURCE_DIR}/EngineBridge/include)
target_compile_features(RenegadeScreenCompositionTests PRIVATE cxx_std_17)
add_test(NAME RenegadeScreenCompositionTests COMMAND RenegadeScreenCompositionTests)
set_tests_properties(RenegadeScreenCompositionTests PROPERTIES TIMEOUT 30)

add_executable(RenegadeRuntimeUiCompositionTests RuntimeUiCompositionTests.cpp)
target_link_libraries(RenegadeRuntimeUiCompositionTests PRIVATE Renegade::EngineBridge)
target_compile_features(RenegadeRuntimeUiCompositionTests PRIVATE cxx_std_17)
add_test(NAME RenegadeRuntimeUiCompositionTests COMMAND RenegadeRuntimeUiCompositionTests)
set_tests_properties(RenegadeRuntimeUiCompositionTests PROPERTIES TIMEOUT 30)

add_executable(RenegadeScreenCompositionPersistenceTests ScreenCompositionPersistenceTests.cpp)
target_link_libraries(RenegadeScreenCompositionPersistenceTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeScreenCompositionPersistenceTests COMMAND RenegadeScreenCompositionPersistenceTests)
set_tests_properties(RenegadeScreenCompositionPersistenceTests PROPERTIES TIMEOUT 30)

add_executable(RenegadeHudSelectionTests HudSelectionTests.cpp)
target_link_libraries(RenegadeHudSelectionTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeHudSelectionTests COMMAND RenegadeHudSelectionTests)
set_tests_properties(RenegadeHudSelectionTests PROPERTIES TIMEOUT 30)

add_executable(RenegadeScreenBindingTargetTests ScreenBindingTargetTests.cpp)
target_link_libraries(RenegadeScreenBindingTargetTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeScreenBindingTargetTests COMMAND RenegadeScreenBindingTargetTests)
set_tests_properties(RenegadeScreenBindingTargetTests PROPERTIES TIMEOUT 30)

add_executable(RenegadeHudReferencePersistenceTests HudReferencePersistenceTests.cpp)
target_link_libraries(RenegadeHudReferencePersistenceTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeHudReferencePersistenceTests COMMAND RenegadeHudReferencePersistenceTests)
set_tests_properties(RenegadeHudReferencePersistenceTests PROPERTIES TIMEOUT 30)

add_executable(RenegadeUiRuntimeHudNativeFixture UiRuntimeHudNativeFixture.cpp)
target_link_libraries(RenegadeUiRuntimeHudNativeFixture PRIVATE Renegade::EngineBridge)
set_target_properties(RenegadeUiRuntimeHudNativeFixture PROPERTIES FOLDER "Renegade/Tests")

add_executable(RenegadeScreenLibraryTests ScreenLibraryTests.cpp)
target_link_libraries(RenegadeScreenLibraryTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeScreenLibraryTests COMMAND RenegadeScreenLibraryTests)
set_tests_properties(RenegadeScreenLibraryTests PROPERTIES TIMEOUT 30)

add_executable(RenegadeGameplayAttributeDocumentTests GameplayAttributeDocumentTests.cpp)
target_include_directories(RenegadeGameplayAttributeDocumentTests PRIVATE "${CMAKE_SOURCE_DIR}/Runtime/src")
target_link_libraries(RenegadeGameplayAttributeDocumentTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeGameplayAttributeDocumentTests COMMAND RenegadeGameplayAttributeDocumentTests)
set_tests_properties(RenegadeGameplayAttributeDocumentTests PROPERTIES TIMEOUT 30)
target_include_directories(RenegadeScreenLibraryTests PRIVATE ${PROJECT_SOURCE_DIR}/WickedEngine/Editor)

# Default creator scaffold: real Flow/Level, assets, optional capabilities and retained resume.
add_executable(RenegadeProjectUiScaffoldTests ProjectUiScaffoldTests.cpp)
target_link_libraries(RenegadeProjectUiScaffoldTests PRIVATE Renegade::EngineBridge Renegade::RuntimeBootstrap)
target_compile_options(RenegadeProjectUiScaffoldTests PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
add_dependencies(RenegadeBridgeTests RenegadeProjectUiScaffoldTests)
add_test(NAME RenegadeProjectUiScaffoldTests COMMAND RenegadeProjectUiScaffoldTests)

add_executable(RenegadeScreenPerformanceBudgetTests ScreenPerformanceBudgetTests.cpp)
target_link_libraries(RenegadeScreenPerformanceBudgetTests PRIVATE Renegade::EngineBridge)
target_compile_options(RenegadeScreenPerformanceBudgetTests PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
add_dependencies(RenegadeBridgeTests RenegadeScreenPerformanceBudgetTests)
add_test(NAME RenegadeScreenPerformanceBudgetTests COMMAND RenegadeScreenPerformanceBudgetTests)
add_executable(RenegadeScreenAuthoringCompletionTests ScreenAuthoringCompletionTests.cpp)
target_link_libraries(RenegadeScreenAuthoringCompletionTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeScreenAuthoringCompletionTests COMMAND RenegadeScreenAuthoringCompletionTests)

# Explicit native GPU diagnostic, run against a disposable scene copy only.
# It is not a headless CI test and does not substitute for Studio owner checks.
if(WIN32)
    add_executable(RenegadeTerrainGraphicsProof TerrainGraphicsProof.cpp)
    target_link_libraries(RenegadeTerrainGraphicsProof PRIVATE Renegade::EngineBridge)
    target_compile_definitions(RenegadeTerrainGraphicsProof PRIVATE UNICODE _UNICODE)
    target_compile_options(RenegadeTerrainGraphicsProof PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/bigobj>")
    set_target_properties(RenegadeTerrainGraphicsProof PROPERTIES
        FOLDER "Renegade/Tests"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/TerrainGraphicsProof")
    add_dependencies(RenegadeTerrainGraphicsProof RenegadeStudio)
    add_custom_command(TARGET RenegadeTerrainGraphicsProof POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/dxcompiler.dll"
            "$<TARGET_FILE_DIR:RenegadeTerrainGraphicsProof>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<TARGET_FILE_DIR:RenegadeStudio>/Content/terrain"
            "$<TARGET_FILE_DIR:RenegadeTerrainGraphicsProof>/Content/terrain")
endif()

# Production Player Start camera-preview isolation regression guard.
add_test(NAME RenegadePlayerPreviewTerrainIsolationSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/PlayerPreviewTerrainIsolationSourceContract.cmake)

add_test(NAME RenegadeLocalTestGameHandoffSourceContract
 COMMAND ${CMAKE_COMMAND} -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
 -P ${CMAKE_CURRENT_SOURCE_DIR}/LocalTestGameHandoffSourceContract.cmake)
add_executable(RenegadeRiverUnderwaterTests RiverUnderwaterTests.cpp)
target_link_libraries(RenegadeRiverUnderwaterTests PRIVATE Renegade::EngineBridge)
set_target_properties(RenegadeRiverUnderwaterTests PROPERTIES FOLDER "Renegade")
add_test(NAME RenegadeRiverUnderwaterTests COMMAND RenegadeRiverUnderwaterTests)

if(WIN32)
    add_executable(RenegadeRiverUnderwaterGraphicsProof RiverUnderwaterGraphicsProof.cpp)
    target_link_libraries(RenegadeRiverUnderwaterGraphicsProof PRIVATE Renegade::EngineBridge)
    set_target_properties(RenegadeRiverUnderwaterGraphicsProof PROPERTIES FOLDER "Renegade")
    renegade_deploy_river_underwater_shader(RenegadeRiverUnderwaterGraphicsProof)
endif()

add_executable(RenegadePlayerSwimmingTests PlayerSwimmingTests.cpp)
target_link_libraries(RenegadePlayerSwimmingTests PRIVATE Renegade::EngineBridge)
add_dependencies(RenegadeBridgeTests RenegadePlayerSwimmingTests)
add_test(NAME RenegadePlayerSwimmingTests COMMAND RenegadePlayerSwimmingTests)
if(WIN32)
    add_executable(RenegadePlayerSwimmingGraphicsProof PlayerSwimmingGraphicsProof.cpp)
    target_link_libraries(RenegadePlayerSwimmingGraphicsProof PRIVATE Renegade::EngineBridge)
    renegade_deploy_river_underwater_shader(RenegadePlayerSwimmingGraphicsProof)
endif()

if(WIN32)
    add_executable(RenegadeRiverCausticsGraphicsProof RiverCausticsGraphicsProof.cpp)
    target_link_libraries(RenegadeRiverCausticsGraphicsProof PRIVATE Renegade::EngineBridge)
endif()

if(WIN32)
    add_executable(RenegadeRiverEndpointGraphicsProof RiverEndpointGraphicsProof.cpp)
    target_link_libraries(RenegadeRiverEndpointGraphicsProof PRIVATE Renegade::EngineBridge)
    add_custom_command(TARGET RenegadeRiverCausticsGraphicsProof POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:RenegadeRiverCausticsGraphicsProof>/Content/river"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/WickedEngine/Content/models/waterNormalMap.jpg" "$<TARGET_FILE_DIR:RenegadeRiverCausticsGraphicsProof>/Content/river/waterNormalMap.jpg")
endif()

# Isolated flat-river contact foam investigation, no editor exposure.
if(WIN32)
 add_executable(RenegadeFlatRiverFoamGraphicsProof FlatRiverFoamGraphicsProof.cpp)
 target_link_libraries(RenegadeFlatRiverFoamGraphicsProof PRIVATE Renegade::EngineBridge)
 target_include_directories(RenegadeFlatRiverFoamGraphicsProof PRIVATE "${PROJECT_SOURCE_DIR}/WickedEngine/Editor")
 add_executable(RenegadeRiversideFoamSceneProof RiversideFoamSceneProof.cpp)
 target_link_libraries(RenegadeRiversideFoamSceneProof PRIVATE Renegade::EngineBridge)
 target_include_directories(RenegadeRiversideFoamSceneProof PRIVATE "${PROJECT_SOURCE_DIR}/WickedEngine/Editor")
 add_executable(RenegadeMeshFoamGraphicsProof MeshFoamGraphicsProof.cpp)
 target_link_libraries(RenegadeMeshFoamGraphicsProof PRIVATE Renegade::EngineBridge)
 target_include_directories(RenegadeMeshFoamGraphicsProof PRIVATE "${PROJECT_SOURCE_DIR}/WickedEngine/Editor")
 add_executable(RenegadeNativeRiverFoamLevelProbe NativeRiverFoamLevelProbe.cpp)
 target_link_libraries(RenegadeNativeRiverFoamLevelProbe PRIVATE Renegade::EngineBridge)
 target_include_directories(RenegadeNativeRiverFoamLevelProbe PRIVATE "${PROJECT_SOURCE_DIR}/WickedEngine/Editor")
endif()

add_executable(RenegadeRiverFoamTests RiverFoamTests.cpp)
target_link_libraries(RenegadeRiverFoamTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeRiverFoamTests COMMAND RenegadeRiverFoamTests)

# Ocean shore foam parity on a saved real terrain/river fixture.
if(WIN32)
 add_executable(RenegadeRiverOceanContactProof RiverOceanContactProof.cpp)
 target_link_libraries(RenegadeRiverOceanContactProof PRIVATE Renegade::EngineBridge)
endif()

# River V2 G0/G1: isolated native baseline and geometry oracle; no Studio integration.
if(WIN32)
    add_executable(RenegadeRiverFlowFieldNativeProof RiverFlowFieldNativeProof.cpp)
    target_link_libraries(RenegadeRiverFlowFieldNativeProof PRIVATE Renegade::EngineBridge)
    set_target_properties(RenegadeRiverFlowFieldNativeProof PROPERTIES FOLDER "Renegade/Tests")
endif()

add_executable(RenegadeRiverFlowFieldTests RiverFlowFieldTests.cpp)
target_link_libraries(RenegadeRiverFlowFieldTests PRIVATE Renegade::EngineBridge)
set_target_properties(RenegadeRiverFlowFieldTests PROPERTIES FOLDER "Renegade/Tests")
add_test(NAME RenegadeRiverFlowFieldTests COMMAND RenegadeRiverFlowFieldTests)

# G2 standalone diagnostic only: temporary native debug primitives, no Studio binding.
if(WIN32)
    add_executable(RenegadeRiverFlowDiagnosticProof RiverFlowDiagnosticProof.cpp)
    target_link_libraries(RenegadeRiverFlowDiagnosticProof PRIVATE Renegade::EngineBridge)
    set_target_properties(RenegadeRiverFlowDiagnosticProof PROPERTIES FOLDER "Renegade/Tests")
endif()

# Isolated G3 normal-motion proof. Native production WATER remains unchanged.
if(WIN32)
    add_executable(RenegadeRiverFlowNormalProof RiverFlowNormalProof.cpp)
    target_link_libraries(RenegadeRiverFlowNormalProof PRIVATE Renegade::EngineBridge)
    set_target_properties(RenegadeRiverFlowNormalProof PROPERTIES FOLDER "Renegade/Tests")
endif()

# G4 isolated geometry classifier; no foam or Studio integration.
add_executable(RenegadeRiverWaterlineTests RiverWaterlineTests.cpp)
target_link_libraries(RenegadeRiverWaterlineTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeRiverWaterlineTests COMMAND RenegadeRiverWaterlineTests)
if(WIN32)
    add_executable(RenegadeRiverWaterlineProof RiverWaterlineProof.cpp)
    target_link_libraries(RenegadeRiverWaterlineProof PRIVATE Renegade::EngineBridge)
    set_target_properties(RenegadeRiverWaterlineProof PROPERTIES FOLDER "Renegade/Tests")
endif()

# G5 bounded contact-source transport and native WATER shading proof only.
add_executable(RenegadeRiverFoamTransportTests RiverFoamTransportTests.cpp)
target_link_libraries(RenegadeRiverFoamTransportTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeRiverFoamTransportTests COMMAND RenegadeRiverFoamTransportTests)
if(WIN32)
    add_executable(RenegadeRiverContactFoamProof RiverContactFoamProof.cpp)
    target_link_libraries(RenegadeRiverContactFoamProof PRIVATE Renegade::EngineBridge)
    set_target_properties(RenegadeRiverContactFoamProof PROPERTIES FOLDER "Renegade/Tests")
endif()

add_executable(RenegadeRiverFlowProfileTests RiverFlowProfileTests.cpp)
target_link_libraries(RenegadeRiverFlowProfileTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeRiverFlowProfileTests COMMAND RenegadeRiverFlowProfileTests)

if(WIN32)
    add_executable(RenegadeRiverFlowIntegrationProof RiverFlowIntegrationProof.cpp)
    target_link_libraries(RenegadeRiverFlowIntegrationProof PRIVATE Renegade::EngineBridge)
    renegade_deploy_river_flow_shaders(RenegadeRiverFlowIntegrationProof)
endif()

if(WIN32)
    add_executable(RenegadeRiverFlowScalabilityProof RiverFlowScalabilityProof.cpp)
    target_link_libraries(RenegadeRiverFlowScalabilityProof PRIVATE Renegade::EngineBridge)
    renegade_deploy_river_flow_shaders(RenegadeRiverFlowScalabilityProof)
endif()

if(WIN32)
    set(_g6_package_stage "${CMAKE_BINARY_DIR}/river-g6-package-stage")
    execute_process(COMMAND "${RENEGADE_RIVER_PYTHON}"
        "${CMAKE_SOURCE_DIR}/Tools/prepare_river_g6_package_proof.py" "${_g6_package_stage}"
        RESULT_VARIABLE _g6_stage_result)
    if(NOT _g6_stage_result EQUAL 0)
        message(FATAL_ERROR "River G6 package proof staging failed")
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE _g6_revision OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/WickedEngine" OUTPUT_VARIABLE _g6_wicked OUTPUT_STRIP_TRAILING_WHITESPACE)
    add_executable(RenegadeRiverFlowPackageProof RiverFlowPackageProof.cpp
        "${_g6_package_stage}/WindowsGameBuildController.cpp")
    target_include_directories(RenegadeRiverFlowPackageProof PRIVATE "${CMAKE_SOURCE_DIR}/Studio/src")
    target_link_libraries(RenegadeRiverFlowPackageProof PRIVATE Renegade::EngineBridge)
    target_compile_definitions(RenegadeRiverFlowPackageProof PRIVATE
        RENEGADE_SOURCE_REVISION="${_g6_revision}" RENEGADE_WICKED_REVISION="${_g6_wicked}")
endif()

if(WIN32)
    add_executable(RenegadeRiverFlowRuntimeLifecycleProof RiverFlowRuntimeLifecycleProof.cpp
        "${_g6_package_stage}/TestLevelRuntimeProcess.cpp")
    target_include_directories(RenegadeRiverFlowRuntimeLifecycleProof PRIVATE "${CMAKE_SOURCE_DIR}/Studio/src")
    target_link_libraries(RenegadeRiverFlowRuntimeLifecycleProof PRIVATE Renegade::EngineBridge)
endif()





add_executable(RenegadeRiverFlowSourceSelectionTests RiverFlowSourceSelectionTests.cpp)
target_link_libraries(RenegadeRiverFlowSourceSelectionTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeRiverFlowSourceSelectionTests COMMAND RenegadeRiverFlowSourceSelectionTests)

# G7A: explicit flat reach topology; no production renderer integration.
add_executable(RenegadeRiverFlowNetworkTests RiverFlowNetworkTests.cpp)
target_link_libraries(RenegadeRiverFlowNetworkTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeRiverFlowNetworkTests COMMAND RenegadeRiverFlowNetworkTests)
if(WIN32)
    add_executable(RenegadeRiverFlowNetworkNativeProof RiverFlowNetworkNativeProof.cpp)
    target_link_libraries(RenegadeRiverFlowNetworkNativeProof PRIVATE Renegade::EngineBridge)
endif()

# G7B isolated native-footprint union and continuous velocity diagnostic.
add_executable(RenegadeRiverJunctionSurfaceTests RiverJunctionSurfaceTests.cpp)
target_link_libraries(RenegadeRiverJunctionSurfaceTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeRiverJunctionSurfaceTests COMMAND RenegadeRiverJunctionSurfaceTests)

if(WIN32)
    add_executable(RenegadeRiverJunctionGraphicsProof RiverJunctionGraphicsProof.cpp)
    target_link_libraries(RenegadeRiverJunctionGraphicsProof PRIVATE Renegade::EngineBridge)
endif()

# G7C isolated conservative transport across the G7B native-footprint union.
set(_junction_foam_stage "${CMAKE_BINARY_DIR}/river-junction-foam-stage")
execute_process(COMMAND "${RENEGADE_RIVER_PYTHON}"
    "${CMAKE_SOURCE_DIR}/Tools/prepare_river_junction_foam_proof.py" "${_junction_foam_stage}"
    RESULT_VARIABLE _junction_foam_stage_result)
if(NOT _junction_foam_stage_result EQUAL 0)
    message(FATAL_ERROR "River junction foam classifier staging failed")
endif()
add_executable(RenegadeRiverJunctionFoamTransportTests RiverJunctionFoamTransportTests.cpp)
target_include_directories(RenegadeRiverJunctionFoamTransportTests PRIVATE "${_junction_foam_stage}")
target_link_libraries(RenegadeRiverJunctionFoamTransportTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeRiverJunctionFoamTransportTests COMMAND RenegadeRiverJunctionFoamTransportTests)

if(WIN32)
    add_executable(RenegadeRiverJunctionFoamGraphicsProof RiverJunctionFoamGraphicsProof.cpp)
    target_include_directories(RenegadeRiverJunctionFoamGraphicsProof PRIVATE "${_junction_foam_stage}")
    target_link_libraries(RenegadeRiverJunctionFoamGraphicsProof PRIVATE Renegade::EngineBridge)
endif()

# G7D persistent UUID attachments and logical native spans.
add_executable(RenegadeRiverWaterwayTests RiverWaterwayTests.cpp)
target_link_libraries(RenegadeRiverWaterwayTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeRiverWaterwayTests COMMAND RenegadeRiverWaterwayTests)

if(WIN32)
    add_executable(RenegadeRiverWaterwayNativeProof RiverWaterwayNativeProof.cpp)
    target_link_libraries(RenegadeRiverWaterwayNativeProof PRIVATE Renegade::EngineBridge)
endif()

# Bridge adoption must preserve the accepted proof's aggregate flux/source/transport.
add_executable(RenegadeRiverWaterwayTransportTests RiverWaterwayTransportTests.cpp)
target_include_directories(RenegadeRiverWaterwayTransportTests PRIVATE "${_junction_foam_stage}")
target_link_libraries(RenegadeRiverWaterwayTransportTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeRiverWaterwayTransportTests COMMAND RenegadeRiverWaterwayTransportTests)

if(WIN32)
    add_executable(RenegadeRiverWaterwayBindingProof RiverWaterwayBindingProof.cpp)
    target_link_libraries(RenegadeRiverWaterwayBindingProof PRIVATE Renegade::EngineBridge)
endif()


# G8A isolated depth-aware rocky riffle prototype; no Studio/Runtime adoption.
add_executable(RenegadeRiverRapidsTests RiverRapidsTests.cpp)
target_link_libraries(RenegadeRiverRapidsTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeRiverRapidsTests COMMAND RenegadeRiverRapidsTests)
if(WIN32)
    add_executable(RenegadeRiverRapidsProof RiverRapidsProof.cpp)
    target_include_directories(RenegadeRiverRapidsProof PRIVATE "${_junction_foam_stage}")
    target_link_libraries(RenegadeRiverRapidsProof PRIVATE Renegade::EngineBridge)
endif()

# G8C optional appearance proof; no production shader deployment.
if(WIN32)
    set(_bubbly_proof_stage "${CMAKE_BINARY_DIR}/river-bubbly-proof-stage")
    execute_process(COMMAND "${RENEGADE_RIVER_PYTHON}"
        "${CMAKE_SOURCE_DIR}/Tools/prepare_river_bubbly_graphics_proof.py" "${_bubbly_proof_stage}"
        RESULT_VARIABLE _bubbly_stage_result)
    if(NOT _bubbly_stage_result EQUAL 0)
        message(FATAL_ERROR "Bubbly foam proof staging failed")
    endif()
    add_executable(RenegadeRiverBubblyFoamProof "${_bubbly_proof_stage}/RiverBubblyFoamProof.cpp")
    target_include_directories(RenegadeRiverBubblyFoamProof PRIVATE
        "${CMAKE_SOURCE_DIR}/Tests" "${_junction_foam_stage}")
    target_link_libraries(RenegadeRiverBubblyFoamProof PRIVATE Renegade::EngineBridge)
endif()

# G8D optional surface proof; production WATER deployment remains unchanged.
if(WIN32)
    set(_surface_proof_stage "${CMAKE_BINARY_DIR}/river-surface-proof-stage")
    execute_process(COMMAND "${RENEGADE_RIVER_PYTHON}"
        "${CMAKE_SOURCE_DIR}/Tools/prepare_river_surface_graphics_proof.py" "${_surface_proof_stage}"
        RESULT_VARIABLE _surface_stage_result)
    if(NOT _surface_stage_result EQUAL 0)
        message(FATAL_ERROR "Surface motion proof staging failed")
    endif()
    add_executable(RenegadeRiverSurfaceMotionProof "${_surface_proof_stage}/RiverSurfaceMotionProof.cpp")
    target_include_directories(RenegadeRiverSurfaceMotionProof PRIVATE
        "${CMAKE_SOURCE_DIR}/Tests" "${_junction_foam_stage}")
    target_link_libraries(RenegadeRiverSurfaceMotionProof PRIVATE Renegade::EngineBridge)
endif()

add_executable(RenegadeRiverSurfaceMotionTests RiverSurfaceMotionTests.cpp)
target_link_libraries(RenegadeRiverSurfaceMotionTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeRiverSurfaceMotionTests COMMAND RenegadeRiverSurfaceMotionTests)

# G8E optional transported foam-detail proof; no production renderer adoption.
if(WIN32)
    set(_strands_stage "${CMAKE_BINARY_DIR}/river-strands-proof-stage")
    execute_process(COMMAND "${RENEGADE_RIVER_PYTHON}"
        "${CMAKE_SOURCE_DIR}/Tools/prepare_river_strands_graphics_proof.py" "${_strands_stage}"
        RESULT_VARIABLE _strands_stage_result)
    if(NOT _strands_stage_result EQUAL 0)
        message(FATAL_ERROR "River strands proof staging failed")
    endif()
    add_executable(RenegadeRiverFoamStrandsProof "${_strands_stage}/RiverFoamStrandsProof.cpp")
    target_include_directories(RenegadeRiverFoamStrandsProof PRIVATE "${CMAKE_SOURCE_DIR}/Tests" "${_junction_foam_stage}")
    target_link_libraries(RenegadeRiverFoamStrandsProof PRIVATE Renegade::EngineBridge)
endif()

# G8F optional connected-web source proof; label-only harness derivative.
if(WIN32)
    set(_foam_web_stage "${CMAKE_BINARY_DIR}/river-web-proof-stage")
    execute_process(COMMAND "${RENEGADE_RIVER_PYTHON}"
        "${CMAKE_SOURCE_DIR}/Tools/prepare_river_web_graphics_proof.py" "${_foam_web_stage}"
        RESULT_VARIABLE _foam_web_result)
    if(NOT _foam_web_result EQUAL 0)
        message(FATAL_ERROR "Connected foam web proof staging failed")
    endif()
    add_executable(RenegadeRiverFoamWebProof "${_foam_web_stage}/RiverFoamWebProof.cpp")
    target_include_directories(RenegadeRiverFoamWebProof PRIVATE "${CMAKE_SOURCE_DIR}/Tests" "${_junction_foam_stage}")
    target_link_libraries(RenegadeRiverFoamWebProof PRIVATE Renegade::EngineBridge)
endif()

# G8G optional original foam-artwork proof; no Unreal assets or production adoption.
if(WIN32)
    set(_foam_artwork_stage "${CMAKE_BINARY_DIR}/river-artwork-proof-stage")
    execute_process(COMMAND "${RENEGADE_RIVER_PYTHON}"
        "${CMAKE_SOURCE_DIR}/Tools/prepare_river_artwork_graphics_proof.py" "${_foam_artwork_stage}"
        RESULT_VARIABLE _foam_artwork_result)
    if(NOT _foam_artwork_result EQUAL 0)
        message(FATAL_ERROR "Original foam artwork proof staging failed")
    endif()
    add_executable(RenegadeRiverFoamArtworkProof "${_foam_artwork_stage}/RiverFoamArtworkProof.cpp")
    target_include_directories(RenegadeRiverFoamArtworkProof PRIVATE "${CMAKE_SOURCE_DIR}/Tests" "${_junction_foam_stage}")
    target_link_libraries(RenegadeRiverFoamArtworkProof PRIVATE Renegade::EngineBridge)
endif()

# G8H optional foam appearance refinement; all previous targets remain intact.
if(WIN32)
    set(_refined_foam_stage "${CMAKE_BINARY_DIR}/river-refined-foam-proof-stage")
    execute_process(COMMAND "${RENEGADE_RIVER_PYTHON}"
        "${CMAKE_SOURCE_DIR}/Tools/prepare_river_refined_foam_graphics_proof.py" "${_refined_foam_stage}"
        RESULT_VARIABLE _refined_foam_result)
    if(NOT _refined_foam_result EQUAL 0)
        message(FATAL_ERROR "Refined foam proof staging failed")
    endif()
    add_executable(RenegadeRiverRefinedFoamProof "${_refined_foam_stage}/RiverRefinedFoamProof.cpp")
    target_include_directories(RenegadeRiverRefinedFoamProof PRIVATE "${CMAKE_SOURCE_DIR}/Tests" "${_junction_foam_stage}")
    target_link_libraries(RenegadeRiverRefinedFoamProof PRIVATE Renegade::EngineBridge)
endif()

# G10A bounded graded-river native-geometry proof; no production adoption.
add_executable(RenegadeRiverGradedTests RiverGradedTests.cpp)
target_link_libraries(RenegadeRiverGradedTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeRiverGradedTests COMMAND RenegadeRiverGradedTests)

if(WIN32)
    set(_graded_stage "${CMAKE_BINARY_DIR}/river-graded-proof-stage")
    execute_process(COMMAND "${RENEGADE_RIVER_PYTHON}"
        "${CMAKE_SOURCE_DIR}/Tools/prepare_river_graded_graphics_proof.py" "${_graded_stage}"
        RESULT_VARIABLE _graded_result)
    if(NOT _graded_result EQUAL 0)
        message(FATAL_ERROR "Graded river proof staging failed")
    endif()
    add_executable(RenegadeRiverGradedDropProof "${_graded_stage}/RiverGradedDropProof.cpp")
    target_include_directories(RenegadeRiverGradedDropProof PRIVATE "${CMAKE_SOURCE_DIR}/Tests" "${_junction_foam_stage}")
    target_link_libraries(RenegadeRiverGradedDropProof PRIVATE Renegade::EngineBridge)
endif()

# G10B isolated mini-cascade; accepted graded and production paths untouched.
add_executable(RenegadeRiverCascadeTests RiverCascadeTests.cpp)
target_link_libraries(RenegadeRiverCascadeTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeRiverCascadeTests COMMAND RenegadeRiverCascadeTests)
if(WIN32)
    set(_cascade_stage "${CMAKE_BINARY_DIR}/river-cascade-proof-stage")
    execute_process(COMMAND "${RENEGADE_RIVER_PYTHON}"
        "${CMAKE_SOURCE_DIR}/Tools/prepare_river_cascade_graphics_proof.py" "${_cascade_stage}"
        RESULT_VARIABLE _cascade_result)
    if(NOT _cascade_result EQUAL 0)
        message(FATAL_ERROR "Cascade proof staging failed")
    endif()
    add_executable(RenegadeRiverCascadeProof "${_cascade_stage}/RiverCascadeProof.cpp")
    target_include_directories(RenegadeRiverCascadeProof PRIVATE "${CMAKE_SOURCE_DIR}/Tests" "${_junction_foam_stage}")
    target_link_libraries(RenegadeRiverCascadeProof PRIVATE Renegade::EngineBridge)
endif()

# G10C automatic smooth connected drop; previous candidates untouched.
add_executable(RenegadeRiverSmoothDropTests RiverSmoothDropTests.cpp)
target_link_libraries(RenegadeRiverSmoothDropTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeRiverSmoothDropTests COMMAND RenegadeRiverSmoothDropTests)
if(WIN32)
    set(_smooth_stage "${CMAKE_BINARY_DIR}/river-smooth-proof-stage")
    execute_process(COMMAND "${RENEGADE_RIVER_PYTHON}"
        "${CMAKE_SOURCE_DIR}/Tools/prepare_river_smooth_cascade_graphics_proof.py" "${_smooth_stage}"
        RESULT_VARIABLE _smooth_result)
    if(NOT _smooth_result EQUAL 0)
        message(FATAL_ERROR "Smooth cascade staging failed")
    endif()
    add_executable(RenegadeRiverSmoothCascadeProof "${_smooth_stage}/RiverSmoothCascadeProof.cpp")
    target_include_directories(RenegadeRiverSmoothCascadeProof PRIVATE "${CMAKE_SOURCE_DIR}/Tests" "${_junction_foam_stage}")
    target_link_libraries(RenegadeRiverSmoothCascadeProof PRIVATE Renegade::EngineBridge)
endif()

# G10D appearance-only downstream recovery; G10C remains immutable.
if(WIN32)
    set(_opening_stage "${CMAKE_BINARY_DIR}/river-opening-foam-proof-stage")
    execute_process(COMMAND "${RENEGADE_RIVER_PYTHON}"
        "${CMAKE_SOURCE_DIR}/Tools/prepare_river_opening_foam_graphics_proof.py" "${_opening_stage}"
        RESULT_VARIABLE _opening_result)
    if(NOT _opening_result EQUAL 0)
        message(FATAL_ERROR "Opening foam proof staging failed")
    endif()
    add_executable(RenegadeRiverOpeningFoamProof "${_opening_stage}/RiverOpeningFoamProof.cpp")
    target_include_directories(RenegadeRiverOpeningFoamProof PRIVATE "${CMAKE_SOURCE_DIR}/Tests" "${_junction_foam_stage}")
    target_link_libraries(RenegadeRiverOpeningFoamProof PRIVATE Renegade::EngineBridge)
endif()

# G11 creator-node driven course geometry and indexed native height queries.
add_executable(RenegadeRiverCourseTests RiverCourseTests.cpp)
target_link_libraries(RenegadeRiverCourseTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeRiverCourseTests COMMAND RenegadeRiverCourseTests)

if(WIN32)
    add_executable(RenegadeRiverCourseNativeProof RiverCourseNativeProof.cpp)
    target_link_libraries(RenegadeRiverCourseNativeProof PRIVATE Renegade::EngineBridge)
endif()

if(WIN32)
    add_executable(RenegadeRiverCourseIntegrationProof RiverCourseIntegrationProof.cpp)
    target_link_libraries(RenegadeRiverCourseIntegrationProof PRIVATE Renegade::EngineBridge)
endif()

if(WIN32)
    add_executable(RenegadeRiverCourseReopenProof RiverCourseReopenProof.cpp)
    target_link_libraries(RenegadeRiverCourseReopenProof PRIVATE Renegade::EngineBridge)
endif()

if(WIN32)
    add_executable(RenegadeRiverCourseLifecycleProof RiverCourseLifecycleProof.cpp "${_g6_package_stage}/TestLevelRuntimeProcess.cpp")
    target_include_directories(RenegadeRiverCourseLifecycleProof PRIVATE "${CMAKE_SOURCE_DIR}/Studio/src")
    target_link_libraries(RenegadeRiverCourseLifecycleProof PRIVATE Renegade::EngineBridge)
endif()

if(WIN32)
    add_executable(RenegadeRiverCourseProjectFixtureProof RiverCourseProjectFixtureProof.cpp)
    target_link_libraries(RenegadeRiverCourseProjectFixtureProof PRIVATE Renegade::EngineBridge)
endif()

if(WIN32)
    add_executable(RenegadeRiverCourseBanksProof RiverCourseBanksProof.cpp)
    target_link_libraries(RenegadeRiverCourseBanksProof PRIVATE Renegade::EngineBridge)
endif()

if(WIN32)
    add_executable(RenegadeRiverCourseBankProjectProof RiverCourseBankProjectProof.cpp)
    target_link_libraries(RenegadeRiverCourseBankProjectProof PRIVATE Renegade::EngineBridge)
endif()

if(WIN32)
 add_executable(RenegadeRiverCourseRockProof RiverCourseRockProof.cpp)
 target_link_libraries(RenegadeRiverCourseRockProof PRIVATE Renegade::EngineBridge)
endif()

# G13D: authored scene-bound waterfall definition, persistent native metadata,
# bounded stable identity, and exact Undo/Redo. Rendering remains G13C owner-approved.
add_executable(RenegadePlaceableWaterfallAuthoringTests PlaceableWaterfallAuthoringTests.cpp)
target_link_libraries(RenegadePlaceableWaterfallAuthoringTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadePlaceableWaterfallAuthoringTests COMMAND RenegadePlaceableWaterfallAuthoringTests)
if(WIN32)
 set_target_properties(RenegadePlaceableWaterfallAuthoringTests PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/BUILD/waterfall-placeable")
endif()

# G13: independent placeable waterfall entity/effect. Never touches river transport.
add_executable(RenegadePlaceableWaterfallTests PlaceableWaterfallTests.cpp)
target_link_libraries(RenegadePlaceableWaterfallTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadePlaceableWaterfallTests COMMAND RenegadePlaceableWaterfallTests)
if(WIN32)
 add_executable(RenegadePlaceableWaterfallProof PlaceableWaterfallProof.cpp)
 target_link_libraries(RenegadePlaceableWaterfallProof PRIVATE Renegade::EngineBridge)
 set_target_properties(RenegadePlaceableWaterfallTests RenegadePlaceableWaterfallProof
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/BUILD/waterfall-placeable")
endif()

# G12A isolated native waterfall presentation proof; accepted renderer unchanged.
add_executable(RenegadeRiverWaterfallFlightTests RiverWaterfallFlightTests.cpp)
target_link_libraries(RenegadeRiverWaterfallFlightTests PRIVATE Renegade::EngineBridge)
add_test(NAME RenegadeRiverWaterfallFlightTests COMMAND RenegadeRiverWaterfallFlightTests)
if(WIN32)
 add_executable(RenegadeRiverWaterfallProof RiverWaterfallProof.cpp)
 target_link_libraries(RenegadeRiverWaterfallProof PRIVATE Renegade::EngineBridge)
 set_target_properties(RenegadeRiverWaterfallFlightTests RenegadeRiverWaterfallProof PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/BUILD/waterfall-g12a")
endif()

# Explicit native DX12 lifecycle proof, invoked with a disposable owner-style project.
add_executable(RenegadeWaterfallRuntimeLifecycleProof WaterfallRuntimeLifecycleProof.cpp "${CMAKE_SOURCE_DIR}/Studio/src/WindowsGameBuildController.cpp")
target_link_libraries(RenegadeWaterfallRuntimeLifecycleProof PRIVATE Renegade::EngineBridge)
target_compile_options(RenegadeWaterfallRuntimeLifecycleProof PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
# The tests subdirectory does not inherit Studio's scoped Git revision
# variables. Resolve exact pinned IDs independently, never pass blank revisions.
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE G13E3_SOURCE_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE G13E3_SOURCE_RESULT)
execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/WickedEngine"
  OUTPUT_VARIABLE G13E3_WICKED_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE G13E3_WICKED_RESULT)
if(NOT G13E3_SOURCE_RESULT EQUAL 0 OR NOT G13E3_WICKED_RESULT EQUAL 0
   OR NOT G13E3_SOURCE_REVISION MATCHES "^[0-9a-f]+$"
   OR NOT G13E3_WICKED_REVISION MATCHES "^[0-9a-f]+$")
  message(FATAL_ERROR "G13E3 lifecycle proof requires pinned Git revisions")
endif()
target_compile_definitions(RenegadeWaterfallRuntimeLifecycleProof PRIVATE
  RENEGADE_SOURCE_REVISION="${G13E3_SOURCE_REVISION}"
  RENEGADE_WICKED_REVISION="${G13E3_WICKED_REVISION}")

# G13E4 independent native DX12 multi-instance and lifetime verification.
add_executable(RenegadeWaterfallMultiSceneProof WaterfallMultiSceneProof.cpp)
target_link_libraries(RenegadeWaterfallMultiSceneProof PRIVATE Renegade::EngineBridge)
target_compile_options(RenegadeWaterfallMultiSceneProof PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
