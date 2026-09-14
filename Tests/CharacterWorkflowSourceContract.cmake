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

foreach(path IN ITEMS "${RECIPE_HEADER}" "${RECIPE_SOURCE}" "${STUDIO_SOURCE}" "${BROWSER_SOURCE}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Character workflow contract input is missing: ${path}")
    endif()
endforeach()

file(READ "${RECIPE_HEADER}" recipe_header)
file(READ "${RECIPE_SOURCE}" recipe_source)
file(READ "${STUDIO_SOURCE}" studio_source)
file(READ "${BROWSER_SOURCE}" browser_source)

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

message(STATUS "Character workflow CW-01 source contract passed")
