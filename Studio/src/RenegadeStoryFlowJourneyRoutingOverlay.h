#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <WickedEngine.h>

#include "RenegadeStoryFlowWorkspace.h"
#include "renegade/bridge/StoryFlowJourneyModel.h"

namespace renegade::studio
{
    // Gate 9C visual routing layer. This is presentation/input over the exact
    // authoritative Story Flow route objects owned by StoryFlowAuthoringSession.
    // It does not serialize or invent a second routing model.
    class RenegadeStoryFlowJourneyRoutingOverlay final : public wi::gui::Widget
    {
    public:
        using ScreenOutcomeQuery = RenegadeStoryFlowWorkspace::ScreenOutcomeQuery;

        void Create()
        {
            SetName("Renegade Story Flow Journey routing overlay");
            SetShadowRadius(0.0f);
            SetEnabled(false);
        }

        void SetLayout(const float width, const float height)
        {
            width_ = std::max(1.0f, width);
            height_ = std::max(1.0f, height);
            SetSize(XMFLOAT2(width_, height_));
        }

        void Bind(
            bridge::StoryFlowAuthoringSession* session,
            bridge::StoryFlowAuthoringModel* model,
            bridge::StoryFlowLayoutDocument* layout,
            RenegadeStoryFlowWorkspace* workspace)
        {
            session_ = session;
            model_ = model;
            layout_ = layout;
            workspace_ = workspace;
            projectionDirty_ = true;
            portsDirty_ = true;
            dragMode_ = DragMode::None;
            dragSourceNodeId_.clear();
            dragRouteId_.clear();
            dragOutcome_.clear();
            routeObjects_.clear();
            portObjects_.clear();
            RefreshProjection();
        }

        void Clear() noexcept
        {
            session_ = nullptr;
            model_ = nullptr;
            layout_ = nullptr;
            workspace_ = nullptr;
            journeyModel_.Clear();
            routeObjects_.clear();
            portObjects_.clear();
            projectionDirty_ = true;
            portsDirty_ = true;
            dragMode_ = DragMode::None;
            dragSourceNodeId_.clear();
            dragRouteId_.clear();
            dragOutcome_.clear();
            hoveredRouteId_.clear();
            hoveredPortKey_.clear();
        }

        void MarkProjectionDirty() noexcept
        {
            projectionDirty_ = true;
            portsDirty_ = true;
        }

        void OnScreenOutcomeQuery(ScreenOutcomeQuery callback)
        {
            screenOutcomeQuery_ = std::move(callback);
            portsDirty_ = true;
        }

        // Registered for rendering only. Wicked GUI updates widgets in
        // registration order; processing routing input here would race the
        // established workspace. RenegadeStoryFlowRenderPath calls
        // UpdateRouting() explicitly after the workspace GUI update instead.
        void Update(const wi::Canvas&, float) override
        {
        }

