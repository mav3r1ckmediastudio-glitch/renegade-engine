#include "RenegadeStoryFlowWorkspace.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <utility>

#include "renegade/bridge/StoryFlowInteractionPolicy.h"

namespace
{
    constexpr float HeaderHeight = 78.0f;
    constexpr float Padding = 28.0f;
    constexpr float NodeWidth = 210.0f;
    constexpr float NodeHeight = 112.0f;
    constexpr float JourneyCardWidth = 244.0f;
    constexpr float JourneyCardHeight = 156.0f;
    constexpr float JourneyColumnSpacing = 304.0f;
    constexpr float JourneyTrackSpacing = 228.0f;
    constexpr float JourneyTrackTop = 44.0f;
    constexpr float MinZoom = 0.20f;
    constexpr float MaxZoom = 2.50f;

    constexpr wi::Color Surface(7, 11, 14, 255);
    constexpr wi::Color Raised(13, 19, 23, 255);
    constexpr wi::Color Border(38, 52, 61, 255);
    constexpr wi::Color Text(244, 244, 244, 255);
    constexpr wi::Color Muted(132, 143, 149, 255);
    constexpr wi::Color Accent(210, 91, 29, 255);
    constexpr wi::Color Route(76, 96, 106, 255);
    constexpr wi::Color Error(229, 92, 92, 255);
    constexpr wi::Color JourneyRail(45, 61, 70, 255);

    void Rect(float x, float y, float w, float h, wi::Color color,
        wi::graphics::CommandList cmd)
    {
        if (w <= 0 || h <= 0) return;
        wi::image::Params params(x, y, w, h, color);
        params.blendFlag = wi::enums::BLENDMODE_ALPHA;
        wi::image::Draw(nullptr, params, cmd);
    }

    void BorderedRect(float x, float y, float w, float h,
        wi::Color fill, wi::Color border, wi::graphics::CommandList cmd)
    {
        Rect(x, y, w, h, fill, cmd);
        Rect(x, y, w, 1.0f, border, cmd);
        Rect(x, y + h - 1.0f, w, 1.0f, border, cmd);
        Rect(x, y, 1.0f, h, border, cmd);
        Rect(x + w - 1.0f, y, 1.0f, h, border, cmd);
    }

    void Label(const std::string& value, float x, float y, int size,
        wi::Color color, wi::graphics::CommandList cmd)
    {
        wi::font::Params params(x, y, size, wi::font::WIFALIGN_LEFT,
            wi::font::WIFALIGN_TOP, color, wi::Color::Transparent());
        params.bolden = 0.14f;
        wi::font::Draw(value, params, cmd);
    }

    void Line(XMFLOAT2 start, XMFLOAT2 end, wi::Color color)
    {
        wi::renderer::RenderableLine2D line;
        line.start = start;
        line.end = end;
        line.color_start = color;
        line.color_end = color;
        wi::renderer::DrawLine(line);
    }

    const char* KindLabel(renegade::bridge::FlowNodeKind kind)
    {
        using renegade::bridge::FlowNodeKind;
        switch (kind)
        {
        case FlowNodeKind::GameStart: return "GAME START";
        case FlowNodeKind::Level: return "LEVEL";
        case FlowNodeKind::Screen: return "SCREEN";
        case FlowNodeKind::CompleteGame: return "COMPLETE GAME";
        case FlowNodeKind::ReturnToMainMenu: return "RETURN TO MENU";
        case FlowNodeKind::Quit: return "QUIT";
        default: return "UNKNOWN";
        }
    }

    bool Contains(const XMFLOAT4& bounds, const XMFLOAT4& pointer)
    {
        return pointer.x >= bounds.x && pointer.y >= bounds.y &&
            pointer.x < bounds.x + bounds.z && pointer.y < bounds.y + bounds.w;
    }

    std::string Trim(std::string value)
    {
        const auto whitespace = [](const unsigned char c)
        {
            return std::isspace(c) != 0;
        };
        while (!value.empty() && whitespace(value.front())) value.erase(value.begin());
        while (!value.empty() && whitespace(value.back())) value.pop_back();
        return value;
    }

    std::string Shorten(std::string value, const std::size_t limit)
    {
        if (value.size() <= limit) return value;
        if (limit <= 3) return value.substr(0, limit);
        value.resize(limit - 3);
        value += "...";
        return value;
    }

    float DistanceToSegment(
        const XMFLOAT2 point,
        const XMFLOAT2 start,
        const XMFLOAT2 end)
    {
        const float dx = end.x - start.x;
        const float dy = end.y - start.y;
        const float lengthSquared = dx * dx + dy * dy;
        if (lengthSquared <= 0.0001f)
        {
            const float px = point.x - start.x;
            const float py = point.y - start.y;
            return std::sqrt(px * px + py * py);
        }
        const float projection = std::clamp(
            ((point.x - start.x) * dx + (point.y - start.y) * dy) /
                lengthSquared,
            0.0f,
            1.0f);
        const float nearestX = start.x + projection * dx;
        const float nearestY = start.y + projection * dy;
        const float px = point.x - nearestX;
        const float py = point.y - nearestY;
        return std::sqrt(px * px + py * py);
    }
}

namespace renegade::studio
{
    void RenegadeStoryFlowWorkspace::Create()
    {
        SetName("Renegade Story Flow workspace");
        SetShadowRadius(0.0f);
        CreateAuthoringControls();
        SetLayout(width_, height_);
    }

    void RenegadeStoryFlowWorkspace::SetLayout(float width, float height)
    {
        width_ = std::max(1.0f, width);
        height_ = std::max(1.0f, height);
        SetSize(XMFLOAT2(width_, height_));
        LayoutAuthoringControls();
    }

    void RenegadeStoryFlowWorkspace::Bind(
        bridge::StoryFlowAuthoringSession* session,
        bridge::StoryFlowAuthoringModel* model,
        bridge::StoryFlowLayoutDocument* layout)
    {
        session_ = session;
        model_ = model;
        layout_ = layout;
        selectedRouteId_.clear();
        connectionSourceNodeId_.clear();
        reconnectRouteId_.clear();
        if (session_ && model_ && layout_ && session_->IsLoaded() && model_->IsLoaded())
        {
            const bool journeyReady = RebuildJourneyProjection();
            selectedNodeId_ = model_->GameStartNodeId();
            if (journeyReady)
            {
                statusMessage_ = layout_->activeView == bridge::StoryFlowViewMode::Journey
                    ? "JOURNEY READY // GRAPH SYNCHRONIZED"
                    : "GRAPH READY // JOURNEY SYNCHRONIZED";
            }
        }
        else
        {
            selectedNodeId_.clear();
            statusMessage_ = "NO FLOW OPEN";
        }
        RefreshInspectorControls();
    }

    void RenegadeStoryFlowWorkspace::Clear() noexcept
    {
        session_ = nullptr;
        model_ = nullptr;
        layout_ = nullptr;
        journeyModel_.Clear();
        selectedNodeId_.clear();
        selectedRouteId_.clear();
        connectionSourceNodeId_.clear();
        reconnectRouteId_.clear();
        draggedNodeId_.clear();
        panning_ = false;
        nodeDragging_ = false;
        previousClickedNodeId_.clear();
        secondsSincePreviousNodeClick_ = 1000.0f;
        statusMessage_ = "NO FLOW OPEN";
    }

    void RenegadeStoryFlowWorkspace::OnSelectionChanged(
        std::function<void(const bridge::StableId&)> callback)
    {
        selectionChanged_ = std::move(callback);
    }

    void RenegadeStoryFlowWorkspace::OnLayoutChanged(
        std::function<void()> callback)
    {
        layoutChanged_ = std::move(callback);
    }

    void RenegadeStoryFlowWorkspace::OnNodeActivated(
        std::function<void(const bridge::StableId&)> callback)
    {
        nodeActivated_ = std::move(callback);
    }

    void RenegadeStoryFlowWorkspace::OnScreenOutcomeQuery(
        ScreenOutcomeQuery callback)
    {
        screenOutcomeQuery_ = std::move(callback);
    }

    void RenegadeStoryFlowWorkspace::SetExternalStatus(std::string message)
    {
        SetStatus(std::move(message));
    }

    const bridge::StoryFlowNodeLayout* RenegadeStoryFlowWorkspace::FindLayout(
        const bridge::StableId& id) const noexcept
    {
        if (!layout_) return nullptr;
        const auto it = std::find_if(layout_->nodes.begin(), layout_->nodes.end(),
            [&](const bridge::StoryFlowNodeLayout& item) { return item.nodeId == id; });
        return it == layout_->nodes.end() ? nullptr : &*it;
    }

    bridge::StoryFlowNodeLayout* RenegadeStoryFlowWorkspace::FindLayout(
        const bridge::StableId& id) noexcept
    {
        if (!layout_) return nullptr;
        const auto it = std::find_if(layout_->nodes.begin(), layout_->nodes.end(),
            [&](const bridge::StoryFlowNodeLayout& item) { return item.nodeId == id; });
        return it == layout_->nodes.end() ? nullptr : &*it;
    }

