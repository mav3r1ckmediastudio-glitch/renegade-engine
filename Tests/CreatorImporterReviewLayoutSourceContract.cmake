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

# The preview is a genuine native render target with explicit rotation controls.
require_text(importer
    "modelImportPreviewImage_.SetSize(XMFLOAT2(512.0f, 320.0f));"
    "512x320 rendered preview")
require_text(importer
    "modelImportRotateLeft_.SetPos(XMFLOAT2(20.0f, 365.0f));"
    "left rotation control below preview")
require_text(importer
    "modelImportRotateRight_.SetPos(XMFLOAT2(285.0f, 365.0f));"
    "right rotation control below preview")
require_text(importer_header
    "std::unique_ptr<ModelImportPreview> modelImportPreview_;"
    "real ModelImportPreview ownership")
require_text(importer_header
    "std::unique_ptr<bridge::ModelImportCandidate> modelImportCandidate_;"
    "governed candidate ownership")

# Static assets keep the compact layout; Characters expand the same native
# importer and expose real clip/action controls rather than a painted panel.
require_text(importer
    "modelImportPanel_.SetSize(XMFLOAT2(560.0f, character ? 850.0f : 610.0f));"
    "static/Character adaptive panel height")
require_text(importer
    "modelImportName_.SetPos(XMFLOAT2(110.0f, character ? 740.0f : 500.0f));"
    "asset-name position below active controls")
require_text(importer
    "modelImportCommit_.SetPos(XMFLOAT2(20.0f, character ? 790.0f : 550.0f));"
    "commit action remains reachable")
require_text(importer
    "modelImportCancel_.SetPos(XMFLOAT2(260.0f, character ? 790.0f : 550.0f));"
    "cancel action remains reachable")
require_text(importer
    "modelImportAddAnimation_.SetPos(XMFLOAT2(20.0f, 685.0f));"
    "external animation action above Character name")
require_text(importer
    "modelImportAction_.SetVisible(character);"
    "Character-only semantic action control")
require_text(importer
    "modelImportClip_.SetVisible(character);"
    "Character-only clip selector")
require_text(importer
    "modelImportTime_.SetVisible(character);"
    "Character-only scrub control")
require_text(importer
    "modelImportSpeed_.SetVisible(character);"
    "Character-only preview speed control")

# Import is only committed after a rendered preview is ready and a thumbnail
# can be captured from that same preview.
require_text(importer
    "modelImportCommit_.SetEnabled(modelImportPreview_->IsReady());"
    "preview readiness gates import")
require_text(importer
    "!modelImportPreview_->CapturePng(request.thumbnailPng, thumbnailError)"
    "commit captures real preview thumbnail")
require_text(importer
    "ModelImportCommitService().CommitModel("
    "governed rebuilt importer commit")

message(STATUS
    "Rebuilt importer native review/layout contract passed")