        void UpdateRouting(const float dt, const bool guiTyping)
        {
            if (!IsVisible() || !session_ || !model_ || !layout_ || !workspace_ ||
                !session_->IsLoaded() || !model_->IsLoaded())
            {
                return;
            }

            if (projectionDirty_)
                RefreshProjection();

            screenOutcomeRefresh_ += std::max(0.0f, dt);
            if (portsDirty_ || screenOutcomeRefresh_ >= 1.0f)
            {
                RebuildPorts();
                portsDirty_ = false;
                screenOutcomeRefresh_ = 0.0f;
            }
            UpdateGeometry();

            if (!guiTyping && !workspace_->SelectedRouteId().empty() &&
                wi::input::Press(wi::input::KEYBOARD_BUTTON_DELETE))
            {
                DeleteSelectedRoute();
                return;
            }

            const XMFLOAT4 pointer = wi::input::GetPointer();
            if (layout_->activeView == bridge::StoryFlowViewMode::Graph)
            {
                hoveredRouteId_.clear();
                hoveredPortKey_.clear();
                if (wi::input::Press(wi::input::MOUSE_BUTTON_LEFT) &&
                    Contains(GraphConnectButtonBounds(), pointer))
                {
                    workspace_->BeginGraphConnectFromRouting();
                }
                return;
            }

            if (layout_->activeView != bridge::StoryFlowViewMode::Journey)
                return;

            hoveredRouteId_.clear();
            hoveredPortKey_.clear();
            if (dragMode_ == DragMode::None)
            {
                for (const auto& [key, port] : portObjects_)
                {
                    if (port && port->HitTest(pointer))
                    {
                        hoveredPortKey_ = key;
                        break;
                    }
                }
                for (const auto& [id, route] : routeObjects_)
                {
                    if (route && route->HitTest(pointer))
                    {
                        hoveredRouteId_ = id;
                        break;
                    }
                }
            }

            if (dragMode_ != DragMode::None)
            {
                dragPointer_ = XMFLOAT2(pointer.x, pointer.y);
                if (wi::input::Release(wi::input::MOUSE_BUTTON_LEFT))
                {
                    const bridge::StableId destination = HitTestDestination(pointer);
                    if (destination.empty())
                    {
                        CancelDrag("ROUTE DRAG CANCELLED // DROP ON A DESTINATION CARD");
                    }
                    else
                    {
                        CommitDrag(destination);
                    }
                }
                return;
            }

            if (!wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
                return;

            // Destination handles win over route bodies so dragging the handle
            // can never be mistaken for a simple route-selection click.
            for (const auto& [id, route] : routeObjects_)
            {
                if (!route || !Contains(route->DestinationHandleBounds(), pointer))
                    continue;
                const bridge::FlowRoute* semantic = FindRoute(id);
                if (!semantic)
                    continue;
                workspace_->SelectRouteFromRouting(id);
                dragMode_ = DragMode::ReconnectDestination;
                dragRouteId_ = id;
                dragSourceNodeId_ = semantic->sourceNodeId;
                dragOutcome_ = semantic->outcome;
                dragStart_ = route->StartPoint();
                dragPointer_ = XMFLOAT2(pointer.x, pointer.y);
                workspace_->SetRoutingStatus(
                    "REWIRE ROUTE // DRAG DESTINATION HANDLE TO A NEW CARD");
                return;
            }

            for (const auto& [key, port] : portObjects_)
            {
                if (!port || !port->HitTest(pointer))
                    continue;
                dragMode_ = DragMode::CreateRoute;
                dragRouteId_.clear();
                dragSourceNodeId_ = port->NodeId();
                dragOutcome_ = port->Outcome();
                dragStart_ = port->Center();
                dragPointer_ = XMFLOAT2(pointer.x, pointer.y);
                workspace_->SetRoutingStatus(
                    "CREATE ROUTE // DRAG OUTPUT PORT TO A DESTINATION CARD");
                return;
            }

            for (const auto& [id, route] : routeObjects_)
            {
                if (!route || !route->HitTest(pointer))
                    continue;
                workspace_->SelectRouteFromRouting(id);
                workspace_->SetRoutingStatus(
                    "ROUTE SELECTED // INSPECTOR EDITS THE SAME STORY FLOW ROUTE");
                return;
            }
        }

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            if (!IsVisible() || !session_ || !model_ || !layout_ || !workspace_)
                return;

            if (layout_->activeView == bridge::StoryFlowViewMode::Graph)
            {
                RenderGraphConnectControl(cmd);
                return;
            }
            if (layout_->activeView != bridge::StoryFlowViewMode::Journey)
                return;

            // Routes are rendered before ports within this overlay. The overlay
            // itself is above the existing card renderer, but orthogonal route
            // geometry terminates outside card bounds so it does not paint over
            // card content.
            for (const auto& route : routeOrder_)
            {
                const auto found = routeObjects_.find(route);
                if (found != routeObjects_.end() && found->second)
                {
                    found->second->SetPresentation(
                        route == workspace_->SelectedRouteId(),
                        route == hoveredRouteId_);
                    found->second->Render(canvas, cmd);
                }
            }

            for (const auto& [key, port] : portObjects_)
            {
                if (!port)
                    continue;
                port->SetHovered(key == hoveredPortKey_);
                port->Render(canvas, cmd);
            }

            if (dragMode_ != DragMode::None)
            {
                DrawOrthogonalPreview(dragStart_, dragPointer_, Forge, cmd);
                DrawHandle(dragPointer_, Forge, cmd);
            }
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStoryFlowJourneyRoutingOverlay";
        }

    private:
        static constexpr float HeaderHeight = 78.0f;
        static constexpr float Padding = 28.0f;
        static constexpr float InspectorWidth = 320.0f;
        static constexpr float JourneyCardWidth = 256.0f;
        static constexpr float JourneyCardHeight = 188.0f;
        static constexpr float JourneyColumnSpacing = 324.0f;
        static constexpr float JourneyTrackSpacing = 252.0f;
        static constexpr float JourneyTrackTop = 58.0f;

