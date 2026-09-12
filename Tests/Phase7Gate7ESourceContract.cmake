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

set(service_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/SpecialistComponentService.cpp")
set(terrain_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/TerrainCreatorGapService.cpp")
set(inspector_source "${RENEGADE_SOURCE_DIR}/Studio/src/Phase7Gate7ESpecialistInspector.cpp")
set(animation_header "${RENEGADE_SOURCE_DIR}/Studio/src/Phase7Gate7AAnimationInspector.h")
set(root_cmake "${RENEGADE_SOURCE_DIR}/CMakeLists.txt")
set(bridge_cmake "${RENEGADE_SOURCE_DIR}/EngineBridge/CMakeLists.txt")

require_file("${service_source}" "SpecialistComponentService")
require_file("${terrain_source}" "TerrainCreatorGapService")
require_file("${inspector_source}" "specialist inspector")

file(READ "${service_source}" service)
file(READ "${terrain_source}" terrain)
file(READ "${inspector_source}" inspector)
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
require_text("${service}" "SetVideoAuthoredStateCommand" "command-backed video authoring")
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

foreach(token IN ITEMS
    "HAIR / FUR"
    "ADD FORCE FIELD"
    "OPEN MP4"
    "ADD NODE"
    "IMPORT PLY"
    "PAINT MATERIAL"
    "IMPORT R16"
    "EXPORT R16"
    "Terrain Virtual Texture Info")
    require_text("${inspector}" "${token}" "creator specialist UI")
endforeach()

require_text("${animation}" "RegisterPhase7Gate7ESpecialistInspector" "7E inspector registration")
require_text("${animation}" "PreparePhase7Gate7ESpecialistInspector" "7E inspector layout preparation")
require_text("${root}" "Phase7Gate7ESpecialistInspector.cpp" "7E Studio target ownership")
require_text("${root}" "include(Tests/Phase7Gate7E.cmake)" "7E test registration")
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

message(STATUS "Phase 7E native specialist component source contract passed")
