add_library(RenegadeEngineBridge STATIC
    include/renegade/bridge/AssetBrowserService.h
    include/renegade/bridge/AssetCatalogueService.h
    include/renegade/bridge/AssetRegistryService.h
    include/renegade/bridge/BuildIdentityService.h
    include/renegade/bridge/BuildPromotionService.h
    include/renegade/bridge/BuildService.h
    include/renegade/bridge/BuildStageService.h
    include/renegade/bridge/BuildVerificationService.h
    include/renegade/bridge/WindowsGameBuildProjectService.h
    include/renegade/bridge/WindowsGameBuildWorkflow.h
    include/renegade/bridge/PackageIntegrityService.h
    include/renegade/bridge/CameraService.h
    include/renegade/bridge/AudioService.h
    include/renegade/bridge/DecalProbeService.h
    include/renegade/bridge/CommandService.h
    include/renegade/bridge/CreatorAssetActionPolicy.h
    include/renegade/bridge/CreatorAssetWorkflowService.h
    include/renegade/bridge/CreatorModelImportRecipe.h
    include/renegade/bridge/CreatorModelMaterialPreparationService.h
    include/renegade/bridge/CreatorSurfaceBuilderService.h
    include/renegade/bridge/CreatorTextureWorkflowService.h
    include/renegade/bridge/DependencyService.h
    include/renegade/bridge/FlowService.h
    include/renegade/bridge/StoryFlowAuthoringModel.h
    include/renegade/bridge/StoryFlowAuthoringSession.h
    include/renegade/bridge/StoryFlowInteractionPolicy.h
    include/renegade/bridge/StoryFlowJourneyModel.h
    include/renegade/bridge/StoryFlowLevelLifecycleService.h
    include/renegade/bridge/StoryFlowLevelReferenceService.h
    include/renegade/bridge/StoryFlowScreenLifecycleService.h
    include/renegade/bridge/StoryFlowScreenReferenceService.h
    include/renegade/bridge/StoryFlowLayoutService.h
    include/renegade/bridge/ScreenService.h
    include/renegade/bridge/ScreenAuthoringSession.h
    include/renegade/bridge/IdentityService.h
    include/renegade/bridge/LightService.h
    include/renegade/bridge/ImportService.h
    include/renegade/bridge/ResourceImportService.h
    include/renegade/bridge/ResourceAssetService.h
    include/renegade/bridge/ResourceAssetCacheIdentityService.h
    include/renegade/bridge/ResourceAssetDependencyService.h
    include/renegade/bridge/ResourceAssetRuntimeService.h
    include/renegade/bridge/ModelImporterFailureAdapter.h
    include/renegade/bridge/ReusableAssetService.h
    include/renegade/bridge/ReusableAssetInstanceService.h
    include/renegade/bridge/ReusableAssetDependencyService.h
    include/renegade/bridge/ReusableAssetRuntimeService.h
    include/renegade/bridge/MaterialService.h
    include/renegade/bridge/MaterialTextureAssetService.h
    include/renegade/bridge/CollisionService.h
    include/renegade/bridge/ConstraintService.h
    include/renegade/bridge/CharacterPhysicsService.h
    include/renegade/bridge/VehiclePhysicsService.h
    include/renegade/bridge/RagdollPhysicsService.h
    include/renegade/bridge/SoftBodyPhysicsService.h
    include/renegade/bridge/PhysicsService.h
    include/renegade/bridge/PlayerService.h
    include/renegade/bridge/GameplayInputService.h
    include/renegade/bridge/OceanService.h
    include/renegade/bridge/PrecipitationService.h
    include/renegade/bridge/SunService.h
    include/renegade/bridge/TerrainService.h
    include/renegade/bridge/VegetationService.h
    include/renegade/bridge/ProjectService.h
    include/renegade/bridge/ProjectDocumentTransaction.h
    include/renegade/bridge/SceneService.h
    include/renegade/bridge/SceneComponentService.h
    include/renegade/bridge/SceneDocumentService.h
    include/renegade/bridge/SelectionService.h
    include/renegade/bridge/StudioSession.h
    include/renegade/bridge/TestLevelSnapshotService.h
    src/AssetBrowserService.cpp
    src/AssetCatalogueService.cpp
    src/AssetRegistryService.cpp
    src/BuildIdentityService.cpp
    src/BuildService.cpp
    src/BuildStageService.cpp
    src/CameraService.cpp
    src/AudioService.cpp
    src/DecalProbeService.cpp
    src/CommandService.cpp
    src/CreatorAssetWorkflowService.cpp
    src/CreatorModelImportRecipe.cpp
    src/CreatorModelMaterialDetectionService.cpp
    src/CreatorModelMaterialPreparationService.cpp
    src/CreatorSurfaceBuilderService.cpp
    src/CreatorTextureWorkflowService.cpp
    src/DependencyService.cpp
    src/FlowService.cpp
    src/StoryFlowAuthoringModel.cpp
    src/StoryFlowAuthoringSession.cpp
    src/StoryFlowInteractionPolicy.cpp
    src/StoryFlowJourneyModel.cpp
    src/StoryFlowLevelLifecycleService.cpp
    src/StoryFlowLevelReferenceService.cpp
    src/StoryFlowScreenLifecycleService.cpp
    src/StoryFlowScreenReferenceService.cpp
    src/StoryFlowLayoutService.cpp
    src/ScreenService.cpp
    src/ScreenAuthoringSession.cpp
    src/IdentityService.cpp
    src/LightService.cpp
    src/ImportService.cpp
    src/ImportServiceModel.cpp
    src/ResourceImportService.cpp
    src/ResourceAssetService.cpp
    src/ResourceAssetReimportService.cpp
    src/ResourceAssetCacheIdentityService.cpp
    src/ResourceAssetDependencyService.cpp
    src/ResourceAssetRuntimeService.cpp
    src/ModelImporterFailureAdapter.cpp
    src/ReusableAssetService.cpp
    src/ReusableAssetReimportService.cpp
    src/ReusableAssetPlacementService.cpp
    src/ReusableAssetInstanceService.cpp
    src/ReusableAssetDependencyService.cpp
    src/ReusableAssetRuntimeService.cpp
    src/MaterialService.cpp
    src/MaterialTextureAssetService.cpp
    src/MaterialTextureClearCommand.cpp
    src/MaterialTextureReimportRefreshService.cpp
    src/CollisionService.cpp
    src/PhysicsService.cpp
    src/PlayerService.cpp
    src/GameplayInputService.cpp
    src/OceanService.cpp
    src/PrecipitationService.cpp
    src/SunService.cpp
    src/TerrainService.cpp
    src/VegetationService.cpp
    src/ProjectService.cpp
    src/ProjectDocumentTransaction.cpp
    src/SceneService.cpp
    src/SceneComponentService.cpp
    src/SceneDocumentService.cpp
    src/SelectionService.cpp
    src/TestLevelSnapshotService.cpp
    src/WindowsGameBuildProjectService.cpp
)

