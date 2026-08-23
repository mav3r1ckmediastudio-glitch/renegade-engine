#include "RenegadeStoryFlowGraphEditor.h"

#include <algorithm>
#include <vector>

#include "RenegadeStoryFlowJourneyChrome.h"
#include "RenegadeStoryFlowWorkspace.h"

namespace renegade::studio
{
    void RenegadeStoryFlowGraphEditor::ResetTransientInteractionState() noexcept
    {
        pendingReconnectRouteId_.clear();
        linkStartedAtPin_ = 0;
        rejectNewLinkFromInput_ = false;

        if (!initialized_ || !imnodesContext_)
            return;

        ImNodes::SetCurrentContext(imnodesContext_);

        // Build the replacement before releasing the old context. Story Flow
        // and the Renegade layout remain authoritative, so replacing ImNodes'
        // transient editor state cannot lose semantic or persisted layout data.
        ImNodesEditorContext* replacement = ImNodes::EditorContextCreate();
        if (!replacement)
            return;

        if (editorContext_)
            ImNodes::EditorContextFree(editorContext_);
        editorContext_ = replacement;
        ImNodes::EditorContextSet(editorContext_);

        if (layout_)
        {
            ImNodes::EditorContextResetPanning(
                ImVec2(layout_->canvas.panX, layout_->canvas.panY));
        }

        initializedNodePositions_.clear();
        bindingsDirty_ = true;
        observedUndoCount_ = static_cast<std::size_t>(-1);
        observedRedoCount_ = static_cast<std::size_t>(-1);
    }

    void RenegadeStoryFlowGraphEditor::RecoverTransientInteractionIfIdle()
    {
        if (!initialized_ || !imnodesContext_ || !editorContext_)
            return;

        if (wi::input::Down(wi::input::MOUSE_BUTTON_LEFT) ||
            wi::input::Down(wi::input::MOUSE_BUTTON_RIGHT) ||
            wi::input::Down(wi::input::MOUSE_BUTTON_MIDDLE))
        {
            return;
        }

        ImNodes::SetCurrentContext(imnodesContext_);
        ImNodes::EditorContextSet(editorContext_);

        int startedPin = 0;
        int activeAttribute = 0;
        const bool linkStillLatched = ImNodes::IsLinkStarted(&startedPin);
        const bool attributeStillLatched =
            ImNodes::IsAnyAttributeActive(&activeAttribute);
        const bool hostStateStillLatched =
            !pendingReconnectRouteId_.empty() || linkStartedAtPin_ != 0;

        if (!linkStillLatched && !attributeStillLatched && !hostStateStillLatched)
            return;

        ResetTransientInteractionState();
        SetStatus("GRAPH INTERACTION RECOVERED // AUTHORITATIVE ROUTES PRESERVED");
    }

    void RenegadeStoryFlowGraphEditor::ReconcileHostInteractions(
        const bool allowDeleteShortcut)
    {
        if (!initialized_ || !session_ || !model_ || !workspace_ ||
            !editorContext_ || !session_->IsLoaded() || !model_->IsLoaded())
        {
            return;
        }

        ImNodes::SetCurrentContext(imnodesContext_);
        ImNodes::EditorContextSet(editorContext_);

        // Host-driven selection (notably governed Level/Screen creation and
        // Journey -> Graph handoff) must become a real ImNodes selection and
        // focus. The previous path only moved the retired legacy canvas, which
        // meant a newly created Screen could be valid in Story Flow but remain
        // completely off-screen in Graph. A node selected directly inside
        // ImNodes is already selected here, so normal graph clicks never cause
        // surprise recentering.
        const bridge::StableId& hostSelectedNodeId = workspace_->SelectedNodeId();
        if (!hostSelectedNodeId.empty())
        {
            const auto found = nodeIndexByStableId_.find(hostSelectedNodeId);
            if (found != nodeIndexByStableId_.end())
            {
                const int editorNodeId = nodes_[found->second].nodeId;
                if (!ImNodes::IsNodeSelected(editorNodeId))
                {
                    ImNodes::ClearLinkSelection();
                    ImNodes::ClearNodeSelection();
                    ImNodes::SelectNode(editorNodeId);
                    ImNodes::EditorContextMoveToNode(editorNodeId);

                    if (layout_)
                    {
                        const ImVec2 panning = ImNodes::EditorContextGetPanning();
                        layout_->canvas.panX = panning.x;
                        layout_->canvas.panY = panning.y;
                    }
                    workspace_->NotifyLayoutChanged();
                    SetStatus("GRAPH FOCUS // SELECTED NODE CENTERED");
                    return;
                }
            }
        }

        if (!pendingReconnectRouteId_.empty() &&
            !wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))
        {
            ResetTransientInteractionState();
            SetStatus("REWIRE CANCELLED // ORIGINAL ROUTE RESTORED");
            return;
        }

