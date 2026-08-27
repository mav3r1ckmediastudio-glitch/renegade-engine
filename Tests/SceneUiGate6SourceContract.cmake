if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(ROOT_CMAKE "${RENEGADE_SOURCE_DIR}/CMakeLists.txt")
set(STORY_FLOW_INTEGRATION
    "${RENEGADE_SOURCE_DIR}/Studio/src/StoryFlowStudioIntegration.h")
set(SCREEN_WORKSPACE
    "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeScreenEditorWorkspace.cpp")
set(STUDIO_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")
set(DRAG_PREVIEW
    "${RENEGADE_SOURCE_DIR}/Studio/src/CreatorAssetDragPreview.cpp")
set(STUDIO_CHROME
    "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioChrome.cpp")
set(BUILD_CONTROLLER
    "${RENEGADE_SOURCE_DIR}/Studio/src/WindowsGameBuildController.cpp")
set(GATE_DOCUMENT
    "${RENEGADE_SOURCE_DIR}/docs/SCENE_UI_GATE6_CONSOLIDATED_ACCEPTANCE.md")

foreach(path IN ITEMS
    "${ROOT_CMAKE}"
    "${STORY_FLOW_INTEGRATION}"
    "${SCREEN_WORKSPACE}"
    "${STUDIO_SOURCE}"
    "${DRAG_PREVIEW}"
    "${STUDIO_CHROME}"
    "${BUILD_CONTROLLER}"
    "${GATE_DOCUMENT}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR
            "Scene UI Gate 6 source contract input is missing: ${path}")
    endif()
endforeach()

foreach(gate IN ITEMS 2 3 4 5)
    if(NOT EXISTS "${RENEGADE_SOURCE_DIR}/Tests/SceneUiGate${gate}.cmake")
        message(FATAL_ERROR
            "Scene UI Gate ${gate} registration was removed")
    endif()
    if(NOT EXISTS
        "${RENEGADE_SOURCE_DIR}/Tests/SceneUiGate${gate}SourceContract.cmake")
        message(FATAL_ERROR
            "Scene UI Gate ${gate} source contract was removed")
    endif()
endforeach()

file(READ "${ROOT_CMAKE}" root_cmake)
file(READ "${STORY_FLOW_INTEGRATION}" story_flow_integration)
file(READ "${SCREEN_WORKSPACE}" screen_workspace)
file(READ "${STUDIO_SOURCE}" studio_source)
file(READ "${DRAG_PREVIEW}" drag_preview)
file(READ "${STUDIO_CHROME}" studio_chrome)
file(READ "${BUILD_CONTROLLER}" build_controller)
file(READ "${GATE_DOCUMENT}" gate_document)

function(require_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "Scene UI Gate 6 contract missing ${description}: ${needle}")
    endif()
endfunction()

# Gate 6 is a consolidation lock: the earlier accepted Scene contracts must
# remain registered alongside this final cross-workspace contract.
foreach(gate IN ITEMS 2 3 4 5 6)
    require_text(root_cmake
        "include(Tests/SceneUiGate${gate}.cmake)"
        "Scene UI Gate ${gate} test registration")
endforeach()

# First-class workspace transitions must remain explicit. The Screen editor
# must continue to refuse a return that would silently discard authored state.
require_text(story_flow_integration
    "void RequestStoryFlow() noexcept"
    "Story Flow return boundary")
require_text(story_flow_integration
    "void RequestLevelEditor() noexcept"
    "Level Editor activation boundary")
require_text(story_flow_integration
    "void RequestScreenEditor() noexcept"
    "Screen Editor activation boundary")
require_text(screen_workspace
    "RETURN BLOCKED // SAVE OR UNDO SCREEN CHANGES FIRST"
    "dirty Screen return guard")

# Scene persistence and governed asset placement remain command/lifecycle owned.
require_text(studio_source
    "SaveSceneAfterTransientCleanup"
    "transient-safe Scene save boundary")
require_text(studio_source
    "void StudioRenderPath::ReopenScene()"
    "Scene reopen boundary")
require_text(drag_preview
    "wi::enums::FILTER_OBJECT_ALL | wi::enums::FILTER_TERRAIN"
    "object and terrain placement surface picking")
require_text(drag_preview
    "PlaceReusableModelCommand"
    "command-owned reusable asset placement")

# Environment/Terrain remain independent command-owned workspaces.
require_text(studio_source
    "std::make_unique<bridge::CreateEnvironmentCommand>"
    "Environment creation command")
require_text(studio_source
    "std::make_unique<bridge::CreateTerrainCommand>"
    "Terrain creation command")
require_text(studio_source
    "std::make_unique<bridge::ExpandTerrainCommand>"
    "non-destructive terrain expansion command")
require_text(studio_chrome
    "SetEnvironmentWorkspaceActive"
    "Environment workspace presentation")
require_text(studio_chrome
    "SetTerrainWorkspaceActive"
    "Terrain workspace presentation")

# The consolidated pass includes both editor-to-Runtime paths and the accepted
# standalone build workflow; none may disappear behind a green Studio-only job.
require_text(studio_source
    "void StudioRenderPath::StartTestLevel()"
    "unsaved Test Level Runtime path")
require_text(studio_source
    "void StudioRenderPath::StartProjectPlay()"
    "Story Flow Runtime preview path")
require_text(build_controller
    "WindowsGameBuildWorkflow"
    "standalone Windows build workflow")

# Keep the owner-test boundary explicit and resistant to being reduced to CI.
foreach(resolution IN ITEMS "1280x720" "1680x945" "1920x1080")
    require_text(gate_document "${resolution}"
        "${resolution} owner Release acceptance")
endforeach()
require_text(gate_document
    "Owner Release acceptance overrides green CI"
    "visual and behavioural authority rule")
require_text(gate_document
    "Horizon visibility, terrain/grid datum alignment and +/- terrain-ring UI"
    "deferred editor-polish boundary")

message(STATUS
    "Scene UI Gate 6 consolidated whole-editor source contract passed")
