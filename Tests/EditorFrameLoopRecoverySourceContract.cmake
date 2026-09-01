cmake_minimum_required(VERSION 3.19)

function(read_required relative out_var)
    set(path "${RENEGADE_SOURCE_DIR}/${relative}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Frame-loop recovery input is missing: ${relative}")
    endif()
    file(READ "${path}" content)
    set(${out_var} "${content}" PARENT_SCOPE)
endfunction()

function(require_text content needle description)
    string(FIND "${content}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Frame-loop recovery contract failed: ${description}")
    endif()
endfunction()

function(reject_text content needle description)
    string(FIND "${content}" "${needle}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Frame-loop recovery contract failed: ${description}")
    endif()
endfunction()

read_required("Studio/src/RenegadeStudioChromeAssets.cpp" creator_chrome)
read_required("Studio/src/StudioApplication.cpp" studio_application)
read_required("Studio/src/StudioApplication.h" studio_header)
read_required("Studio/src/Phase5Gate5RenderWorkspace.cpp" render_workspace)
read_required("Studio/src/Phase5Gate9RenderDiagnostics.cpp" diagnostics)
read_required("Studio/src/WD01Vegetation.cpp" vegetation)
read_required("Runtime/src/RuntimeApplication.cpp" runtime)
read_required("EngineBridge/include/renegade/bridge/SceneService.h" scene_header)
read_required("EngineBridge/include/renegade/bridge/ProjectLifecycleDiagnostics.h" lifecycle_log)

reject_text("${creator_chrome}"
    "bridge::RestoreMaterialTextureBindings("
    "Creator chrome Update must not restore governed textures")
require_text("${studio_application}"
    "void StudioRenderPath::RestoreGovernedMaterialTextures()"
    "Scene lifecycle texture restoration seam is missing")

reject_text("${lifecycle_log}"
    "PR58Gate1Lifecycle.log"
    "PR58 lifecycle timing must not append a production file")
reject_text("${lifecycle_log}"
    "std::ofstream"
    "lifecycle diagnostics still perform synchronous file I/O")

reject_text("${studio_application}"
    "SetToDrawDebugEnvProbes"
    "ordinary Studio update still owns native probe diagnostics")
require_text("${diagnostics}"
    "SetToDrawDebugEnvProbes"
    "Gate 9 Diagnostics must remain the native probe-debug owner")

require_text("${scene_header}"
    "std::uint64_t Revision() const noexcept;"
    "Scene replacement revision is missing")
require_text("${studio_application}"
    "appliedRenderSettingsSceneRevision_ !="
    "Studio render settings are not revision-gated")
require_text("${render_workspace}"
    "appliedRenderSettingsSceneRevision_ = session_->Scenes().Revision();"
    "Studio does not acknowledge the applied Scene revision")
require_text("${runtime}"
    "renderSettingsSceneRevision_ != scenes_->Revision()"
    "Runtime render settings are not revision-gated")

require_text("${vegetation}"
    "wd01SynchronizedChunkCount_ == terrain->chunks.size()"
    "idle vegetation maintenance lacks a chunk lifecycle gate")
require_text("${vegetation}"
    "bridge::SynchronizeWickedVegetation(scene, *terrain);"
    "new terrain chunks no longer receive Wicked vegetation")
require_text("${studio_header}"
    "wd01SynchronizedTerrainEntity_"
    "vegetation lifecycle identity tracking is missing")

message(STATUS "Editor frame-loop recovery source contract passed")
