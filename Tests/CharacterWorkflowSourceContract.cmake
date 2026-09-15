if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(RECIPE_HEADER
    "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/CreatorModelImportRecipe.h")
set(RECIPE_SOURCE
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/CreatorModelImportRecipe.cpp")
set(STUDIO_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")
set(BROWSER_SOURCE
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/AssetBrowserService.cpp")
set(EXTERNAL_ANIMATION_HEADER
    "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/CreatorExternalAnimationImportService.h")
set(IMPORTER_PREVIEW
    "${RENEGADE_SOURCE_DIR}/Studio/src/CreatorImportPreviewWindow.h")

foreach(path IN ITEMS
        "${RECIPE_HEADER}"
        "${RECIPE_SOURCE}"
        "${STUDIO_SOURCE}"
        "${BROWSER_SOURCE}"
        "${EXTERNAL_ANIMATION_HEADER}"
        "${IMPORTER_PREVIEW}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Character workflow contract input is missing: ${path}")
    endif()
endforeach()

file(READ "${RECIPE_HEADER}" recipe_header)
file(READ "${RECIPE_SOURCE}" recipe_source)
file(READ "${STUDIO_SOURCE}" studio_source)
file(READ "${BROWSER_SOURCE}" browser_source)
file(READ "${EXTERNAL_ANIMATION_HEADER}" external_animation_header)
file(READ "${IMPORTER_PREVIEW}" importer_preview)

function(require_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Character workflow contract missing ${description}: ${needle}")
    endif()
endfunction()

require_text(recipe_header
    "enum class CreatorAssetImportKind"
    "governed import classification")
require_text(recipe_header
    "CreatorAssetImportKind assetKind = CreatorAssetImportKind::Model;"
    "backward-compatible Model default")
require_text(recipe_source
    "root[\"asset_kind\"] = \"character\";"
    "durable Character designation")
require_text(recipe_source
    "asset_kind must be model or character"
    "fail-closed classification validation")
require_text(studio_source
    "creatorImportAssetKind.AddItem(\"IMPORT AS // CHARACTER\");"
    "normal importer Character selection")
require_text(studio_source
    "? \"Content/Characters\""
    "Character destination choice")
require_text(studio_source
    "const std::string importDestination ="
    "final import destination authority")
require_text(studio_source
    "state->destinationFolder = importDestination;"
    "commit uses governed Character destination")
require_text(studio_source
    "materials.recipe.assetKind = state->assetKind;"
    "designation reaches durable recipe")
require_text(browser_source
    "if (category == \"characters\") return AssetType::Character;"
    "Asset Browser Character classification")

# CW-05 owner-validation importer repair. External Character animations are a
# deliberately simple file-slot workflow: local picker, multi-select and one
# visible creator slot per source file. The low-level CW-02 action inspector is
# retained underneath for source validation/provenance, but the creator surface
# must call the file-slot wrapper rather than exposing every native take.
require_text(external_animation_header
    "QueueCreatorExternalAnimationFileSlot"
    "one-file animation-slot ingestion wrapper")
require_text(external_animation_header
    "sourceAlreadyQueued"
    "re-adding a file preserves an existing renamed slot")
require_text(external_animation_header
    "matching.rbegin()"
    "multi-action source collapse to a single file slot")
require_text(importer_preview
    "+ ADD ANIMATION FILES..."
    "explicit local animation-file action")
require_text(importer_preview
    "params.multiselect = true;"
    "multi-select animation file browser")
require_text(importer_preview
    "QueueCreatorExternalAnimationFileSlot(fileName, error)"
    "creator importer uses one-file slot contract")
require_text(importer_preview
    "wi::gui::TreeList externalAnimationClips_"
    "visible scrollable animation file-slot list")
require_text(importer_preview
    "EVENT_THREAD_SAFE_POINT"
    "safe-point local file-browser integration")
require_text(importer_preview
    "One row per external animation file"
    "creator-facing one-file-one-row UX")

message(STATUS "Character workflow CW-01 + owner-validation importer source contract passed")
