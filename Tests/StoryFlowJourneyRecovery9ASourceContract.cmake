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
set(INSPECTOR_TEXT "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowInspectorText.h")
set(STUDIO_CHROME "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioChrome.cpp")
set(PROJECT_HUB "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeProjectHub.cpp")
set(STUDIO_APPLICATION "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")
set(STUDIO_CMAKE "${RENEGADE_SOURCE_DIR}/Studio/CMakeLists.txt")
set(BRAND_LOGO "${RENEGADE_SOURCE_DIR}/Studio/assets/renegade-engine-fractured-crest-logo.png")
set(LEGACY_LEVEL "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowLevelPanel.h")
set(LEGACY_SCREEN "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowScreenPanel.h")

foreach(path IN ITEMS "${RENDER_PATH}" "${INTEGRATION}" "${COMPOSER}"
        "${CHROME}" "${LAYOUT}" "${CARD}" "${LANE}" "${ROLE}" "${WORKSPACE}"
        "${INSPECTOR_TEXT}"
        "${STUDIO_CHROME}" "${PROJECT_HUB}" "${STUDIO_APPLICATION}"
        "${STUDIO_CMAKE}" "${BRAND_LOGO}")
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
file(READ "${INSPECTOR_TEXT}" inspector_text_source)
file(READ "${STUDIO_CHROME}" studio_chrome_source)
file(READ "${PROJECT_HUB}" project_hub_source)
file(READ "${STUDIO_APPLICATION}" studio_application_source)
file(READ "${STUDIO_CMAKE}" studio_cmake_source)

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
require_text(chrome_source "Select, Arrange, Filter, Search, BuildGame, Validate" "complete Journey toolbar actions")
require_text(chrome_source "Undo, Redo, ProjectSelector, Settings, MainMenu" "complete Journey utility actions")
require_text(chrome_source "Hub, StoryFlow, Levels, Screens, Assets, Variables, TestPlay" "complete Journey rail actions")
foreach(action IN ITEMS Hub StoryFlow Levels Screens Assets Variables TestPlay
        Select Arrange Filter Search BuildGame Validate Undo Redo ProjectSelector
        Settings MainMenu ZoomOut ZoomIn Fit Start)
    require_text(render_path_source "case Action::${action}:" "routed ${action} shell action")
endforeach()
require_text(chrome_source "shell_.inspector.y + 14.0f, 13, TextStrong" "readable Inspector title")
forbid_text(chrome_source "Text(\"x\", shell_.inspector.Right()" "decorative unwired Inspector close control")
require_text(render_path_source "journeyChrome_.CanvasOverlayOwnsPointer" "fixed chrome input ownership")
require_text(render_path_source "workspace_.FindAndFocusJourneyNode(findDraft_)" "Journey-native search/focus")
require_text(render_path_source "graphViewButton_.SetVisible(active);" "restored Graph editor access")
require_text(render_path_source "workspace_.ActivateView(bridge::StoryFlowViewMode::Graph)" "real Graph view activation")

# The owner-supplied transparent fractured-crest logo is the sole packaged
# header brand. All consumers preserve aspect ratio and use alpha blending;
# the superseded additive wordmark must not return.
file(SHA256 "${BRAND_LOGO}" brand_logo_sha256)
if(NOT brand_logo_sha256 STREQUAL "9acc347e3e46602142ec9cdceeb846d3eb96fddac0b07d10ebc33a0a912e2a05")
    message(FATAL_ERROR "Journey recovery 9F authoritative brand logo hash changed: ${brand_logo_sha256}")
endif()
require_text(chrome_source "renegade-engine-fractured-crest-logo.png" "Journey fractured-crest logo")
require_text(chrome_source "logo.blendFlag = wi::enums::BLENDMODE_ALPHA;" "Journey logo alpha blending")
require_text(chrome_source "drawWidth = drawHeight * aspect;" "Journey logo aspect preservation")
require_text(studio_chrome_source "renegade-engine-fractured-crest-logo.png" "Studio fractured-crest logo")
require_text(studio_chrome_source "logo.blendFlag = wi::enums::BLENDMODE_ALPHA;" "Studio logo alpha blending")
require_text(project_hub_source "renegade-engine-fractured-crest-logo.png" "Project Hub fractured-crest logo")
require_text(studio_cmake_source "Content/ui/renegade-engine-fractured-crest-logo.png" "packaged fractured-crest logo")
forbid_text(chrome_source "renegade-engine-wordmark.png" "superseded Journey wordmark")
forbid_text(studio_chrome_source "renegade-engine-wordmark.png" "superseded Studio wordmark")
forbid_text(project_hub_source "renegade-engine-wordmark.png" "superseded Project Hub wordmark")
forbid_text(studio_cmake_source "renegade-engine-wordmark.png" "superseded packaged wordmark")

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

