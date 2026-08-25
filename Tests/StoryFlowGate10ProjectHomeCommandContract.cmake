if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowJourneyChrome.h" chrome)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowRenderPath.h" render_path)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/StoryFlowStudioIntegration.h" integration)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.h" application)
file(READ "${RENEGADE_SOURCE_DIR}/EngineBridge/src/BuildVerificationService.cpp" build_verification)

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

# The project home must own runtime lifecycle and build dispatch. Reintroducing
# the old inactive-Level-Editor pending action would recreate the owner's dead
# TEST GAME control despite green compilation.
require_contains(integration "StartProjectPlayFromStoryFlowNow" "direct project runtime start")
require_contains(integration "PollProjectPlayFromStoryFlow" "project runtime polling")
require_contains(integration "StopProjectPlayFromStoryFlowNow" "project runtime stop")
require_contains(integration "BuildActiveWindowsGame" "project-home Windows build dispatch")
require_not_contains(integration "RequestProjectPlayFromStoryFlow();" "inactive Level Editor play queue")

require_contains(application "StartProjectPlayFromStoryFlowNow" "Studio runtime lifecycle seam")
require_contains(application "PollProjectPlayFromStoryFlow" "Studio runtime poll seam")
require_contains(application "StopProjectPlayFromStoryFlowNow" "Studio runtime stop seam")
require_contains(application "IsProjectPlayFromStoryFlowActive" "Studio runtime active-state seam")

# Gate 10 packaged verification must preserve legacy startup-Screen proof while
# accepting modern Flow-native smoke only when it explicitly proves there was
# no fabricated legacy Screen/action and still reaches the exact Complete Game
# Flow terminal. This is the owner-reported build failure regression.
require_contains(build_verification "const bool legacyStartupScreen" "runtime entry-mode split")
require_contains(build_verification "\"screen_was_loaded\", \"true\"" "legacy startup Screen evidence")
require_contains(build_verification "\"last_action_id\", \"play\"" "legacy Play action evidence")
require_contains(build_verification "\"screen_was_loaded\", \"false\"" "Flow-native no-screen evidence")
require_contains(build_verification "\"last_action_id\", \"\"" "Flow-native no fabricated action")
require_contains(build_verification "Gate 4 Flow-native Runtime evidence does not prove a governed startup Story Flow." "Flow-native startup Flow proof")
require_contains(build_verification "runtime_entry_mode" "build report runtime entry mode")
require_contains(build_verification "story_flow_native" "Flow-native build report value")

message(STATUS "Gate 10 Story Flow project-home command contract passed")
