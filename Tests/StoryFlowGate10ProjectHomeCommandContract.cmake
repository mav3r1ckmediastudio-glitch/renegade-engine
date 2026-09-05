if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowJourneyChrome.h" chrome)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioChrome.cpp" scene_chrome)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowRenderPath.h" render_path)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowWorkspace.cpp" workspace)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/StoryFlowStudioIntegration.h" integration)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.h" application)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp" application_source)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/WindowsGameBuildController.cpp" build_controller)
file(READ "${RENEGADE_SOURCE_DIR}/EngineBridge/src/BuildVerificationService.cpp" build_verification)
file(READ "${RENEGADE_SOURCE_DIR}/EngineBridge/src/WindowsGameBuildProjectService.cpp" build_project)

function(require_contains haystack needle label)
    string(FIND "${${haystack}}" "${needle}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Gate 10 project-home command contract missing ${label}: ${needle}")
    endif()
endfunction()

function(require_not_contains haystack needle label)
    string(FIND "${${haystack}}" "${needle}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "Gate 10 project-home command contract still contains ${label}: ${needle}")
    endif()
endfunction()

# Creator-facing project-home commands must be distinct and honestly named.
require_contains(chrome "TEST GAME" "Test Game label")
require_contains(chrome "BUILD GAME" "Build Game label")
require_contains(chrome "Action::BuildGame" "Build Game action wiring")
require_not_contains(chrome "\"PREVIEW\"" "duplicate Preview toolbar label")

# Test Game and Build Game must remain separate save-first commands.
require_contains(render_path "NativeCommand::TestGame" "Test Game native command")
require_contains(render_path "NativeCommand::BuildGame" "Build Game native command")
require_contains(render_path "TEST GAME BLOCKED // STORY FLOW SAVE FAILED" "Test Game save failure")
require_contains(render_path "BUILD GAME BLOCKED // STORY FLOW SAVE FAILED" "Build Game save failure")

# Final Gate 10 UX recovery: STORYFLOW owns an explicit save command in both
# views and Ctrl+S reaches that same transactional SaveJourney seam.
require_contains(render_path "Story Flow Save" "manual StoryFlow Save control")
require_contains(render_path "NativeCommand::Save" "manual StoryFlow Save command")
require_contains(render_path "Shortcut: Ctrl+S" "Save shortcut discoverability")
require_contains(render_path "KEYBOARD_BUTTON_LCONTROL" "Ctrl+S control modifier")
require_contains(render_path "static_cast<wi::input::BUTTON>('S')" "Ctrl+S S key")
require_contains(render_path "workspace_.SaveJourney();" "manual transactional save dispatch")

# Terminal destinations must be creatable without leaving Journey View.
require_contains(render_path "Story Flow Journey Add Complete Game" "Journey Complete Game control")
require_contains(render_path "Story Flow Journey Add Return Menu" "Journey Return Menu control")
require_contains(render_path "Story Flow Journey Add Quit" "Journey Quit control")
require_contains(render_path "workspace_.AddJourneyTerminal" "Journey terminal authoring dispatch")
require_contains(workspace "void RenegadeStoryFlowWorkspace::AddJourneyTerminal" "Journey terminal implementation")
require_contains(workspace "if (!session_ || !layout_) return;" "view-neutral terminal authoring guard")
require_not_contains(workspace "layout_->activeView != bridge::StoryFlowViewMode::Graph" "Graph-only terminal restriction")

# Inspector validation must use the same Screen outcome parity authority as
# Runtime rather than reporting green while Runtime rejects an unrouted action.
require_contains(workspace "StoryFlowScreenReferenceService" "Runtime-parity Screen validator")
require_contains(workspace "AuditScreenOutcomes" "Runtime-parity outcome audit")
require_contains(workspace "runtimeValidationReady_" "Runtime readiness state")
require_contains(workspace "VALIDATION FAILED //" "visible Runtime-readiness failure")
require_contains(workspace "Runtime Ready" "honest green Runtime-ready state")
require_contains(workspace "ResolveStoryFlowRuntimeRoute" "deterministic Runtime route validation")

# The Graph Inspector must reserve a dedicated row beneath stable/document IDs
# before node action buttons, preventing the owner-observed SCENE/APPLY overlap.
require_contains(workspace "const float nodeY0" "Graph node-control row")
require_contains(workspace "HeaderHeight + 232.0f" "Graph node-control clearance")
require_contains(workspace "const float routeY0" "separate Graph route-control row")

# Build Game must expose a dedicated responsive progress window driven by real
# workflow milestones while leaving EngineBridge build semantics untouched.
require_contains(build_controller "class WindowsBuildProgressWindow" "dedicated build progress window")
require_contains(build_controller "VALIDATING STORYFLOW" "StoryFlow validation build stage")
require_contains(build_controller "RESOLVING DEPENDENCIES" "dependency build stage")
require_contains(build_controller "STAGING PACKAGE INPUTS" "package staging build stage")
require_contains(build_controller "VALIDATING PACKAGED RUNTIME" "packaged Runtime build stage")
require_contains(build_controller "BUILD COMPLETE" "successful build completion state")
require_contains(build_controller "BUILD FAILED" "failed build completion state")
require_contains(build_controller "progress.Complete(true" "successful progress completion")
require_contains(build_controller "progress.Complete(false" "failed progress completion")
require_contains(build_controller "renegade-terrain-default-grass-basecolor" "bundled Terrain base-colour support")
require_contains(build_controller "renegade-terrain-default-grass-normal" "bundled Terrain normal support")
require_contains(build_controller "renegade-terrain-default-grass-surface" "bundled Terrain surface support")
require_contains(build_controller "renegade-weather-snowflake" "bundled weather support")
require_contains(build_controller "projectState.bundledResources" "validated bundled resource staging handoff")
require_contains(build_project "WindowsGameBundledResource" "exact bundled Runtime resource policy")
require_contains(build_project "fs::equivalent" "filesystem-identity bundled resource admission")

# The project home must own runtime lifecycle and build dispatch. Reintroducing
# the old inactive-Level-Editor pending action would recreate the owner's dead
# TEST GAME control despite green compilation.
require_contains(integration "StartProjectPlayFromStoryFlowNow" "direct project runtime start")
require_contains(integration "PollProjectPlayFromStoryFlow" "project runtime polling")
require_contains(integration "StopProjectPlayFromStoryFlowNow" "project runtime stop")
require_contains(integration "BuildActiveWindowsGame" "project-home Windows build dispatch")
require_contains(integration "ConsumeWindowsGameBuildRequest" "Scene Editor build request handoff")
require_contains(integration "authoringSession_.Save(saveError)" "dirty StoryFlow save before Scene build")
require_contains(integration "levelEditor.RequestWindowsGameBuild();" "shared save-first Story Flow build entry")
require_not_contains(integration "RequestProjectPlayFromStoryFlow();" "inactive Level Editor play queue")

require_contains(application "StartProjectPlayFromStoryFlowNow" "Studio runtime lifecycle seam")
require_contains(application "PollProjectPlayFromStoryFlow" "Studio runtime poll seam")
require_contains(application "StopProjectPlayFromStoryFlowNow" "Studio runtime stop seam")
require_contains(application "IsProjectPlayFromStoryFlowActive" "Studio runtime active-state seam")
require_contains(application "BuildWindowsGame" "Scene Editor build action")
require_contains(application_source "SaveSceneAfterTransientCleanup(scenePath, finishScenePreparation)" "dirty Scene save before build")
require_contains(application_source "windowsGameBuildRequested_ = true" "saved Scene build handoff")
require_not_contains(scene_chrome "BuildActiveWindowsGame" "direct stale-disk Scene chrome build")

# Gate 10 packaged verification must preserve legacy startup-Screen proof while
# accepting modern Flow-native smoke only when it explicitly proves there was
# no fabricated legacy Screen/action and still reaches the exact Complete Game
# Flow terminal. The immutable project startup_screen_id, not mutable resolved
# startup_screen state, discriminates the two contracts.
require_contains(build_verification "evidence.find(\"startup_screen_id\")" "immutable startup Screen identity discriminator")
require_contains(build_verification "const bool legacyStartupScreen" "runtime entry-mode split")
require_contains(build_verification "\"screen_was_loaded\", \"true\"" "legacy startup Screen evidence")
require_contains(build_verification "\"last_action_id\", \"play\"" "legacy Play action evidence")
require_contains(build_verification "evidence.find(\"startup_flow_id\")" "Flow-native startup Flow identity")
require_contains(build_verification "flowDocumentId->second != startupFlowId->second" "Flow document identity parity")
require_contains(build_verification "\"screen_was_loaded\", \"false\"" "Flow-native no-screen evidence")
require_contains(build_verification "\"last_action_id\", \"\"" "Flow-native no fabricated action")
require_contains(build_verification "Gate 4 Flow-native Runtime evidence does not prove the governed startup Story Flow identity." "Flow-native startup Flow proof")
require_contains(build_verification "runtime_entry_mode" "build report runtime entry mode")
require_contains(build_verification "story_flow_native" "Flow-native build report value")

message(STATUS "Gate 10 Story Flow project-home command contract passed")