# The real Inspector must remain readable and must not surface decorative
# pseudo-tabs. Manual native controls are each rendered from a restored
# workspace scissor so a preceding TextInput cannot clip Add Action/combos.
require_text(workspace_source "DESTINATION DETAILS" "readable Inspector details hierarchy")
forbid_text(workspace_source "\"i\", \"=\", \"IMG\", \">\", \"</>\"" "non-functional Inspector pseudo-tabs")
require_text(workspace_source "preceding field can never clip Add Action" "per-control Inspector scissor restoration")
require_text(workspace_source "nodeNameInput_.SetRenderTextSize(12)" "readable Inspector display-name control")
require_text(workspace_source "addJourneyActionButton_.SetRenderTextSize(11)" "readable Journey Add Action control")
require_text(workspace_source "No unused authored Screen actions available." "honest disabled Screen action reason")
require_text(workspace_source "addJourneyActionButton_.SetText(\"TERMINAL\")" "terminal Add Action reason")
require_text(workspace_source "addJourneyActionButton_.SetText(\"LIMIT REACHED\")" "exit-capacity Add Action reason")
require_text(workspace_source "addJourneyActionButton_.SetText(\"ENTRY SET\")" "Game Start Add Action reason")
require_text(workspace_source "addJourneyActionButton_.SetText(\"NO ACTIONS\")" "Screen Add Action reason")
require_text(chrome_source "commandBounds_[i].y + 39.0f, 8" "readable top-command labels")
require_text(chrome_source "bounds.y + 7.0f, 8, TextSecondary" "readable Story Overview title")
require_text(chrome_source "Text(\"N/A\", settingsBounds_" "visibly unavailable Settings utility")
require_text(chrome_source "Text(\"N/A\", menuBounds_" "visibly unavailable Main Menu utility")
require_text(chrome_source "action_(Action::Settings);" "Settings disabled-reason routing")
require_text(chrome_source "action_(Action::MainMenu);" "Main Menu disabled-reason routing")
require_text(chrome_source "const bool available = !(graphViewActive_ && i == 2);" "Graph-safe Journey Filter disablement")
require_text(render_path_source "FILTER UNAVAILABLE // JOURNEY VIEW ONLY" "Graph Filter disabled reason")
require_text(workspace_source "Unreachable from Game Start:" "human-readable unreachable diagnostic")
require_text(workspace_source "constexpr std::size_t modelCapacity = 2;" "non-overlapping validation diagnostic capacity")
require_text(inspector_text_source "WrapInspectorText" "complete Inspector message wrapping")
require_text(inspector_text_source "word.size() > limit" "long Inspector identifier wrapping")
require_text(inspector_text_source "ComputeInspectorMessageLayout" "non-overlapping Inspector message layout")
require_text(workspace_source "for (const auto& line : validationLines)" "wrapped Validation rendering")
require_text(workspace_source "for (std::size_t i = 0; i < statusLines.size(); ++i)" "wrapped Status rendering")
require_text(workspace_source "const auto messageLayout = ComputeInspectorMessageLayout(" "raised wrapped Status placement")
forbid_text(workspace_source "Label(Shorten(ReadableStatus(statusMessage_)" "truncated Inspector Status message")
forbid_text(workspace_source "Label(Shorten(std::move(message), 48)" "truncated Validation message")
forbid_text(workspace_source "diagnostic.code + \" // \" + diagnostic.message" "raw internal diagnostic codes in Inspector")

# Gate 10 recovery keeps project actions fail-closed and saved-first. TEST GAME
# launches the governed project descriptor from Story Flow itself; BUILD GAME is
# a distinct project-home command using the existing governed build controller.
require_text(render_path_source "workspace_.SaveJourney();" "project command save-before-launch/build")
require_text(render_path_source "TEST GAME BLOCKED // STORY FLOW SAVE FAILED" "Test Game fail-closed status")
require_text(render_path_source "BUILD GAME BLOCKED // STORY FLOW SAVE FAILED" "Build Game fail-closed status")
require_text(integration_source "StartProjectPlayFromStoryFlowNow" "direct Story Flow Runtime launch")
require_text(integration_source "PollProjectPlayFromStoryFlow" "Story Flow Runtime lifecycle polling")
require_text(integration_source "BuildActiveWindowsGame" "Story Flow governed Windows build")
require_text(studio_application_source "options.ownsSnapshot = false;" "snapshot-free Story Flow Test Game")
require_text(studio_application_source "project.descriptorPath" "project-descriptor Test Game launch")
require_text(studio_application_source "bridge::TestLevelSnapshot noSnapshot;" "empty Test Game snapshot handoff")
forbid_text(integration_source "RequestProjectPlayFromStoryFlow();" "inactive Level Editor project-play queue")
forbid_text(integration_source "RequestTestLevelSnapshotFromStoryFlow" "Story Flow LP04 snapshot launch")

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