    const bridge::StoryFlowJourneyCardLayout*
    RenegadeStoryFlowWorkspace::FindJourneyLayout(
        const bridge::StableId& id) const noexcept
    {
        if (!layout_) return nullptr;
        const auto it = std::find_if(
            layout_->journeyCards.begin(), layout_->journeyCards.end(),
            [&](const bridge::StoryFlowJourneyCardLayout& item)
            {
                return item.nodeId == id;
            });
        return it == layout_->journeyCards.end() ? nullptr : &*it;
    }

    bridge::StoryFlowJourneyCardLayout*
    RenegadeStoryFlowWorkspace::FindJourneyLayout(
        const bridge::StableId& id) noexcept
    {
        if (!layout_) return nullptr;
        const auto it = std::find_if(
            layout_->journeyCards.begin(), layout_->journeyCards.end(),
            [&](const bridge::StoryFlowJourneyCardLayout& item)
            {
                return item.nodeId == id;
            });
        return it == layout_->journeyCards.end() ? nullptr : &*it;
    }

    const bridge::FlowNode* RenegadeStoryFlowWorkspace::FindDocumentNode(
        const bridge::StableId& id) const noexcept
    {
        if (!session_ || !session_->IsLoaded()) return nullptr;
        const auto& nodes = session_->Document().nodes;
        const auto it = std::find_if(nodes.begin(), nodes.end(),
            [&](const bridge::FlowNode& item) { return item.id == id; });
        return it == nodes.end() ? nullptr : &*it;
    }

    const bridge::FlowRoute* RenegadeStoryFlowWorkspace::FindDocumentRoute(
        const bridge::StableId& id) const noexcept
    {
        if (!session_ || !session_->IsLoaded()) return nullptr;
        const auto& routes = session_->Document().routes;
        const auto it = std::find_if(routes.begin(), routes.end(),
            [&](const bridge::FlowRoute& item) { return item.id == id; });
        return it == routes.end() ? nullptr : &*it;
    }

    XMFLOAT2 RenegadeStoryFlowWorkspace::CanvasToScreen(float x, float y) const noexcept
    {
        const auto& canvas = ActiveCanvas();
        const float zoom = layout_ ? canvas.zoom : 1.0f;
        return XMFLOAT2(
            translation.x + Padding + (layout_ ? canvas.panX : 0) + x * zoom,
            translation.y + HeaderHeight + Padding + (layout_ ? canvas.panY : 0) + y * zoom);
    }

    XMFLOAT2 RenegadeStoryFlowWorkspace::ScreenToCanvas(float x, float y) const noexcept
    {
        const auto& canvas = ActiveCanvas();
        const float zoom = layout_ ? std::max(0.001f, canvas.zoom) : 1.0f;
        return XMFLOAT2(
            (x - translation.x - Padding - (layout_ ? canvas.panX : 0)) / zoom,
            (y - translation.y - HeaderHeight - Padding - (layout_ ? canvas.panY : 0)) / zoom);
    }

    XMFLOAT4 RenegadeStoryFlowWorkspace::NodeScreenBounds(
        const bridge::StoryFlowNodeLayout& node) const noexcept
    {
        const XMFLOAT2 p = CanvasToScreen(node.x, node.y);
        const float zoom = layout_ ? layout_->canvas.zoom : 1.0f;
        return XMFLOAT4(p.x, p.y, NodeWidth * zoom, NodeHeight * zoom);
    }

    XMFLOAT4 RenegadeStoryFlowWorkspace::JourneyCardScreenBounds(
        const bridge::StoryFlowJourneyCard& card) const noexcept
    {
        const auto* offset = FindJourneyLayout(card.nodeId);
        const float x = static_cast<float>(card.columnIndex) * JourneyColumnSpacing +
            (offset ? offset->offsetX : 0.0f);
        const float y = JourneyTrackTop +
            static_cast<float>(card.trackIndex) * JourneyTrackSpacing +
            (offset ? offset->offsetY : 0.0f);
        const XMFLOAT2 position = CanvasToScreen(x, y);
        const float zoom = layout_ ? layout_->journeyCanvas.zoom : 1.0f;
        return XMFLOAT4(
            position.x, position.y,
            JourneyCardWidth * zoom, JourneyCardHeight * zoom);
    }

    XMFLOAT4 RenegadeStoryFlowWorkspace::NodeBounds(
        const bridge::StableId& nodeId) const noexcept
    {
        if (layout_ && layout_->activeView == bridge::StoryFlowViewMode::Journey)
        {
            const auto* card = journeyModel_.FindCard(nodeId);
            return card ? JourneyCardScreenBounds(*card) : XMFLOAT4{};
        }
        const auto* position = FindLayout(nodeId);
        return position ? NodeScreenBounds(*position) : XMFLOAT4{};
    }

    bridge::StoryFlowCanvasLayout&
    RenegadeStoryFlowWorkspace::ActiveCanvas() noexcept
    {
        static bridge::StoryFlowCanvasLayout fallback;
        if (!layout_) return fallback;
        return layout_->activeView == bridge::StoryFlowViewMode::Journey
            ? layout_->journeyCanvas
            : layout_->canvas;
    }

    const bridge::StoryFlowCanvasLayout&
    RenegadeStoryFlowWorkspace::ActiveCanvas() const noexcept
    {
        static const bridge::StoryFlowCanvasLayout fallback;
        if (!layout_) return fallback;
        return layout_->activeView == bridge::StoryFlowViewMode::Journey
            ? layout_->journeyCanvas
            : layout_->canvas;
    }

    float RenegadeStoryFlowWorkspace::GraphWidth() const noexcept
    {
        const float reserved = std::min(InspectorWidth, width_ * 0.42f);
        return std::max(1.0f, width_ - reserved);
    }

    bool RenegadeStoryFlowWorkspace::PointerInsideGraph(const XMFLOAT4& p) const noexcept
    {
        return p.x >= translation.x && p.y >= translation.y + HeaderHeight &&
            p.x < translation.x + GraphWidth() && p.y < translation.y + scale.y;
    }

    bridge::StableId RenegadeStoryFlowWorkspace::HitTestRoute(
        const XMFLOAT4& pointer) const
    {
        if (!model_ || !layout_) return {};
        const XMFLOAT2 point(pointer.x, pointer.y);
        for (const auto& route : model_->Routes())
        {
            const auto* a = FindLayout(route.sourceNodeId);
            const auto* b = FindLayout(route.destinationNodeId);
            if (!a || !b) continue;
            const XMFLOAT4 ab = NodeScreenBounds(*a);
            const XMFLOAT4 bb = NodeScreenBounds(*b);
            const XMFLOAT2 start(ab.x + ab.z, ab.y + ab.w * 0.5f);
            const XMFLOAT2 end(bb.x, bb.y + bb.w * 0.5f);
            if (DistanceToSegment(point, start, end) <= 8.0f)
            {
                return route.id;
            }
        }
        return {};
    }

    bool RenegadeStoryFlowWorkspace::IsTerminalKind(
        const bridge::FlowNodeKind kind) noexcept
    {
        return kind == bridge::FlowNodeKind::CompleteGame ||
            kind == bridge::FlowNodeKind::ReturnToMainMenu ||
            kind == bridge::FlowNodeKind::Quit;
    }

    void RenegadeStoryFlowWorkspace::SelectNode(const bridge::StableId& id)
    {
        if (selectedNodeId_ == id && selectedRouteId_.empty()) return;
        selectedNodeId_ = id;
        selectedRouteId_.clear();
        RefreshInspectorControls();
        if (selectionChanged_) selectionChanged_(selectedNodeId_);
    }

    void RenegadeStoryFlowWorkspace::SelectRoute(const bridge::StableId& id)
    {
        selectedRouteId_ = id;
        selectedNodeId_.clear();
        connectionSourceNodeId_.clear();
        reconnectRouteId_.clear();
        RefreshInspectorControls();
        if (selectionChanged_) selectionChanged_({});
    }

    void RenegadeStoryFlowWorkspace::NotifyLayoutChanged()
    {
        if (layoutChanged_) layoutChanged_();
    }

    void RenegadeStoryFlowWorkspace::SetStatus(std::string message)
    {
        statusMessage_ = std::move(message);
    }

    void RenegadeStoryFlowWorkspace::EnsureSelectionValid()
    {
        if (model_ && !selectedNodeId_.empty() && !model_->FindNode(selectedNodeId_))
            selectedNodeId_.clear();
        if (model_ && !selectedRouteId_.empty() && !model_->FindRoute(selectedRouteId_))
            selectedRouteId_.clear();
        if (model_ && !connectionSourceNodeId_.empty() &&
            !model_->FindNode(connectionSourceNodeId_))
        {
            connectionSourceNodeId_.clear();
            reconnectRouteId_.clear();
        }
    }