        if (wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
        {
            const XMFLOAT4 pointer = wi::input::GetPointer();
            const float headerTop = viewport_.y -
                RenegadeStoryFlowJourneyChrome::WorkspaceHeaderHeight;
            const float graphRight = viewport_.x + viewport_.z;
            const XMFLOAT4 fitBounds(
                graphRight - 136.0f,
                headerTop + 10.0f,
                52.0f,
                28.0f);
            const XMFLOAT4 startBounds(
                graphRight - 76.0f,
                headerTop + 10.0f,
                64.0f,
                28.0f);
            const auto contains = [&](const XMFLOAT4& bounds)
            {
                return pointer.x >= bounds.x &&
                    pointer.y >= bounds.y &&
                    pointer.x < bounds.x + bounds.z &&
                    pointer.y < bounds.y + bounds.w;
            };

            if (contains(fitBounds) && !nodes_.empty())
            {
                bool haveBounds = false;
                float minX = 0.0f;
                float minY = 0.0f;
                float maxX = 0.0f;
                float maxY = 0.0f;
                for (const auto& node : nodes_)
                {
                    const ImVec2 position = ImNodes::GetNodeGridSpacePos(node.nodeId);
                    const ImVec2 dimensions = ImNodes::GetNodeDimensions(node.nodeId);
                    if (!haveBounds)
                    {
                        minX = position.x;
                        minY = position.y;
                        maxX = position.x + dimensions.x;
                        maxY = position.y + dimensions.y;
                        haveBounds = true;
                    }
                    else
                    {
                        minX = std::min(minX, position.x);
                        minY = std::min(minY, position.y);
                        maxX = std::max(maxX, position.x + dimensions.x);
                        maxY = std::max(maxY, position.y + dimensions.y);
                    }
                }

                if (haveBounds)
                {
                    const ImVec2 panning(
                        viewport_.z * 0.5f - (minX + maxX) * 0.5f,
                        viewport_.w * 0.5f - (minY + maxY) * 0.5f);
                    ImNodes::EditorContextResetPanning(panning);
                    if (layout_)
                    {
                        layout_->canvas.panX = panning.x;
                        layout_->canvas.panY = panning.y;
                    }
                    workspace_->NotifyLayoutChanged();
                    SetStatus("GRAPH FIT // CONTENT CENTERED");
                }
                return;
            }

            if (contains(startBounds))
            {
                const auto found = nodeIndexByStableId_.find(
                    model_->GameStartNodeId());
                if (found != nodeIndexByStableId_.end())
                {
                    ImNodes::EditorContextMoveToNode(nodes_[found->second].nodeId);
                    workspace_->SelectNode(model_->GameStartNodeId());
                    SetStatus("GRAPH START // GAME START CENTERED");
                }
                return;
            }
        }

        if (!allowDeleteShortcut ||
            !wi::input::Press(wi::input::KEYBOARD_BUTTON_DELETE))
        {
            return;
        }

        if (ImNodes::NumSelectedLinks() > 0)
            return;

        const int selectedCount = ImNodes::NumSelectedNodes();
        if (selectedCount <= 0)
            return;

        std::vector<int> selected(static_cast<std::size_t>(selectedCount));
        ImNodes::GetSelectedNodes(selected.data());
        const auto* binding = FindNodeBinding(selected.back());
        const auto* node = binding ? FindDocumentNode(binding->stableId) : nullptr;
        if (!node)
            return;
        if (node->kind == bridge::FlowNodeKind::GameStart)
        {
            SetStatus("DELETE REJECTED // GAME START IS PERMANENT");
            return;
        }

        const bridge::StableId deleting = node->id;
        std::string error;
        if (!session_->DeleteNode(deleting, error))
        {
            SetStatus("DELETE REJECTED // " + error);
            return;
        }

        ImNodes::ClearNodeSelection();
        workspace_->selectedNodeId_.clear();
        workspace_->selectedRouteId_.clear();
        MarkSemanticRefresh();
        SetStatus("NODE + CONNECTED ROUTES DELETED // UNSAVED");
    }

    bool RenegadeStoryFlowWorkspace::CanDeleteSelection() const noexcept
    {
        if (!session_ || !session_->IsLoaded())
            return false;
        if (!selectedRouteId_.empty())
            return FindDocumentRoute(selectedRouteId_) != nullptr;
        const auto* node = FindDocumentNode(selectedNodeId_);
        return node && node->kind != bridge::FlowNodeKind::GameStart;
    }

    void RenegadeStoryFlowWorkspace::DeleteSelection()
    {
        if (!session_ || !session_->IsLoaded())
            return;
        if (!selectedRouteId_.empty())
        {
            DeleteSelectedRoute();
            return;
        }
        if (!selectedNodeId_.empty())
            DeleteSelectedNode();
    }
}
