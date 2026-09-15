if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(PREFAB_HEADER
    "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/CharacterPrefabService.h")
set(PREFAB_SOURCE
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/CharacterPrefabService.cpp")
set(DUPLICATE_SOURCE
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/CharacterDuplicateCommand.cpp")
set(PLACEMENT_HEADER
    "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/ReusableAssetInstanceService.h")
set(PLACEMENT_SOURCE
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/ReusableAssetInstanceService.cpp")
set(PREFAB_ROUTER
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/CharacterPrefabPlacementRouter.cpp")
set(PREFAB_TEMPLATE
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/CharacterPrefabPlacementTemplate.cpp")
set(STUDIO_INTEGRATION
    "${RENEGADE_SOURCE_DIR}/Studio/src/CharacterPrefabStudioIntegration.cpp")
set(INSPECTOR_EXTENSION
    "${RENEGADE_SOURCE_DIR}/Studio/src/CharacterPrefabInspectorExtension.cpp")
set(ASSET_POLICY
    "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/CreatorAssetActionPolicy.h")
set(ROOT_CMAKE "${RENEGADE_SOURCE_DIR}/CMakeLists.txt")
set(BRIDGE_CMAKE "${RENEGADE_SOURCE_DIR}/EngineBridge/CMakeLists.txt")

foreach(path IN ITEMS
    "${PREFAB_HEADER}" "${PREFAB_SOURCE}" "${DUPLICATE_SOURCE}"
    "${PLACEMENT_HEADER}" "${PLACEMENT_SOURCE}" "${PREFAB_ROUTER}"
    "${PREFAB_TEMPLATE}" "${STUDIO_INTEGRATION}" "${INSPECTOR_EXTENSION}"
    "${ASSET_POLICY}" "${ROOT_CMAKE}" "${BRIDGE_CMAKE}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "CW-05 source contract input is missing: ${path}")
    endif()
endforeach()

file(READ "${PREFAB_HEADER}" prefab_header)
file(READ "${PREFAB_SOURCE}" prefab_source)
file(READ "${DUPLICATE_SOURCE}" duplicate_source)
file(READ "${PLACEMENT_HEADER}" placement_header)
file(READ "${PLACEMENT_SOURCE}" placement_source)
file(READ "${PREFAB_ROUTER}" prefab_router)
file(READ "${PREFAB_TEMPLATE}" prefab_template)
file(READ "${STUDIO_INTEGRATION}" studio_integration)
file(READ "${INSPECTOR_EXTENSION}" inspector_extension)
file(READ "${ASSET_POLICY}" asset_policy)
file(READ "${ROOT_CMAKE}" root_cmake)
file(READ "${BRIDGE_CMAKE}" bridge_cmake)

function(require_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "CW-05 source contract missing ${description}: ${needle}")
    endif()
endfunction()

# Portable authored prefab contract and stable physical dependency.
require_text(prefab_header
    "CharacterPrefabExtension = \".rcharprefab\""
    "dedicated portable Character Prefab product")
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
    "prefab self-reference remap on direct placement")
require_text(prefab_source
    "record.dependencyAssetIds = {document.baseCharacterAssetId};"
    "stable base Character Asset dependency")
require_text(prefab_source
    "ProjectDocumentTransaction transaction;"
    "atomic prefab plus registry persistence")

# Normal Scene duplicate remains the creator command, but it is Character-aware
# and can atomically carry the external scripting companion using fresh IDs.
require_text(duplicate_source
    "RemoveCharacterCommand remove"
    "copied native controller teardown")
require_text(duplicate_source
    "MakeCharacterCommand make"
    "native Character foundation rebuild")
require_text(duplicate_source
    "SetDuplicateEntityCompanionFactory"
    "external authoring companion seam")
require_text(duplicate_source
    "AssignNewPersistentEntityId"
    "fresh Scene identity on duplicate")

# Existing reusable placement is still the one Asset Browser/drag command. Its
# companion seam applies prefab authoring only after fresh Scene identity exists
# and participates symmetrically in Undo/Redo.
require_text(placement_header
    "ReusablePlacementCompanionFactory"
    "placement companion contract")
require_text(placement_source
    "SetReusablePlacementCompanionFactory"
    "placement companion registration")
require_text(placement_source
    "ApplyPlacementCompanion()"
    "placement companion execution")
require_text(placement_source
    "companion_.redo()"
    "placement companion Redo")
require_text(placement_source
    "companion_.undo()"
    "placement companion Undo")
require_text(prefab_router
    "PrepareCharacterPrefabPlacement("
    "stable-ID Character Prefab placement routing")
require_text(prefab_router
    "PrepareModelAssetPlacementLegacy(request)"
    "ordinary reusable placement preservation")
require_text(prefab_template
    "CharacterPrefabPlacementTemplateMetadataKey"
    "transient prefab layer carriage")

# Studio integration binds the existing commands to .rscripts, keeps the live
# reusable instance referenced to its base Character Asset, and strips the
# transient prefab marker before the Scene snapshot is captured.
require_text(studio_integration
    "DuplicateEntityScriptAttachments("
    "fresh Action/Script duplication")
require_text(studio_integration
    "SetReusablePlacementCompanionFactory("
    "prefab placement Studio hook")
require_text(studio_integration
    "ReusableAssetInstanceIdMetadataKey"
    "base Character Asset instance binding")
require_text(studio_integration
    "CharacterPrefabOriginAssetMetadataKey"
    "separate prefab-origin provenance")
require_text(studio_integration
    "CharacterPrefabPlacementTemplateMetadataKey"
    "transient placement marker cleanup")
require_text(studio_integration
    "SaveCharacterPrefab(request)"
    "creator prefab persistence action")

# The Character Inspector exposes an explicit save action immediately after the
# accepted Character section; no second editor or hidden asset workflow exists.
require_text(inspector_extension
    "SAVE CHARACTER PREFAB"
    "explicit Character Prefab save action")
require_text(inspector_extension
    "SaveSelectedCharacterPrefab("
    "Character Inspector prefab save routing")
require_text(inspector_extension
    "descriptor_.order = 18;"
    "Character-adjacent prefab section order")
require_text(asset_policy
    "CanPlaceCreatorCharacterPrefabAsset"
    "Asset Browser Character Prefab placement policy")
require_text(asset_policy
    "CanPlaceCreatorSceneAsset"
    "shared model/prefab placement policy")

# Build wiring deliberately wraps only the exported inspector functions and the
# accepted LP07 placement backend; all existing creator surfaces remain intact.
require_text(root_cmake
    "CharacterPrefabInspectorExtension.cpp"
    "Character Prefab Inspector build registration")
require_text(root_cmake
    "CharacterPrefabStudioIntegration.cpp"
    "Character Prefab Studio integration build registration")
require_text(root_cmake
    "RegisterAICharacterInspector=RegisterAICharacterInspectorLegacy"
    "bounded legacy Character Inspector wrapper")
require_text(bridge_cmake
    "CharacterPrefabPlacementRouter.cpp"
    "prefab placement router build registration")
require_text(bridge_cmake
    "PrepareModelAssetPlacement=PrepareModelAssetPlacementLegacy"
    "accepted reusable placement backend isolation")

message(STATUS "Character workflow CW-05 source contract passed")