    void RenegadeStoryFlowWorkspace::SetActiveView(
        const bridge::StoryFlowViewMode view)
    {
        if (!layout_ || layout_->activeView == view) return;
        layout_->activeView = view;
        panning_ = false;
        nodeDragging_ = false;
        draggedNodeId_.clear();
        SetStatus(view == bridge::StoryFlowViewMode::Journey
            ? "JOURNEY VIEW // SHARED FLOW MODEL"
            : "GRAPH VIEW // EXACT SHARED TOPOLOGY");
        NotifyLayoutChanged();
    }

    bool RenegadeStoryFlowWorkspace::RebuildJourneyProjection()
    {
        if (!model_ || !model_->IsLoaded())
        {
            journeyModel_.Clear();
            return false;
        }
        std::string error;
        if (!journeyModel_.Build(*model_, error))
        {
            SetStatus("JOURNEY PROJECTION ERROR // " + error);
            return false;
        }
        return true;
    }

    void RenegadeStoryFlowWorkspace::RememberOrActivateNodeClick(
        const bridge::StoryFlowNodeView& node,
        const XMFLOAT4& pointer)
    {
        const float dx = pointer.x - previousClickPointer_.x;
        const float dy = pointer.y - previousClickPointer_.y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        const bool activate = bridge::ShouldActivateStoryFlowNodeClick(
            node.kind,
            previousClickedNodeId_ == node.id,
            secondsSincePreviousNodeClick_,
            distance);

        previousClickedNodeId_ = node.id;
        previousClickPointer_ = XMFLOAT2(pointer.x, pointer.y);
        secondsSincePreviousNodeClick_ = activate
            ? 1000.0f
            : 0.0f;
        if (activate && nodeActivated_)
            nodeActivated_(node.id);
    }

    void RenegadeStoryFlowWorkspace::FitToContent()
    {
        if (!model_ || !layout_) return;
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();
        if (layout_->activeView == bridge::StoryFlowViewMode::Journey)
        {
            for (const auto& card : journeyModel_.Cards())
            {
                const auto* offset = FindJourneyLayout(card.nodeId);
                const float x = static_cast<float>(card.columnIndex) * JourneyColumnSpacing +
                    (offset ? offset->offsetX : 0.0f);
                const float y = JourneyTrackTop +
                    static_cast<float>(card.trackIndex) * JourneyTrackSpacing +
                    (offset ? offset->offsetY : 0.0f);
                minX = std::min(minX, x); minY = std::min(minY, y);
                maxX = std::max(maxX, x + JourneyCardWidth);
                maxY = std::max(maxY, y + JourneyCardHeight);
            }
        }
        else
        {
            for (const auto& node : layout_->nodes)
            {
                if (!model_->FindNode(node.nodeId)) continue;
                minX = std::min(minX, node.x); minY = std::min(minY, node.y);
                maxX = std::max(maxX, node.x + NodeWidth);
                maxY = std::max(maxY, node.y + NodeHeight);
            }
        }
        if (minX > maxX || minY > maxY) return;
        const float cw = std::max(1.0f, maxX - minX);
        const float ch = std::max(1.0f, maxY - minY);
        const float graphWidth = GraphWidth();
        auto& activeCanvas = ActiveCanvas();
        activeCanvas.zoom = std::clamp(std::min((graphWidth - 4 * Padding) / cw,
            (height_ - HeaderHeight - 4 * Padding) / ch), MinZoom, MaxZoom);
        activeCanvas.panX = (graphWidth - cw * activeCanvas.zoom) * 0.5f -
            Padding - minX * activeCanvas.zoom;
        activeCanvas.panY = (height_ - HeaderHeight - ch * activeCanvas.zoom) * 0.5f -
            Padding - minY * activeCanvas.zoom;
        NotifyLayoutChanged();
    }

    void RenegadeStoryFlowWorkspace::CenterOnGameStart()
    {
        if (!model_ || !layout_) return;
        auto& activeCanvas = ActiveCanvas();
        float x = 0.0f;
        float y = 0.0f;
        float width = NodeWidth;
        float height = NodeHeight;
        if (layout_->activeView == bridge::StoryFlowViewMode::Journey)
        {
            const auto* card = journeyModel_.FindCard(model_->GameStartNodeId());
            if (!card) return;
            const auto* offset = FindJourneyLayout(card->nodeId);
            x = static_cast<float>(card->columnIndex) * JourneyColumnSpacing +
                (offset ? offset->offsetX : 0.0f);
            y = JourneyTrackTop + static_cast<float>(card->trackIndex) * JourneyTrackSpacing +
                (offset ? offset->offsetY : 0.0f);
            width = JourneyCardWidth;
            height = JourneyCardHeight;
        }
        else
        {
            const auto* node = FindLayout(model_->GameStartNodeId());
            if (!node) return;
            x = node->x;
            y = node->y;
        }
        activeCanvas.panX = GraphWidth() * 0.5f - Padding -
            (x + width * 0.5f) * activeCanvas.zoom;
        activeCanvas.panY = (height_ - HeaderHeight) * 0.5f - Padding -
            (y + height * 0.5f) * activeCanvas.zoom;
        NotifyLayoutChanged();
    }

    void RenegadeStoryFlowWorkspace::SelectAndFocusNode(
        const bridge::StableId& nodeId)
    {
        if (!model_ || !layout_ || !model_->FindNode(nodeId)) return;
        SelectNode(nodeId);
        const XMFLOAT4 bounds = NodeBounds(nodeId);
        if (bounds.z <= 0.0f || bounds.w <= 0.0f) return;
        auto& activeCanvas = ActiveCanvas();
        activeCanvas.panX += translation.x + GraphWidth() * 0.5f -
            (bounds.x + bounds.z * 0.5f);
        activeCanvas.panY += translation.y + HeaderHeight +
            (height_ - HeaderHeight) * 0.5f -
            (bounds.y + bounds.w * 0.5f);
        NotifyLayoutChanged();
        SetStatus("CREATED DESTINATION SELECTED // READY TO CONNECT");
    }