        static constexpr wi::Color Surface0 = wi::Color(8, 12, 16, 255);
        static constexpr wi::Color Surface2 = wi::Color(16, 23, 28, 255);
        static constexpr wi::Color Border = wi::Color(38, 52, 61, 255);
        static constexpr wi::Color Muted = wi::Color(142, 151, 156, 255);
        static constexpr wi::Color Text = wi::Color(214, 214, 214, 255);
        static constexpr wi::Color TextStrong = wi::Color(244, 244, 244, 255);
        static constexpr wi::Color Forge = wi::Color(210, 91, 29, 255);
        static constexpr wi::Color RouteColor = wi::Color(76, 96, 106, 255);
        static constexpr wi::Color BranchColor = wi::Color(91, 122, 143, 255);
        static constexpr wi::Color HoverColor = wi::Color(194, 204, 209, 255);

        enum class DragMode
        {
            None,
            CreateRoute,
            ReconnectDestination,
        };

        class PortObject final : public wi::gui::Widget
        {
        public:
            void Create(
                bridge::StableId nodeId,
                std::string outcome,
                std::string label)
            {
                nodeId_ = std::move(nodeId);
                outcome_ = std::move(outcome);
                label_ = std::move(label);
                SetName("Journey route port " + nodeId_ + " // " + outcome_);
                SetShadowRadius(0.0f);
            }

            void SetBounds(const XMFLOAT4& bounds)
            {
                SetPos(XMFLOAT2(bounds.x, bounds.y));
                SetSize(XMFLOAT2(
                    std::max(1.0f, bounds.z),
                    std::max(1.0f, bounds.w)));
            }

            void SetHovered(const bool hovered) noexcept
            {
                hovered_ = hovered;
            }

            [[nodiscard]] const bridge::StableId& NodeId() const noexcept
            {
                return nodeId_;
            }

            [[nodiscard]] const std::string& Outcome() const noexcept
            {
                return outcome_;
            }

            [[nodiscard]] XMFLOAT2 Center() const noexcept
            {
                return XMFLOAT2(
                    translation.x + scale.x * 0.5f,
                    translation.y + scale.y * 0.5f);
            }

            [[nodiscard]] bool HitTest(const XMFLOAT4& pointer) const noexcept
            {
                const XMFLOAT4 expanded(
                    translation.x - 4.0f,
                    translation.y - 4.0f,
                    scale.x + 8.0f,
                    scale.y + 8.0f);
                return Contains(expanded, pointer);
            }

            void Render(
                const wi::Canvas&,
                const wi::graphics::CommandList cmd) const override
            {
                if (!IsVisible())
                    return;
                const wi::Color edge = hovered_ ? Forge : BranchColor;
                DrawPanel(
                    translation.x,
                    translation.y,
                    scale.x,
                    scale.y,
                    Surface0,
                    edge,
                    cmd);
                if (!label_.empty())
                {
                    DrawText(
                        label_,
                        translation.x + scale.x + 5.0f,
                        translation.y - 1.0f,
                        7,
                        hovered_ ? TextStrong : Muted,
                        cmd);
                }
            }

            const char* GetWidgetTypeName() const override
            {
                return "RenegadeStoryFlowJourneyPort";
            }

        private:
            bridge::StableId nodeId_;
            std::string outcome_;
            std::string label_;
            bool hovered_ = false;
        };

        class RouteObject final : public wi::gui::Widget
        {
        public:
            void Create(bridge::StableId routeId)
            {
                routeId_ = std::move(routeId);
                SetName("Journey route " + routeId_);
                SetShadowRadius(0.0f);
            }

            void SetGeometry(
                const XMFLOAT2 start,
                const XMFLOAT2 end,
                std::string outcome,
                const bool primary)
            {
                start_ = start;
                end_ = end;
                outcome_ = std::move(outcome);
                primary_ = primary;
                BuildPath(start_, end_, points_, pointCount_);
            }

            void SetPresentation(const bool selected, const bool hovered) const noexcept
            {
                selected_ = selected;
                hovered_ = hovered;
            }

            [[nodiscard]] XMFLOAT2 StartPoint() const noexcept
            {
                return start_;
            }

            [[nodiscard]] XMFLOAT4 DestinationHandleBounds() const noexcept
            {
                return XMFLOAT4(end_.x - 6.0f, end_.y - 6.0f, 12.0f, 12.0f);
            }

            [[nodiscard]] bool HitTest(const XMFLOAT4& pointer) const noexcept
            {
                if (Contains(DestinationHandleBounds(), pointer))
                    return true;
                const XMFLOAT2 point(pointer.x, pointer.y);
                for (std::size_t i = 1; i < pointCount_; ++i)
                {
                    if (DistanceToSegment(point, points_[i - 1], points_[i]) <= 7.0f)
                        return true;
                }
                return false;
            }

