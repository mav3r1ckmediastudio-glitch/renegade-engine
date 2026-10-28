add_library(RenegadeEngineBridge STATIC
    src/PlaceableWaterfallRenderer.cpp
    src/RiverCourseRenderer.cpp
    src/RiverCourseSurfaceBinding.cpp
    src/RiverCourseService.cpp
    include/renegade/bridge/RiverCourseService.h
    src/RiverFlowSpanService.cpp
    include/renegade/bridge/RiverFlowSpanService.h
    src/RiverWaterwayRenderer.cpp
    src/RiverWaterwayRenderer.h
    src/RiverWaterwaySurfaceBinding.cpp
    src/RiverWaterwaySurfaceBinding.h
    src/RiverWaterwayTransport.h
    src/RiverWaterwayEdit.cpp
    src/RiverWaterwayService.cpp
    include/renegade/bridge/RiverWaterwayService.h
    src/RiverJunctionSurfaceService.cpp
    include/renegade/bridge/RiverJunctionSurfaceService.h
    src/RiverFlowNetworkService.cpp
    include/renegade/bridge/RiverFlowNetworkService.h
    src/RiverFlowProfileService.cpp
    src/RiverFlowRenderer.cpp
    include/renegade/bridge/RiverFlowRenderer.h
    src/RiverFlowFieldService.cpp
    include/renegade/bridge/RiverFlowFieldService.h
    src/RiverFoamService.cpp
    include/renegade/bridge/RiverFoamService.h
    src/PlayerSwimmingService.cpp
    include/renegade/bridge/PlayerSwimmingService.h
    src/RiverUnderwaterService.cpp
    include/renegade/bridge/RiverUnderwaterService.h
    src/ScreenPortableLibrary.cpp
    src/ProjectUiScaffoldService.cpp
    src/ScreenPerformanceBudget.cpp
    src/GameplayAttributeDocumentService.cpp
    include/renegade/bridge/GameplayAttributeDocumentService.h
    src/ImpactDefaultsService.cpp
    include/renegade/bridge/ImpactDefaultsService.h
    src/ImpactAudioService.cpp
    include/renegade/bridge/ImpactAudioService.h
    src/LaunchSocketService.cpp
    include/renegade/bridge/LaunchSocketService.h
    src/ProjectileAssetService.cpp
    include/renegade/bridge/ProjectileAssetService.h
    include/renegade/bridge/FirstPersonAssemblyService.h
    src/FirstPersonAssemblyService.cpp
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
    include/renegade/bridge/HumanoidRetargetService.h
    include/renegade/bridge/DiagnosticService.h
    include/renegade/bridge/CreatorAssetActionPolicy.h
    include/renegade/bridge/CreatorAssetWorkflowService.h
    include/renegade/bridge/CreatorModelImportRecipe.h
    include/renegade/bridge/CreatorSurfaceBuilderService.h
    include/renegade/bridge/CreatorTextureWorkflowService.h
    include/renegade/bridge/CreatorVideoWorkflowService.h
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
    include/renegade/bridge/RiverSplineService.h
    include/renegade/bridge/ImportService.h
    include/renegade/bridge/MatchingRigAnimationService.h
    src/MatchingRigAnimationService.cpp
    include/renegade/bridge/ModelImportCandidateService.h
    include/renegade/bridge/ModelImportCommitService.h
    include/renegade/bridge/ResourceImportService.h
    include/renegade/bridge/ResourceAssetService.h
    include/renegade/bridge/ResourceAssetCacheIdentityService.h
    include/renegade/bridge/ResourceAssetDependencyService.h
    include/renegade/bridge/ResourceAssetRuntimeService.h
    include/renegade/bridge/VideoAssetService.h
    include/renegade/bridge/ModelImporterFailureAdapter.h
    include/renegade/bridge/ReusableAssetService.h
    include/renegade/bridge/ReusableAssetInstanceService.h
    include/renegade/bridge/ReusableAssetDependencyService.h
    include/renegade/bridge/ReusableAssetRuntimeService.h
    include/renegade/bridge/CharacterPrefabService.h
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
    include/renegade/bridge/NavigationService.h
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
    src/CharacterDuplicateCommand.cpp
    src/HumanoidRetargetService.cpp
    src/DiagnosticService.cpp
    src/DiagnosticEndpoint.cpp
    src/CreatorAssetWorkflowService.cpp
    src/CreatorModelImportRecipe.cpp
    src/CreatorSurfaceBuilderService.cpp
    src/CreatorTextureWorkflowService.cpp
    src/CreatorVideoWorkflowService.cpp
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
    src/RiverSplineService.cpp
    src/RiverTerrainLayerService.cpp
    include/renegade/bridge/RiverTerrainLayerService.h
    src/RiverbedMaterialService.cpp
    include/renegade/bridge/RiverbedMaterialService.h
    src/ImportService.cpp
    src/ImportServiceModel.cpp
    src/ModelImportCandidateService.cpp
    src/ModelImportCommitService.cpp
    src/ResourceImportService.cpp
    src/ResourceAssetService.cpp
    src/ResourceAssetReimportService.cpp
    src/ResourceAssetCacheIdentityService.cpp
    src/ResourceAssetDependencyService.cpp
    src/ResourceAssetRuntimeService.cpp
    src/VideoAssetService.cpp
    src/ModelImporterFailureAdapter.cpp
    src/ReusableAssetService.cpp
    src/ReusableAssetPlacementService.cpp
    src/CharacterPrefabPlacementRouter.cpp
    src/ReusableAssetInstanceService.cpp
    src/ReusableAssetDependencyService.cpp
    src/ReusableAssetRuntimeService.cpp
    src/CharacterPrefabService.cpp
    src/CharacterPrefabPlacementTemplate.cpp
    src/MaterialService.cpp
    src/MaterialTextureAssetService.cpp
    src/MaterialTextureClearCommand.cpp
    src/MaterialTextureReimportRefreshService.cpp
    src/CollisionService.cpp
    src/PhysicsService.cpp
    src/PlayerService.cpp
    src/EquipmentAssetService.cpp
    include/renegade/bridge/EquipmentAssetService.h
    src/PlayerPrefabService.cpp
    include/renegade/bridge/PlayerPrefabService.h
    src/PlayerViewGripService.cpp
    src/NavigationService.cpp
    src/GameplayInputService.cpp
    include/renegade/bridge/GameplayEventService.h
    src/GameplayEventService.cpp
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

# CW-05 replaces only DuplicateEntityCommand with a Character-aware definition
# in CharacterDuplicateCommand.cpp. Keep all other established command
# implementations in CommandService.cpp by renaming that translation unit's
# legacy duplicate class token at compile time.
set_source_files_properties(
    src/CommandService.cpp
    PROPERTIES
        COMPILE_DEFINITIONS "DuplicateEntityCommand=LegacyDuplicateEntityCommand"
)

# Likewise the existing LP07/CW-04 reusable placement backend is retained
# byte-for-byte as PrepareModelAssetPlacementLegacy(). The CW-05 router owns the
# public PrepareModelAssetPlacement() symbol and diverts only registered
# Character Prefabs to their stable base Character asset. Hide the public router
# declaration while compiling the legacy translation unit, otherwise the token
# rename turns both declarations into PrepareModelAssetPlacementLegacy().
set_source_files_properties(
    src/ReusableAssetPlacementService.cpp
    PROPERTIES
        COMPILE_DEFINITIONS "PrepareModelAssetPlacement=PrepareModelAssetPlacementLegacy;RENEGADE_LEGACY_PLACEMENT_IMPLEMENTATION=1"
)

# Wicked's format converters are self-contained even though upstream keeps
# them in the reference Editor directory. Compile only the converter units
# Renegade owns a bridge for; this does not enable or link the stock Wicked
# Editor application or UI.
target_sources(RenegadeEngineBridge PRIVATE
    ${PROJECT_SOURCE_DIR}/WickedEngine/Editor/ModelImporter_GLTF.cpp
    ${PROJECT_SOURCE_DIR}/WickedEngine/Editor/ModelImporter_FBX.cpp
    ${PROJECT_SOURCE_DIR}/WickedEngine/Editor/ModelImporter_PLY.cpp
    ${PROJECT_SOURCE_DIR}/WickedEngine/Editor/miniply.cpp
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

# Native scene/combat headers can exceed MSVC's Debug COFF section limit.
# Consumers (including gameplay tests) need the same policy as the bridge.
target_compile_options(RenegadeEngineBridge
    PUBLIC
        "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/bigobj>"
)

if(WIN32)
    target_link_libraries(RenegadeEngineBridge PRIVATE ws2_32)
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

# One embedded source identity shared by Studio and Runtime diagnostics.
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE RENEGADE_DIAGNOSTIC_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
target_compile_definitions(RenegadeEngineBridge PRIVATE
    RENEGADE_DIAGNOSTIC_REVISION="${RENEGADE_DIAGNOSTIC_REVISION}"
    RENEGADE_DIAGNOSTIC_CONFIGURATION="$<CONFIG>")

target_sources(RenegadeEngineBridge PRIVATE
    src/ScreenLibraryService.cpp
    include/renegade/bridge/ScreenLibraryService.h)

if(WIN32)
    add_executable(RenegadeRiverShaderCompiler "${CMAKE_SOURCE_DIR}/Tools/RiverShaderCompiler.cpp")
    target_link_libraries(RenegadeRiverShaderCompiler PRIVATE WickedEngine)
    set_target_properties(RenegadeRiverShaderCompiler PROPERTIES FOLDER "Renegade/Tools")
    add_custom_command(TARGET RenegadeRiverShaderCompiler POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/dxcompiler.dll"
            "$<TARGET_FILE_DIR:RenegadeRiverShaderCompiler>" VERBATIM)
    file(GLOB_RECURSE RENEGADE_RIVER_SHADER_INCLUDES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders/*.h"
        "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders/*.hlsli")
    set(RENEGADE_RIVER_UNDERWATER_CSO "${CMAKE_BINARY_DIR}/river-shaders/RenegadeRiverUnderwaterCS.cso" CACHE INTERNAL "Owned river shader")
    add_custom_command(OUTPUT "${RENEGADE_RIVER_UNDERWATER_CSO}"
        COMMAND $<TARGET_FILE:RenegadeRiverShaderCompiler>
            "${CMAKE_SOURCE_DIR}/EngineBridge/shaders/RenegadeRiverUnderwaterCS.hlsl"
            "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders"
            "${RENEGADE_RIVER_UNDERWATER_CSO}"
        DEPENDS RenegadeRiverShaderCompiler
            "${CMAKE_SOURCE_DIR}/EngineBridge/shaders/RenegadeRiverUnderwaterCS.hlsl"
            ${RENEGADE_RIVER_SHADER_INCLUDES}
        VERBATIM)
    add_custom_target(RenegadeRiverUnderwaterShader DEPENDS "${RENEGADE_RIVER_UNDERWATER_CSO}")
endif()

# Shader edits must deploy even when no C++ object needs relinking.
function(renegade_deploy_river_underwater_shader consumer)
    add_custom_target(${consumer}RiverShaderContent
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${consumer}>/Content/river"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RENEGADE_RIVER_UNDERWATER_CSO}"
            "$<TARGET_FILE_DIR:${consumer}>/Content/river/RenegadeRiverUnderwaterCS.cso"
        DEPENDS RenegadeRiverUnderwaterShader VERBATIM)
    add_dependencies(${consumer} ${consumer}RiverShaderContent)
    set_target_properties(${consumer}RiverShaderContent PROPERTIES FOLDER "Renegade/Tools")
endfunction()

# Opt-in River V2 shaders remain in owned/staged files; native objectHF is hashed.
if(WIN32)
    find_program(RENEGADE_RIVER_PYTHON NAMES python REQUIRED)
    set(_flow_stage "${CMAKE_BINARY_DIR}/river-flow-stage")
    set(_flow_ps "${_flow_stage}/RiverContactFoamPS.cso")
    set(_flow_cs "${_flow_stage}/RiverFoamTransportCS.cso")
    add_custom_command(OUTPUT "${_flow_ps}" "${_flow_cs}"
        COMMAND "${RENEGADE_RIVER_PYTHON}"
            "${CMAKE_SOURCE_DIR}/Tools/prepare_river_contact_foam_proof.py" "${_flow_stage}"
        COMMAND $<TARGET_FILE:RenegadeRiverShaderCompiler>
            "${_flow_stage}/RiverContactFoamPS.hlsl"
            "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders" "${_flow_ps}" ps
        COMMAND $<TARGET_FILE:RenegadeRiverShaderCompiler>
            "${CMAKE_SOURCE_DIR}/EngineBridge/shaders/RiverFoamTransportCS.hlsl"
            "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders" "${_flow_cs}"
        DEPENDS RenegadeRiverShaderCompiler
            "${CMAKE_SOURCE_DIR}/Tools/prepare_river_contact_foam_proof.py"
            "${CMAKE_SOURCE_DIR}/Tools/prepare_river_flow_normal_proof.py"
            "${CMAKE_SOURCE_DIR}/EngineBridge/shaders/RiverFlowNormalBlock.hlsli"
            "${CMAKE_SOURCE_DIR}/EngineBridge/shaders/RiverContactFoamBlock.hlsli"
            "${CMAKE_SOURCE_DIR}/EngineBridge/shaders/RiverFoamTransportCS.hlsl"
            ${RENEGADE_RIVER_SHADER_INCLUDES}
        VERBATIM)
    add_custom_target(RenegadeRiverFlowShaders DEPENDS "${_flow_ps}" "${_flow_cs}")
endif()
if(WIN32)
    set(_waterway_stage "${CMAKE_BINARY_DIR}/river-waterway-stage")
    set(_waterway_ps "${_waterway_stage}/RiverWaterwayFoamPS.cso")
    set(_waterway_cs "${_waterway_stage}/RiverWaterwayFoamCS.cso")
    add_custom_command(OUTPUT "${_waterway_ps}" "${_waterway_cs}"
        COMMAND "${RENEGADE_RIVER_PYTHON}" "${CMAKE_SOURCE_DIR}/Tools/prepare_river_junction_foam_shading.py" "${_waterway_stage}"
        COMMAND $<TARGET_FILE:RenegadeRiverShaderCompiler>
            "${_waterway_stage}/RiverContactFoamPS.hlsl" "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders" "${_waterway_ps}" ps
        COMMAND $<TARGET_FILE:RenegadeRiverShaderCompiler>
            "${CMAKE_SOURCE_DIR}/EngineBridge/shaders/RiverJunctionFoamTransportCS.hlsl" "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders" "${_waterway_cs}"
        DEPENDS RenegadeRiverShaderCompiler
            "${CMAKE_SOURCE_DIR}/Tools/prepare_river_junction_foam_shading.py"
            "${CMAKE_SOURCE_DIR}/EngineBridge/shaders/RiverFlowNormalBlock.hlsli"
            "${CMAKE_SOURCE_DIR}/EngineBridge/shaders/RiverJunctionFoamShadingBlock.hlsli"
            "${CMAKE_SOURCE_DIR}/EngineBridge/shaders/RiverJunctionFoamTransportCS.hlsl"
            ${RENEGADE_RIVER_SHADER_INCLUDES}
        VERBATIM)
    add_custom_target(RenegadeRiverWaterwayShaders DEPENDS "${_waterway_ps}" "${_waterway_cs}")
endif()
if(WIN32)
    set(_rapids_stage "${CMAKE_BINARY_DIR}/river-rapids-stage")
    set(_rapids_ps "${_rapids_stage}/RiverRapidsPS.cso")
    set(_rapids_cs "${_rapids_stage}/RiverRapidsCS.cso")
    add_custom_command(OUTPUT "${_rapids_ps}" "${_rapids_cs}"
        COMMAND "${RENEGADE_RIVER_PYTHON}" "${CMAKE_SOURCE_DIR}/Tools/prepare_river_rapids_shading.py" "${_rapids_stage}"
        COMMAND $<TARGET_FILE:RenegadeRiverShaderCompiler>
            "${_rapids_stage}/RiverContactFoamPS.hlsl" "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders" "${_rapids_ps}" ps
        COMMAND $<TARGET_FILE:RenegadeRiverShaderCompiler>
            "${CMAKE_SOURCE_DIR}/EngineBridge/shaders/RiverRapidsTransportCS.hlsl" "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders" "${_rapids_cs}"
        DEPENDS RenegadeRiverShaderCompiler
            "${CMAKE_SOURCE_DIR}/Tools/prepare_river_rapids_shading.py"
            "${CMAKE_SOURCE_DIR}/Tools/prepare_river_junction_foam_shading.py"
            "${CMAKE_SOURCE_DIR}/EngineBridge/shaders/RiverFlowNormalBlock.hlsli"
            "${CMAKE_SOURCE_DIR}/EngineBridge/shaders/RiverJunctionFoamShadingBlock.hlsli"
            "${CMAKE_SOURCE_DIR}/EngineBridge/shaders/RiverRapidsTransportCS.hlsl"
            ${RENEGADE_RIVER_SHADER_INCLUDES}
        VERBATIM)
    add_custom_target(RenegadeRiverRapidsShaders DEPENDS "${_rapids_ps}" "${_rapids_cs}")
endif()
if(WIN32)
    set(_appearance_stage "${CMAKE_BINARY_DIR}/river-appearance-stage")
    file(GLOB _appearance_python CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Tools/prepare_river_*.py")
    file(GLOB _appearance_hlsl CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/EngineBridge/shaders/River*.hlsl*")
    add_custom_command(OUTPUT
        "${_appearance_stage}/RiverRefinedFoamPS.cso"
        "${_appearance_stage}/RiverFoamDetailCS.cso"
        "${_appearance_stage}/RiverSurfaceVS.cso"
        "${_appearance_stage}/RiverSurfaceShadowVS.cso"
        "${_appearance_stage}/OriginalFoamCoverage.png"
        COMMAND "${RENEGADE_RIVER_PYTHON}" "${CMAKE_SOURCE_DIR}/Tools/prepare_river_appearance.py" "${_appearance_stage}"
        COMMAND $<TARGET_FILE:RenegadeRiverShaderCompiler> "${_appearance_stage}/RiverContactFoamPS.hlsl" "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders" "${_appearance_stage}/RiverRefinedFoamPS.cso" ps
        COMMAND $<TARGET_FILE:RenegadeRiverShaderCompiler> "${_appearance_stage}/RiverFoamArtworkCS.hlsl" "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders" "${_appearance_stage}/RiverFoamDetailCS.cso"
        COMMAND $<TARGET_FILE:RenegadeRiverShaderCompiler> "${_appearance_stage}/RiverSurfaceVS.hlsl" "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders" "${_appearance_stage}/RiverSurfaceVS.cso" vs
        COMMAND $<TARGET_FILE:RenegadeRiverShaderCompiler> "${_appearance_stage}/RiverSurfaceShadowVS.hlsl" "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders" "${_appearance_stage}/RiverSurfaceShadowVS.cso" vs
        DEPENDS RenegadeRiverShaderCompiler ${_appearance_python} ${_appearance_hlsl}
            "${CMAKE_SOURCE_DIR}/EngineBridge/assets/river-v2-original-foam/foam_coverage_v1.png"
            ${RENEGADE_RIVER_SHADER_INCLUDES}
        VERBATIM)
    add_custom_target(RenegadeRiverAppearanceShaders DEPENDS
        "${_appearance_stage}/RiverRefinedFoamPS.cso" "${_appearance_stage}/RiverFoamDetailCS.cso"
        "${_appearance_stage}/RiverSurfaceVS.cso" "${_appearance_stage}/RiverSurfaceShadowVS.cso"
        "${_appearance_stage}/OriginalFoamCoverage.png")
endif()
function(renegade_deploy_river_flow_shaders consumer)
    add_custom_target(${consumer}RiverAppearanceContent
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${consumer}>/Content/river"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/river-appearance-stage/RiverRefinedFoamPS.cso"
            "${CMAKE_BINARY_DIR}/river-appearance-stage/RiverFoamDetailCS.cso"
            "${CMAKE_BINARY_DIR}/river-appearance-stage/RiverSurfaceVS.cso"
            "${CMAKE_BINARY_DIR}/river-appearance-stage/RiverSurfaceShadowVS.cso"
            "${CMAKE_BINARY_DIR}/river-appearance-stage/OriginalFoamCoverage.png"
            "$<TARGET_FILE_DIR:${consumer}>/Content/river"
        DEPENDS RenegadeRiverAppearanceShaders VERBATIM)
    add_dependencies(${consumer} ${consumer}RiverAppearanceContent)

    add_custom_target(${consumer}RiverFlowContent
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${consumer}>/Content/river"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/river-flow-stage/RiverContactFoamPS.cso"
            "${CMAKE_BINARY_DIR}/river-flow-stage/RiverFoamTransportCS.cso"
            "$<TARGET_FILE_DIR:${consumer}>/Content/river"
        DEPENDS RenegadeRiverFlowShaders VERBATIM)
    add_custom_target(${consumer}RiverWaterwayContent
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${consumer}>/Content/river"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/river-waterway-stage/RiverWaterwayFoamPS.cso"
            "${CMAKE_BINARY_DIR}/river-waterway-stage/RiverWaterwayFoamCS.cso"
            "${CMAKE_BINARY_DIR}/river-rapids-stage/RiverRapidsPS.cso"
            "${CMAKE_BINARY_DIR}/river-rapids-stage/RiverRapidsCS.cso"
            "$<TARGET_FILE_DIR:${consumer}>/Content/river"
        DEPENDS RenegadeRiverWaterwayShaders RenegadeRiverRapidsShaders VERBATIM)
    add_dependencies(${consumer} ${consumer}RiverWaterwayContent)
    add_dependencies(${consumer} ${consumer}RiverFlowContent)
endfunction()

if(WIN32)
    set(_course_stage "${CMAKE_BINARY_DIR}/river-course-stage")
    add_custom_command(OUTPUT
        "${_course_stage}/RiverCoursePS.cso"
        "${_course_stage}/RiverCourseVS.cso"
        "${_course_stage}/RiverCourseShadowVS.cso"
        COMMAND "${RENEGADE_RIVER_PYTHON}" "${CMAKE_SOURCE_DIR}/Tools/prepare_river_course_shading.py" "${_course_stage}"
        COMMAND $<TARGET_FILE:RenegadeRiverShaderCompiler> "${_course_stage}/RiverContactFoamPS.hlsl" "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders" "${_course_stage}/RiverCoursePS.cso" ps
        COMMAND $<TARGET_FILE:RenegadeRiverShaderCompiler> "${_course_stage}/RiverSurfaceVS.hlsl" "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders" "${_course_stage}/RiverCourseVS.cso" vs
        COMMAND $<TARGET_FILE:RenegadeRiverShaderCompiler> "${_course_stage}/RiverSurfaceShadowVS.hlsl" "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders" "${_course_stage}/RiverCourseShadowVS.cso" vs
        DEPENDS RenegadeRiverShaderCompiler ${_appearance_python} ${_appearance_hlsl}
            "${CMAKE_SOURCE_DIR}/EngineBridge/assets/river-v2-original-foam/foam_coverage_v1.png"
            ${RENEGADE_RIVER_SHADER_INCLUDES}
        VERBATIM)
    add_custom_target(RenegadeRiverCourseShaders DEPENDS
        "${_course_stage}/RiverCoursePS.cso" "${_course_stage}/RiverCourseVS.cso" "${_course_stage}/RiverCourseShadowVS.cso")
endif()
function(renegade_deploy_river_course_shaders consumer)
    add_custom_target(${consumer}RiverCourseContent
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${consumer}>/Content/river"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/river-course-stage/RiverCoursePS.cso"
            "${CMAKE_BINARY_DIR}/river-course-stage/RiverCourseVS.cso"
            "${CMAKE_BINARY_DIR}/river-course-stage/RiverCourseShadowVS.cso"
            "$<TARGET_FILE_DIR:${consumer}>/Content/river"
        DEPENDS RenegadeRiverCourseShaders VERBATIM)
    add_dependencies(${consumer} ${consumer}RiverCourseContent)
endfunction()

if(WIN32)
 include("${CMAKE_SOURCE_DIR}/cmake/RenegadeWaterfallResources.cmake")
endif()
