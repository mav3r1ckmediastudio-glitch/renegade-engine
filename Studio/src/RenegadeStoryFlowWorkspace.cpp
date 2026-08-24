#include "RenegadeStoryFlowWorkspace.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <utility>

#include "renegade/bridge/StoryFlowInteractionPolicy.h"
#include "renegade/bridge/ScreenService.h"
#include "RenegadeStoryFlowJourneyLayout.h"

namespace
{
    constexpr float HeaderHeight = 182.0f;
    constexpr float Padding = 18.0f;
    constexpr float NodeWidth = 210.0f;
    constexpr float NodeHeight = 112.0f;
    constexpr float JourneyCardWidth = 164.0f;
    constexpr float JourneyCardHeight = 214.0f;
    constexpr float JourneyColumnSpacing = 176.0f;
    constexpr float JourneyTrackTop = 22.0f;
    constexpr float JourneyBranchCardWidth = 214.0f;
    constexpr float JourneyBranchCardHeight = 72.0f;
    constexpr float JourneyBranchColumnSpacing = 228.0f;
    constexpr float JourneyBranchIndent = 244.0f;
    constexpr float JourneyBranchTrackTop = 310.0f;
    constexpr float JourneyBranchTrackSpacing = 92.0f;
    constexpr float MinZoom = 0.82f;
    constexpr float MaxZoom = 1.18f;

    constexpr wi::Color Surface(7, 11, 14, 255);
    constexpr wi::Color Raised(13, 19, 23, 255);
    constexpr wi::Color Border(38, 52, 61, 255);
    constexpr wi::Color Text(244, 244, 244, 255);
    constexpr wi::Color Muted(132, 143, 149, 255);
    constexpr wi::Color Accent(210, 91, 29, 255);
    constexpr wi::Color Error(229, 92, 92, 255);

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

    void RoundedRect(float x, float y, float w, float h, float radius,
        wi::Color color, wi::graphics::CommandList cmd)
    {
        if (w <= 0.0f || h <= 0.0f) return;
        wi::image::Params params(x, y, w, h, color);
        params.blendFlag = wi::enums::BLENDMODE_ALPHA;
        params.enableCornerRounding();
        for (auto& corner : params.corners_rounding)
        {
            corner.radius = radius;
            corner.segments = 8;
        }
        wi::image::Draw(nullptr, params, cmd);
    }

    wi::Color JourneyRoleColor(
        const renegade::studio::JourneyBranchRole role)
    {
        using renegade::studio::JourneyBranchRole;
        switch (role)
        {
        case JourneyBranchRole::Options: return wi::Color(143, 73, 205, 255);
        case JourneyBranchRole::LoadSave: return wi::Color(53, 166, 174, 255);
        case JourneyBranchRole::Failure: return wi::Color(205, 67, 61, 255);
        case JourneyBranchRole::Detached: return wi::Color(210, 157, 62, 255);
        case JourneyBranchRole::Custom: return wi::Color(65, 158, 230, 255);
        case JourneyBranchRole::Main:
        default: return wi::Color(113, 205, 111, 255);
        }
    }

    void DrawResourceCover(const wi::Resource& resource,
        const XMFLOAT4& bounds, wi::graphics::CommandList cmd)
    {
        if (!resource.IsValid()) return;
        const auto desc = resource.GetTexture().GetDesc();
        XMFLOAT4 sourceRect(
            0.0f, 0.0f,
            static_cast<float>(desc.width),
            static_cast<float>(desc.height));
        if (desc.width > 0 && desc.height > 0)
        {
            const float sourceAspect = static_cast<float>(desc.width) /
                static_cast<float>(desc.height);
            const float targetAspect = bounds.z / bounds.w;
            if (sourceAspect > targetAspect)
            {
                sourceRect.z = static_cast<float>(desc.height) * targetAspect;
                sourceRect.x =
                    (static_cast<float>(desc.width) - sourceRect.z) * 0.5f;
            }
            else
            {
                sourceRect.w = static_cast<float>(desc.width) / targetAspect;
                sourceRect.y =
                    (static_cast<float>(desc.height) - sourceRect.w) * 0.5f;
            }
        }
        wi::image::Params params(
            bounds.x, bounds.y, bounds.z, bounds.w);
        params.blendFlag = wi::enums::BLENDMODE_ALPHA;
        params.sampleFlag = wi::image::SAMPLEMODE_CLAMP;
        params.drawRect = sourceRect;
        wi::image::Draw(&resource.GetTexture(), params, cmd);
    }

