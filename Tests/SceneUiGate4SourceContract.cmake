if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(CHROME_HEADER "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioChrome.h")
set(ASSET_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioChromeAssets.cpp")
set(READABILITY_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioAssetBrowserReadability.cpp")
set(DRAG_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/CreatorAssetDragPreview.cpp")
set(STUDIO_CMAKE "${RENEGADE_SOURCE_DIR}/Studio/CMakeLists.txt")

foreach(path IN ITEMS
    "${CHROME_HEADER}"
    "${ASSET_SOURCE}"
    "${READABILITY_SOURCE}"
    "${DRAG_SOURCE}"
    "${STUDIO_CMAKE}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Scene UI Gate 4 source contract input is missing: ${path}")
    endif()
endforeach()

file(READ "${CHROME_HEADER}" chrome_header)
file(READ "${ASSET_SOURCE}" asset_source)
file(READ "${READABILITY_SOURCE}" readability_source)
file(READ "${DRAG_SOURCE}" drag_source)
file(READ "${STUDIO_CMAKE}" studio_cmake)

function(require_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Scene UI Gate 4 contract missing ${description}: ${needle}")
    endif()
endfunction()

# Gate 4 opts the Asset Browser into readable typography without changing the
# shared defaults already accepted by Story Flow, Screen Editor and Gate 3.
require_text(chrome_header
    "CreatorAssetTextInputField() { SetRenderTextSize(12); }"
    "12 px creator text input")
require_text(chrome_header
    "CreatorAssetSearchField() { SetRenderTextSize(12); }"
    "12 px creator search")
require_text(chrome_header
    "CreatorAssetComboBox() { SetRenderTextSize(11); }"
    "11 px creator filter combo")
require_text(chrome_header
    "CreatorAssetButton() { SetRenderTextSize(10); }"
    "compact readable creator action buttons")
require_text(chrome_header
    "RenderCreatorAssetBrowserReadabilityOverlay(cmd);"
    "post-base browser readability overlay")
require_text(chrome_header
    "CreatorAssetSearchField creatorAssetSearch_;"
    "Asset Browser search owns Gate 4 presentation hook")
require_text(chrome_header
    "CreatorAssetTextInputField creatorAssetTags_;"
    "readable creator tags field")
require_text(chrome_header
    "CreatorAssetComboBox creatorAssetStateCombo_;"
    "readable creator state filter")
require_text(chrome_header
    "CreatorAssetButton creatorAssetImportButton_;"
    "readable import action")
require_text(chrome_header
    "AssetBrowserFolderScrollRow() const noexcept"
    "read-only folder scroll presentation seam")
require_text(chrome_header
    "AssetBrowserAssetScrollRow() const noexcept"
    "read-only card scroll presentation seam")
require_text(chrome_header
    "VisibleAssetFolderRows() const noexcept"
    "read-only visible folder presentation seam")
require_text(chrome_header
    "AssetBrowserSelectedPath() const noexcept"
    "read-only selection presentation seam")

# The overlay retains the accepted card/drawer geometry while raising every
# creator-facing folder/card text surface to a readable floor.
require_text(readability_source
    "constexpr float AssetCardWidth = 148.0f;"
    "accepted Asset Browser card width")
require_text(readability_source
    "constexpr float AssetCardHeight = 112.0f;"
    "accepted Asset Browser card height")
require_text(readability_source
    "constexpr int AssetFolderTextSize = 11;"
    "readable folder names")
require_text(readability_source
    "constexpr int AssetCardNameTextSize = 11;"
    "readable card names")
require_text(readability_source
    "constexpr int AssetCardMetadataTextSize = 10;"
    "readable card metadata")
require_text(readability_source
    "constexpr int AssetCardFallbackTextSize = 10;"
    "readable missing-thumbnail fallback")
require_text(readability_source
    "chrome->AssetBrowserFolderScrollRow()"
    "overlay follows folder scrolling")
require_text(readability_source
    "chrome->AssetBrowserAssetScrollRow()"
    "overlay follows card scrolling")
require_text(readability_source
    "chrome->VisibleAssetFolderRows()"
    "overlay follows collapsed folder state")
require_text(readability_source
    "chrome->AssetBrowserSelectedPath()"
    "overlay follows actual selected card")
require_text(readability_source
    "\"NO PREVIEW\""
    "honest missing-thumbnail state")
require_text(readability_source
    "\"FOLDER\""
    "readable folder card fallback")

# Preserve the governed import -> reveal -> thumbnail -> stable-ID handoff.
require_text(asset_source
    "bool CreatorAssetStudioChrome::RevealCreatorAsset("
    "post-import browser reveal")
require_text(asset_source
    "creatorVisibleThumbnailPaths_.count(normalizedPath) == 0"
    "post-import thumbnail verification")
require_text(asset_source
    "detail::RequestCreatorAssetDragPreparation(assetId, normalizedPath);"
    "revealed model drag preparation")
require_text(asset_source
    "bridge::CanPlaceCreatorModelAsset"
    "governed model placeability policy")
require_text(asset_source
    "creatorAssetPlaceButton_.SetEnabled(modelProduct || textureAssignable);"
    "honest PLACE/ASSIGN enable state")
require_text(asset_source
    "creatorAction_(Action::ImportModel);"
    "guided preview-first model import route")
require_text(asset_source
    "detail::WarmCreatorAssetDragPreparation("
    "visible-card placement warm-up")
require_text(asset_source
    "creatorAssetStateCombo_.OnSelect"
    "state filter wiring")
require_text(asset_source
    "creatorAssetFormatCombo_.OnSelect"
    "format filter wiring")
require_text(asset_source
    "creatorAssetRigCombo_.OnSelect"
    "rig filter wiring")

# Preserve the existing last-mile drag/drop path: surface resolution, grounding,
# queued cold drops, command/undo ownership and cancellation.
require_text(drag_source
    "void QueueCreatorAssetDrop("
    "queued release while preparation is pending")
require_text(drag_source
    "wi::enums::FILTER_OBJECT_ALL | wi::enums::FILTER_TERRAIN"
    "object/terrain placement surface picking")
require_text(drag_source
    "ImportService::ResolveGroundedPlacementY("
    "grounded reusable model placement")
require_text(drag_source
    "std::make_unique<bridge::PlaceReusableModelCommand>("
    "undoable reusable model placement command")
require_text(drag_source
    "session->Commands().Execute(std::move(command))"
    "placement command execution through Studio command stack")
require_text(drag_source
    "ASSET DRAG // CANCELLED // OUTSIDE VIEWPORT"
    "outside-viewport cancellation")
require_text(drag_source
    "wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE)"
    "Escape cancellation")
require_text(drag_source
    "wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT)"
    "right-click cancellation")

require_text(studio_cmake
    "src/RenegadeStudioAssetBrowserReadability.cpp"
    "Gate 4 readability source in RenegadeStudio target")

message(STATUS "Scene UI Gate 4 Asset Browser and placement source contract passed")
