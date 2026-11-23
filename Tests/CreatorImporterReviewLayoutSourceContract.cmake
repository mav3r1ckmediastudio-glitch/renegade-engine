# Rebuilt importer geometry and interaction regression.
# The retired multi-stage Review UI was removed with the legacy importer.
if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(STUDIO_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")
set(STUDIO_HEADER "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.h")
foreach(path IN ITEMS "${STUDIO_SOURCE}" "${STUDIO_HEADER}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Rebuilt importer contract input missing: ${path}")
    endif()
endforeach()
file(READ "${STUDIO_SOURCE}" importer)
file(READ "${STUDIO_HEADER}" importer_header)

function(require_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "Rebuilt importer contract missing ${description}: ${needle}")
    endif()
endfunction()

file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/UnifiedImportWorkspace.inc" workspace)
require_text(importer "ResizeImportWorkspace(width, height);" "follow Studio window size")
require_text(workspace "place(modelImportPreviewImage_, pad, 52, viewWidth, viewHeight);" "expanding model viewport")
require_text(importer "modelImportPreview_->SetViewportSize(" "native resolution preview")
require_text(workspace "modelImportMaterial_.SetVisible(materials);" "material inspection")
require_text(workspace "modelImportClip_.SetVisible(actions);" "clip inspection")
require_text(workspace "modelImportTime_.SetVisible(actions);" "scrub control")
require_text(workspace "modelImportAction_.SetVisible(actions && character);" "gameplay assignment")
require_text(workspace "modelImportFolder_.SetVisible(chosen);" "explicit destination")
require_text(workspace "ApplyImportTransform();" "editable import transform")
require_text(importer "SelectImportWorkflow(fromProjectile ? ImportWorkflow::Static : ImportWorkflow::None);" "choose after preview")

# Import is only committed after a rendered preview is ready and a thumbnail
# can be captured from that same preview.
require_text(importer
    "modelImportCommit_.SetEnabled(modelImportPreview_->IsReady() && importWorkflow_ != ImportWorkflow::None);"
    "preview readiness gates import")
require_text(importer
    "!modelImportPreview_->CapturePng(request.thumbnailPng, thumbnailError)"
    "commit captures real preview thumbnail")
require_text(importer
    "ModelImportCommitService().CommitModel("
    "governed rebuilt importer commit")

message(STATUS
    "Rebuilt importer native review/layout contract passed")