    void RenegadeStoryFlowWorkspace::CreateAuthoringControls()
    {
        saveButton_.Create("Story Flow Save");
        saveButton_.SetText("SAVE");
        saveButton_.SetTooltip("Transactionally save the authoritative Story Flow document.");
        saveButton_.OnClick([this](const wi::gui::EventArgs&) { SaveFlow(); });

        undoButton_.Create("Story Flow Undo");
        undoButton_.SetText("UNDO");
        undoButton_.SetTooltip("Undo the previous Story Flow semantic edit.");
        undoButton_.OnClick([this](const wi::gui::EventArgs&) { UndoFlow(); });

        redoButton_.Create("Story Flow Redo");
        redoButton_.SetText("REDO");
        redoButton_.SetTooltip("Redo the next Story Flow semantic edit.");
        redoButton_.OnClick([this](const wi::gui::EventArgs&) { RedoFlow(); });

        connectButton_.Create("Story Flow Connect");
        connectButton_.SetText("CONNECT");
        connectButton_.SetTooltip("Connect the selected non-terminal node to another destination.");
        connectButton_.OnClick([this](const wi::gui::EventArgs&) { BeginConnect(); });

        nodeNameInput_.Create("Story Flow Node Name");
        nodeNameInput_.SetDescription("NAME  ");
        nodeNameInput_.SetPlaceholder("Destination name...");
        nodeNameInput_.SetCancelInputEnabled(false);
        nodeNameInput_.OnInputAccepted([this](const wi::gui::EventArgs&) { ApplySelectedNode(); });

        applyNodeButton_.Create("Story Flow Apply Node");
        applyNodeButton_.SetText("APPLY NAME");
        applyNodeButton_.OnClick([this](const wi::gui::EventArgs&) { ApplySelectedNode(); });

        deleteNodeButton_.Create("Story Flow Delete Node");
        deleteNodeButton_.SetText("DELETE NODE");
        deleteNodeButton_.SetTooltip("Delete this node and its connected routes as one semantic edit.");
        deleteNodeButton_.OnClick([this](const wi::gui::EventArgs&) { DeleteSelectedNode(); });

        routeOutcomeInput_.Create("Story Flow Route Outcome");
        routeOutcomeInput_.SetDescription("OUTCOME  ");
        routeOutcomeInput_.SetPlaceholder("next / failed / new_game...");
        routeOutcomeInput_.SetCancelInputEnabled(false);

        routeEntryInput_.Create("Story Flow Route Player Entry");
        routeEntryInput_.SetDescription("ENTRY  ");
        routeEntryInput_.SetPlaceholder("player_entry");
        routeEntryInput_.SetCancelInputEnabled(false);

        routePriorityInput_.Create("Story Flow Route Priority");
        routePriorityInput_.SetDescription("PRIORITY  ");
        routePriorityInput_.SetPlaceholder("0");
        routePriorityInput_.SetCancelInputEnabled(false);
        routePriorityInput_.OnInputAccepted([this](const wi::gui::EventArgs&) { ApplySelectedRoute(); });

        applyRouteButton_.Create("Story Flow Apply Route");
        applyRouteButton_.SetText("APPLY ROUTE");
        applyRouteButton_.OnClick([this](const wi::gui::EventArgs&) { ApplySelectedRoute(); });

        reconnectRouteButton_.Create("Story Flow Reconnect Route");
        reconnectRouteButton_.SetText("RECONNECT");
        reconnectRouteButton_.SetTooltip("Keep this route identity and source, then choose a new destination node.");
        reconnectRouteButton_.OnClick([this](const wi::gui::EventArgs&) { BeginReconnect(); });

        deleteRouteButton_.Create("Story Flow Delete Route");
        deleteRouteButton_.SetText("DELETE ROUTE");
        deleteRouteButton_.OnClick([this](const wi::gui::EventArgs&) { DeleteSelectedRoute(); });

        addCompleteButton_.Create("Story Flow Add Complete Game");
        addCompleteButton_.SetText("+ COMPLETE");
        addCompleteButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            AddTerminalNode(bridge::FlowNodeKind::CompleteGame, "Complete Game");
        });

        addReturnButton_.Create("Story Flow Add Return To Menu");
        addReturnButton_.SetText("+ RETURN MENU");
        addReturnButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            AddTerminalNode(bridge::FlowNodeKind::ReturnToMainMenu, "Return To Main Menu");
        });

        addQuitButton_.Create("Story Flow Add Quit");
        addQuitButton_.SetText("+ QUIT");
        addQuitButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            AddTerminalNode(bridge::FlowNodeKind::Quit, "Quit");
        });

        for (wi::gui::Widget* widget : {
            static_cast<wi::gui::Widget*>(&saveButton_),
            static_cast<wi::gui::Widget*>(&undoButton_),
            static_cast<wi::gui::Widget*>(&redoButton_),
            static_cast<wi::gui::Widget*>(&connectButton_),
            static_cast<wi::gui::Widget*>(&nodeNameInput_),
            static_cast<wi::gui::Widget*>(&applyNodeButton_),
            static_cast<wi::gui::Widget*>(&deleteNodeButton_),
            static_cast<wi::gui::Widget*>(&routeOutcomeInput_),
            static_cast<wi::gui::Widget*>(&routeEntryInput_),
            static_cast<wi::gui::Widget*>(&routePriorityInput_),
            static_cast<wi::gui::Widget*>(&applyRouteButton_),
            static_cast<wi::gui::Widget*>(&reconnectRouteButton_),
            static_cast<wi::gui::Widget*>(&deleteRouteButton_),
            static_cast<wi::gui::Widget*>(&addCompleteButton_),
            static_cast<wi::gui::Widget*>(&addReturnButton_),
            static_cast<wi::gui::Widget*>(&addQuitButton_)})
        {
            widget->SetShadowRadius(0.0f);
            widget->SetVisible(false);
        }
    }

    void RenegadeStoryFlowWorkspace::LayoutAuthoringControls()
    {
        const float top = translation.y + 10.0f;
        float x = translation.x + 220.0f;
        const auto headerButton = [&x, top](wi::gui::Widget& widget, const float width)
        {
            widget.SetPos(XMFLOAT2(x, top));
            widget.SetSize(XMFLOAT2(width, 28.0f));
            x += width + 5.0f;
        };
        headerButton(undoButton_, 56.0f);
        headerButton(redoButton_, 56.0f);
        headerButton(saveButton_, 56.0f);
        headerButton(connectButton_, 78.0f);

        const float inspectorX = translation.x + GraphWidth() + 14.0f;
        const float inspectorWidth = std::max(90.0f, width_ - GraphWidth() - 28.0f);
        const float fieldWidth = inspectorWidth;
        const float y0 = translation.y + HeaderHeight + 56.0f;

        nodeNameInput_.SetPos(XMFLOAT2(inspectorX, y0));
        nodeNameInput_.SetSize(XMFLOAT2(fieldWidth, 28.0f));
        applyNodeButton_.SetPos(XMFLOAT2(inspectorX, y0 + 38.0f));
        applyNodeButton_.SetSize(XMFLOAT2(fieldWidth * 0.52f - 3.0f, 28.0f));
        deleteNodeButton_.SetPos(XMFLOAT2(inspectorX + fieldWidth * 0.52f + 3.0f, y0 + 38.0f));
        deleteNodeButton_.SetSize(XMFLOAT2(fieldWidth * 0.48f - 3.0f, 28.0f));

        routeOutcomeInput_.SetPos(XMFLOAT2(inspectorX, y0));
        routeOutcomeInput_.SetSize(XMFLOAT2(fieldWidth, 28.0f));
        routeEntryInput_.SetPos(XMFLOAT2(inspectorX, y0 + 38.0f));
        routeEntryInput_.SetSize(XMFLOAT2(fieldWidth, 28.0f));
        routePriorityInput_.SetPos(XMFLOAT2(inspectorX, y0 + 76.0f));
        routePriorityInput_.SetSize(XMFLOAT2(fieldWidth, 28.0f));
        applyRouteButton_.SetPos(XMFLOAT2(inspectorX, y0 + 114.0f));
        applyRouteButton_.SetSize(XMFLOAT2(fieldWidth, 28.0f));
        reconnectRouteButton_.SetPos(XMFLOAT2(inspectorX, y0 + 152.0f));
        reconnectRouteButton_.SetSize(XMFLOAT2(fieldWidth * 0.52f - 3.0f, 28.0f));
        deleteRouteButton_.SetPos(XMFLOAT2(inspectorX + fieldWidth * 0.52f + 3.0f, y0 + 152.0f));
        deleteRouteButton_.SetSize(XMFLOAT2(fieldWidth * 0.48f - 3.0f, 28.0f));

        const float addY = translation.y + HeaderHeight + 356.0f;
        addCompleteButton_.SetPos(XMFLOAT2(inspectorX, addY));
        addCompleteButton_.SetSize(XMFLOAT2(fieldWidth, 27.0f));
        addReturnButton_.SetPos(XMFLOAT2(inspectorX, addY + 35.0f));
        addReturnButton_.SetSize(XMFLOAT2(fieldWidth, 27.0f));
        addQuitButton_.SetPos(XMFLOAT2(inspectorX, addY + 70.0f));
        addQuitButton_.SetSize(XMFLOAT2(fieldWidth, 27.0f));
    }

    void RenegadeStoryFlowWorkspace::UpdateAuthoringControls(
        const wi::Canvas& canvas,
        const float dt)
    {
        LayoutAuthoringControls();
        const bool loaded = session_ && model_ && layout_ &&
            session_->IsLoaded() && model_->IsLoaded();

        saveButton_.SetVisible(loaded);
        undoButton_.SetVisible(loaded);
        redoButton_.SetVisible(loaded);
        connectButton_.SetVisible(loaded);
        saveButton_.SetEnabled(loaded && session_->IsDirty());
        undoButton_.SetEnabled(loaded && session_->CanUndo());
        redoButton_.SetEnabled(loaded && session_->CanRedo());

        const auto* selectedNode = FindDocumentNode(selectedNodeId_);
        const auto* selectedRoute = FindDocumentRoute(selectedRouteId_);
        const bool connecting = !connectionSourceNodeId_.empty();
        connectButton_.SetText(connecting ? "CANCEL LINK" : "CONNECT");
        connectButton_.SetEnabled(loaded && (connecting ||
            (selectedNode != nullptr && !IsTerminalKind(selectedNode->kind))));

        const bool nodeSelected = loaded && selectedNode != nullptr;
        nodeNameInput_.SetVisible(nodeSelected);
        applyNodeButton_.SetVisible(nodeSelected);
        deleteNodeButton_.SetVisible(nodeSelected);
        deleteNodeButton_.SetEnabled(nodeSelected && selectedNode->kind != bridge::FlowNodeKind::GameStart);

        const bool routeSelected = loaded && selectedRoute != nullptr;
        routeOutcomeInput_.SetVisible(routeSelected);
        routeEntryInput_.SetVisible(routeSelected);
        routePriorityInput_.SetVisible(routeSelected);
        applyRouteButton_.SetVisible(routeSelected);
        reconnectRouteButton_.SetVisible(routeSelected);
        deleteRouteButton_.SetVisible(routeSelected);
        if (routeSelected)
        {
            const auto* destination = FindDocumentNode(selectedRoute->destinationNodeId);
            routeEntryInput_.SetEnabled(destination && destination->kind == bridge::FlowNodeKind::Level);
        }

        addCompleteButton_.SetVisible(loaded);
        addReturnButton_.SetVisible(loaded);
        addQuitButton_.SetVisible(loaded);

        for (wi::gui::Widget* widget : {
            static_cast<wi::gui::Widget*>(&saveButton_),
            static_cast<wi::gui::Widget*>(&undoButton_),
            static_cast<wi::gui::Widget*>(&redoButton_),
            static_cast<wi::gui::Widget*>(&connectButton_),
            static_cast<wi::gui::Widget*>(&nodeNameInput_),
            static_cast<wi::gui::Widget*>(&applyNodeButton_),
            static_cast<wi::gui::Widget*>(&deleteNodeButton_),
            static_cast<wi::gui::Widget*>(&routeOutcomeInput_),
            static_cast<wi::gui::Widget*>(&routeEntryInput_),
            static_cast<wi::gui::Widget*>(&routePriorityInput_),
            static_cast<wi::gui::Widget*>(&applyRouteButton_),
            static_cast<wi::gui::Widget*>(&reconnectRouteButton_),
            static_cast<wi::gui::Widget*>(&deleteRouteButton_),
            static_cast<wi::gui::Widget*>(&addCompleteButton_),
            static_cast<wi::gui::Widget*>(&addReturnButton_),
            static_cast<wi::gui::Widget*>(&addQuitButton_)})
        {
            if (widget->IsVisible()) widget->Update(canvas, dt);
        }
    }

    void RenegadeStoryFlowWorkspace::RenderAuthoringControls(
        const wi::Canvas& canvas,
        const wi::graphics::CommandList cmd) const
    {
        for (const wi::gui::Widget* widget : {
            static_cast<const wi::gui::Widget*>(&saveButton_),
            static_cast<const wi::gui::Widget*>(&undoButton_),
            static_cast<const wi::gui::Widget*>(&redoButton_),
            static_cast<const wi::gui::Widget*>(&connectButton_),
            static_cast<const wi::gui::Widget*>(&nodeNameInput_),
            static_cast<const wi::gui::Widget*>(&applyNodeButton_),
            static_cast<const wi::gui::Widget*>(&deleteNodeButton_),
            static_cast<const wi::gui::Widget*>(&routeOutcomeInput_),
            static_cast<const wi::gui::Widget*>(&routeEntryInput_),
            static_cast<const wi::gui::Widget*>(&routePriorityInput_),
            static_cast<const wi::gui::Widget*>(&applyRouteButton_),
            static_cast<const wi::gui::Widget*>(&reconnectRouteButton_),
            static_cast<const wi::gui::Widget*>(&deleteRouteButton_),
            static_cast<const wi::gui::Widget*>(&addCompleteButton_),
            static_cast<const wi::gui::Widget*>(&addReturnButton_),
            static_cast<const wi::gui::Widget*>(&addQuitButton_)})
        {
            if (widget->IsVisible()) widget->Render(canvas, cmd);
        }
    }

    void RenegadeStoryFlowWorkspace::RefreshInspectorControls()
    {
        if (const auto* node = FindDocumentNode(selectedNodeId_))
        {
            nodeNameInput_.SetValue(node->name);
        }
        if (const auto* route = FindDocumentRoute(selectedRouteId_))
        {
            routeOutcomeInput_.SetValue(route->outcome);
            routeEntryInput_.SetValue(route->destinationEntry);
            routePriorityInput_.SetValue(std::to_string(route->priority));
        }
    }

    bool RenegadeStoryFlowWorkspace::RefreshPresentationAfterSemanticChange()
    {
        if (!session_ || !model_ || !layout_ || !session_->IsLoaded()) return false;
        std::string error;
        if (!model_->Load(session_->Document(), session_->ProjectId(), error))
        {
            SetStatus("PRESENTATION ERROR // " + error);
            return false;
        }

        const bridge::StableId flowId = session_->Document().envelope.documentId;
        if (!bridge::ReconcileStoryFlowLayout(
                *model_, session_->ProjectId(), flowId, *layout_, error))
        {
            *layout_ = bridge::BuildDeterministicStoryFlowLayout(
                *model_, session_->ProjectId(), flowId);
            SetStatus("LAYOUT REBUILT // " + error);
        }
        if (!RebuildJourneyProjection())
            return false;
        EnsureSelectionValid();
        RefreshInspectorControls();
        NotifyLayoutChanged();
        return true;
    }

    void RenegadeStoryFlowWorkspace::SaveFlow()
    {
        if (!session_) return;
        std::string error;
        if (!session_->Save(error))
        {
            SetStatus("SAVE FAILED // " + error);
            return;
        }
        SetStatus("FLOW SAVED // TRANSACTION COMMITTED");
    }

    void RenegadeStoryFlowWorkspace::UndoFlow()
    {
        if (!session_) return;
        std::string error;
        if (!session_->Undo(error))
        {
            SetStatus("UNDO REFUSED // " + error);
            return;
        }
        RefreshPresentationAfterSemanticChange();
        SetStatus("FLOW UNDO // VALID STATE RESTORED");
    }

    void RenegadeStoryFlowWorkspace::RedoFlow()
    {
        if (!session_) return;
        std::string error;
        if (!session_->Redo(error))
        {
            SetStatus("REDO REFUSED // " + error);
            return;
        }
        RefreshPresentationAfterSemanticChange();
        SetStatus("FLOW REDO // VALID STATE RESTORED");
    }

    void RenegadeStoryFlowWorkspace::ApplySelectedNode()
    {
        if (!session_ || selectedNodeId_.empty()) return;
        std::string error;
        const std::string name = Trim(nodeNameInput_.GetValue());
        if (!session_->RenameNode(selectedNodeId_, name, error))
        {
            SetStatus("NODE EDIT REJECTED // " + error);
            RefreshInspectorControls();
            return;
        }
        RefreshPresentationAfterSemanticChange();
        SetStatus("NODE UPDATED // UNSAVED FLOW CHANGE");
    }

    void RenegadeStoryFlowWorkspace::DeleteSelectedNode()
    {
        if (!session_ || selectedNodeId_.empty()) return;
        const bridge::StableId deleting = selectedNodeId_;
        std::string error;
        if (!session_->DeleteNode(deleting, error))
        {
            SetStatus("DELETE REJECTED // " + error);
            return;
        }
        selectedNodeId_.clear();
        selectedRouteId_.clear();
        RefreshPresentationAfterSemanticChange();
        SetStatus("NODE + CONNECTED ROUTES DELETED // UNSAVED");
    }

    bool RenegadeStoryFlowWorkspace::QueryScreenOutcomes(
        const bridge::StableId& sourceNodeId,
        std::vector<std::string>& outcomes,
        std::string& error) const
    {
        outcomes.clear();
        const auto* source = FindDocumentNode(sourceNodeId);
        if (!source)
        {
            error = "route source no longer exists";
            return false;
        }
        if (source->kind != bridge::FlowNodeKind::Screen)
        {
            error.clear();
            return true;
        }
        if (!screenOutcomeQuery_)
        {
            error = "Screen outcome authority is unavailable";
            return false;
        }
        return screenOutcomeQuery_(sourceNodeId, outcomes, error);
    }

    bool RenegadeStoryFlowWorkspace::ValidateRouteOutcomeForSource(
        const bridge::StableId& sourceNodeId,
        const std::string& outcome,
        std::string& error) const
    {
        const auto* source = FindDocumentNode(sourceNodeId);
        if (!source)
        {
            error = "route source no longer exists";
            return false;
        }
        if (source->kind != bridge::FlowNodeKind::Screen)
        {
            error.clear();
            return true;
        }

        std::vector<std::string> outcomes;
        if (!QueryScreenOutcomes(sourceNodeId, outcomes, error))
            return false;
        if (outcomes.empty())
        {
            error = "Screen has no authored actions";
            return false;
        }
        if (std::find(outcomes.begin(), outcomes.end(), outcome) == outcomes.end())
        {
            error = "'" + outcome + "' IS NOT AN AUTHORED SCREEN ACTION";
            return false;
        }
        error.clear();
        return true;
    }

    void RenegadeStoryFlowWorkspace::ApplySelectedRoute()
    {
        if (!session_ || selectedRouteId_.empty()) return;
        const auto* current = FindDocumentRoute(selectedRouteId_);
        if (!current) return;
        bridge::FlowRoute route = *current;
        route.outcome = Trim(routeOutcomeInput_.GetValue());
        route.destinationEntry = Trim(routeEntryInput_.GetValue());

        std::string error;
        if (!ValidateRouteOutcomeForSource(
                route.sourceNodeId, route.outcome, error))
        {
            SetStatus("ROUTE EDIT REJECTED // " + error);
            RefreshInspectorControls();
            return;
        }

        const std::string priorityText = Trim(routePriorityInput_.GetValue());
        try
        {
            std::size_t used = 0;
            route.priority = std::stoi(priorityText, &used);
            if (used != priorityText.size())
            {
                SetStatus("ROUTE EDIT REJECTED // PRIORITY MUST BE AN INTEGER");
                RefreshInspectorControls();
                return;
            }
        }
        catch (...)
        {
            SetStatus("ROUTE EDIT REJECTED // PRIORITY MUST BE AN INTEGER");
            RefreshInspectorControls();
            return;
        }

        if (!session_->UpdateRoute(selectedRouteId_, std::move(route), error))
        {
            SetStatus("ROUTE EDIT REJECTED // " + error);
            RefreshInspectorControls();
            return;
        }
        RefreshPresentationAfterSemanticChange();
        SetStatus("ROUTE UPDATED // UNSAVED FLOW CHANGE");
    }

    void RenegadeStoryFlowWorkspace::DeleteSelectedRoute()
    {
        if (!session_ || selectedRouteId_.empty()) return;
        const bridge::StableId deleting = selectedRouteId_;
        std::string error;
        if (!session_->DeleteRoute(deleting, error))
        {
            SetStatus("ROUTE DELETE REJECTED // " + error);
            return;
        }
        selectedRouteId_.clear();
        RefreshPresentationAfterSemanticChange();
        SetStatus("ROUTE DELETED // UNSAVED FLOW CHANGE");
    }

    void RenegadeStoryFlowWorkspace::AddTerminalNode(
        const bridge::FlowNodeKind kind,
        const char* defaultName)
    {
        if (!session_ || !layout_) return;
        XMFLOAT2 anchor(0.0f, 0.0f);
        bool hasAnchor = false;
        if (const auto* selectedLayout = FindLayout(selectedNodeId_))
        {
            anchor = XMFLOAT2(selectedLayout->x, selectedLayout->y);
            hasAnchor = true;
        }

        bridge::FlowNode node;
        node.kind = kind;
        node.name = defaultName;
        bridge::StableId createdId;
        std::string error;
        if (!session_->AddNode(std::move(node), createdId, error))
        {
            SetStatus("ADD NODE REJECTED // " + error);
            return;
        }
        RefreshPresentationAfterSemanticChange();
        if (auto* createdLayout = FindLayout(createdId))
        {
            if (hasAnchor)
            {
                createdLayout->x = anchor.x + 300.0f;
                createdLayout->y = anchor.y + 135.0f;
            }
            NotifyLayoutChanged();
        }
        SelectNode(createdId);
        SetStatus("TERMINAL DESTINATION ADDED // UNSAVED FLOW CHANGE");
    }

    void RenegadeStoryFlowWorkspace::BeginConnect()
    {
        if (!connectionSourceNodeId_.empty())
        {
            connectionSourceNodeId_.clear();
            reconnectRouteId_.clear();
            SetStatus("CONNECT CANCELLED");
            return;
        }
        const auto* source = FindDocumentNode(selectedNodeId_);
        if (!source || IsTerminalKind(source->kind))
        {
            SetStatus("CONNECT REQUIRES A NON-TERMINAL SOURCE NODE");
            return;
        }
        connectionSourceNodeId_ = source->id;
        reconnectRouteId_.clear();
        SetStatus("CONNECT // SELECT A DESTINATION NODE");
    }

    void RenegadeStoryFlowWorkspace::BeginReconnect()
    {
        const auto* route = FindDocumentRoute(selectedRouteId_);
        if (!route)
        {
            SetStatus("RECONNECT REQUIRES A SELECTED ROUTE");
            return;
        }
        connectionSourceNodeId_ = route->sourceNodeId;
        reconnectRouteId_ = route->id;
        SetStatus("RECONNECT // SELECT THE NEW DESTINATION NODE");
    }

    void RenegadeStoryFlowWorkspace::CommitConnectionTo(
        const bridge::StableId& destinationNodeId)
    {
        if (!session_ || connectionSourceNodeId_.empty()) return;
        const auto* destination = FindDocumentNode(destinationNodeId);
        if (!destination || destination->kind == bridge::FlowNodeKind::GameStart)
        {
            SetStatus("CONNECT REJECTED // GAME START CANNOT BE A DESTINATION");
            return;
        }

        std::string error;
        bridge::StableId resultingRouteId;
        if (!reconnectRouteId_.empty())
        {
            const auto* current = FindDocumentRoute(reconnectRouteId_);
            if (!current)
            {
                SetStatus("RECONNECT REJECTED // ROUTE NO LONGER EXISTS");
                connectionSourceNodeId_.clear();
                reconnectRouteId_.clear();
                return;
            }
            bridge::FlowRoute route = *current;
            route.destinationNodeId = destinationNodeId;
            if (destination->kind == bridge::FlowNodeKind::Level)
            {
                if (route.destinationEntry.empty()) route.destinationEntry = "player_entry";
            }
            else
            {
                route.destinationEntry.clear();
            }
            resultingRouteId = reconnectRouteId_;
            if (!session_->UpdateRoute(reconnectRouteId_, std::move(route), error))
            {
                SetStatus("RECONNECT REJECTED // " + error);
                return;
            }
        }
        else
        {
            const auto* source = FindDocumentNode(connectionSourceNodeId_);
            if (!source)
            {
                SetStatus("CONNECT REJECTED // SOURCE NODE NO LONGER EXISTS");
                connectionSourceNodeId_.clear();
                return;
            }
            bridge::FlowRoute route;
            route.sourceNodeId = connectionSourceNodeId_;
            if (source->kind == bridge::FlowNodeKind::GameStart)
            {
                route.outcome = bridge::GameStartOutcome;
            }
            else if (source->kind == bridge::FlowNodeKind::Screen)
            {
                std::vector<std::string> outcomes;
                if (!QueryScreenOutcomes(source->id, outcomes, error))
                {
                    SetStatus("CONNECT REJECTED // " + error);
                    return;
                }
                if (outcomes.empty())
                {
                    SetStatus("CONNECT REJECTED // SCREEN HAS NO AUTHORED ACTIONS");
                    return;
                }
                route.outcome = outcomes.front();
            }
            else
            {
                route.outcome = "next";
            }
            route.destinationNodeId = destinationNodeId;
            if (destination->kind == bridge::FlowNodeKind::Level)
                route.destinationEntry = "player_entry";
            if (!session_->AddRoute(std::move(route), resultingRouteId, error))
            {
                SetStatus("CONNECT REJECTED // " + error);
                return;
            }
        }

        connectionSourceNodeId_.clear();
        reconnectRouteId_.clear();
        RefreshPresentationAfterSemanticChange();
        SelectRoute(resultingRouteId);
        SetStatus("ROUTE COMMITTED TO HISTORY // UNSAVED FLOW CHANGE");
    }

    void RenegadeStoryFlowWorkspace::Update(const wi::Canvas& canvas, float dt)
    {
        Widget::Update(canvas, dt);
        secondsSincePreviousNodeClick_ = std::min(
            1000.0f, secondsSincePreviousNodeClick_ + std::max(0.0f, dt));
        pointerConsumed_ = false;
        UpdateAuthoringControls(canvas, dt);
        if (!IsVisible() || !IsEnabled() || !model_ || !layout_) return;

        const XMFLOAT4 pointer = wi::input::GetPointer();
        const float graphRight = translation.x + GraphWidth();
        const XMFLOAT4 fitBounds(graphRight - 136.0f, translation.y + 10.0f, 52.0f, 28.0f);
        const XMFLOAT4 startBounds(graphRight - 76.0f, translation.y + 10.0f, 64.0f, 28.0f);
        const XMFLOAT4 journeyBounds(translation.x + 112.0f, translation.y + 10.0f, 58.0f, 28.0f);
        const XMFLOAT4 graphBounds(translation.x + 174.0f, translation.y + 10.0f, 44.0f, 28.0f);
        const bool insideHeader =
            pointer.x >= translation.x && pointer.x < translation.x + scale.x &&
            pointer.y >= translation.y && pointer.y < translation.y + HeaderHeight;
        const bool insideInspector = pointer.x >= graphRight &&
            pointer.x < translation.x + scale.x &&
            pointer.y >= translation.y + HeaderHeight &&
            pointer.y < translation.y + scale.y;

        if (insideHeader)
        {
            pointerConsumed_ = true;
            if (wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
            {
                if (Contains(journeyBounds, pointer))
                {
                    SetActiveView(bridge::StoryFlowViewMode::Journey);
                    return;
                }
                if (Contains(graphBounds, pointer))
                {
                    SetActiveView(bridge::StoryFlowViewMode::Graph);
                    return;
                }
                if (Contains(fitBounds, pointer))
                {
                    FitToContent();
                    return;
                }
                if (Contains(startBounds, pointer))
                {
                    CenterOnGameStart();
                    return;
                }
            }
            return;
        }
        if (insideInspector)
        {
            pointerConsumed_ = true;
            if (layout_->activeView == bridge::StoryFlowViewMode::Journey &&
                wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
            {
                const auto* selected = model_->FindNode(selectedNodeId_);
                if (selected)
                {
                    const float exitX = graphRight + 14.0f;
                    const float exitY = translation.y + HeaderHeight + 156.0f;
                    const std::size_t count = std::min<std::size_t>(
                        selected->outgoingRouteIds.size(), 5);
                    for (std::size_t i = 0; i < count; ++i)
                    {
                        const XMFLOAT4 exitBounds(
                            exitX, exitY + static_cast<float>(i) * 29.0f,
                            std::max(1.0f, width_ - GraphWidth() - 28.0f), 24.0f);
                        if (Contains(exitBounds, pointer))
                        {
                            SelectRoute(selected->outgoingRouteIds[i]);
                            return;
                        }
                    }
                }
            }
            return;
        }

        const bool inside = PointerInsideGraph(pointer);
        pointerConsumed_ = pointerConsumed_ || inside;
        if (inside && std::abs(pointer.z) > 0.001f)
        {
            const XMFLOAT2 before = ScreenToCanvas(pointer.x, pointer.y);
            auto& activeCanvas = ActiveCanvas();
            activeCanvas.zoom = std::clamp(
                activeCanvas.zoom * (pointer.z > 0 ? 1.1f : 1.0f / 1.1f),
                MinZoom,
                MaxZoom);
            const XMFLOAT2 after = CanvasToScreen(before.x, before.y);
            activeCanvas.panX += pointer.x - after.x;
            activeCanvas.panY += pointer.y - after.y;
            NotifyLayoutChanged();
        }

        if (inside && wi::input::Press(wi::input::MOUSE_BUTTON_MIDDLE))
        {
            panning_ = true;
            panPointerAnchor_ = XMFLOAT2(pointer.x, pointer.y);
            panValueAnchor_ = XMFLOAT2(ActiveCanvas().panX, ActiveCanvas().panY);
        }
        if (panning_ && wi::input::Down(wi::input::MOUSE_BUTTON_MIDDLE))
        {
            ActiveCanvas().panX = panValueAnchor_.x + pointer.x - panPointerAnchor_.x;
            ActiveCanvas().panY = panValueAnchor_.y + pointer.y - panPointerAnchor_.y;
            NotifyLayoutChanged();
        }
        else if (panning_)
        {
            panning_ = false;
        }

        if (nodeDragging_ && wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))
        {
            const float zoom = std::max(0.001f, ActiveCanvas().zoom);
            if (layout_->activeView == bridge::StoryFlowViewMode::Journey)
            {
                if (auto* pos = FindJourneyLayout(draggedNodeId_))
                {
                    pos->offsetX = nodeDragValueAnchor_.x +
                        (pointer.x - nodeDragPointerAnchor_.x) / zoom;
                    pos->offsetY = nodeDragValueAnchor_.y +
                        (pointer.y - nodeDragPointerAnchor_.y) / zoom;
                    NotifyLayoutChanged();
                }
            }
            else if (auto* pos = FindLayout(draggedNodeId_))
            {
                pos->x = nodeDragValueAnchor_.x +
                    (pointer.x - nodeDragPointerAnchor_.x) / zoom;
                pos->y = nodeDragValueAnchor_.y +
                    (pointer.y - nodeDragPointerAnchor_.y) / zoom;
                NotifyLayoutChanged();
            }
        }
        else if (nodeDragging_)
        {
            nodeDragging_ = false;
            draggedNodeId_.clear();
        }

        if (!inside || !wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)) return;

        for (const auto& node : model_->Nodes())
        {
            const XMFLOAT4 bounds = NodeBounds(node.id);
            if (bounds.z <= 0.0f || !Contains(bounds, pointer)) continue;
            if (!connectionSourceNodeId_.empty())
            {
                CommitConnectionTo(node.id);
                return;
            }
            SelectNode(node.id);
            RememberOrActivateNodeClick(node, pointer);
            if (secondsSincePreviousNodeClick_ >= 999.0f)
                return;
            nodeDragging_ = true;
            draggedNodeId_ = node.id;
            nodeDragPointerAnchor_ = XMFLOAT2(pointer.x, pointer.y);
            if (layout_->activeView == bridge::StoryFlowViewMode::Journey)
            {
                const auto* pos = FindJourneyLayout(node.id);
                nodeDragValueAnchor_ = pos
                    ? XMFLOAT2(pos->offsetX, pos->offsetY)
                    : XMFLOAT2{};
            }
            else
            {
                const auto* pos = FindLayout(node.id);
                nodeDragValueAnchor_ = pos
                    ? XMFLOAT2(pos->x, pos->y)
                    : XMFLOAT2{};
            }
            return;
        }

        const bridge::StableId routeId =
            layout_->activeView == bridge::StoryFlowViewMode::Graph
                ? HitTestRoute(pointer)
                : bridge::StableId{};
        if (!routeId.empty())
        {
            SelectRoute(routeId);
            return;
        }

        connectionSourceNodeId_.clear();
        reconnectRouteId_.clear();
        SelectNode({});
    }

    void RenegadeStoryFlowWorkspace::Render(
        const wi::Canvas& canvas,
        wi::graphics::CommandList cmd) const
    {
        if (!IsVisible()) return;
        ApplyScissor(canvas, scissorRect, cmd);
        const float graphWidth = GraphWidth();
        const float graphRight = translation.x + graphWidth;
        const float inspectorWidth = std::max(0.0f, scale.x - graphWidth);

        Rect(translation.x, translation.y, graphWidth, scale.y, Surface, cmd);
        Rect(graphRight, translation.y, inspectorWidth, scale.y, Raised, cmd);
        Rect(translation.x, translation.y, scale.x, HeaderHeight, Raised, cmd);
        Rect(graphRight, translation.y + HeaderHeight, 1.0f, scale.y - HeaderHeight, Border, cmd);

        Label("STORY FLOW", translation.x + 18, translation.y + 13, 14, Text, cmd);
        const XMFLOAT4 journeyBounds(translation.x + 112.0f, translation.y + 10.0f, 58.0f, 28.0f);
        const XMFLOAT4 graphBounds(translation.x + 174.0f, translation.y + 10.0f, 44.0f, 28.0f);
        const bool journeyActive = !layout_ ||
            layout_->activeView == bridge::StoryFlowViewMode::Journey;
        BorderedRect(journeyBounds.x, journeyBounds.y, journeyBounds.z, journeyBounds.w,
            journeyActive ? wi::Color(35, 24, 18, 255) : Surface,
            journeyActive ? Accent : Border, cmd);
        BorderedRect(graphBounds.x, graphBounds.y, graphBounds.z, graphBounds.w,
            journeyActive ? Surface : wi::Color(35, 24, 18, 255),
            journeyActive ? Border : Accent, cmd);
        Label("JOURNEY", journeyBounds.x + 7.0f, journeyBounds.y + 8.0f, 8,
            journeyActive ? Accent : Muted, cmd);
        Label("GRAPH", graphBounds.x + 8.0f, graphBounds.y + 8.0f, 8,
            journeyActive ? Muted : Accent, cmd);

        if (!model_ || !layout_ || !session_)
        {
            RenderAuthoringControls(canvas, cmd);
            return;
        }

        const XMFLOAT4 fitBounds(graphRight - 136.0f, translation.y + 10.0f, 52.0f, 28.0f);
        const XMFLOAT4 startBounds(graphRight - 76.0f, translation.y + 10.0f, 64.0f, 28.0f);
        BorderedRect(fitBounds.x, fitBounds.y, fitBounds.z, fitBounds.w,
            Surface, Border, cmd);
        BorderedRect(startBounds.x, startBounds.y, startBounds.z, startBounds.w,
            Surface, Border, cmd);
        Label("FIT", fitBounds.x + 15.0f, fitBounds.y + 8.0f, 9, Text, cmd);
        Label("START", startBounds.x + 10.0f, startBounds.y + 8.0f, 9, Text, cmd);

        const int zoomPercent = static_cast<int>(std::round(ActiveCanvas().zoom * 100.0f));
        const std::string dirty = session_->IsDirty() ? "DIRTY" : "SAVED";
        Label(
            dirty + " // " + std::to_string(model_->Nodes().size()) + " NODES // " +
                std::to_string(model_->Routes().size()) + " ROUTES // " +
                std::to_string(zoomPercent) + "%",
            translation.x + 112.0f,
            translation.y + 52.0f,
            9,
            session_->IsDirty() ? Accent : Muted,
            cmd);

        if (!connectionSourceNodeId_.empty())
        {
            Label(
                reconnectRouteId_.empty()
                    ? "CONNECT MODE // SELECT DESTINATION"
                    : "RECONNECT MODE // SELECT NEW DESTINATION",
                translation.x + 112.0f,
                translation.y + 65.0f,
                8,
                Accent,
                cmd);
        }

        if (layout_->activeView == bridge::StoryFlowViewMode::Graph)
        {
            for (const auto& route : model_->Routes())
            {
                const auto* a = FindLayout(route.sourceNodeId);
                const auto* b = FindLayout(route.destinationNodeId);
                if (!a || !b) continue;
                const XMFLOAT4 ab = NodeScreenBounds(*a);
                const XMFLOAT4 bb = NodeScreenBounds(*b);
                const XMFLOAT2 start(ab.x + ab.z, ab.y + ab.w * 0.5f);
                const XMFLOAT2 end(bb.x, bb.y + bb.w * 0.5f);
                const wi::Color routeColor =
                    route.id == selectedRouteId_ ||
                    route.sourceNodeId == selectedNodeId_ ||
                    route.destinationNodeId == selectedNodeId_
                        ? Accent : Route;
                Line(start, end, routeColor);
                if (layout_->canvas.zoom >= 0.55f)
                {
                    Label(
                        route.outcome,
                        (start.x + end.x) * 0.5f - 42.0f,
                        (start.y + end.y) * 0.5f - 15.0f,
                        8,
                        routeColor,
                        cmd);
                }
            }

            for (const auto& node : model_->Nodes())
            {
                const auto* pos = FindLayout(node.id);
                if (!pos) continue;
                const XMFLOAT4 b = NodeScreenBounds(*pos);
                const bool selected = node.id == selectedNodeId_;
                const bool start = node.kind == bridge::FlowNodeKind::GameStart;
                Rect(b.x, b.y, b.z, b.w, selected ? wi::Color(28, 20, 16, 255) : Raised, cmd);
                Rect(b.x, b.y, b.z, 1.0f, selected || start ? Accent : Border, cmd);
                Rect(b.x, b.y + b.w - 1.0f, b.z, 1.0f, selected || start ? Accent : Border, cmd);
                Rect(b.x, b.y, 1.0f, b.w, selected || start ? Accent : Border, cmd);
                Rect(b.x + b.z - 1.0f, b.y, 1.0f, b.w, selected || start ? Accent : Border, cmd);
                if (layout_->canvas.zoom >= 0.45f)
                {
                    Label(KindLabel(node.kind), b.x + 10, b.y + 10, 8, start ? Accent : Muted, cmd);
                    Label(node.name, b.x + 10, b.y + 31, 11, Text, cmd);
                }
            }
        }
        else
        {
            for (const auto& track : journeyModel_.Tracks())
            {
                if (track.cardNodeIds.empty()) continue;
                const auto* firstCard = journeyModel_.FindCard(track.cardNodeIds.front());
                const auto* lastCard = journeyModel_.FindCard(track.cardNodeIds.back());
                if (!firstCard || !lastCard) continue;
                const XMFLOAT4 first = JourneyCardScreenBounds(*firstCard);
                const XMFLOAT4 last = JourneyCardScreenBounds(*lastCard);
                const float railY = first.y + first.w * 0.5f;
                Line(XMFLOAT2(first.x - 18.0f, railY),
                    XMFLOAT2(last.x + last.z + 18.0f, railY), JourneyRail);
                Label(
                    track.mainTrack ? "MAIN JOURNEY" :
                        (track.detached ? "DETACHED" : "ALTERNATE"),
                    first.x, first.y - 20.0f, 8,
                    track.mainTrack ? Accent : Muted, cmd);
            }

            for (const auto& card : journeyModel_.Cards())
            {
                const auto* node = model_->FindNode(card.nodeId);
                if (!node) continue;
                const XMFLOAT4 b = JourneyCardScreenBounds(card);
                const bool selected = node->id == selectedNodeId_;
                const bool start = node->kind == bridge::FlowNodeKind::GameStart;
                BorderedRect(b.x, b.y, b.z, b.w,
                    selected ? wi::Color(28, 20, 16, 255) : Raised,
                    selected || start ? Accent : Border, cmd);
                Rect(b.x, b.y, b.z, std::max(2.0f, b.w * 0.24f),
                    start ? wi::Color(48, 27, 18, 255) : wi::Color(18, 27, 32, 255), cmd);
                if (layout_->journeyCanvas.zoom >= 0.42f)
                {
                    Label(KindLabel(node->kind), b.x + 12.0f, b.y + 10.0f, 8,
                        start ? Accent : Muted, cmd);
                    Label(node->name, b.x + 12.0f, b.y + 38.0f, 12, Text, cmd);
                    const std::string reference = node->kind == bridge::FlowNodeKind::Level
                        ? node->scenePathHint
                        : (node->kind == bridge::FlowNodeKind::Screen
                            ? node->screenPathHint : std::string{});
                    if (!reference.empty())
                        Label(Shorten(reference, 34), b.x + 12.0f, b.y + 64.0f, 8, Muted, cmd);
                    Label(
                        std::to_string(node->outgoingRouteIds.size()) + " EXIT" +
                            (node->outgoingRouteIds.size() == 1 ? "" : "S"),
                        b.x + 12.0f, b.y + b.w - 24.0f, 8,
                        node->outgoingRouteIds.empty() ? Muted : Accent, cmd);
                }
            }
        }

        const float inspectorX = graphRight + 14.0f;
        Label("INSPECTOR", inspectorX, translation.y + HeaderHeight + 14.0f, 12, Text, cmd);
        if (const auto* node = FindDocumentNode(selectedNodeId_))
        {
            Label(KindLabel(node->kind), inspectorX, translation.y + HeaderHeight + 35.0f, 8,
                node->kind == bridge::FlowNodeKind::GameStart ? Accent : Muted, cmd);
            if (layout_->activeView == bridge::StoryFlowViewMode::Graph)
            {
                Label("STABLE ID // " + Shorten(node->id, 30), inspectorX,
                    translation.y + HeaderHeight + 181.0f, 8, Muted, cmd);
                if (node->kind == bridge::FlowNodeKind::Level)
                {
                    Label("SCENE // " + Shorten(node->scenePathHint, 35), inspectorX,
                        translation.y + HeaderHeight + 198.0f, 8, Muted, cmd);
                }
                else if (node->kind == bridge::FlowNodeKind::Screen)
                {
                    Label("SCREEN // " + Shorten(node->screenPathHint, 35), inspectorX,
                        translation.y + HeaderHeight + 198.0f, 8, Muted, cmd);
                }
            }
            if (layout_->activeView == bridge::StoryFlowViewMode::Journey)
            {
                Label("EXITS // CLICK TO EDIT", inspectorX,
                    translation.y + HeaderHeight + 134.0f, 8, Muted, cmd);
                const auto* nodeView = model_->FindNode(node->id);
                const std::size_t count = std::min<std::size_t>(
                    nodeView ? nodeView->outgoingRouteIds.size() : 0, 5);
                for (std::size_t i = 0; i < count; ++i)
                {
                    const auto* route = model_->FindRoute(
                        nodeView->outgoingRouteIds[i]);
                    const auto* destination = route
                        ? model_->FindNode(route->destinationNodeId) : nullptr;
                    if (!route) continue;
                    const float y = translation.y + HeaderHeight + 156.0f +
                        static_cast<float>(i) * 29.0f;
                    BorderedRect(inspectorX, y,
                        std::max(1.0f, width_ - GraphWidth() - 28.0f), 24.0f,
                        Surface, Border, cmd);
                    Label(Shorten(route->outcome + " -> " +
                        (destination ? destination->name : route->destinationNodeId), 38),
                        inspectorX + 7.0f, y + 7.0f, 8, Text, cmd);
                }
            }
        }
        else if (const auto* route = FindDocumentRoute(selectedRouteId_))
        {
            Label("ROUTE", inspectorX, translation.y + HeaderHeight + 35.0f, 8, Accent, cmd);
            Label("ID // " + Shorten(route->id, 31), inspectorX,
                translation.y + HeaderHeight + 235.0f, 8, Muted, cmd);
            Label("CONDITIONS // " + std::to_string(route->conditions.size()), inspectorX,
                translation.y + HeaderHeight + 252.0f, 8, Muted, cmd);
        }
        else
        {
            Label("SELECT A NODE OR ROUTE", inspectorX,
                translation.y + HeaderHeight + 38.0f, 9, Muted, cmd);
        }

        Label("ADD TERMINAL DESTINATION", inspectorX,
            translation.y + HeaderHeight + 334.0f, 8, Muted, cmd);
        Label("LEVEL/SCREEN CREATION USES GOVERNED LIFECYCLE CONTROLS", inspectorX,
            translation.y + HeaderHeight + 460.0f, 7, Muted, cmd);

        Label("VALIDATION", inspectorX,
            translation.y + HeaderHeight + 498.0f, 8, Muted, cmd);
        if (model_->Diagnostics().empty())
        {
            Label("NO PRESENTATION DIAGNOSTICS", inspectorX,
                translation.y + HeaderHeight + 516.0f, 8, Muted, cmd);
        }
        else
        {
            float diagnosticY = translation.y + HeaderHeight + 516.0f;
            const std::size_t count = std::min<std::size_t>(3, model_->Diagnostics().size());
            for (std::size_t i = 0; i < count; ++i)
            {
                const auto& diagnostic = model_->Diagnostics()[i];
                const wi::Color color = diagnostic.severity == bridge::StoryFlowDiagnosticSeverity::Error
                    ? Error : Muted;
                Label(Shorten(diagnostic.code + " // " + diagnostic.message, 42),
                    inspectorX, diagnosticY, 8, color, cmd);
                diagnosticY += 17.0f;
            }
        }

        Label("STATUS // " + Shorten(statusMessage_, 42), inspectorX,
            translation.y + scale.y - 28.0f, 8,
            statusMessage_.find("FAILED") != std::string::npos ||
            statusMessage_.find("REJECTED") != std::string::npos ||
            statusMessage_.find("ERROR") != std::string::npos
                ? Error : Muted,
            cmd);

        RenderAuthoringControls(canvas, cmd);
    }
}
