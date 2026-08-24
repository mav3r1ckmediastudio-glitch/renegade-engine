#include "RenegadeStoryFlowGraphEditor.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <vector>

#include "RenegadeStoryFlowWorkspace.h"

namespace renegade::studio
{
    namespace
    {
        constexpr float OverviewNodeWidth = 210.0f;
        constexpr float OverviewNodeHeight = 112.0f;
        constexpr wi::Color OverviewSurface(8, 12, 16, 232);
        constexpr wi::Color OverviewBorder(38, 52, 61, 255);
        constexpr wi::Color OverviewNode(98, 116, 126, 255);
        constexpr wi::Color OverviewRoute(65, 82, 91, 255);
        constexpr wi::Color OverviewAccent(210, 91, 29, 255);
        constexpr wi::Color OverviewText(174, 184, 189, 255);

        std::string LowerTrimmed(std::string value)
        {
            const auto whitespace = [](const unsigned char c)
            {
                return std::isspace(c) != 0;
            };
            while (!value.empty() && whitespace(value.front())) value.erase(value.begin());
            while (!value.empty() && whitespace(value.back())) value.pop_back();
            std::transform(
                value.begin(), value.end(), value.begin(),
                [](const unsigned char c)
                {
                    return static_cast<char>(std::tolower(c));
                });
            return value;
        }

        void OverviewRect(
            const float x,
            const float y,
            const float width,
            const float height,
            const wi::Color color,
            const wi::graphics::CommandList cmd)
        {
            if (width <= 0.0f || height <= 0.0f)
                return;
            wi::image::Params params(x, y, width, height, color);
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            wi::image::Draw(nullptr, params, cmd);
        }

        void OverviewBorderedRect(
            const float x,
            const float y,
            const float width,
            const float height,
            const wi::Color fill,
            const wi::Color border,
            const wi::graphics::CommandList cmd)
        {
            OverviewRect(x, y, width, height, fill, cmd);
            OverviewRect(x, y, width, 1.0f, border, cmd);
            OverviewRect(x, y + height - 1.0f, width, 1.0f, border, cmd);
            OverviewRect(x, y, 1.0f, height, border, cmd);
            OverviewRect(x + width - 1.0f, y, 1.0f, height, border, cmd);
        }
    }

