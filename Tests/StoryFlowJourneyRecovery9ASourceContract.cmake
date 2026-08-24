if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(RENDER_PATH "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowRenderPath.h")
set(INTEGRATION "${RENEGADE_SOURCE_DIR}/Studio/src/StoryFlowStudioIntegration.h")
set(COMPOSER "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowDestinationComposer.h")
set(CHROME "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowJourneyChrome.h")
set(LAYOUT "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowJourneyLayout.h")
set(CARD "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowJourneyCard.h")
set(LANE "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowJourneyLane.h")
set(ROLE "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowJourneyRole.h")
set(WORKSPACE "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowWorkspace.cpp")
set(LEGACY_LEVEL "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowLevelPanel.h")
set(LEGACY_SCREEN "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowScreenPanel.h")

foreach(path IN ITEMS "${RENDER_PATH}" "${INTEGRATION}" "${COMPOSER}"
        "${CHROME}" "${LAYOUT}" "${CARD}" "${LANE}" "${ROLE}" "${WORKSPACE}")
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
file(READ "${CHROME}" chrome_source)
file(READ "${LAYOUT}" layout_source)
file(READ "${CARD}" card_source)
file(READ "${LANE}" lane_source)
file(READ "${ROLE}" role_source)
file(READ "${WORKSPACE}" workspace_source)

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
forbid_text(integration_source "levelPanel_" "legacy Level panel dependency")
forbid_text(integration_source "screenPanel_" "legacy Screen panel dependency")

# The fixed native shell matches the approved concept hierarchy. Canvas zoom
# is never allowed to scale the rail, top bar, Inspector, controls or overview.
require_text(layout_source "constexpr float topBarHeight = 70.0f;" "fixed top application bar")
require_text(layout_source "constexpr float railWidth = 96.0f;" "fixed left navigation rail")
require_text(layout_source "width * 0.185f, 280.0f, 336.0f" "bounded responsive Inspector")
require_text(layout_source "layout.storyOverview" "fixed story overview host")
require_text(chrome_source "Select, Arrange, Filter, Search, Preview, Validate" "complete Journey toolbar actions")
require_text(chrome_source "Undo, Redo, ProjectSelector, Settings, MainMenu" "complete Journey utility actions")
require_text(chrome_source "Hub, StoryFlow, Levels, Screens, Assets, Variables, TestPlay" "complete Journey rail actions")
require_text(render_path_source "journeyChrome_.CanvasOverlayOwnsPointer" "fixed chrome input ownership")
require_text(render_path_source "workspace_.FindAndFocusJourneyNode(findDraft_)" "Journey-native search/focus")
require_text(render_path_source "graphViewButton_.SetVisible(active);" "restored Graph editor access")
require_text(render_path_source "workspace_.ActivateView(bridge::StoryFlowViewMode::Graph)" "real Graph view activation")

# Main cards are neutral rounded image surfaces with shadow. Validation is a
# footer state mark; only selection may colour the frame blue.
require_text(card_source "image-led, rounded and shadowed" "approved card presentation")
require_text(card_source "selected_ ? SelectionBlue : Border" "selection-only blue frame")
require_text(card_source "bounds.y + 5.0f" "card drop shadow")
require_text(card_source "image.drawRect = sourceRect" "thumbnail cover crop inside rounded card")
forbid_text(card_source "TypeColor" "type-coloured card frames")

# Alternate role colour is owned by lanes and shared with Inspector bullets.
require_text(lane_source "mainTrack_ ? Border : roleColour" "neutral main lane and role-coloured branch lanes")
require_text(role_source "One classification seam drives both branch-lane accents and Inspector" "shared role colour authority")
require_text(workspace_source "JourneyRoleColor(role)" "Inspector/overview role colour consumption")
require_text(workspace_source "JourneyCanvasScissorRect" "hard Journey-to-Inspector render boundary")
require_text(workspace_source "object.scissorRect = journeyClip;" "card viewport clipping")
require_text(workspace_source "ApplyScissor(canvas, scissorRect, cmd);" "fixed overlay scissor restoration")

# Semantic zoom remains readable and obsolete tiny persisted layouts migrate.
require_text(workspace_source "constexpr float MinZoom = 0.82f;" "readable semantic minimum zoom")
require_text(workspace_source "layout_->journeyCanvas.zoom = 1.0f;" "legacy tiny-zoom migration")
require_text(workspace_source "layout_->activeView = bridge::StoryFlowViewMode::Journey;" "Journey-default recovery surface")
require_text(chrome_source "zoomRequested_(0.82f + fraction * (1.18f - 0.82f))" "functional fixed zoom slider")
forbid_text(workspace_source "constexpr float MinZoom = 0.20f;" "legacy unreadable zoom range")

# Journey authors and rewires exits in the Inspector without switching to the
# frozen Graph surface or changing stable route identity.
require_text(workspace_source "RewireJourneyExit" "Inspector route destination authoring")
require_text(workspace_source "pendingJourneyExitIndex_ = index;" "deferred Inspector rewire intent")
require_text(workspace_source "session_->UpdateRoute(routeId, std::move(replacement), error)" "stable-ID Journey rewire")
require_text(workspace_source "AddJourneyAction" "Inspector action authoring")
require_text(workspace_source "addJourneyActionButton_.SetVisible(journeyMode && nodeSelected)" "visible Journey Add Action control")
require_text(workspace_source "journeyMode && journeyAddActionAvailable_" "honest Journey Add Action enablement")
require_text(workspace_source "journeyAddActionAvailable_ = source->outgoingRouteIds.empty();" "single Game Start entry enforcement")
require_text(workspace_source "usedOutcomes.find(outcome) ==" "unused Screen action availability")
require_text(workspace_source "session_->AddRoute(std::move(route), createdRouteId, error)" "governed Journey route creation")
forbid_text(workspace_source "JOURNEY EXIT // OPENED IN GRAPH" "Journey-to-Graph exit routing")

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
