if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(RECIPE_HEADER
    "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/CreatorModelImportRecipe.h")
set(RECIPE_SOURCE
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/CreatorModelImportRecipe.cpp")
set(COMMIT_SOURCE
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/ModelImportCommitService.cpp")
set(STUDIO_SOURCE
    "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")

foreach(path IN ITEMS
    "${RECIPE_HEADER}" "${RECIPE_SOURCE}"
    "${COMMIT_SOURCE}" "${STUDIO_SOURCE}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Character workflow contract input is missing: ${path}")
    endif()
endforeach()

file(READ "${RECIPE_HEADER}" recipe_header)
file(READ "${RECIPE_SOURCE}" recipe_source)
file(READ "${COMMIT_SOURCE}" commit_source)
file(READ "${STUDIO_SOURCE}" studio_source)

function(require_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "Character workflow contract missing ${description}: ${needle}")
    endif()
endfunction()

# Durable recipe classification remains backward compatible.
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

# The rebuilt importer no longer exposes the retired Model/Character selector.
# It derives Character status from the real imported rig and carries that exact
# classification into the governed commit request.
require_text(studio_source
    "const bool character = candidate->Evidence().skinnedMeshes != 0 &&"
    "automatic skinned-mesh Character detection")
require_text(studio_source
    "candidate->Evidence().armatureBones != 0;"
    "automatic skeleton requirement")
require_text(studio_source
    "request.characterAsset = modelImportCandidate_ &&"
    "commit request receives detected Character classification")
require_text(studio_source
    "modelImportAction_.SetVisible(character);"
    "Character-only gameplay action authoring")
require_text(studio_source
    "modelImportAddAnimation_.SetVisible(character);"
    "Character-only external animation authoring")

# The commit service is the durable authority. It maps the detected request to
# recipe kind and stores semantic animation actions on native clip metadata.
require_text(commit_source
    "authoredRecipe.assetKind = request.characterAsset ? CreatorAssetImportKind::Character : CreatorAssetImportKind::Model;"
    "durable recipe kind from rebuilt importer")
require_text(commit_source
    "CreatorCharacterAnimationActionMetadataKey"
    "durable semantic Character animation action metadata")
require_text(commit_source
    "if (request.characterAsset && (candidate.Evidence().skinnedMeshes == 0 ||"
    "commit-time Character rig validation")

message(STATUS
    "Character workflow CW-01 rebuilt importer classification contract passed")