    void RenegadeStoryFlowGraphEditor::ResetTransientInteractionState() noexcept
    {
        pendingReconnectRouteId_.clear();
        linkStartedAtPin_ = 0;
        rejectNewLinkFromInput_ = false;

        if (!initialized_ || !imnodesContext_)
            return;

        ImNodes::SetCurrentContext(imnodesContext_);

        // Story Flow and the Renegade layout are authoritative. Rebuilding only
        // ImNodes' editor context discards a poisoned click/link gesture without
        // losing semantic routes or persisted node positions.
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

    void RenegadeStoryFlowGraphEditor::FitToContent()
    {
        if (!layout_ || !workspace_ || !model_ || !model_->IsLoaded())
            return;

        // Native Graph navigation must never depend on transient ImNodes node
        // objects being alive. The shared Story Flow workspace owns the durable
        // canvas layout and already has view-aware fit math, so update that
        // authority first and copy only the resulting pan into ImNodes.
        workspace_->FitToContent();

        if (initialized_ && imnodesContext_ && editorContext_)
        {
            ImNodes::SetCurrentContext(imnodesContext_);
            ImNodes::EditorContextSet(editorContext_);
            ImNodes::EditorContextResetPanning(
                ImVec2(layout_->canvas.panX, layout_->canvas.panY));
        }

        SetStatus("GRAPH FIT // CONTENT CENTERED");
    }

    void RenegadeStoryFlowGraphEditor::CenterOnGameStart()
    {
        if (!model_ || !layout_ || !workspace_ || !model_->IsLoaded())
            return;

        const bridge::StableId startId = model_->GameStartNodeId();
        workspace_->SelectNode(startId);
        workspace_->CenterOnGameStart();

        if (initialized_ && imnodesContext_ && editorContext_)
        {
            ImNodes::SetCurrentContext(imnodesContext_);
            ImNodes::EditorContextSet(editorContext_);
            ImNodes::EditorContextResetPanning(
                ImVec2(layout_->canvas.panX, layout_->canvas.panY));
        }

        SetStatus("GRAPH START // GAME START CENTERED");
    }

    bool RenegadeStoryFlowGraphEditor::FocusNodeByName(const std::string& query)
    {
        if (!model_ || !workspace_ || !model_->IsLoaded())
            return false;

        const std::string needle = LowerTrimmed(query);
        if (needle.empty())
        {
            SetStatus("FIND REJECTED // ENTER A NODE NAME");
            return false;
        }

        const bridge::StoryFlowNodeView* match = nullptr;
        for (const auto& node : model_->Nodes())
        {
            if (LowerTrimmed(node.name) == needle)
            {
                match = &node;
                break;
            }
        }
        if (!match)
        {
            for (const auto& node : model_->Nodes())
            {
                if (LowerTrimmed(node.name).find(needle) != std::string::npos)
                {
                    match = &node;
                    break;
                }
            }
        }
        if (!match)
        {
            SetStatus("FIND // NO STORY FLOW NODE MATCHES '" + query + "'");
            return false;
        }

        if (layout_ && layout_->activeView == bridge::StoryFlowViewMode::Journey)
        {
            workspace_->SelectAndFocusNode(match->id);
            SetStatus("JOURNEY FIND // " + match->name);
            return true;
        }

        // Graph focus follows the same durable-layout rule as FIT and START.
        // SelectAndFocusNode updates the authoritative Graph canvas even when a
        // native control has temporarily forced ImNodes to rebuild its context.
        workspace_->SelectAndFocusNode(match->id);
        if (layout_ && initialized_ && imnodesContext_ && editorContext_)
        {
            ImNodes::SetCurrentContext(imnodesContext_);
            ImNodes::EditorContextSet(editorContext_);
            ImNodes::EditorContextResetPanning(
                ImVec2(layout_->canvas.panX, layout_->canvas.panY));
        }
        SetStatus("GRAPH FIND // " + match->name);
        return true;
    }

    void RenegadeStoryFlowGraphEditor::RenderOverview(
        const wi::graphics::CommandList cmd) const
    {
        if (!frameActive_ || !model_ || !layout_ || !workspace_ ||
            viewport_.z < 260.0f || viewport_.w < 220.0f)
        {
            return;
        }

        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();
        bool haveBounds = false;
        for (const auto& node : layout_->nodes)
        {
            if (!model_->FindNode(node.nodeId))
                continue;
            minX = std::min(minX, node.x);
            minY = std::min(minY, node.y);
            maxX = std::max(maxX, node.x + OverviewNodeWidth);
            maxY = std::max(maxY, node.y + OverviewNodeHeight);
            haveBounds = true;
        }
        if (!haveBounds)
            return;

        const float panelWidth = std::clamp(viewport_.z * 0.19f, 170.0f, 238.0f);
        const float panelHeight = std::clamp(viewport_.w * 0.17f, 112.0f, 156.0f);
        const float panelX = viewport_.x + viewport_.z - panelWidth - 14.0f;
        const float panelY = viewport_.y + viewport_.w - panelHeight - 14.0f;
        OverviewBorderedRect(
            panelX, panelY, panelWidth, panelHeight,
            OverviewSurface, OverviewBorder, cmd);

        wi::font::Params label(
            panelX + 9.0f,
            panelY + 7.0f,
            7,
            wi::font::WIFALIGN_LEFT,
            wi::font::WIFALIGN_TOP,
            OverviewText,
            wi::Color::Transparent());
        label.bolden = 0.14f;
        wi::font::Draw("GRAPH OVERVIEW", label, cmd);

        const float contentX = panelX + 9.0f;
        const float contentY = panelY + 23.0f;
        const float contentW = panelWidth - 18.0f;
        const float contentH = panelHeight - 32.0f;
        const float worldW = std::max(1.0f, maxX - minX);
        const float worldH = std::max(1.0f, maxY - minY);
        const float scale = std::min(contentW / worldW, contentH / worldH);
        const float offsetX = contentX + (contentW - worldW * scale) * 0.5f;
        const float offsetY = contentY + (contentH - worldH * scale) * 0.5f;
        const auto mapPoint = [&](const float x, const float y)
        {
            return XMFLOAT2(
                offsetX + (x - minX) * scale,
                offsetY + (y - minY) * scale);
        };

        for (const auto& route : model_->Routes())
        {
            const auto source = std::find_if(
                layout_->nodes.begin(), layout_->nodes.end(),
                [&](const bridge::StoryFlowNodeLayout& item)
                {
                    return item.nodeId == route.sourceNodeId;
                });
            const auto destination = std::find_if(
                layout_->nodes.begin(), layout_->nodes.end(),
                [&](const bridge::StoryFlowNodeLayout& item)
                {
                    return item.nodeId == route.destinationNodeId;
                });
            if (source == layout_->nodes.end() || destination == layout_->nodes.end())
                continue;

            wi::renderer::RenderableLine2D line;
            line.start = mapPoint(
                source->x + OverviewNodeWidth * 0.5f,
                source->y + OverviewNodeHeight * 0.5f);
            line.end = mapPoint(
                destination->x + OverviewNodeWidth * 0.5f,
                destination->y + OverviewNodeHeight * 0.5f);
            line.color_start = OverviewRoute;
            line.color_end = OverviewRoute;
            wi::renderer::DrawLine(line);
        }

        const bridge::StableId selected = workspace_->SelectedNodeId();
        for (const auto& node : layout_->nodes)
        {
            if (!model_->FindNode(node.nodeId))
                continue;
            const XMFLOAT2 p = mapPoint(node.x, node.y);
            const float w = std::max(3.0f, OverviewNodeWidth * scale);
            const float h = std::max(2.0f, OverviewNodeHeight * scale);
            OverviewRect(
                p.x, p.y, w, h,
                node.nodeId == selected ? OverviewAccent : OverviewNode,
                cmd);
        }

        const float zoom = std::max(0.001f, layout_->canvas.zoom);
        const float visibleX = -layout_->canvas.panX / zoom;
        const float visibleY = -layout_->canvas.panY / zoom;
        const float visibleW = viewport_.z / zoom;
        const float visibleH = viewport_.w / zoom;
        const XMFLOAT2 visibleTopLeft = mapPoint(visibleX, visibleY);
        const float mappedW = visibleW * scale;
        const float mappedH = visibleH * scale;
        if (mappedW > 1.0f && mappedH > 1.0f)
        {
            OverviewBorderedRect(
                visibleTopLeft.x,
                visibleTopLeft.y,
                mappedW,
                mappedH,
                wi::Color(0, 0, 0, 0),
                OverviewAccent,
                cmd);
        }
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

        // Host selection and focus are already represented by the authoritative
        // Story Flow layout. Reconciliation mirrors selection into ImNodes only;
        // it must not independently move the canvas and override FIT/START/FIND.
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
                    if (layout_)
                    {
                        ImNodes::EditorContextResetPanning(
                            ImVec2(layout_->canvas.panX, layout_->canvas.panY));
                    }
                    SetStatus("GRAPH SELECTION // HOST NODE SYNCHRONIZED");
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