            void Render(
                const wi::Canvas&,
                const wi::graphics::CommandList cmd) const override
            {
                const wi::Color color = selected_
                    ? Forge
                    : (hovered_ ? HoverColor : (primary_ ? RouteColor : BranchColor));
                const float thickness = selected_ ? 3.0f : (hovered_ ? 2.5f : 2.0f);
                for (std::size_t i = 1; i < pointCount_; ++i)
                    DrawAxisSegment(points_[i - 1], points_[i], thickness, color, cmd);
                DrawHandle(end_, color, cmd);

                if ((selected_ || hovered_) && !outcome_.empty())
                {
                    const XMFLOAT2 labelPoint = pointCount_ >= 3
                        ? points_[1]
                        : XMFLOAT2(
                            (start_.x + end_.x) * 0.5f,
                            (start_.y + end_.y) * 0.5f);
                    DrawText(
                        Shorten(outcome_, 22),
                        labelPoint.x + 5.0f,
                        labelPoint.y - 15.0f,
                        7,
                        color,
                        cmd);
                }
            }

            const char* GetWidgetTypeName() const override
            {
                return "RenegadeStoryFlowJourneyRoute";
            }

        private:
            bridge::StableId routeId_;
            XMFLOAT2 start_ = {};
            XMFLOAT2 end_ = {};
            std::array<XMFLOAT2, 4> points_ = {};
            std::size_t pointCount_ = 0;
            std::string outcome_;
            bool primary_ = true;
            mutable bool selected_ = false;
            mutable bool hovered_ = false;
        };

        [[nodiscard]] float GraphWidth() const noexcept
        {
            const float reserved = std::min(InspectorWidth, width_ * 0.42f);
            return std::max(1.0f, width_ - reserved);
        }

        [[nodiscard]] XMFLOAT4 GraphConnectButtonBounds() const noexcept
        {
            const float graphRight = translation.x + GraphWidth();
            const float available = std::max(1.0f, width_ - GraphWidth() - 28.0f);
            return XMFLOAT4(
                graphRight + 14.0f,
                translation.y + HeaderHeight + 286.0f,
                std::min(available, 136.0f),
                28.0f);
        }

        void RenderGraphConnectControl(const wi::graphics::CommandList cmd) const
        {
            if (!workspace_ || !model_)
                return;
            const auto* selected = model_->FindNode(workspace_->SelectedNodeId());
            const bool connecting = workspace_->IsConnectionModeActive();
            const bool canConnect = connecting ||
                (selected != nullptr && !IsTerminalKind(selected->kind));
            if (!canConnect)
                return;

            const XMFLOAT4 bounds = GraphConnectButtonBounds();
            const XMFLOAT4 pointer = wi::input::GetPointer();
            const bool hovered = Contains(bounds, pointer);
            DrawPanel(
                bounds.x,
                bounds.y,
                bounds.z,
                bounds.w,
                hovered ? wi::Color(35, 24, 18, 255) : Surface0,
                connecting || hovered ? Forge : Border,
                cmd);
            DrawText(
                connecting ? "CANCEL LINK" : "CONNECT",
                bounds.x + bounds.z * 0.5f,
                bounds.y + 8.0f,
                8,
                connecting || hovered ? Forge : Text,
                cmd,
                wi::font::WIFALIGN_CENTER);
        }

        void RefreshProjection()
        {
            projectionDirty_ = false;
            routeObjects_.clear();
            routeOrder_.clear();
            if (!model_ || !model_->IsLoaded())
            {
                journeyModel_.Clear();
                return;
            }

            std::string error;
            if (!journeyModel_.Build(*model_, error))
            {
                if (workspace_)
                    workspace_->SetRoutingStatus("ROUTING PROJECTION ERROR // " + error);
                return;
            }

            for (const auto& route : model_->Routes())
            {
                auto object = std::make_unique<RouteObject>();
                object->Create(route.id);
                routeObjects_.emplace(route.id, std::move(object));
                routeOrder_.push_back(route.id);
            }
            portsDirty_ = true;
        }

