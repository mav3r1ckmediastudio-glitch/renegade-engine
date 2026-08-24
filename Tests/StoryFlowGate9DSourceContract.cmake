if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(WORKSPACE "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowWorkspace.cpp")
set(RENDER_PATH "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowRenderPath.h")
set(GRAPH_EDITOR "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowGraphEditor.h")
set(GRAPH_RECOVERY "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStoryFlowGate9COwnerRecovery.cpp")
set(PROJECT_LOADING_OVERLAY "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeProjectLoadingOverlay.cpp")

foreach(path IN ITEMS "${WORKSPACE}" "${RENDER_PATH}" "${GRAPH_EDITOR}" "${GRAPH_RECOVERY}" "${PROJECT_LOADING_OVERLAY}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Gate 9D source contract input is missing: ${path}")
    endif()
endforeach()

file(READ "${WORKSPACE}" workspace_source)
file(READ "${RENDER_PATH}" render_path_source)
file(READ "${GRAPH_EDITOR}" graph_editor_source)
file(READ "${GRAPH_RECOVERY}" graph_recovery_source)
file(READ "${PROJECT_LOADING_OVERLAY}" project_loading_overlay_source)

function(require_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 9D contract missing ${description}: ${needle}")
    endif()
endfunction()

function(forbid_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Gate 9D contract regressed ${description}: ${needle}")
    endif()
endfunction()

# Journey is a high-level navigation surface, never a second topology editor.
require_text(workspace_source "EXITS // OPEN IN GRAPH" "Journey-to-Graph exit affordance")
forbid_text(workspace_source "EXITS // CLICK TO EDIT" "legacy Journey route-edit affordance")
require_text(workspace_source "deleteNodeButton_.SetVisible(nodeSelected && graphMode);" "Graph-only node deletion")
require_text(workspace_source "const bool routeSelected = graphMode && selectedRoute != nullptr;" "Graph-only route editing")
require_text(workspace_source "connectButton_.SetVisible(false);" "retired legacy CONNECT control")
require_text(workspace_source "reconnectRouteButton_.SetVisible(false);" "retired legacy RECONNECT control")

# ImNodes is the only Graph canvas renderer and runtime interaction owner.
require_text(workspace_source "Graph is intentionally not rendered here." "single Graph renderer boundary")
forbid_text(workspace_source "Line(start, end, routeColor);" "primitive legacy Graph route renderer")
forbid_text(workspace_source "if (Contains(fitBounds, pointer))" "legacy header FIT hit testing")
forbid_text(workspace_source "if (Contains(startBounds, pointer))" "legacy header START hit testing")
require_text(render_path_source "nativeCanvasNavigationOwnsPointer" "Journey native-navigation input ownership")
require_text(render_path_source "!nativeCanvasNavigationOwnsPointer" "Journey canvas click-through cutoff")
forbid_text(render_path_source "KEYBOARD_BUTTON_DELETE" "Journey-level Delete shortcut")

# Native commands queue intent and execute only after the complete wiGUI update.
require_text(render_path_source "pendingNativeCommand_ = NativeCommand::Fit;" "deferred FIT command")
require_text(render_path_source "pendingNativeCommand_ = NativeCommand::Start;" "deferred START command")
require_text(render_path_source "ProcessPendingNativeCommand();" "post-GUI native command execution")
require_text(render_path_source "std::exchange(\n                pendingNativeCommand_, NativeCommand::None)" "single-consumption command queue")

# Graph navigation is durable-layout based. Native FIT/START/FIND may execute
# after ImNodes recovery has recreated an empty editor context, so this seam
# must never dereference transient node objects or move to a node by editor ID.
require_text(graph_recovery_source "GraphNavigationMinZoom" "Graph-native bounded navigation zoom")
require_text(graph_recovery_source "layout_->canvas.panX = viewport_.z * 0.5f" "authoritative Graph pan calculation")
require_text(graph_recovery_source "ImNodes::EditorContextResetPanning" "safe ImNodes pan synchronization")
forbid_text(graph_recovery_source "ImNodes::GetNodeGridSpacePos" "native navigation dereference of transient ImNodes node positions")
forbid_text(graph_recovery_source "ImNodes::GetNodeDimensions" "native navigation dereference of transient ImNodes node dimensions")
forbid_text(graph_recovery_source "ImNodes::EditorContextMoveToNode" "secondary ImNodes-owned navigation authority")

# Project Hub -> Story Flow handoff remains visually opaque for the final
# Level Editor frame while becoming non-blocking for the post-frame path switch.
require_text(project_loading_overlay_source "if (phase == Phase::Idle)" "stale handoff-cover cleanup")
require_text(project_loading_overlay_source "phase_.store(static_cast<int>(Phase::Idle), std::memory_order_release);" "post-frame handoff release")
require_text(project_loading_overlay_source "storedPhase == Phase::Idle ? Phase::Ready : storedPhase" "opaque final handoff frame")
forbid_text(project_loading_overlay_source "SetVisible(false);\n                phase_.store(static_cast<int>(Phase::Idle)" "same-frame loader removal before Story Flow handoff")

# Gate 9D navigation is native and presentation-only.
require_text(render_path_source "Story Flow Journey View" "native Journey view control")
require_text(render_path_source "Story Flow Graph View" "native Graph view control")
require_text(render_path_source "Story Flow Find Node" "native node-name search")
require_text(graph_editor_source "bool FocusNodeByName" "Graph/Journey focus navigation API")
require_text(graph_editor_source "void RenderOverview" "presentation-only Graph overview API")

message(STATUS "Story Flow Gate 9D source ownership contract passed")
