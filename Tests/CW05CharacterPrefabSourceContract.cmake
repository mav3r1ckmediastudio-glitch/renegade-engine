if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(PREFAB_HEADER
    "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/CharacterPrefabService.h")
set(PREFAB_SOURCE
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/CharacterPrefabService.cpp")
set(COMMAND_SOURCE
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/CommandService.cpp")
set(STUDIO_SOURCE
    "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")
set(INSPECTOR_SOURCE
    "${RENEGADE_SOURCE_DIR}/Studio/src/AICharacterInspector.cpp")
set(DRAG_SOURCE
    "${RENEGADE_SOURCE_DIR}/Studio/src/CreatorAssetDragPreview.cpp")
set(ASSET_POLICY
    "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/CreatorAssetActionPolicy.h")

foreach(path IN ITEMS
    "${PREFAB_HEADER}" "${PREFAB_SOURCE}" "${COMMAND_SOURCE}"
    "${STUDIO_SOURCE}" "${INSPECTOR_SOURCE}" "${DRAG_SOURCE}"
    "${ASSET_POLICY}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "CW-05 source contract input is missing: ${path}")
    endif()
endforeach()

file(READ "${PREFAB_HEADER}" prefab_header)
file(READ "${PREFAB_SOURCE}" prefab_source)
file(READ "${COMMAND_SOURCE}" command_source)
file(READ "${STUDIO_SOURCE}" studio_source)
file(READ "${INSPECTOR_SOURCE}" inspector_source)
file(READ "${DRAG_SOURCE}" drag_source)
file(READ "${ASSET_POLICY}" asset_policy)

function(require_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "CW-05 source contract missing ${description}: ${needle}")
    endif()
endfunction()

require_text(prefab_header
    "CharacterPrefabExtension = \".rcharprefab\""
    "dedicated portable Character Prefab product")
require_text(prefab_header
    "DuplicateCharacterInstanceCommand"
    "Character-aware duplication command")
require_text(prefab_header
    "PlaceCharacterPrefabCommand"
    "Character Prefab placement command")
require_text(prefab_source
    "DuplicateEntityScriptAttachments("
    "fresh Action/Script attachment duplication")
require_text(prefab_source
    "RemoveCharacterCommand remove"
    "copied native controller teardown before Character rebuild")
require_text(prefab_source
    "MakeCharacterCommand make"
    "native Character foundation reuse during duplication")
require_text(prefab_source
    "document.settings.patrolRouteEntityId.clear();"
    "scene-local patrol reference clearing")
require_text(prefab_source
    "document.settings.weaponEntityId.clear();"
    "scene-local weapon reference clearing")
require_text(prefab_source
    "property.selfEntityReference = true;"
    "portable script self-reference capture")
require_text(prefab_source
    "property.referenceId = ownerId;"
    "prefab self-reference remap on placement")
require_text(prefab_source
    "record.dependencyAssetIds = {document.baseCharacterAssetId};"
    "stable base Character Asset dependency")
require_text(prefab_source
    "ProjectDocumentTransaction transaction;"
    "atomic prefab plus registry persistence")
require_text(prefab_source
    "CharacterPrefabOriginAssetMetadataKey"
    "prefab-origin scene metadata")

# Studio must choose the Character-aware command for configured Characters,
# expose explicit SAVE CHARACTER PREFAB authoring, and accept prefab placement
# through the existing browser/drag command path rather than a second editor.
require_text(studio_source
    "DuplicateCharacterInstanceCommand"
    "Studio Character duplication routing")
require_text(inspector_source
    "SAVE CHARACTER PREFAB"
    "explicit Character Prefab save action")
require_text(inspector_source
    "SaveCharacterPrefab("
    "Character Inspector prefab persistence call")
require_text(asset_policy
    "CanPlaceCreatorCharacterPrefabAsset"
    "Asset Browser Character Prefab placement policy")
require_text(drag_source
    "PrepareCharacterPrefabPlacement("
    "Character Prefab drag preparation")
require_text(drag_source
    "PlaceCharacterPrefabCommand"
    "Character Prefab drag commit command")

message(STATUS "Character workflow CW-05 source contract passed")