        void RebuildPorts()
        {
            if (!model_ || !journeyModel_.IsLoaded())
            {
                portObjects_.clear();
                return;
            }

            std::unordered_set<std::string> expected;
            for (const auto& node : model_->Nodes())
            {
                if (IsTerminalKind(node.kind))
                    continue;
                const std::vector<std::string> outcomes = OutcomesForNode(node);
                for (const auto& outcome : outcomes)
                {
                    const std::string key = PortKey(node.id, outcome);
                    expected.insert(key);
                    if (portObjects_.find(key) != portObjects_.end())
                        continue;
                    auto object = std::make_unique<PortObject>();
                    object->Create(node.id, outcome, PortLabel(node.kind, outcome));
                    portObjects_.emplace(key, std::move(object));
                }
            }

            for (auto it = portObjects_.begin(); it != portObjects_.end();)
            {
                if (expected.find(it->first) == expected.end())
                    it = portObjects_.erase(it);
                else
                    ++it;
            }
        }

        void UpdateGeometry()
        {
            if (!model_ || !layout_ || !journeyModel_.IsLoaded())
                return;

            std::unordered_map<bridge::StableId, std::vector<std::string>> outcomesByNode;
            for (const auto& node : model_->Nodes())
                outcomesByNode[node.id] = OutcomesForNode(node);

            for (const auto& node : model_->Nodes())
            {
                const auto* card = journeyModel_.FindCard(node.id);
                if (!card)
                    continue;
                const XMFLOAT4 bounds = CardBounds(*card);
                const auto found = outcomesByNode.find(node.id);
                const std::vector<std::string>& outcomes = found != outcomesByNode.end()
                    ? found->second
                    : EmptyOutcomes();
                const std::size_t count = outcomes.size();
                if (count == 0)
                    continue;

                const float portSize = std::clamp(bounds.w * 0.055f, 8.0f, 11.0f);
                const float top = bounds.y + std::max(34.0f, bounds.w * 0.26f);
                const float available = std::max(18.0f, bounds.w - 62.0f);
                const float spacing = count <= 1
                    ? 0.0f
                    : std::clamp(
                        available / static_cast<float>(count - 1),
                        13.0f,
                        22.0f);
                for (std::size_t i = 0; i < count; ++i)
                {
                    const auto port = portObjects_.find(PortKey(node.id, outcomes[i]));
                    if (port == portObjects_.end() || !port->second)
                        continue;
                    port->second->SetBounds(XMFLOAT4(
                        bounds.x + bounds.z + 5.0f,
                        top + static_cast<float>(i) * spacing - portSize * 0.5f,
                        portSize,
                        portSize));
                    port->second->SetVisible(true);
                }
            }

            for (const auto& route : model_->Routes())
            {
                const auto* sourceCard = journeyModel_.FindCard(route.sourceNodeId);
                const auto* destinationCard = journeyModel_.FindCard(route.destinationNodeId);
                const auto object = routeObjects_.find(route.id);
                if (!sourceCard || !destinationCard ||
                    object == routeObjects_.end() || !object->second)
                {
                    continue;
                }

                const XMFLOAT4 sourceBounds = CardBounds(*sourceCard);
                const XMFLOAT4 destinationBounds = CardBounds(*destinationCard);
                XMFLOAT2 start(
                    sourceBounds.x + sourceBounds.z + 10.0f,
                    sourceBounds.y + sourceBounds.w * 0.5f);
                const auto port = portObjects_.find(PortKey(route.sourceNodeId, route.outcome));
                if (port != portObjects_.end() && port->second)
                    start = port->second->Center();
                const XMFLOAT2 end(
                    destinationBounds.x - 7.0f,
                    destinationBounds.y + destinationBounds.w * 0.5f);
                object->second->SetGeometry(
                    start,
                    end,
                    route.outcome,
                    IsPrimaryRoute(route.id));
            }
        }

        [[nodiscard]] XMFLOAT4 CardBounds(
            const bridge::StoryFlowJourneyCard& card) const noexcept
        {
            const auto* offset = FindJourneyLayout(card.nodeId);
            const float x = static_cast<float>(card.columnIndex) * JourneyColumnSpacing +
                (offset ? offset->offsetX : 0.0f);
            const float y = JourneyTrackTop +
                static_cast<float>(card.trackIndex) * JourneyTrackSpacing +
                (offset ? offset->offsetY : 0.0f);
            const float zoom = layout_ ? layout_->journeyCanvas.zoom : 1.0f;
            return XMFLOAT4(
                translation.x + Padding + (layout_ ? layout_->journeyCanvas.panX : 0.0f) +
                    x * zoom,
                translation.y + HeaderHeight + Padding +
                    (layout_ ? layout_->journeyCanvas.panY : 0.0f) + y * zoom,
                JourneyCardWidth * zoom,
                JourneyCardHeight * zoom);
        }

