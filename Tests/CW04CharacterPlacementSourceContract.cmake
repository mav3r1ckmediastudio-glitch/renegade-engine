if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(PLACEMENT_PREP
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/ReusableAssetPlacementService.cpp")
set(INSTANCE_SOURCE
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/ReusableAssetInstanceService.cpp")
set(CHARACTER_HEADER
    "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/CharacterService.h")
set(DRAG_SOURCE
    "${RENEGADE_SOURCE_DIR}/Studio/src/CreatorAssetDragPreview.cpp")
set(INSPECTOR_SOURCE
    "${RENEGADE_SOURCE_DIR}/Studio/src/AICharacterInspector.cpp")

foreach(path IN ITEMS
    "${PLACEMENT_PREP}"
    "${INSTANCE_SOURCE}"
    "${CHARACTER_HEADER}"
    "${DRAG_SOURCE}"
    "${INSPECTOR_SOURCE}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "CW-04 source contract input is missing: ${path}")
    endif()
endforeach()

file(READ "${PLACEMENT_PREP}" placement_prep)
file(READ "${INSTANCE_SOURCE}" instance_source)
file(READ "${CHARACTER_HEADER}" character_header)
file(READ "${DRAG_SOURCE}" drag_source)
file(READ "${INSPECTOR_SOURCE}" inspector_source)

function(require_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "CW-04 source contract missing ${description}: ${needle}")
    endif()
endfunction()

require_text(character_header
    "CharacterAssetTemplateMetadataKey"
    "prepared Character Asset template marker")
require_text(character_header
    "CharacterAssetTemplateInHierarchy"
    "drag/live hierarchy Character classification")
require_text(placement_prep
    "ParseCreatorModelImportOptions("
    "durable import recipe classification at placement preparation")
require_text(placement_prep
    "creatorRecipe.assetKind == CreatorAssetImportKind::Character"
    "Character recipe ownership")
require_text(placement_prep
    "MarkCharacterAssetTemplate(*prepared.scene_)"
    "in-memory Character placement marker")
require_text(instance_source
    "AssignFreshReusableHierarchyIdentities(*scene_, entity_)"
    "fresh per-placement identity assignment")
require_text(instance_source
    "PromotePreparedCharacter()"
    "atomic prepared Character promotion")
require_text(instance_source
    "MakeCharacterCommand promotion("
    "reuse of accepted Character foundation")
require_text(instance_source
    "CharacterAuthoringSettings{}"
    "fresh default gameplay authoring settings")
require_text(drag_source
    "std::make_unique<bridge::PlaceReusableModelCommand>("
    "drag/drop using the Character-aware placement command")
require_text(drag_source
    "placed->PlacedEntity()"
    "placed Character wrapper selection handoff")
require_text(inspector_source
    "descriptor_.defaultExpanded = true;"
    "default-expanded Character inspector")
require_text(inspector_source
    "isCharacter_ = promotion.alreadyCharacter;"
    "selection routing from actual Character state")
require_text(inspector_source
    "CHARACTER ACTIVE // native controller"
    "active prepared Character inspector state")

# Prove the critical ordering inside the placement command: scene identity must
# be fresh before the existing Character service adopts it. A later refactor may
# reorganize the function, but it must preserve this invariant or update this
# contract with equivalent/stronger coverage.
string(FIND "${instance_source}"
    "AssignFreshReusableHierarchyIdentities(*scene_, entity_)"
    identity_pos)
string(FIND "${instance_source}"
    "if (!PromotePreparedCharacter())"
    promotion_pos)
if(identity_pos EQUAL -1 OR promotion_pos EQUAL -1 OR
   promotion_pos LESS identity_pos)
    message(FATAL_ERROR
        "CW-04 requires fresh reusable scene identity before Character promotion")
endif()

message(STATUS "CW-04 prepared Character placement source contract passed")