# Wicked's format converters are self-contained even though upstream keeps
# them in the reference Editor directory. Compile only the converter units
# Renegade owns a bridge for; this does not enable or link the stock Wicked
# Editor application or UI.
target_sources(RenegadeEngineBridge PRIVATE
    ${PROJECT_SOURCE_DIR}/WickedEngine/Editor/ModelImporter_GLTF.cpp
    ${PROJECT_SOURCE_DIR}/WickedEngine/Editor/ModelImporter_FBX.cpp
)

# Gate 1 explicitly required malformed importer failures to become non-modal
# before Gate 3's permanent creator-facing transaction. Redirect only the
# legacy messageBox token inside the two converter translation units Renegade
# compiles. Wicked's actual helper and every other Wicked translation unit stay
# untouched. On parse/read failure the converters call the Renegade adapter,
# return with an empty scene, and ImportService produces its structured failure.
set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/WickedEngine/Editor/ModelImporter_GLTF.cpp
    ${PROJECT_SOURCE_DIR}/WickedEngine/Editor/ModelImporter_FBX.cpp
    PROPERTIES
        COMPILE_DEFINITIONS "messageBox=RenegadeImporterMessageBox"
)

target_include_directories(RenegadeEngineBridge PRIVATE
    ${PROJECT_SOURCE_DIR}/WickedEngine/Editor
)

add_library(Renegade::EngineBridge ALIAS RenegadeEngineBridge)

target_include_directories(RenegadeEngineBridge
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(RenegadeEngineBridge
    PUBLIC
        WickedEngine
)

if(WIN32)
    # LP06 packaging is Windows-first. Keep its Win32/BCrypt implementation out
    # of non-Windows EngineBridge configuration while retaining shared public
    # contracts for the Windows Runtime and build pipeline.
    target_sources(RenegadeEngineBridge PRIVATE
        src/BuildPromotionService.cpp
        src/BuildPromotionServiceInternal.h
        src/BuildVerificationService.cpp
        src/PackageIntegrityService.cpp
        src/WindowsGameBuildWorkflow.cpp
    )
    target_link_libraries(RenegadeEngineBridge PRIVATE bcrypt)
endif()

set_target_properties(RenegadeEngineBridge PROPERTIES
    FOLDER "Renegade"
)