        [[nodiscard]] const bridge::StoryFlowJourneyCardLayout* FindJourneyLayout(
            const bridge::StableId& nodeId) const noexcept
        {
            if (!layout_)
                return nullptr;
            const auto found = std::find_if(
                layout_->journeyCards.begin(),
                layout_->journeyCards.end(),
                [&](const bridge::StoryFlowJourneyCardLayout& item)
                {
                    return item.nodeId == nodeId;
                });
            return found == layout_->journeyCards.end() ? nullptr : &*found;
        }

        [[nodiscard]] bridge::StableId HitTestDestination(
            const XMFLOAT4& pointer) const
        {
            for (const auto& node : model_->Nodes())
            {
                if (node.kind == bridge::FlowNodeKind::GameStart)
                    continue;
                const auto* card = journeyModel_.FindCard(node.id);
                if (card && Contains(CardBounds(*card), pointer))
                    return node.id;
            }
            return {};
        }

        void CommitDrag(const bridge::StableId& destinationNodeId)
        {
            const auto* destination = FindNode(destinationNodeId);
            if (!destination)
            {
                CancelDrag("ROUTE DRAG CANCELLED // DESTINATION NO LONGER EXISTS");
                return;
            }

            std::string error;
            bridge::StableId resultingRouteId;
            if (dragMode_ == DragMode::ReconnectDestination)
            {
                const bridge::FlowRoute* current = FindRoute(dragRouteId_);
                if (!current)
                {
                    CancelDrag("REWIRE CANCELLED // ROUTE NO LONGER EXISTS");
                    return;
                }
                bridge::FlowRoute route = *current;
                route.destinationNodeId = destinationNodeId;
                if (destination->kind == bridge::FlowNodeKind::Level)
                {
                    if (route.destinationEntry.empty())
                        route.destinationEntry = "player_entry";
                }
                else
                {
                    route.destinationEntry.clear();
                }
                resultingRouteId = route.id;
                if (!session_->UpdateRoute(route.id, std::move(route), error))
                {
                    CancelDrag("REWIRE REJECTED // " + error);
                    return;
                }
            }
            else
            {
                const bridge::FlowNode* source = FindNode(dragSourceNodeId_);
                if (!source)
                {
                    CancelDrag("CONNECT CANCELLED // SOURCE NO LONGER EXISTS");
                    return;
                }
                if (source->kind == bridge::FlowNodeKind::Screen &&
                    !IsCurrentScreenOutcome(source->id, dragOutcome_))
                {
                    CancelDrag(
                        "CONNECT REJECTED // SCREEN ACTION CHANGED DURING ROUTE DRAG");
                    return;
                }

                bridge::FlowRoute route;
                route.sourceNodeId = source->id;
                route.outcome = dragOutcome_;
                route.destinationNodeId = destinationNodeId;
                if (destination->kind == bridge::FlowNodeKind::Level)
                    route.destinationEntry = "player_entry";
                if (!session_->AddRoute(std::move(route), resultingRouteId, error))
                {
                    CancelDrag("CONNECT REJECTED // " + error);
                    return;
                }
            }

            dragMode_ = DragMode::None;
            dragRouteId_.clear();
            dragSourceNodeId_.clear();
            dragOutcome_.clear();
            if (!workspace_->RefreshAfterExternalRoutingChange())
                return;
            workspace_->SelectRouteFromRouting(resultingRouteId);
            workspace_->SetRoutingStatus(
                "ROUTE COMMITTED // JOURNEY AND GRAPH SHARE THE SAME TOPOLOGY");
            MarkProjectionDirty();
        }

        void CancelDrag(std::string status)
        {
            dragMode_ = DragMode::None;
            dragRouteId_.clear();
            dragSourceNodeId_.clear();
            dragOutcome_.clear();
            if (workspace_)
                workspace_->SetRoutingStatus(std::move(status));
        }

        void DeleteSelectedRoute()
        {
            if (!workspace_ || !session_)
                return;
            const bridge::StableId routeId = workspace_->SelectedRouteId();
            if (routeId.empty())
                return;
            std::string error;
            if (!session_->DeleteRoute(routeId, error))
            {
                workspace_->SetRoutingStatus("ROUTE DELETE REJECTED // " + error);
                return;
            }
            if (!workspace_->RefreshAfterExternalRoutingChange())
                return;
            workspace_->SelectRouteFromRouting({});
            workspace_->SetRoutingStatus(
                "ROUTE DELETED // UNDO RESTORES THE SAME ROUTE IDENTITY");
            MarkProjectionDirty();
        }