    void Label(const std::string& value, float x, float y, int size,
        wi::Color color, wi::graphics::CommandList cmd)
    {
        wi::font::Params params(x, y, size, wi::font::WIFALIGN_LEFT,
            wi::font::WIFALIGN_TOP, color, wi::Color::Transparent());
        params.bolden = 0.14f;
        wi::font::Draw(value, params, cmd);
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
        projectRoot_.clear();
        journeyThumbnailResources_.clear();
        journeyCardObjects_.clear();
        journeyLaneObjects_.clear();
        collapsedJourneyTracks_.clear();
        pendingJourneyExitIndex_ = MaxJourneyInspectorExits;
        pendingJourneyDestinationIndex_ = 0;
        pendingAddJourneyAction_ = false;

        if (session_ && model_ && layout_ && session_->IsLoaded() && model_->IsLoaded())
        {
            // Journey is the locked project-home surface for this recovery.
            // Graph remains synchronized and intact, but never steals the
            // default workspace from a persisted legacy view flag.
            layout_->activeView = bridge::StoryFlowViewMode::Journey;
            // Layouts written by the rejected legacy canvas allowed Journey to
            // reopen as an unreadable 20-35% strip. Gate 9D uses bounded
            // semantic zoom: migrate that obsolete state once, then preserve
            // all valid modern Journey navigation normally.
            if (layout_->journeyCanvas.zoom < MinZoom ||
                layout_->journeyCanvas.zoom > MaxZoom)
            {
                layout_->journeyCanvas.zoom = 1.0f;
                layout_->journeyCanvas.panX = 0.0f;
                layout_->journeyCanvas.panY = 0.0f;
            }
            std::string thumbnailError;
            if (!bridge::StoryFlowJourneyThumbnailService::ResolveProjectRootFromFlowPath(
                    session_->FilePath(), projectRoot_, thumbnailError))
            {
                projectRoot_.clear();
                wi::backlog::post(
                    "Renegade Story Flow Gate 9B: Level thumbnails unavailable: " +
                        thumbnailError,
                    wi::backlog::LogLevel::Warning);
            }

            const bool journeyReady = RebuildJourneyProjection();
            selectedNodeId_ = model_->GameStartNodeId();
            if (journeyReady)
            {
                statusMessage_ = "JOURNEY READY // GRAPH SYNCHRONIZED";
                if (projectRoot_.empty())
                    statusMessage_ += " // THUMBNAILS UNAVAILABLE";
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
        journeyCardObjects_.clear();
        journeyLaneObjects_.clear();
        journeyThumbnailResources_.clear();
        collapsedJourneyTracks_.clear();
        pendingJourneyExitIndex_ = MaxJourneyInspectorExits;
        pendingJourneyDestinationIndex_ = 0;
        pendingAddJourneyAction_ = false;
        projectRoot_.clear();
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

    void RenegadeStoryFlowWorkspace::OnSemanticChanged(
        std::function<void()> callback)
    {
        semanticChanged_ = std::move(callback);
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
        const bool mainCard = card.trackIndex == 0;
        std::size_t trackStartColumn = card.columnIndex - card.sequenceIndex;
        const auto track = std::find_if(
            journeyModel_.Tracks().begin(), journeyModel_.Tracks().end(),
            [&](const bridge::StoryFlowJourneyTrack& item)
            {
                return item.index == card.trackIndex;
            });
        if (track != journeyModel_.Tracks().end())
            trackStartColumn = track->startColumn;
        const float x = mainCard
            ? static_cast<float>(card.columnIndex) * JourneyColumnSpacing
            : std::max(JourneyBranchIndent,
                static_cast<float>(trackStartColumn) * JourneyColumnSpacing) +
                static_cast<float>(card.sequenceIndex) * JourneyBranchColumnSpacing;
        const float y = mainCard
            ? JourneyTrackTop
            : JourneyBranchTrackTop +
                static_cast<float>(card.trackIndex - 1) * JourneyBranchTrackSpacing;
        const XMFLOAT2 position = CanvasToScreen(x, y);
        const float zoom = layout_ ? layout_->journeyCanvas.zoom : 1.0f;
        return XMFLOAT4(
            position.x + (offset ? offset->offsetX * zoom : 0.0f),
            position.y + (offset ? offset->offsetY * zoom : 0.0f),
            (mainCard ? JourneyCardWidth : JourneyBranchCardWidth) * zoom,
            (mainCard ? JourneyCardHeight : JourneyBranchCardHeight) * zoom);
    }

    wi::graphics::Rect
    RenegadeStoryFlowWorkspace::JourneyCanvasScissorRect() const noexcept
    {
        wi::graphics::Rect clip;
        clip.left = static_cast<std::int32_t>(std::floor(translation.x));
        clip.top = static_cast<std::int32_t>(
            std::floor(translation.y + HeaderHeight));
        clip.right = static_cast<std::int32_t>(
            std::ceil(translation.x + GraphWidth()));
        clip.bottom = static_cast<std::int32_t>(
            std::ceil(translation.y + scale.y));
        return clip;
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
        if (layout_ && layout_->activeView == bridge::StoryFlowViewMode::Journey)
        {
            const auto shell = renegade::studio::ComputeJourneyShellLayout(
                width_ + translation.x, height_);
            return std::max(1.0f, shell.inspector.x - translation.x);
        }
        const float reserved = std::min(InspectorWidth, width_ * 0.42f);
        return std::max(1.0f, width_ - reserved);
    }

    bool RenegadeStoryFlowWorkspace::PointerInsideGraph(const XMFLOAT4& p) const noexcept
    {
        return p.x >= translation.x && p.y >= translation.y + HeaderHeight &&
            p.x < translation.x + GraphWidth() && p.y < translation.y + scale.y;
    }

    bridge::StableId RenegadeStoryFlowWorkspace::HitTestRoute(
        const XMFLOAT4&) const
    {
        // Graph route hit-testing is owned exclusively by ImNodes from Gate 9D.
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

        // If the creator leaves Graph while a wire is selected, preserve useful
        // context by selecting the route's source card. Journey edits that
        // route's destination in the Inspector without exposing topology wires.
        if (view == bridge::StoryFlowViewMode::Journey && !selectedRouteId_.empty())
        {
            const auto* route = FindDocumentRoute(selectedRouteId_);
            selectedNodeId_ = route ? route->sourceNodeId : bridge::StableId{};
            selectedRouteId_.clear();
            RefreshInspectorControls();
            if (selectionChanged_) selectionChanged_(selectedNodeId_);
        }

        connectionSourceNodeId_.clear();
        reconnectRouteId_.clear();
        layout_->activeView = view;
        panning_ = false;
        nodeDragging_ = false;
        draggedNodeId_.clear();
        SetStatus(view == bridge::StoryFlowViewMode::Journey
            ? "JOURNEY VIEW // INSPECTOR ROUTING"
            : "GRAPH VIEW // EXACT SHARED TOPOLOGY");
        NotifyLayoutChanged();
    }

    bool RenegadeStoryFlowWorkspace::RebuildJourneyProjection()
    {
        if (!model_ || !model_->IsLoaded())
        {
            journeyModel_.Clear();
            journeyCardObjects_.clear();
            journeyLaneObjects_.clear();
            journeyThumbnailResources_.clear();
            return false;
        }
        std::string error;
        if (!journeyModel_.Build(*model_, error))
        {
            SetStatus("JOURNEY PROJECTION ERROR // " + error);
            return false;
        }
        RebuildJourneyObjects();
        RefreshJourneyThumbnailResources();
        return true;
    }

    void RenegadeStoryFlowWorkspace::RebuildJourneyObjects()
    {
        for (const auto& card : journeyModel_.Cards())
        {
            if (journeyCardObjects_.find(card.nodeId) != journeyCardObjects_.end())
                continue;
            auto object = std::make_unique<RenegadeStoryFlowJourneyCard>();
            object->Create(card.nodeId);
            journeyCardObjects_.emplace(card.nodeId, std::move(object));
        }
        for (auto it = journeyCardObjects_.begin(); it != journeyCardObjects_.end();)
        {
            if (journeyModel_.FindCard(it->first) == nullptr)
                it = journeyCardObjects_.erase(it);
            else
                ++it;
        }

        for (const auto& track : journeyModel_.Tracks())
        {
            if (journeyLaneObjects_.find(track.index) != journeyLaneObjects_.end())
                continue;
            auto object = std::make_unique<RenegadeStoryFlowJourneyLane>();
            object->Create(track.index);
            journeyLaneObjects_.emplace(track.index, std::move(object));
        }
        for (auto it = journeyLaneObjects_.begin(); it != journeyLaneObjects_.end();)
        {
            const bool exists = std::any_of(
                journeyModel_.Tracks().begin(), journeyModel_.Tracks().end(),
                [&](const bridge::StoryFlowJourneyTrack& track)
                {
                    return track.index == it->first;
                });
            if (!exists)
                it = journeyLaneObjects_.erase(it);
            else
                ++it;
        }
    }

    std::string RenegadeStoryFlowWorkspace::JourneyCardSubtitle(
        const bridge::StoryFlowNodeView& node) const
    {
        try
        {
            if (node.kind == bridge::FlowNodeKind::Level)
            {
                const std::string stem = std::filesystem::u8path(node.scenePathHint)
                    .stem().generic_u8string();
                return stem.empty() ? "GOVERNED LEVEL" : stem;
            }
            if (node.kind == bridge::FlowNodeKind::Screen)
            {
                const std::string stem = std::filesystem::u8path(node.screenPathHint)
                    .stem().generic_u8string();
                return stem.empty() ? "GOVERNED SCREEN" : stem;
            }
        }
        catch (...)
        {
        }

        switch (node.kind)
        {
        case bridge::FlowNodeKind::GameStart: return "PROJECT ENTRY";
        case bridge::FlowNodeKind::CompleteGame: return "TERMINAL OUTCOME";
        case bridge::FlowNodeKind::ReturnToMainMenu: return "RETURN DESTINATION";
        case bridge::FlowNodeKind::Quit: return "APPLICATION EXIT";
        default: return {};
        }
    }

    void RenegadeStoryFlowWorkspace::RefreshJourneyThumbnailResources()
    {
        journeyThumbnailResources_.clear();
        if (projectRoot_.empty() || !model_)
            return;

        for (const auto& card : journeyModel_.Cards())
        {
            const auto* node = model_->FindNode(card.nodeId);
            if (!node || (node->kind != bridge::FlowNodeKind::Level &&
                    node->kind != bridge::FlowNodeKind::Screen))
                continue;

            std::string relativePath;
            std::string resolvedPath;
            std::string error;
            if (!journeyThumbnailService_.ResolveManaged(
                    projectRoot_, node->id,
                    relativePath, resolvedPath, error))
            {
                wi::backlog::post(
                    "Renegade Story Flow: thumbnail slot for '" + node->name +
                        "' is invalid: " + error,
                    wi::backlog::LogLevel::Warning);
                continue;
            }
            if (resolvedPath.empty() &&
                node->kind == bridge::FlowNodeKind::Screen && session_)
            {
                std::string screenPath;
                bridge::ScreenDocument screen;
                if (bridge::ResolveRuntimeScreenDocumentPath(
                        projectRoot_, session_->ProjectId(),
                        node->screenDocumentId, node->screenPathHint,
                        screenPath, error) &&
                    bridge::ReadScreenDocument(
                        screenPath, session_->ProjectId(), screen, error))
                {
                    const bridge::ScreenWidget* largestImage = nullptr;
                    float largestArea = 0.0f;
                    for (const auto& widget : screen.widgets)
                    {
                        if (!widget.visible ||
                            widget.kind != bridge::ScreenWidgetKind::Image ||
                            widget.resourcePath.empty())
                            continue;
                        const float area = widget.rect.width * widget.rect.height;
                        if (area > largestArea)
                        {
                            largestArea = area;
                            largestImage = &widget;
                        }
                    }
                    if (largestImage)
                    {
                        std::string screenResource;
                        if (bridge::ResolveScreenResourcePath(
                                projectRoot_, largestImage->resourcePath,
                                screenResource, error))
                        {
                            resolvedPath = std::move(screenResource);
                            relativePath = largestImage->resourcePath;
                        }
                    }
                }
            }
            if (resolvedPath.empty()) continue;

            wi::Resource resource = wi::resourcemanager::Load(resolvedPath);
            if (!resource.IsValid())
            {
                wi::backlog::post(
                    "Renegade Story Flow: could not load Journey thumbnail '" +
                        relativePath + "'.",
                    wi::backlog::LogLevel::Warning);
                continue;
            }
            journeyThumbnailResources_.emplace(node->id, std::move(resource));
        }
    }

    void RenegadeStoryFlowWorkspace::UpdateJourneyObjects(
        const wi::Canvas& canvas,
        const float dt)
    {
        if (!model_ || !layout_ ||
            layout_->activeView != bridge::StoryFlowViewMode::Journey)
        {
            return;
        }

        const wi::graphics::Rect journeyClip = JourneyCanvasScissorRect();

        std::size_t visibleLane = 0;
        for (const auto& track : journeyModel_.Tracks())
        {
            auto laneIt = journeyLaneObjects_.find(track.index);
            if (laneIt == journeyLaneObjects_.end() || track.cardNodeIds.empty())
                continue;
            if (hideDetached_ && track.detached)
            {
                laneIt->second->SetVisible(false);
                continue;
            }

            float minX = std::numeric_limits<float>::max();
            float minY = std::numeric_limits<float>::max();
            float maxX = std::numeric_limits<float>::lowest();
            float maxY = std::numeric_limits<float>::lowest();
            for (const auto& nodeId : track.cardNodeIds)
            {
                const auto* card = journeyModel_.FindCard(nodeId);
                if (!card) continue;
                const XMFLOAT4 bounds = JourneyCardScreenBounds(*card);
                minX = std::min(minX, bounds.x);
                minY = std::min(minY, bounds.y);
                maxX = std::max(maxX, bounds.x + bounds.z);
                maxY = std::max(maxY, bounds.y + bounds.w);
            }
            if (minX > maxX || minY > maxY)
                continue;

            auto& lane = *laneIt->second;
            JourneyBranchRole role = track.mainTrack
                ? JourneyBranchRole::Main
                : (track.detached
                    ? JourneyBranchRole::Detached
                    : JourneyBranchRole::Custom);
            if (!track.sourceRouteId.empty())
            {
                const auto* sourceRoute = FindDocumentRoute(track.sourceRouteId);
                if (sourceRoute)
                    role = JourneyRoleForOutcome(sourceRoute->outcome, track.detached);
            }
            else if (track.detached && !track.cardNodeIds.empty())
            {
                const auto* first = FindDocumentNode(track.cardNodeIds.front());
                if (first)
                {
                    const JourneyBranchRole inferred =
                        JourneyRoleForOutcome(first->name, false);
                    if (inferred != JourneyBranchRole::Custom)
                        role = inferred;
                }
            }
            const float laneX = track.mainTrack
                ? translation.x + 18.0f
                : std::min(minX - 22.0f,
                    CanvasToScreen(JourneyBranchIndent, 0.0f).x - 22.0f);
            const float laneRight = translation.x + GraphWidth() - 18.0f;
            lane.SetBounds(XMFLOAT4(
                laneX,
                minY - (track.mainTrack ? 34.0f : 4.0f),
                std::max(1.0f, laneRight - laneX),
                std::max(1.0f, maxY - minY +
                    (track.mainTrack ? 52.0f : 8.0f))));
            lane.SetPresentation(
                visibleLane++, track.mainTrack, track.detached, role,
                collapsedJourneyTracks_.find(track.index) !=
                    collapsedJourneyTracks_.end(), {});
            lane.SetVisible(true);
            lane.Update(canvas, dt);
            // These presentation objects are rendered manually rather than as
            // parented wiGUI children. Restore the Journey viewport clip after
            // Widget::Update computes each object's own bounds so no lane can
            // paint through the fixed Inspector.
            lane.scissorRect = journeyClip;
        }

        for (const auto& card : journeyModel_.Cards())
        {
            const auto* node = model_->FindNode(card.nodeId);
            const auto objectIt = journeyCardObjects_.find(card.nodeId);
            if (!node || objectIt == journeyCardObjects_.end())
                continue;
            if ((hideDetached_ && !card.reachableFromStart) ||
                collapsedJourneyTracks_.find(card.trackIndex) !=
                    collapsedJourneyTracks_.end())
            {
                objectIt->second->SetVisible(false);
                continue;
            }

            bool hasError = false;
            bool hasWarning = !card.reachableFromStart;
            for (const auto& diagnostic : model_->Diagnostics())
            {
                if (diagnostic.nodeId != node->id)
                    continue;
                if (diagnostic.severity == bridge::StoryFlowDiagnosticSeverity::Error)
                    hasError = true;
                else if (diagnostic.severity == bridge::StoryFlowDiagnosticSeverity::Warning)
                    hasWarning = true;
            }

            wi::Resource thumbnail;
            const auto thumbnailIt = journeyThumbnailResources_.find(node->id);
            if (thumbnailIt != journeyThumbnailResources_.end())
                thumbnail = thumbnailIt->second;

            auto& object = *objectIt->second;
            object.SetBounds(JourneyCardScreenBounds(card));
            object.SetPresentation(
                node->kind,
                card.sequenceIndex,
                node->name,
                JourneyCardSubtitle(*node),
                node->outgoingRouteIds.size(),
                node->id == selectedNodeId_,
                hasError,
                hasWarning,
                card.trackIndex == 0,
                layout_->journeyCanvas.zoom >= MinZoom,
                (node->kind == bridge::FlowNodeKind::Level ||
                    node->kind == bridge::FlowNodeKind::Screen) &&
                    !projectRoot_.empty(),
                std::move(thumbnail));
            object.SetVisible(true);
            object.Update(canvas, dt);
            object.scissorRect = journeyClip;
        }
    }

    void RenegadeStoryFlowWorkspace::ChooseLevelThumbnail(
        const bridge::StableId& nodeId)
    {
        const auto* node = model_ ? model_->FindNode(nodeId) : nullptr;
        if (!node || (node->kind != bridge::FlowNodeKind::Level &&
                node->kind != bridge::FlowNodeKind::Screen))
        {
            SetStatus("THUMBNAIL REJECTED // SELECT A LEVEL OR SCREEN CARD");
            return;
        }
        if (projectRoot_.empty() || !session_ || !session_->IsLoaded())
        {
            SetStatus("THUMBNAIL REJECTED // PROJECT RESOURCE ROOT UNAVAILABLE");
            return;
        }

        SelectNode(nodeId);
        previousClickedNodeId_.clear();
        secondsSincePreviousNodeClick_ = 1000.0f;
        const std::string expectedRoot = projectRoot_;
        const bridge::StableId expectedFlowId =
            session_->Document().envelope.documentId;

        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::OPEN;
        params.description = "Choose Journey Level Thumbnail";
        params.extensions = {"jpg", "jpeg", "png", "bmp", "tga"};
        wi::helper::FileDialog(
            params,
            [this, nodeId, expectedRoot, expectedFlowId](
                const std::string& selectedPath)
            {
                if (selectedPath.empty())
                    return;
                if (!session_ || !session_->IsLoaded() || !model_ ||
                    projectRoot_ != expectedRoot ||
                    session_->Document().envelope.documentId != expectedFlowId ||
                    model_->FindNode(nodeId) == nullptr)
                {
                    SetStatus("THUMBNAIL CANCELLED // STORY FLOW CONTEXT CHANGED");
                    return;
                }

                const auto imported = journeyThumbnailService_.Import(
                    expectedRoot, nodeId, selectedPath);
                if (!imported.succeeded)
                {
                    SetStatus("THUMBNAIL FAILED // " + imported.message);
                    return;
                }

                const auto old = journeyThumbnailResources_.find(nodeId);
                if (old != journeyThumbnailResources_.end() && old->second.IsValid())
                    old->second.SetOutdated();

                wi::Resource resource = wi::resourcemanager::Load(imported.resolvedPath);
                if (!resource.IsValid())
                {
                    journeyThumbnailResources_.erase(nodeId);
                    SetStatus(
                        "THUMBNAIL IMPORTED // PREVIEW LOAD FAILED // " +
                        imported.relativePath);
                    return;
                }
                journeyThumbnailResources_[nodeId] = std::move(resource);
                SetStatus("JOURNEY THUMBNAIL UPDATED // PROJECT RESOURCE COMMITTED");
            });
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
                const bool mainCard = card.trackIndex == 0;
                std::size_t trackStartColumn = card.columnIndex - card.sequenceIndex;
                const auto track = std::find_if(
                    journeyModel_.Tracks().begin(), journeyModel_.Tracks().end(),
                    [&](const bridge::StoryFlowJourneyTrack& item)
                    {
                        return item.index == card.trackIndex;
                    });
                if (track != journeyModel_.Tracks().end())
                    trackStartColumn = track->startColumn;
                const float x = (mainCard
                    ? static_cast<float>(card.columnIndex) * JourneyColumnSpacing
                    : std::max(JourneyBranchIndent,
                        static_cast<float>(trackStartColumn) * JourneyColumnSpacing) +
                        static_cast<float>(card.sequenceIndex) * JourneyBranchColumnSpacing) +
                    (offset ? offset->offsetX : 0.0f);
                const float y = (mainCard
                    ? JourneyTrackTop
                    : JourneyBranchTrackTop +
                        static_cast<float>(card.trackIndex - 1) *
                            JourneyBranchTrackSpacing) +
                    (offset ? offset->offsetY : 0.0f);
                const float cardWidth = mainCard
                    ? JourneyCardWidth : JourneyBranchCardWidth;
                const float cardHeight = mainCard
                    ? JourneyCardHeight : JourneyBranchCardHeight;
                minX = std::min(minX, x); minY = std::min(minY, y);
                maxX = std::max(maxX, x + cardWidth);
                maxY = std::max(maxY, y + cardHeight);
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

    void RenegadeStoryFlowWorkspace::AdjustJourneyZoom(const float factor)
    {
        if (!layout_ || layout_->activeView != bridge::StoryFlowViewMode::Journey ||
            factor <= 0.0f)
        {
            return;
        }
        auto& canvas = layout_->journeyCanvas;
        const float oldZoom = std::clamp(canvas.zoom, MinZoom, MaxZoom);
        const float newZoom = std::clamp(oldZoom * factor, MinZoom, MaxZoom);
        if (std::abs(newZoom - oldZoom) < 0.0001f)
            return;

        const float viewportCenterX = GraphWidth() * 0.5f;
        const float viewportCenterY = (height_ - HeaderHeight) * 0.5f;
        const float canvasCenterX =
            (viewportCenterX - Padding - canvas.panX) / oldZoom;
        const float canvasCenterY =
            (viewportCenterY - Padding - canvas.panY) / oldZoom;
        canvas.zoom = newZoom;
        canvas.panX = viewportCenterX - Padding - canvasCenterX * newZoom;
        canvas.panY = viewportCenterY - Padding - canvasCenterY * newZoom;
        NotifyLayoutChanged();
        SetStatus("JOURNEY ZOOM // " +
            std::to_string(static_cast<int>(std::round(newZoom * 100.0f))) + "%");
    }

    void RenegadeStoryFlowWorkspace::SetJourneyZoom(const float zoom)
    {
        if (!layout_ || layout_->activeView != bridge::StoryFlowViewMode::Journey)
            return;
        const float current = std::clamp(
            layout_->journeyCanvas.zoom, MinZoom, MaxZoom);
        const float target = std::clamp(zoom, MinZoom, MaxZoom);
        if (std::abs(target - current) < 0.0001f) return;
        AdjustJourneyZoom(target / current);
    }

    void RenegadeStoryFlowWorkspace::SaveJourney()
    {
        SaveFlow();
    }

    void RenegadeStoryFlowWorkspace::ToggleJourneyFilter()
    {
        hideDetached_ = !hideDetached_;
        SetStatus(hideDetached_
            ? "FILTER // DETACHED JOURNEYS HIDDEN"
            : "FILTER // ALL JOURNEYS VISIBLE");
    }

    void RenegadeStoryFlowWorkspace::UndoJourney()
    {
        UndoFlow();
    }

    void RenegadeStoryFlowWorkspace::RedoJourney()
    {
        RedoFlow();
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
            y = JourneyTrackTop +
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
        SetStatus("NODE SELECTED // VIEW CENTERED");
    }

    bool RenegadeStoryFlowWorkspace::FindAndFocusJourneyNode(
        const std::string& query)
    {
        if (!model_ || query.empty()) return false;
        std::string loweredQuery = query;
        std::transform(loweredQuery.begin(), loweredQuery.end(),
            loweredQuery.begin(), [](const unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        for (const auto& node : model_->Nodes())
        {
            std::string loweredName = node.name;
            std::transform(loweredName.begin(), loweredName.end(),
                loweredName.begin(), [](const unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                });
            if (loweredName.find(loweredQuery) == std::string::npos)
                continue;
            SelectAndFocusNode(node.id);
            SetStatus("SEARCH // " + node.name);
            return true;
        }
        SetStatus("SEARCH // NO JOURNEY DESTINATION MATCHED '" + query + "'");
        return false;
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

        // Retained as private compatibility objects while Gate 9D removes the
        // old canvas path. Direct ImNodes sockets are the only surfaced route
        // connect/reconnect interaction.
        connectButton_.Create("Story Flow Connect");
        connectButton_.SetText("CONNECT");
        connectButton_.SetVisible(false);
        connectButton_.SetEnabled(false);

        nodeNameInput_.Create("Story Flow Node Name");
        nodeNameInput_.SetDescription("DISPLAY NAME  ");
        nodeNameInput_.SetPlaceholder("Destination name...");
        nodeNameInput_.SetCancelInputEnabled(false);
        nodeNameInput_.OnInputAccepted([this](const wi::gui::EventArgs&) { ApplySelectedNode(); });

        applyNodeButton_.Create("Story Flow Apply Node");
        applyNodeButton_.SetText("APPLY NAME");
        applyNodeButton_.OnClick([this](const wi::gui::EventArgs&) { ApplySelectedNode(); });

        deleteNodeButton_.Create("Story Flow Delete Node");
        deleteNodeButton_.SetText("DELETE NODE");
        deleteNodeButton_.SetTooltip("Delete this Graph node and its connected routes as one semantic edit.");
        deleteNodeButton_.OnClick([this](const wi::gui::EventArgs&) { DeleteSelectedNode(); });

        openDestinationButton_.Create("Story Flow Open Selected Destination");
        openDestinationButton_.SetText("OPEN EDITOR");
        openDestinationButton_.SetTooltip(
            "Open the selected governed Level or Screen editor.");
        openDestinationButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            if (nodeActivated_ && !selectedNodeId_.empty())
                nodeActivated_(selectedNodeId_);
        });

        addJourneyActionButton_.Create("Story Flow Add Journey Action");
        addJourneyActionButton_.SetText("+ ADD ACTION");
        addJourneyActionButton_.SetTooltip(
            "Create a governed exit and configure its destination here in the Inspector.");
        addJourneyActionButton_.SetColor(Raised, wi::gui::IDLE);
        addJourneyActionButton_.SetColor(
            wi::Color(18, 45, 66, 255), wi::gui::FOCUS);
        addJourneyActionButton_.SetColor(
            wi::Color(25, 72, 105, 255), wi::gui::ACTIVE);
        addJourneyActionButton_.SetColor(Raised, wi::gui::DEACTIVATING);
        addJourneyActionButton_.font.params.color = Text;
        addJourneyActionButton_.font.params.shadowColor =
            wi::Color::Transparent();
        addJourneyActionButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAddJourneyAction_ = true;
        });

        for (std::size_t index = 0;
            index < journeyExitDestinationCombos_.size(); ++index)
        {
            auto& combo = journeyExitDestinationCombos_[index];
            combo.Create("Story Flow Journey Exit Destination " +
                std::to_string(index + 1));
            combo.SetTooltip(
                "Route this authored action directly to a governed Journey destination.");
            combo.OnSelect([this, index](const wi::gui::EventArgs& args)
            {
                pendingJourneyExitIndex_ = index;
                pendingJourneyDestinationIndex_ =
                    static_cast<std::size_t>(args.userdata);
            });
        }

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
        reconnectRouteButton_.SetVisible(false);
        reconnectRouteButton_.SetEnabled(false);

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
            static_cast<wi::gui::Widget*>(&openDestinationButton_),
            static_cast<wi::gui::Widget*>(&addJourneyActionButton_),
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
        for (auto& combo : journeyExitDestinationCombos_)
        {
            combo.SetShadowRadius(0.0f);
            combo.SetVisible(false);
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
        const float inspectorTop = translation.y + 108.0f;
        const float previewHeight = std::clamp(
            height_ * 0.16f, 100.0f, 150.0f);
        const float generalY =
            inspectorTop + 46.0f + previewHeight + 9.0f + 45.0f;
        const float y0 = layout_ &&
            layout_->activeView == bridge::StoryFlowViewMode::Journey
            ? generalY + 22.0f
            : translation.y + 350.0f;

        nodeNameInput_.SetPos(XMFLOAT2(inspectorX, y0));
        nodeNameInput_.SetSize(XMFLOAT2(fieldWidth, 28.0f));
        applyNodeButton_.SetPos(XMFLOAT2(inspectorX, y0 + 38.0f));
        applyNodeButton_.SetSize(XMFLOAT2(fieldWidth * 0.52f - 3.0f, 28.0f));
        deleteNodeButton_.SetPos(XMFLOAT2(inspectorX + fieldWidth * 0.52f + 3.0f, y0 + 38.0f));
        deleteNodeButton_.SetSize(XMFLOAT2(fieldWidth * 0.48f - 3.0f, 28.0f));
        openDestinationButton_.SetPos(XMFLOAT2(
            inspectorX, translation.y + height_ -
                (height_ >= 850.0f ? 167.0f : 55.0f)));
        openDestinationButton_.SetSize(XMFLOAT2(fieldWidth, 34.0f));

        addJourneyActionButton_.SetPos(XMFLOAT2(
            inspectorX + std::max(0.0f, fieldWidth - 91.0f),
            generalY + 104.0f));
        addJourneyActionButton_.SetSize(XMFLOAT2(91.0f, 23.0f));

        const float exitDestinationX = inspectorX + 133.0f;
        const float exitDestinationWidth = std::max(80.0f,
            fieldWidth - 133.0f);
        const float exitsY = generalY + 114.0f;
        for (std::size_t index = 0;
            index < journeyExitDestinationCombos_.size(); ++index)
        {
            journeyExitDestinationCombos_[index].SetPos(XMFLOAT2(
                exitDestinationX,
                exitsY + 20.0f + static_cast<float>(index) * 27.0f));
            journeyExitDestinationCombos_[index].SetSize(XMFLOAT2(
                exitDestinationWidth, 23.0f));
        }

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
        deleteRouteButton_.SetPos(XMFLOAT2(inspectorX, y0 + 152.0f));
        deleteRouteButton_.SetSize(XMFLOAT2(fieldWidth, 28.0f));

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
        const bool graphMode = loaded &&
            layout_->activeView == bridge::StoryFlowViewMode::Graph;

        // Journey's fixed concept header owns Save state and Undo/Redo. These
        // compatibility widgets remain available only to the frozen Graph
        // presentation so there is never an invisible clickable layer under
        // the native Journey toolbar.
        saveButton_.SetVisible(graphMode);
        undoButton_.SetVisible(graphMode);
        redoButton_.SetVisible(graphMode);
        connectButton_.SetVisible(false);
        connectButton_.SetEnabled(false);
        saveButton_.SetEnabled(graphMode && session_->IsDirty());
        undoButton_.SetEnabled(graphMode && session_->CanUndo());
        redoButton_.SetEnabled(graphMode && session_->CanRedo());

        const auto* selectedNode = FindDocumentNode(selectedNodeId_);
        const auto* selectedRoute = FindDocumentRoute(selectedRouteId_);

        const bool nodeSelected = loaded && selectedNode != nullptr;
        nodeNameInput_.SetVisible(nodeSelected);
        applyNodeButton_.SetVisible(nodeSelected && graphMode);
        deleteNodeButton_.SetVisible(nodeSelected && graphMode);
        deleteNodeButton_.SetEnabled(
            nodeSelected && graphMode &&
            selectedNode->kind != bridge::FlowNodeKind::GameStart);
        const bool activatableNode = nodeSelected &&
            (selectedNode->kind == bridge::FlowNodeKind::Level ||
                selectedNode->kind == bridge::FlowNodeKind::Screen);
        openDestinationButton_.SetVisible(
            activatableNode && layout_->activeView == bridge::StoryFlowViewMode::Journey);
        openDestinationButton_.SetEnabled(
            activatableNode && layout_->activeView == bridge::StoryFlowViewMode::Journey);

        const bool journeyMode = loaded &&
            layout_->activeView == bridge::StoryFlowViewMode::Journey;
        const auto* selectedNodeView = journeyMode && model_
            ? model_->FindNode(selectedNodeId_) : nullptr;
        const std::size_t inspectorExitCapacity = height_ < 800.0f
            ? std::size_t{4} : MaxJourneyInspectorExits;
        const std::size_t journeyExitCount = std::min<std::size_t>(
            selectedNodeView ? selectedNodeView->outgoingRouteIds.size() : 0,
            inspectorExitCapacity);
        addJourneyActionButton_.SetVisible(journeyMode && nodeSelected);
        addJourneyActionButton_.SetEnabled(
            journeyMode && journeyAddActionAvailable_);
        for (std::size_t index = 0;
            index < journeyExitDestinationCombos_.size(); ++index)
        {
            journeyExitDestinationCombos_[index].SetVisible(
                index < journeyExitCount);
            journeyExitDestinationCombos_[index].SetEnabled(
                index < journeyExitCount && journeyExitDestinationIds_.size() > 1);
        }

        const bool routeSelected = graphMode && selectedRoute != nullptr;
        routeOutcomeInput_.SetVisible(routeSelected);
        routeEntryInput_.SetVisible(routeSelected);
        routePriorityInput_.SetVisible(routeSelected);
        applyRouteButton_.SetVisible(routeSelected);
        reconnectRouteButton_.SetVisible(false);
        reconnectRouteButton_.SetEnabled(false);
        deleteRouteButton_.SetVisible(routeSelected);
        if (routeSelected)
        {
            const auto* destination = FindDocumentNode(selectedRoute->destinationNodeId);
            routeEntryInput_.SetEnabled(
                destination && destination->kind == bridge::FlowNodeKind::Level);
        }

        addCompleteButton_.SetVisible(graphMode);
        addReturnButton_.SetVisible(graphMode);
        addQuitButton_.SetVisible(graphMode);

        for (wi::gui::Widget* widget : {
            static_cast<wi::gui::Widget*>(&saveButton_),
            static_cast<wi::gui::Widget*>(&undoButton_),
            static_cast<wi::gui::Widget*>(&redoButton_),
            static_cast<wi::gui::Widget*>(&connectButton_),
            static_cast<wi::gui::Widget*>(&nodeNameInput_),
            static_cast<wi::gui::Widget*>(&applyNodeButton_),
            static_cast<wi::gui::Widget*>(&deleteNodeButton_),
            static_cast<wi::gui::Widget*>(&openDestinationButton_),
            static_cast<wi::gui::Widget*>(&addJourneyActionButton_),
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
        for (auto& combo : journeyExitDestinationCombos_)
        {
            if (combo.IsVisible()) combo.Update(canvas, dt);
        }
        if (pendingAddJourneyAction_)
        {
            pendingAddJourneyAction_ = false;
            AddJourneyAction();
        }
        if (pendingJourneyExitIndex_ < MaxJourneyInspectorExits)
        {
            const std::size_t exitIndex = pendingJourneyExitIndex_;
            const std::size_t destinationIndex =
                pendingJourneyDestinationIndex_;
            pendingJourneyExitIndex_ = MaxJourneyInspectorExits;
            pendingJourneyDestinationIndex_ = 0;
            RewireJourneyExit(exitIndex, destinationIndex);
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
            static_cast<const wi::gui::Widget*>(&openDestinationButton_),
            static_cast<const wi::gui::Widget*>(&addJourneyActionButton_),
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
        for (auto combo = journeyExitDestinationCombos_.rbegin();
            combo != journeyExitDestinationCombos_.rend(); ++combo)
        {
            if (combo->IsVisible()) combo->Render(canvas, cmd);
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
        RefreshJourneyExitControls();
    }

    void RenegadeStoryFlowWorkspace::RefreshJourneyExitControls()
    {
        journeyAddActionAvailable_ = false;
        journeyExitDestinationIds_.clear();
        for (auto& routeId : journeyExitRouteIds_) routeId.clear();
        for (auto& combo : journeyExitDestinationCombos_)
        {
            combo.ClearItems();
            combo.SetSelectedWithoutCallback(-1);
        }
        if (!model_ || !model_->IsLoaded() || selectedNodeId_.empty()) return;

        for (const auto& destination : model_->Nodes())
        {
            if (destination.kind == bridge::FlowNodeKind::GameStart)
                continue;
            journeyExitDestinationIds_.push_back(destination.id);
        }

        const auto* source = model_->FindNode(selectedNodeId_);
        if (!source) return;

        const auto* sourceDocument = FindDocumentNode(selectedNodeId_);
        if (sourceDocument && !IsTerminalKind(sourceDocument->kind) &&
            source->outgoingRouteIds.size() < MaxJourneyInspectorExits)
        {
            if (sourceDocument->kind == bridge::FlowNodeKind::GameStart)
            {
                journeyAddActionAvailable_ = source->outgoingRouteIds.empty();
            }
            else if (sourceDocument->kind == bridge::FlowNodeKind::Screen)
            {
                std::vector<std::string> authoredOutcomes;
                std::string ignoredError;
                if (QueryScreenOutcomes(
                        sourceDocument->id, authoredOutcomes, ignoredError))
                {
                    std::unordered_set<std::string> usedOutcomes;
                    for (const auto& routeId : source->outgoingRouteIds)
                    {
                        const auto* route = model_->FindRoute(routeId);
                        if (route) usedOutcomes.insert(route->outcome);
                    }
                    journeyAddActionAvailable_ = std::any_of(
                        authoredOutcomes.begin(), authoredOutcomes.end(),
                        [&](const std::string& outcome)
                        {
                            return usedOutcomes.find(outcome) ==
                                usedOutcomes.end();
                        });
                }
            }
            else
            {
                journeyAddActionAvailable_ = true;
            }
        }

        const std::size_t exitCount = std::min<std::size_t>(
            source->outgoingRouteIds.size(), MaxJourneyInspectorExits);
        for (std::size_t exitIndex = 0; exitIndex < exitCount; ++exitIndex)
        {
            auto& combo = journeyExitDestinationCombos_[exitIndex];
            journeyExitRouteIds_[exitIndex] = source->outgoingRouteIds[exitIndex];
            const auto* route = model_->FindRoute(source->outgoingRouteIds[exitIndex]);
            int selectedDestination = -1;
            for (std::size_t destinationIndex = 0;
                destinationIndex < journeyExitDestinationIds_.size(); ++destinationIndex)
            {
                const auto* destination = model_->FindNode(
                    journeyExitDestinationIds_[destinationIndex]);
                if (!destination) continue;
                combo.AddItem(Shorten(destination->name, 24),
                    static_cast<std::uint64_t>(destinationIndex));
                if (route && destination->id == route->destinationNodeId)
                    selectedDestination = static_cast<int>(destinationIndex);
            }
            combo.SetSelectedWithoutCallback(selectedDestination);
        }
    }

    void RenegadeStoryFlowWorkspace::RewireJourneyExit(
        const std::size_t exitIndex,
        const std::size_t destinationIndex)
    {
        if (!session_ || exitIndex >= journeyExitRouteIds_.size() ||
            destinationIndex >= journeyExitDestinationIds_.size())
        {
            return;
        }
        const bridge::StableId routeId = journeyExitRouteIds_[exitIndex];
        const auto* current = FindDocumentRoute(routeId);
        if (!current) return;

        bridge::FlowRoute replacement = *current;
        replacement.destinationNodeId =
            journeyExitDestinationIds_[destinationIndex];
        const auto* destination = FindDocumentNode(replacement.destinationNodeId);
        if (!destination || destination->kind != bridge::FlowNodeKind::Level)
            replacement.destinationEntry.clear();

        std::string error;
        if (!session_->UpdateRoute(routeId, std::move(replacement), error))
        {
            SetStatus("JOURNEY ROUTE REJECTED // " + error);
            RefreshJourneyExitControls();
            return;
        }
        RefreshPresentationAfterSemanticChange();
        SetStatus("JOURNEY EXIT ROUTED // UNSAVED FLOW CHANGE");
    }

    void RenegadeStoryFlowWorkspace::AddJourneyAction()
    {
        if (!session_ || !model_ || selectedNodeId_.empty()) return;
        const auto* source = FindDocumentNode(selectedNodeId_);
        const auto* sourceView = model_->FindNode(selectedNodeId_);
        if (!source || !sourceView || IsTerminalKind(source->kind))
        {
            SetStatus("ADD ACTION REJECTED // SELECT A NON-TERMINAL DESTINATION");
            return;
        }

        std::unordered_set<std::string> usedOutcomes;
        for (const auto& routeId : sourceView->outgoingRouteIds)
        {
            const auto* route = model_->FindRoute(routeId);
            if (route) usedOutcomes.insert(route->outcome);
        }

        std::string outcome;
        std::string error;
        if (source->kind == bridge::FlowNodeKind::Screen)
        {
            std::vector<std::string> authoredOutcomes;
            if (!QueryScreenOutcomes(source->id, authoredOutcomes, error))
            {
                SetStatus("ADD ACTION REJECTED // " + error);
                return;
            }
            const auto available = std::find_if(
                authoredOutcomes.begin(), authoredOutcomes.end(),
                [&](const std::string& candidate)
                {
                    return usedOutcomes.find(candidate) == usedOutcomes.end();
                });
            if (available == authoredOutcomes.end())
            {
                SetStatus("ADD ACTION REJECTED // ALL SCREEN ACTIONS ARE ALREADY ROUTED");
                return;
            }
            outcome = *available;
        }
        else if (source->kind == bridge::FlowNodeKind::GameStart)
        {
            outcome = bridge::GameStartOutcome;
            if (usedOutcomes.find(outcome) != usedOutcomes.end())
            {
                SetStatus("ADD ACTION REJECTED // GAME START ALREADY HAS ITS ENTRY ROUTE");
                return;
            }
        }
        else
        {
            outcome = "next";
            std::size_t branch = 2;
            while (usedOutcomes.find(outcome) != usedOutcomes.end())
                outcome = "branch_" + std::to_string(branch++);
        }

        const bridge::StoryFlowNodeView* destination = nullptr;
        for (const auto& candidate : model_->Nodes())
        {
            if (candidate.id == source->id ||
                candidate.kind == bridge::FlowNodeKind::GameStart)
            {
                continue;
            }
            if (!destination ||
                (candidate.presentationColumn > sourceView->presentationColumn &&
                 destination->presentationColumn <= sourceView->presentationColumn))
            {
                destination = &candidate;
            }
        }
        if (!destination)
        {
            SetStatus("ADD ACTION REJECTED // ADD ANOTHER DESTINATION FIRST");
            return;
        }

        bridge::FlowRoute route;
        route.sourceNodeId = source->id;
        route.outcome = std::move(outcome);
        route.destinationNodeId = destination->id;
        if (destination->kind == bridge::FlowNodeKind::Level)
            route.destinationEntry = "player_entry";

        bridge::StableId createdRouteId;
        if (!session_->AddRoute(std::move(route), createdRouteId, error))
        {
            SetStatus("ADD ACTION REJECTED // " + error);
            return;
        }
        RefreshPresentationAfterSemanticChange();
        SetStatus("JOURNEY ACTION ADDED // CHOOSE ITS DESTINATION");
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
        if (semanticChanged_) semanticChanged_();
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
        if (!session_ || !layout_ ||
            layout_->activeView != bridge::StoryFlowViewMode::Graph)
        {
            return;
        }
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
        // Retired in Gate 9D. Direct ImNodes output sockets own route creation.
        connectionSourceNodeId_.clear();
        reconnectRouteId_.clear();
        SetStatus("GRAPH ROUTES // DRAG FROM AN OUTPUT SOCKET");
    }

    void RenegadeStoryFlowWorkspace::BeginReconnect()
    {
        // Retired in Gate 9D. Detach and drag an existing ImNodes link endpoint.
        connectionSourceNodeId_.clear();
        reconnectRouteId_.clear();
        SetStatus("GRAPH REWIRE // DRAG THE EXISTING LINK ENDPOINT");
    }

    void RenegadeStoryFlowWorkspace::CommitConnectionTo(
        const bridge::StableId&)
    {
        // Retired compatibility seam. StoryFlowAuthoringSession remains the
        // authority, but all surfaced connection gestures now originate in the
        // ImNodes Graph adapter.
        connectionSourceNodeId_.clear();
        reconnectRouteId_.clear();
    }

    void RenegadeStoryFlowWorkspace::Update(const wi::Canvas& canvas, float dt)
    {
        Widget::Update(canvas, dt);
        secondsSincePreviousNodeClick_ = std::min(
            1000.0f, secondsSincePreviousNodeClick_ + std::max(0.0f, dt));
        pointerConsumed_ = false;
        UpdateAuthoringControls(canvas, dt);
        UpdateJourneyObjects(canvas, dt);
        if (!IsVisible() || !IsEnabled() || !model_ || !layout_) return;

        // The shared workspace owns canvas gestures only in Journey. RenderPath
        // disables it while Graph is active, and this guard makes that ownership
        // explicit even if a future caller enables the widget accidentally.
        if (layout_->activeView != bridge::StoryFlowViewMode::Journey)
            return;

        const XMFLOAT4 pointer = wi::input::GetPointer();
        const float graphRight = translation.x + GraphWidth();
        const bool insideHeader =
            pointer.x >= translation.x && pointer.x < translation.x + scale.x &&
            pointer.y >= translation.y && pointer.y < translation.y + HeaderHeight;
        const bool insideInspector = pointer.x >= graphRight &&
            pointer.x < translation.x + scale.x &&
            pointer.y >= translation.y + HeaderHeight &&
            pointer.y < translation.y + scale.y;

        // View/FIT/START are real native controls in the RenderPath. The
        // workspace paints no interactive header hit targets anymore.
        if (insideHeader)
        {
            pointerConsumed_ = true;
            return;
        }

        if (insideInspector)
        {
            pointerConsumed_ = true;
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

        // The governed thumbnail utility is deliberately hit-tested before the
        // card body. It cannot count as card selection/double-click activation
        // or begin a Journey card drag.
        if (inside && wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
        {
            for (const auto& card : journeyModel_.Cards())
            {
                const auto* node = model_->FindNode(card.nodeId);
                const auto objectIt = journeyCardObjects_.find(card.nodeId);
                if (!node ||
                    (node->kind != bridge::FlowNodeKind::Level &&
                        node->kind != bridge::FlowNodeKind::Screen) ||
                    objectIt == journeyCardObjects_.end())
                {
                    continue;
                }
                if (Contains(objectIt->second->ThumbnailButtonBounds(), pointer))
                {
                    ChooseLevelThumbnail(node->id);
                    return;
                }
            }
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
            if (auto* pos = FindJourneyLayout(draggedNodeId_))
            {
                pos->offsetX = nodeDragValueAnchor_.x +
                    (pointer.x - nodeDragPointerAnchor_.x) / zoom;
                pos->offsetY = nodeDragValueAnchor_.y +
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

        for (const auto& track : journeyModel_.Tracks())
        {
            if (track.mainTrack) continue;
            const auto lane = journeyLaneObjects_.find(track.index);
            if (lane == journeyLaneObjects_.end() || !lane->second->IsVisible())
                continue;
            const XMFLOAT2 lanePos = lane->second->GetPos();
            const XMFLOAT2 laneSize = lane->second->GetSize();
            const XMFLOAT4 collapseBounds(
                lanePos.x + laneSize.x - 36.0f,
                lanePos.y,
                36.0f,
                laneSize.y);
            if (!Contains(collapseBounds, pointer)) continue;
            if (collapsedJourneyTracks_.erase(track.index) == 0)
                collapsedJourneyTracks_.insert(track.index);
            SetStatus(collapsedJourneyTracks_.find(track.index) !=
                    collapsedJourneyTracks_.end()
                ? "BRANCH LANE // COLLAPSED"
                : "BRANCH LANE // EXPANDED");
            return;
        }

        for (const auto& node : model_->Nodes())
        {
            const XMFLOAT4 bounds = NodeBounds(node.id);
            if (bounds.z <= 0.0f || !Contains(bounds, pointer)) continue;
            SelectNode(node.id);
            RememberOrActivateNodeClick(node, pointer);
            if (secondsSincePreviousNodeClick_ >= 999.0f)
                return;
            nodeDragging_ = true;
            draggedNodeId_ = node.id;
            nodeDragPointerAnchor_ = XMFLOAT2(pointer.x, pointer.y);
            const auto* pos = FindJourneyLayout(node.id);
            nodeDragValueAnchor_ = pos
                ? XMFLOAT2(pos->offsetX, pos->offsetY)
                : XMFLOAT2{};
            return;
        }

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

        if (!model_ || !layout_ || !session_)
        {
            RenderAuthoringControls(canvas, cmd);
            return;
        }

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

        // Graph is intentionally not rendered here. ImNodes is the sole Graph
        // renderer and is composed by RenegadeStoryFlowGraphLayer. Journey keeps
        // its native lane/card projection in this shared presentation workspace.
        if (layout_->activeView == bridge::StoryFlowViewMode::Journey)
        {
            const float branchHeaderY =
                CanvasToScreen(0.0f, JourneyBranchTrackTop).y - 38.0f;
            Rect(translation.x + 18.0f, branchHeaderY,
                std::max(1.0f, graphWidth - 36.0f), 30.0f,
                wi::Color(13, 21, 28, 246), cmd);
            Rect(translation.x + 18.0f, branchHeaderY,
                std::max(1.0f, graphWidth - 36.0f), 1.0f, Border, cmd);
            Rect(translation.x + 18.0f, branchHeaderY + 29.0f,
                std::max(1.0f, graphWidth - 36.0f), 1.0f, Border, cmd);
            Label("02", translation.x + 30.0f,
                branchHeaderY + 9.0f, 7, Muted, cmd);
            Label("ALTERNATE BRANCHES", translation.x + 60.0f,
                branchHeaderY + 8.0f, 8, Text, cmd);

            for (const auto& track : journeyModel_.Tracks())
            {
                const auto laneIt = journeyLaneObjects_.find(track.index);
                if (laneIt != journeyLaneObjects_.end())
                    laneIt->second->Render(canvas, cmd);
            }
            for (const auto& card : journeyModel_.Cards())
            {
                const auto objectIt = journeyCardObjects_.find(card.nodeId);
                if (objectIt != journeyCardObjects_.end())
                    objectIt->second->Render(canvas, cmd);
            }

            // Lane/card renderers bind the Journey-only scissor. Rebind the
            // workspace clip before fixed overlays and Inspector details are
            // painted; otherwise the last card also clips the Story Overview
            // contents and selected-node Inspector presentation.
            ApplyScissor(canvas, scissorRect, cmd);

            float mainTrackRight = 0.0f;
            float mainTrackCenterY = 0.0f;
            for (const auto& card : journeyModel_.Cards())
            {
                if (card.trackIndex != 0) continue;
                const XMFLOAT4 bounds = JourneyCardScreenBounds(card);
                mainTrackRight = std::max(mainTrackRight, bounds.x + bounds.z);
                mainTrackCenterY = bounds.y + bounds.w * 0.5f;
            }
            if (mainTrackRight > graphRight - 24.0f)
            {
                RoundedRect(graphRight - 38.0f, mainTrackCenterY - 20.0f,
                    26.0f, 40.0f, 5.0f, wi::Color(7, 14, 19, 235), cmd);
                Label(">", graphRight - 29.0f,
                    mainTrackCenterY - 7.0f, 10, Muted, cmd);
            }

            // Gate 9D overview contents share the shell's fixed screen-space
            // rectangle. Only these miniature bars represent zoomed content;
            // the frame itself never scales or moves with Journey navigation.
            const auto shell = ComputeJourneyShellLayout(
                translation.x + width_, height_);
            const auto& overview = shell.storyOverview;
            RoundedRect(overview.x, overview.y, overview.width, overview.height,
                5.0f, wi::Color(8, 15, 20, 244), cmd);
            const float barLeft = overview.x + 18.0f;
            const float barWidth = std::max(1.0f, overview.width - 36.0f);
            const float barTop = overview.y + 30.0f;
            std::size_t maximumColumn = 1;
            for (const auto& card : journeyModel_.Cards())
                maximumColumn = std::max(maximumColumn, card.columnIndex + 1);
            for (const auto& track : journeyModel_.Tracks())
            {
                JourneyBranchRole role = track.mainTrack
                    ? JourneyBranchRole::Main
                    : (track.detached ? JourneyBranchRole::Detached
                        : JourneyBranchRole::Custom);
                if (!track.sourceRouteId.empty())
                {
                    const auto* route = FindDocumentRoute(track.sourceRouteId);
                    if (route)
                        role = JourneyRoleForOutcome(route->outcome, track.detached);
                }
                else if (track.detached && !track.cardNodeIds.empty())
                {
                    const auto* first = FindDocumentNode(track.cardNodeIds.front());
                    if (first)
                    {
                        const JourneyBranchRole inferred =
                            JourneyRoleForOutcome(first->name, false);
                        if (inferred != JourneyBranchRole::Custom)
                            role = inferred;
                    }
                }
                const float y = barTop + static_cast<float>(track.index) * 10.0f;
                for (const auto& nodeId : track.cardNodeIds)
                {
                    const auto* card = journeyModel_.FindCard(nodeId);
                    if (!card) continue;
                    const float x = barLeft + barWidth *
                        (static_cast<float>(card->columnIndex) /
                            static_cast<float>(maximumColumn));
                    const float w = std::max(5.0f,
                        barWidth / static_cast<float>(maximumColumn) - 2.0f);
                    Rect(x, y, w, 6.0f, JourneyRoleColor(role), cmd);
                }
            }
            const float visibleFraction = std::clamp(
                graphWidth /
                    std::max(graphWidth,
                        static_cast<float>(maximumColumn) * JourneyColumnSpacing *
                            layout_->journeyCanvas.zoom),
                0.12f, 1.0f);
            const float viewportX = barLeft + std::clamp(
                -layout_->journeyCanvas.panX /
                    std::max(1.0f,
                        static_cast<float>(maximumColumn) * JourneyColumnSpacing *
                            layout_->journeyCanvas.zoom),
                0.0f, 1.0f - visibleFraction) * barWidth;
            BorderedRect(viewportX, barTop - 3.0f,
                barWidth * visibleFraction,
                std::max(18.0f, overview.height - 39.0f),
                wi::Color(40, 92, 127, 28),
                wi::Color(103, 171, 219, 220), cmd);
        }

        const float inspectorX = graphRight + 14.0f;
        if (layout_->activeView == bridge::StoryFlowViewMode::Graph)
        {
            Label("INSPECTOR", inspectorX,
                translation.y + HeaderHeight + 14.0f, 12, Text, cmd);
        }
        if (const auto* node = FindDocumentNode(selectedNodeId_))
        {
            if (layout_->activeView == bridge::StoryFlowViewMode::Graph)
            {
                Label(KindLabel(node->kind), inspectorX,
                    translation.y + HeaderHeight + 35.0f, 8,
                    node->kind == bridge::FlowNodeKind::GameStart
                        ? Accent : Muted, cmd);
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
            else
            {
                const float inspectorTop = translation.y + 108.0f;
                Label(Shorten(node->name, 34), inspectorX,
                    inspectorTop, 10, Text, cmd);
                Label(KindLabel(node->kind), inspectorX,
                    inspectorTop + 18.0f, 7, Muted, cmd);

                const float previewY = inspectorTop + 46.0f;
                const float previewHeight = std::clamp(
                    height_ * 0.16f, 100.0f, 150.0f);
                RoundedRect(inspectorX, previewY,
                    std::max(1.0f, width_ - GraphWidth() - 28.0f),
                    previewHeight, 5.0f, wi::Color(15, 25, 33, 255), cmd);
                const auto preview = journeyThumbnailResources_.find(node->id);
                if (preview != journeyThumbnailResources_.end())
                {
                    DrawResourceCover(preview->second,
                        XMFLOAT4(inspectorX, previewY,
                            std::max(1.0f, width_ - GraphWidth() - 28.0f),
                            previewHeight), cmd);
                }
                else
                {
                    Rect(inspectorX, previewY + previewHeight * 0.58f,
                        std::max(1.0f, width_ - GraphWidth() - 28.0f),
                        previewHeight * 0.42f,
                        wi::Color(7, 16, 21, 235), cmd);
                    Label("GOVERNED THUMBNAIL",
                        inspectorX +
                            std::max(1.0f, width_ - GraphWidth() - 28.0f) * 0.5f,
                        previewY + previewHeight * 0.48f,
                        7, Muted, cmd);
                }

                const float tabsY = previewY + previewHeight + 9.0f;
                Rect(inspectorX, tabsY + 27.0f,
                    std::max(1.0f, width_ - GraphWidth() - 28.0f),
                    1.0f, Border, cmd);
                constexpr std::array<const char*, 5> inspectorTabs = {
                    "i", "=", "IMG", ">", "</>"};
                for (std::size_t tab = 0; tab < inspectorTabs.size(); ++tab)
                {
                    const float tabX = inspectorX + 22.0f +
                        static_cast<float>(tab) * 52.0f;
                    Label(inspectorTabs[tab], tabX, tabsY + 7.0f,
                        tab == 0 ? 9 : 7,
                        tab == 0 ? wi::Color(65, 158, 230, 255) : Muted, cmd);
                }
                Rect(inspectorX + 4.0f, tabsY + 26.0f,
                    40.0f, 2.0f, wi::Color(65, 158, 230, 255), cmd);

                const float generalY =
                    tabsY + 45.0f;
                Label("GENERAL", inspectorX, generalY, 8, Text, cmd);
                Label("Type", inspectorX, generalY + 58.0f, 7, Muted, cmd);
                Label(KindLabel(node->kind), inspectorX + 104.0f,
                    generalY + 58.0f, 7, Text, cmd);
                Label(node->kind == bridge::FlowNodeKind::Level
                        ? "Scene Document"
                        : (node->kind == bridge::FlowNodeKind::Screen
                            ? "Screen Document" : "Flow Destination"),
                    inspectorX, generalY + 80.0f, 7, Muted, cmd);
                Label(Shorten(node->kind == bridge::FlowNodeKind::Level
                        ? node->scenePathHint
                        : node->screenPathHint, 35),
                    inspectorX + 104.0f, generalY + 80.0f, 7, Text, cmd);

                const float exitsY = generalY + 114.0f;
                Label("ACTIONS / EXITS", inspectorX, exitsY, 8, Text, cmd);
                const auto* nodeView = model_->FindNode(node->id);
                const std::size_t inspectorExitCapacity = height_ < 800.0f
                    ? std::size_t{4} : MaxJourneyInspectorExits;
                const std::size_t count = std::min<std::size_t>(
                    nodeView ? nodeView->outgoingRouteIds.size() : 0,
                    inspectorExitCapacity);
                for (std::size_t i = 0; i < count; ++i)
                {
                    const auto* route = model_->FindRoute(
                        nodeView->outgoingRouteIds[i]);
                    const auto* destination = route
                        ? model_->FindNode(route->destinationNodeId) : nullptr;
                    if (!route) continue;
                    const float y = exitsY + 20.0f +
                        static_cast<float>(i) * 27.0f;
                    const JourneyBranchRole role = JourneyRoleForOutcome(
                        route->outcome, false);
                    RoundedRect(inspectorX + 2.0f, y + 7.0f,
                        8.0f, 8.0f, 4.0f, JourneyRoleColor(role), cmd);
                    Label(Shorten(route->outcome, 16),
                        inspectorX + 19.0f, y + 4.0f, 7, Text, cmd);
                    Label("->", inspectorX + 111.0f, y + 4.0f, 7, Muted, cmd);
                    BorderedRect(inspectorX + 133.0f, y,
                        std::max(1.0f,
                            width_ - GraphWidth() - 161.0f), 23.0f,
                        Surface, Border, cmd);
                    Label(Shorten(destination ? destination->name
                            : route->destinationNodeId, 19),
                        inspectorX + 141.0f, y + 6.0f, 7, Text, cmd);
                }

                if (height_ >= 850.0f)
                {
                    const float notesY = exitsY + 28.0f +
                        static_cast<float>(count) * 27.0f;
                    Label("NOTES", inspectorX, notesY, 8, Text, cmd);
                    RoundedRect(inspectorX, notesY + 18.0f,
                        std::max(1.0f, width_ - GraphWidth() - 28.0f),
                        44.0f, 4.0f, wi::Color(13, 21, 27, 255), cmd);
                    Label("Production notes are not yet stored for this destination.",
                        inspectorX + 9.0f, notesY + 30.0f, 6, Muted, cmd);
                }
            }
        }
        else if (layout_->activeView == bridge::StoryFlowViewMode::Graph)
        {
            if (const auto* route = FindDocumentRoute(selectedRouteId_))
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
        }
        else
        {
            Label("SELECT A JOURNEY CARD", inspectorX,
                translation.y + 112.0f, 9, Muted, cmd);
        }

        if (layout_->activeView == bridge::StoryFlowViewMode::Graph)
        {
            Label("ADD TERMINAL DESTINATION", inspectorX,
                translation.y + HeaderHeight + 334.0f, 8, Muted, cmd);
            Label("LEVEL/SCREEN CREATION USES GOVERNED LIFECYCLE CONTROLS", inspectorX,
                translation.y + HeaderHeight + 460.0f, 7, Muted, cmd);
        }

        const float validationY = layout_->activeView == bridge::StoryFlowViewMode::Journey
            ? translation.y + height_ -
                (height_ >= 850.0f ? 235.0f : 150.0f)
            : translation.y + HeaderHeight + 498.0f;
        Label("VALIDATION", inspectorX,
            validationY, 8, Text, cmd);
        if (model_->Diagnostics().empty())
        {
            RoundedRect(inspectorX, validationY + 20.0f,
                8.0f, 8.0f, 4.0f, wi::Color(113, 205, 111, 255), cmd);
            Label("Valid", inspectorX + 16.0f,
                validationY + 16.0f, 8,
                wi::Color(113, 205, 111, 255), cmd);
            Label("All governed references resolve correctly.", inspectorX,
                validationY + 34.0f, 7, Muted, cmd);
        }
        else
        {
            float diagnosticY = validationY + 20.0f;
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
            translation.y + scale.y - 80.0f, 7,
            statusMessage_.find("FAILED") != std::string::npos ||
            statusMessage_.find("REJECTED") != std::string::npos ||
            statusMessage_.find("ERROR") != std::string::npos
                ? Error : Muted,
            cmd);

        RenderAuthoringControls(canvas, cmd);
    }
}
