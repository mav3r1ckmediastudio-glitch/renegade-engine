if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(RENDER_PATH "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowRenderPath.h")
set(INTEGRATION "${RENEGADE_SOURCE_DIR}/Studio/src/StoryFlowStudioIntegration.h")
set(COMPOSER "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowDestinationComposer.h")
set(LEGACY_LEVEL "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowLevelPanel.h")
set(LEGACY_SCREEN "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowScreenPanel.h")

foreach(path IN ITEMS "${RENDER_PATH}" "${INTEGRATION}" "${COMPOSER}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Journey recovery 9A source contract input is missing: ${path}")
    endif()
endforeach()

foreach(path IN ITEMS "${LEGACY_LEVEL}" "${LEGACY_SCREEN}")
    if(EXISTS "${path}")
        message(FATAL_ERROR "Journey recovery 9A legacy lifecycle panel still exists: ${path}")
    endif()
endforeach()

file(READ "${RENDER_PATH}" render_path_source)
file(READ "${INTEGRATION}" integration_source)
file(READ "${COMPOSER}" composer_source)

function(require_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Journey recovery 9A contract missing ${description}: ${needle}")
    endif()
endfunction()

function(forbid_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Journey recovery 9A contract regressed ${description}: ${needle}")
    endif()
endfunction()

# Levels/Screens now open one compact Journey-native destination sheet.
require_text(render_path_source "RenegadeStoryFlowDestinationComposer destinationComposer_" "Journey destination composer ownership")
require_text(render_path_source "destinationComposer_.Toggle(" "rail-to-composer routing")
require_text(composer_source "ADD DESTINATION" "compact destination sheet")
require_text(composer_source "Mode::Level" "Level destination mode")
require_text(composer_source "Mode::Screen" "Screen destination mode")
forbid_text(integration_source "JourneyPanel" "legacy Level/Screen panel state switch")
forbid_text(integration_source "PlaceWorkspaceBehindLifecycleControls" "legacy lifecycle layering repair")

# The replacement UI must continue to queue the accepted governed lifecycle
# services and stable-ID editor handoffs rather than implementing new semantics.
require_text(integration_source "storyFlow.OnCreateLevel" "Level creation callback")
require_text(integration_source "storyFlow.OnAdoptLevel" "existing Level adoption callback")
require_text(integration_source "storyFlow.OnCreateScreen" "Screen creation callback")
require_text(integration_source "storyFlow.OnOpenSelectedDestination" "selected destination open callback")
require_text(integration_source "ProcessPendingLevelAction" "governed Level lifecycle processing")
require_text(integration_source "ProcessPendingScreenAction" "governed Screen lifecycle processing")
require_text(integration_source "StoryFlowLevelLifecycleService service" "Level lifecycle service authority")
require_text(integration_source "StoryFlowScreenLifecycleService service" "Screen lifecycle service authority")
require_text(integration_source "StoryFlowLevelReferenceService service" "Level stable-ID resolution")
require_text(integration_source "StoryFlowScreenReferenceService service" "Screen stable-ID resolution")

message(STATUS "Story Flow Journey recovery 9A lifecycle presentation contract passed")