        [[nodiscard]] const bridge::FlowNode* FindNode(
            const bridge::StableId& nodeId) const noexcept
        {
            if (!session_ || !session_->IsLoaded())
                return nullptr;
            const auto& nodes = session_->Document().nodes;
            const auto found = std::find_if(
                nodes.begin(), nodes.end(),
                [&](const bridge::FlowNode& node) { return node.id == nodeId; });
            return found == nodes.end() ? nullptr : &*found;
        }

        [[nodiscard]] const bridge::FlowRoute* FindRoute(
            const bridge::StableId& routeId) const noexcept
        {
            if (!session_ || !session_->IsLoaded())
                return nullptr;
            const auto& routes = session_->Document().routes;
            const auto found = std::find_if(
                routes.begin(), routes.end(),
                [&](const bridge::FlowRoute& route) { return route.id == routeId; });
            return found == routes.end() ? nullptr : &*found;
        }

        [[nodiscard]] std::vector<std::string> OutcomesForNode(
            const bridge::StoryFlowNodeView& node) const
        {
            if (IsTerminalKind(node.kind))
                return {};
            if (node.kind == bridge::FlowNodeKind::GameStart)
                return {bridge::GameStartOutcome};
            if (node.kind == bridge::FlowNodeKind::Level)
                return {"next"};
            if (node.kind != bridge::FlowNodeKind::Screen || !screenOutcomeQuery_)
                return {};

            std::vector<std::string> outcomes;
            std::string error;
            if (!screenOutcomeQuery_(node.id, outcomes, error))
                return {};
            outcomes.erase(
                std::remove_if(
                    outcomes.begin(), outcomes.end(),
                    [](const std::string& value) { return value.empty(); }),
                outcomes.end());
            std::sort(outcomes.begin(), outcomes.end());
            outcomes.erase(std::unique(outcomes.begin(), outcomes.end()), outcomes.end());
            return outcomes;
        }

        [[nodiscard]] bool IsCurrentScreenOutcome(
            const bridge::StableId& nodeId,
            const std::string& outcome) const
        {
            const auto* node = model_ ? model_->FindNode(nodeId) : nullptr;
            if (!node)
                return false;
            const auto outcomes = OutcomesForNode(*node);
            return std::find(outcomes.begin(), outcomes.end(), outcome) != outcomes.end();
        }

        [[nodiscard]] bool IsPrimaryRoute(const bridge::StableId& routeId) const noexcept
        {
            for (const auto& exit : journeyModel_.Exits())
            {
                if (exit.routeId == routeId)
                    return exit.primary;
            }
            return true;
        }

        [[nodiscard]] static bool IsTerminalKind(
            const bridge::FlowNodeKind kind) noexcept
        {
            return kind == bridge::FlowNodeKind::CompleteGame ||
                kind == bridge::FlowNodeKind::ReturnToMainMenu ||
                kind == bridge::FlowNodeKind::Quit;
        }

        [[nodiscard]] static std::string PortKey(
            const bridge::StableId& nodeId,
            const std::string& outcome)
        {
            return nodeId + "\n" + outcome;
        }

        [[nodiscard]] static std::string PortLabel(
            const bridge::FlowNodeKind kind,
            const std::string& outcome)
        {
            if (kind == bridge::FlowNodeKind::GameStart)
                return "START";
            if (kind == bridge::FlowNodeKind::Level && outcome == "next")
                return "NEXT";
            return Shorten(outcome, 16);
        }

        [[nodiscard]] static const std::vector<std::string>& EmptyOutcomes()
        {
            static const std::vector<std::string> empty;
            return empty;
        }

        static void BuildPath(
            const XMFLOAT2 start,
            const XMFLOAT2 end,
            std::array<XMFLOAT2, 4>& points,
            std::size_t& count)
        {
            if (end.x >= start.x + 36.0f)
            {
                const float middleX = (start.x + end.x) * 0.5f;
                points[0] = start;
                points[1] = XMFLOAT2(middleX, start.y);
                points[2] = XMFLOAT2(middleX, end.y);
                points[3] = end;
                count = 4;
            }
            else
            {
                const float gutterX = std::max(start.x, end.x) + 46.0f;
                points[0] = start;
                points[1] = XMFLOAT2(gutterX, start.y);
                points[2] = XMFLOAT2(gutterX, end.y);
                points[3] = end;
                count = 4;
            }
        }

        static void DrawOrthogonalPreview(
            const XMFLOAT2 start,
            const XMFLOAT2 end,
            const wi::Color color,
            const wi::graphics::CommandList cmd)
        {
            std::array<XMFLOAT2, 4> points = {};
            std::size_t count = 0;
            BuildPath(start, end, points, count);
            for (std::size_t i = 1; i < count; ++i)
                DrawAxisSegment(points[i - 1], points[i], 2.0f, color, cmd);
        }

