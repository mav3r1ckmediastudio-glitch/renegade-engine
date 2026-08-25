if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(STUDIO_HEADER "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.h")
set(STUDIO_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")
set(STUDIO_CHROME "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioChrome.cpp")
set(STORY_FLOW_INTEGRATION "${RENEGADE_SOURCE_DIR}/Studio/src/StoryFlowStudioIntegration.h")

foreach(path IN ITEMS
    "${STUDIO_HEADER}"
    "${STUDIO_SOURCE}"
    "${STUDIO_CHROME}"
    "${STORY_FLOW_INTEGRATION}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Scene UI Gate 2 source contract input is missing: ${path}")
    endif()
endforeach()

file(READ "${STUDIO_HEADER}" studio_header_source)
file(READ "${STUDIO_SOURCE}" studio_source)
file(READ "${STUDIO_CHROME}" studio_chrome_source)
file(READ "${STORY_FLOW_INTEGRATION}" story_flow_integration_source)

function(require_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Scene UI Gate 2 contract missing ${description}: ${needle}")
    endif()
endfunction()

function(forbid_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Scene UI Gate 2 contract regressed ${description}: ${needle}")
    endif()
endfunction()

# Wicked renders top-level wiGUI widgets in reverse registration order. The
# Story Flow return control is attached after Studio construction, therefore the
# Level Editor must deliberately re-register the Scene chrome after the control
# so the control paints and receives input above the chrome.
require_text(studio_header_source
    "void RegisterStoryFlowLifecycleControl(wi::gui::Widget& control)"
    "owned lifecycle-control registration seam")
require_text(studio_header_source
    "gui.AddWidget(&control);\n            gui.RemoveWidget(&studioChrome_);\n            gui.AddWidget(&studioChrome_);"
    "reverse-order lifecycle layering")
require_text(story_flow_integration_source
    "levelEditor.RegisterStoryFlowLifecycleControl(\n                returnToStoryFlowButton_);"
    "Story Flow return registration through the Level Editor seam")
forbid_text(story_flow_integration_source
    "levelEditor.StoryFlowGui().AddWidget(&returnToStoryFlowButton_);"
    "direct late registration behind Scene chrome")

# Return-to-StoryFlow belongs in the scene-tab strip, not the viewport chip row.
# Keep it right-aligned before the Inspector and clear of the 230 px active tab.
require_text(story_flow_integration_source
    "constexpr float ReturnButtonWidth = 132.0f;"
    "return-control width")
require_text(story_flow_integration_source
    "constexpr float SceneTabSafeOffset = 242.0f;"
    "scene-tab collision boundary")
require_text(story_flow_integration_source
    "bounds.z - ReturnButtonWidth - 12.0f"
    "right-aligned return control")
require_text(story_flow_integration_source
    "bounds.y - 31.0f"
    "scene-tab-strip vertical placement")
forbid_text(story_flow_integration_source
    "bounds.x + 12.0f,\n                bounds.y + 12.0f"
    "return control overlapping PERSPECTIVE viewport chip")

# The shell clamps preserve at least a 420 px central authoring surface. Prove
# the return control remains clear of the 230 px scene tab and inside the
# viewport-right boundary at that minimum and at the accepted audit widths:
# 1280 -> 710 px centre, 1680 -> 1000 px, 1920 -> 1240 px.
foreach(viewport_width IN ITEMS 420 710 1000 1240)
    math(EXPR right_aligned_x "${viewport_width} - 132 - 12")
    if(right_aligned_x LESS 242)
        set(return_x 242)
    else()
        set(return_x ${right_aligned_x})
    endif()
    math(EXPR return_right "${return_x} + 132")
    if(return_x LESS 242 OR return_right GREATER viewport_width)
        message(FATAL_ERROR
            "Scene UI Gate 2 return-control geometry escapes a ${viewport_width}px viewport")
    endif()
endforeach()

# Level/Story Flow workspace ownership must stay explicit on both sides of the
# post-frame render-path switch. The Level frame owns only the return control;
# Story Flow owns its own workspace and disables Level-only lifecycle controls.
require_text(story_flow_integration_source
    "if (desiredWorkspace_ == Workspace::LevelEditor)\n            {\n                FlushLayout(true);\n                storyFlow.SetWorkspaceActive(false);\n                SetContentControlsActive(false, !activeLevelNodeId_.empty());\n                EnsureActive(application, levelEditor);"
    "Level Editor transition ownership")
require_text(story_flow_integration_source
    "storyFlow.SetWorkspaceActive(true);\n            SetContentControlsActive(true, false);\n            EnsureActive(application, storyFlow);"
    "Story Flow transition ownership")

# Historical Environment/Terrain panel contamination must remain impossible.
# Each specialist workspace explicitly deactivates the other and synchronizes
# both chrome flags before refreshing the Inspector.
require_text(studio_source
    "case EditorAction::OpenEnvironmentWorkspace:\n            SetEnvironmentWorkspaceActive(true);\n            break;\n        case EditorAction::OpenTerrainWorkspace:\n            SetTerrainWorkspaceActive(true);"
    "specialist workspace actions routed through the exclusive setters")
require_text(studio_source
    "case EditorAction::OpenSceneWorkspace:\n            SetEnvironmentWorkspaceActive(false);\n            SetTerrainWorkspaceActive(false);"
    "Scene workspace clears both specialist modes")
require_text(studio_source
    "if (environmentWorkspaceActive_)\n        {\n            terrainWorkspaceActive_ = false;\n        }"
    "Environment-to-Terrain mutual exclusion")
require_text(studio_source
    "if (terrainWorkspaceActive_)\n        {\n            environmentWorkspaceActive_ = false;\n        }"
    "Terrain-to-Environment mutual exclusion")
require_text(studio_source
    "studioChrome_.SetEnvironmentWorkspaceActive(\n            environmentWorkspaceActive_);"
    "Environment chrome synchronization")
require_text(studio_source
    "studioChrome_.SetTerrainWorkspaceActive(terrainWorkspaceActive_);"
    "Terrain chrome synchronization")

# Inspector component ownership follows the active specialist workspace rather
# than the previous selection. This is the key guard against the historical
# Environment + Terrain lists appearing together: Terrain selects its terrain
# component (or INVALID when none exists), while Environment selects weather.
require_text(studio_source
    "const auto entity = environmentWorkspaceActive_\n            ? EditableWeatherEntity()\n            : terrainWorkspaceActive_\n                ? terrainWorkspaceEntity\n                : selectedEntity;"
    "specialist Inspector entity arbitration")
require_text(studio_source
    "const auto setEnvironmentVisible ="
    "dedicated Environment Inspector visibility group")
require_text(studio_source
    "widget.SetVisible(hasWeather);"
    "Environment Inspector visibility owner")
require_text(studio_source
    "const auto setTerrainVisible = [this, hasTerrain](wi::gui::Widget& widget)"
    "dedicated Terrain Inspector visibility group")
require_text(studio_source
    "widget.SetVisible(terrainWorkspaceActive_ && hasTerrain);"
    "Terrain Inspector visibility owner")
require_text(studio_source
    "createTerrainButton_.SetVisible(\n            hasSession && terrainWorkspaceActive_ && !hasTerrain);"
    "Terrain-empty-state ownership without Environment leakage")

# Scene shell bounds remain owned by the resizable Renegade chrome, with a
# minimum central authoring area and enough drawer height for complete cards.
require_text(studio_chrome_source
    "const float availableWidth = std::max(760.0f, width_);"
    "minimum shell width clamp")
require_text(studio_chrome_source
    "availableWidth - hierarchyWidth_ - 420.0f"
    "minimum central Scene width")
require_text(studio_chrome_source
    "constexpr float MinimumDrawerHeight = 200.0f;"
    "minimum usable drawer height")
require_text(studio_source
    "viewportBounds_ = studioChrome_.ViewportBounds();"
    "single Scene viewport-bounds authority")
require_text(studio_source
    "if (studioChrome_.ConsumedPointerThisFrame())"
    "chrome-to-viewport input cutoff")

message(STATUS "Scene UI Gate 2 shell and workspace isolation contract passed")
