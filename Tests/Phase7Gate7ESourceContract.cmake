if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

function(require_file path label)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Phase 7E missing ${label}: ${path}")
    endif()
endfunction()

function(require_text text needle label)
    string(FIND "${text}" "${needle}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Phase 7E source contract missing ${label}: ${needle}")
    endif()
endfunction()

function(forbid_text text needle label)
    string(FIND "${text}" "${needle}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "Phase 7E source contract forbids ${label}: ${needle}")
    endif()
endfunction()

set(service_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/SpecialistComponentService.cpp")
set(terrain_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/TerrainCreatorGapService.cpp")
set(video_workflow "${RENEGADE_SOURCE_DIR}/EngineBridge/src/CreatorVideoWorkflowService.cpp")
set(video_asset_header "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/VideoAssetService.h")
set(video_asset "${RENEGADE_SOURCE_DIR}/EngineBridge/src/VideoAssetService.cpp")
set(resource_dependencies "${RENEGADE_SOURCE_DIR}/EngineBridge/src/ResourceAssetDependencyService.cpp")
set(studio_session "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/StudioSession.h")
set(runtime_assets "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeReusableAssets.cpp")
set(inspector_source "${RENEGADE_SOURCE_DIR}/Studio/src/Phase7Gate7ESpecialistInspector.cpp")
set(async_guard "${RENEGADE_SOURCE_DIR}/Studio/src/Phase7AsyncSceneGuard.h")
set(animation_header "${RENEGADE_SOURCE_DIR}/Studio/src/Phase7Gate7AAnimationInspector.h")
set(root_cmake "${RENEGADE_SOURCE_DIR}/CMakeLists.txt")
set(bridge_cmake "${RENEGADE_SOURCE_DIR}/EngineBridge/CMakeLists.txt")

require_file("${service_source}" "SpecialistComponentService")
require_file("${terrain_source}" "TerrainCreatorGapService")
require_file("${video_workflow}" "CreatorVideoWorkflowService")
require_file("${video_asset_header}" "VideoAssetService header")
require_file("${video_asset}" "VideoAssetService")
require_file("${resource_dependencies}" "ResourceAssetDependencyService")
require_file("${studio_session}" "StudioSession")
require_file("${runtime_assets}" "Runtime governed resource restore")
require_file("${inspector_source}" "specialist inspector")
require_file("${async_guard}" "Phase 7 async scene guard")

file(READ "${service_source}" service)
file(READ "${terrain_source}" terrain)
file(READ "${video_workflow}" video_workflow_text)
file(READ "${video_asset_header}" video_asset_h)
file(READ "${video_asset}" video_asset_text)
file(READ "${resource_dependencies}" resource_dependencies_text)
file(READ "${studio_session}" studio_session_text)
file(READ "${runtime_assets}" runtime_assets_text)
file(READ "${inspector_source}" inspector)
file(READ "${async_guard}" guard)
file(READ "${animation_header}" animation)
file(READ "${root_cmake}" root)
file(READ "${bridge_cmake}" bridge)

require_text("${service}" "scene_->hairs" "native HairParticle manager")
require_text("${service}" "scene_->forces" "native ForceField manager")
require_text("${service}" "scene_->videos" "native VideoComponent manager")
require_text("${service}" "scene_->splines" "native Spline manager")
require_text("${service}" "scene.gaussian_splats" "native Gaussian manager")
require_text("${service}" "ImportModel_PLY" "native Wicked PLY importer")
require_text("${service}" "SetHairParticleStateCommand" "command-backed hair authoring")
require_text("${service}" "SetForceFieldStateCommand" "command-backed force authoring")
require_text("${service}" "SetVideoAuthoredStateCommand" "command-backed video loop authoring")
require_text("${service}" "SetSplineStateCommand" "command-backed spline authoring")
require_text("${service}" "AddSplineNodeCommand" "native spline node authoring")
require_text("${service}" "GetSplatCount" "Gaussian splat inspection")

require_text("${terrain}" "enable_blendmap_layer" "native terrain blendmap layers")
require_text("${terrain}" "CreateChunkRegionTexture" "native terrain region texture refresh")
require_text("${terrain}" "chunk->vt->invalidate()" "native virtual-texture invalidation")
require_text("${terrain}" "TerrainMaterialPaintCommand" "command-backed terrain material paint")
require_text("${terrain}" "ExportTerrainHeightmapR16" "heightmap export")
require_text("${terrain}" "ImportTerrainHeightmapR16Command" "command-backed heightmap import")
require_text("${terrain}" "ApplyTerrainSculpt" "existing Renegade terrain authority reuse")

require_text("${video_workflow_text}" "SourceAssets\" / \"Video" "retained original video source")
require_text("${video_workflow_text}" "Content\" / \"Video" "governed video product destination")
require_text("${video_workflow_text}" "ImportResourceAsset" "LP08 governed video import")
require_text("${video_asset_text}" "VideoAssetIdMetadataKey" "stable Scene video binding")
require_text("${video_asset_text}" "BuildResourcePayloadCacheName" "stable payload cache identity")
require_text("${video_asset_text}" "PreparePackagedResourceAsset" "packaged video resolver")
require_text("${video_asset_text}" "SetVideoAssetCommand" "undoable governed video assignment")
require_text("${video_asset_h}" "wi::Resource afterResource_" "resource-backed deterministic video Redo")
require_text("${video_asset_h}" "bool capturedAfter_" "first-apply video Redo capture")
require_text("${video_asset_text}" "RestoreAfter" "resource-backed video Redo path")
require_text("${video_asset_text}" "std::vector<std::uint8_t>().swap(prepared_.payload)" "release raw MP4 payload from Undo history")
require_text("${video_asset_text}" "MetadataEmpty(*metadata)" "preserve unrelated metadata during video Undo")
require_text("${resource_dependencies_text}" "DependencyClass::Video" "video package dependency closure")
require_text("${resource_dependencies_text}" "lp08.video_asset_binding:" "video dependency provenance")
require_text("${studio_session_text}" "RestoreGovernedVideoBindingsAfterOpen" "Studio reopen video restore")
require_text("${runtime_assets_text}" "RestorePackagedVideoAssetBindings" "packaged Runtime video restore")
require_text("${runtime_assets_text}" "RestoreVideoAssetBindings" "authored/Test Level Runtime video restore")

require_text("${guard}" "Scenes().Revision()" "scene lifecycle revision guard")
require_text("${guard}" "CurrentProject().projectId" "project identity guard")
require_text("${inspector}" "CapturePhase7AsyncSceneGuard" "dialog captures original scene context")
require_text("${inspector}" "MatchesPhase7AsyncSceneGuard" "dialog validates context at final mutation callback")
require_text("${inspector}" "wi::video::CreateVideo(filename, &probe)" "pre-commit Wicked H264 decode preflight")
require_text("${inspector}" "governed H264 MP4" "supported codec disclosure")
forbid_text("${inspector}" "H264/H265" "unsupported H265 creator claim")
forbid_text("${inspector}" "PersistentEntityId" "unsaved-entity adoption regression")
forbid_text("${inspector}" "EntityIdentityIndex" "unnecessary persistent-identity resolver")
forbid_text("${inspector}" "RENEGADE_PHASE7_GUARDED_FILE_DIALOG" "preprocessor dialog interception")
forbid_text("${inspector}" "Phase7AsyncFileDialogGuard" "rejected v2 macro guard")
forbid_text("${inspector}" "state.filename = filename" "external video path as Scene authority")

foreach(token IN ITEMS
    "HAIR / FUR"
    "ADD FORCE FIELD"
    "ADOPT MP4"
    "ADD NODE"
    "IMPORT PLY"
    "PAINT MATERIAL"
    "IMPORT R16"
    "EXPORT R16"
    "Terrain Virtual Texture Info")
    require_text("${inspector}" "${token}" "creator specialist UI")
endforeach()

require_text("${inspector}" "CreatorVideoWorkflowService" "governed creator video adoption")
require_text("${inspector}" "PrepareVideoAsset" "governed video payload preparation")
require_text("${inspector}" "SetVideoAssetCommand" "governed video command assignment")
require_text("${inspector}" "VideoAssetIdMetadataKey" "creator stable-video presentation")

require_text("${animation}" "RegisterPhase7Gate7ESpecialistInspector" "7E inspector registration")
require_text("${animation}" "PreparePhase7Gate7ESpecialistInspector" "7E inspector layout preparation")
require_text("${root}" "Phase7Gate7ESpecialistInspector.cpp" "7E Studio target ownership")
require_text("${root}" "include(Tests/Phase7Gate7E.cmake)" "7E test registration")
require_text("${bridge}" "CreatorVideoWorkflowService.cpp" "governed creator video target ownership")
require_text("${bridge}" "VideoAssetService.cpp" "governed video binding target ownership")
require_text("${bridge}" "ModelImporter_PLY.cpp" "PLY converter ownership")
require_text("${bridge}" "miniply.cpp" "PLY parser ownership")

foreach(stock_header IN ITEMS
    "HairParticleWindow.h"
    "ForceFieldWindow.h"
    "VideoWindow.h"
    "SplineWindow.h"
    "GaussianSplatWindow.h"
    "TerrainWindow.h"
    "PaintToolWindow.h")
    string(FIND "${inspector}" "${stock_header}" stock_window)
    if(NOT stock_window EQUAL -1)
        message(FATAL_ERROR "Phase 7E must not embed Wicked stock editor window: ${stock_header}")
    endif()
endforeach()

message(STATUS "Phase 7E repaired native specialist component source contract passed")