        static void DrawAxisSegment(
            const XMFLOAT2 a,
            const XMFLOAT2 b,
            const float thickness,
            const wi::Color color,
            const wi::graphics::CommandList cmd)
        {
            if (std::abs(a.y - b.y) <= 0.5f)
            {
                DrawRect(
                    std::min(a.x, b.x),
                    a.y - thickness * 0.5f,
                    std::max(1.0f, std::abs(b.x - a.x)),
                    thickness,
                    color,
                    cmd);
            }
            else
            {
                DrawRect(
                    a.x - thickness * 0.5f,
                    std::min(a.y, b.y),
                    thickness,
                    std::max(1.0f, std::abs(b.y - a.y)),
                    color,
                    cmd);
            }
        }

        static void DrawHandle(
            const XMFLOAT2 point,
            const wi::Color color,
            const wi::graphics::CommandList cmd)
        {
            DrawPanel(point.x - 5.0f, point.y - 5.0f, 10.0f, 10.0f, Surface0, color, cmd);
        }

        static void DrawRect(
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

        static void DrawPanel(
            const float x,
            const float y,
            const float width,
            const float height,
            const wi::Color fill,
            const wi::Color edge,
            const wi::graphics::CommandList cmd)
        {
            DrawRect(x, y, width, height, edge, cmd);
            DrawRect(
                x + 1.0f,
                y + 1.0f,
                std::max(0.0f, width - 2.0f),
                std::max(0.0f, height - 2.0f),
                fill,
                cmd);
        }

        static void DrawText(
            const std::string& text,
            const float x,
            const float y,
            const int size,
            const wi::Color color,
            const wi::graphics::CommandList cmd,
            const wi::font::Alignment align = wi::font::WIFALIGN_LEFT)
        {
            wi::font::Params params(
                x,
                y,
                size,
                align,
                wi::font::WIFALIGN_TOP,
                color,
                wi::Color::Transparent());
            params.bolden = 0.12f;
            wi::font::Draw(text, params, cmd);
        }

        [[nodiscard]] static bool Contains(
            const XMFLOAT4& bounds,
            const XMFLOAT4& pointer) noexcept
        {
            return bounds.z > 0.0f && bounds.w > 0.0f &&
                pointer.x >= bounds.x && pointer.y >= bounds.y &&
                pointer.x < bounds.x + bounds.z &&
                pointer.y < bounds.y + bounds.w;
        }

        [[nodiscard]] static float DistanceToSegment(
            const XMFLOAT2 point,
            const XMFLOAT2 start,
            const XMFLOAT2 end) noexcept
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
            const float t = std::clamp(
                ((point.x - start.x) * dx + (point.y - start.y) * dy) /
                    lengthSquared,
                0.0f,
                1.0f);
            const float nearestX = start.x + t * dx;
            const float nearestY = start.y + t * dy;
            const float px = point.x - nearestX;
            const float py = point.y - nearestY;
            return std::sqrt(px * px + py * py);
        }

        [[nodiscard]] static std::string Shorten(
            std::string value,
            const std::size_t maximum)
        {
            if (value.size() <= maximum)
                return value;
            if (maximum <= 3)
                return value.substr(0, maximum);
            value.resize(maximum - 3);
            value += "...";
            return value;
        }

        bridge::StoryFlowAuthoringSession* session_ = nullptr;
        bridge::StoryFlowAuthoringModel* model_ = nullptr;
        bridge::StoryFlowLayoutDocument* layout_ = nullptr;
        RenegadeStoryFlowWorkspace* workspace_ = nullptr;
        bridge::StoryFlowJourneyModel journeyModel_;
        ScreenOutcomeQuery screenOutcomeQuery_;
        std::unordered_map<bridge::StableId, std::unique_ptr<RouteObject>> routeObjects_;
        std::vector<bridge::StableId> routeOrder_;
        std::unordered_map<std::string, std::unique_ptr<PortObject>> portObjects_;
        bridge::StableId hoveredRouteId_;
        std::string hoveredPortKey_;
        DragMode dragMode_ = DragMode::None;
        bridge::StableId dragSourceNodeId_;
        bridge::StableId dragRouteId_;
        std::string dragOutcome_;
        XMFLOAT2 dragStart_ = {};
        XMFLOAT2 dragPointer_ = {};
        bool projectionDirty_ = true;
        bool portsDirty_ = true;
        float screenOutcomeRefresh_ = 0.0f;
        float width_ = 1.0f;
        float height_ = 1.0f;
    };
}
