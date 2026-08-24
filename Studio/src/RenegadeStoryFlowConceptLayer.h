#pragma once

#include <WickedEngine.h>

#include "renegade/bridge/StoryFlowAuthoringModel.h"
#include "renegade/bridge/StoryFlowAuthoringSession.h"
#include "renegade/bridge/StoryFlowJourneyModel.h"
#include "renegade/bridge/StoryFlowLayoutService.h"
#include "RenegadeStoryFlowVisualTheme.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <utility>

namespace renegade::studio
{
    // Gate 9E is a presentation pass only. This layer consumes the shared model,
    // deterministic Journey projection and persisted layout. It has no semantic
    // mutation callbacks and therefore cannot become a second route authority.
    class RenegadeStoryFlowConceptLayer final : public wi::gui::Widget
    {
    public:
        void Create()
        {
            SetName("Story Flow Gate 9E concept presentation");
            SetShadowRadius(0.0f);
            SetVisible(false);
            SetEnabled(false);
        }

        void Bind(
            bridge::StoryFlowAuthoringSession* session,
            bridge::StoryFlowAuthoringModel* model,
            bridge::StoryFlowLayoutDocument* layout)
        {
            session_ = session;
            model_ = model;
            layout_ = layout;
            fingerprint_ = 0;
            projection_.Clear();
            RefreshProjection();
        }

        void Clear() noexcept
        {
            session_ = nullptr;
            model_ = nullptr;
            layout_ = nullptr;
            selectedNodeId_.clear();
            selectedRouteId_.clear();
            projection_.Clear();
            fingerprint_ = 0;
        }

        void SetViewport(
            const XMFLOAT4& journeyViewport,
            const XMFLOAT4& inspectorBounds)
        {
            journeyViewport_ = journeyViewport;
            inspectorBounds_ = inspectorBounds;
            SetPos(XMFLOAT2(0.0f, 0.0f));
            SetSize(XMFLOAT2(
                std::max(
                    journeyViewport.x + journeyViewport.z,
                    inspectorBounds.x + inspectorBounds.z),
                std::max(
                    journeyViewport.y + journeyViewport.w,
                    inspectorBounds.y + inspectorBounds.w)));
        }

        void SetSelection(
            bridge::StableId nodeId,
            bridge::StableId routeId)
        {
            selectedNodeId_ = std::move(nodeId);
            selectedRouteId_ = std::move(routeId);
        }

        void Update(const wi::Canvas&, float) override
        {
            RefreshProjection();
        }

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            if (!IsVisible() || !model_ || !layout_ || !model_->IsLoaded())
                return;

            const auto& theme = StoryFlowVisualTheme::Get();
            if (layout_->activeView == bridge::StoryFlowViewMode::Journey)
            {
                ApplyScissor(canvas, RectToScissor(journeyViewport_), cmd);
                RenderJourneyRoutes(cmd);
                RenderJourneyOverview(cmd);
            }

            // The concept Inspector deliberately covers the old debug-looking
            // workspace Inspector. Graph semantic editor controls are still real
            // native controls and render after this layer, so functionality is
            // preserved while the shell becomes coherent.
            ApplyScissor(canvas, RectToScissor(inspectorBounds_), cmd);
            RenderInspector(theme, cmd);
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStoryFlowConceptLayer";
        }

    private:
        [[nodiscard]] static wi::primitive::Rect RectToScissor(
            const XMFLOAT4& value) noexcept
        {
            wi::primitive::Rect rect;
            rect.left = static_cast<int>(std::floor(value.x));
            rect.top = static_cast<int>(std::floor(value.y));
            rect.right = static_cast<int>(std::ceil(value.x + value.z));
            rect.bottom = static_cast<int>(std::ceil(value.y + value.w));
            return rect;
        }

        void RefreshProjection()
        {
            if (!model_ || !model_->IsLoaded())
            {
                projection_.Clear();
                fingerprint_ = 0;
                return;
            }

            std::size_t fingerprint = model_->Nodes().size() * 1315423911u +
                model_->Routes().size();
            const auto combine = [&fingerprint](const std::string& value)
            {
                const std::size_t hash = std::hash<std::string>{}(value);
                fingerprint ^= hash + 0x9e3779b97f4a7c15ull +
                    (fingerprint << 6u) + (fingerprint >> 2u);
            };
            for (const auto& route : model_->Routes())
            {
                combine(route.id);
                combine(route.sourceNodeId);
                combine(route.destinationNodeId);
                combine(route.outcome);
            }
            if (fingerprint == fingerprint_ && projection_.IsLoaded())
                return;

            std::string error;
            if (projection_.Build(*model_, error))
                fingerprint_ = fingerprint;
        }

        [[nodiscard]] const bridge::StoryFlowJourneyCardLayout* FindOffset(
            const bridge::StableId& nodeId) const noexcept
        {
            if (!layout_) return nullptr;
            const auto iterator = std::find_if(
                layout_->journeyCards.begin(),
                layout_->journeyCards.end(),
                [&](const bridge::StoryFlowJourneyCardLayout& item)
                {
                    return item.nodeId == nodeId;
                });
            return iterator == layout_->journeyCards.end() ? nullptr : &*iterator;
        }

        [[nodiscard]] XMFLOAT4 CardBounds(
            const bridge::StoryFlowJourneyCard& card) const noexcept
        {
            const auto& theme = StoryFlowVisualTheme::Get();
            const auto* offset = FindOffset(card.nodeId);
            const float x = static_cast<float>(card.columnIndex) *
                theme.journeyColumnSpacing + (offset ? offset->offsetX : 0.0f);
            const float y = theme.journeyTrackTop +
                static_cast<float>(card.trackIndex) * theme.journeyTrackSpacing +
                (offset ? offset->offsetY : 0.0f);
            const float zoom = std::max(0.001f, layout_->journeyCanvas.zoom);
            return XMFLOAT4(
                journeyViewport_.x + theme.shellPadding +
                    layout_->journeyCanvas.panX + x * zoom,
                journeyViewport_.y + theme.shellPadding +
                    layout_->journeyCanvas.panY + y * zoom,
                theme.journeyCardWidth * zoom,
                theme.journeyCardHeight * zoom);
        }

        [[nodiscard]] wi::Color RouteColor(
            const bridge::StoryFlowJourneyExit& exit) const
        {
            const auto& theme = StoryFlowVisualTheme::Get();
            const auto* route = model_ ? model_->FindRoute(exit.routeId) : nullptr;
            if (!route) return theme.routeOther;
            std::string outcome = route->outcome;
            std::transform(outcome.begin(), outcome.end(), outcome.begin(),
                [](const unsigned char c)
                {
                    return static_cast<char>(std::tolower(c));
                });
            if (outcome.find("death") != std::string::npos ||
                outcome.find("fail") != std::string::npos ||
                outcome.find("quit") != std::string::npos ||
                outcome.find("restart") != std::string::npos)
                return theme.routeFailure;
            if (outcome.find("load") != std::string::npos ||
                outcome.find("continue") != std::string::npos)
                return theme.routeLoad;
            if (outcome.find("option") != std::string::npos ||
                outcome.find("setting") != std::string::npos ||
                outcome.find("menu") != std::string::npos)
                return theme.routeSystem;
            if (outcome.find("victory") != std::string::npos ||
                outcome.find("complete") != std::string::npos ||
                outcome.find("ending") != std::string::npos)
                return theme.routeEnding;
            return exit.primaryContinuation ? theme.routeMain : theme.routeOther;
        }

        void RenderJourneyRoutes(const wi::graphics::CommandList cmd) const
        {
            if (!projection_.IsLoaded()) return;
            const auto& theme = StoryFlowVisualTheme::Get();

            for (const auto& exit : projection_.Exits())
            {
                const auto* source = projection_.FindCard(exit.sourceNodeId);
                const auto* destination = projection_.FindCard(exit.destinationNodeId);
                if (!source || !destination) continue;

                const XMFLOAT4 a = CardBounds(*source);
                const XMFLOAT4 b = CardBounds(*destination);
                const float ax = a.x + a.z;
                const float ay = a.y + a.w * 0.5f;
                const float bx = b.x;
                const float by = b.y + b.w * 0.5f;
                const float horizontal = std::max(36.0f, std::abs(bx - ax) * 0.42f);
                const XMFLOAT2 p0(ax, ay);
                const XMFLOAT2 p1(ax + horizontal, ay);
                const XMFLOAT2 p2(bx - horizontal, by);
                const XMFLOAT2 p3(bx, by);
                const wi::Color color = RouteColor(exit);

                constexpr int Segments = 18;
                XMFLOAT2 previous = p0;
                for (int i = 1; i <= Segments; ++i)
                {
                    const float t = static_cast<float>(i) /
                        static_cast<float>(Segments);
                    const float u = 1.0f - t;
                    const XMFLOAT2 current(
                        u * u * u * p0.x + 3.0f * u * u * t * p1.x +
                            3.0f * u * t * t * p2.x + t * t * t * p3.x,
                        u * u * u * p0.y + 3.0f * u * u * t * p1.y +
                            3.0f * u * t * t * p2.y + t * t * t * p3.y);
                    DrawLine(previous, current, theme.routeGlowThickness,
                        WithAlpha(color, 42), cmd);
                    DrawLine(previous, current, theme.routeThickness, color, cmd);
                    previous = current;
                }

                const auto* route = model_->FindRoute(exit.routeId);
                if (route && !route->outcome.empty())
                {
                    const float t = 0.5f;
                    const float u = 1.0f - t;
                    const XMFLOAT2 mid(
                        u * u * u * p0.x + 3.0f * u * u * t * p1.x +
                            3.0f * u * t * t * p2.x + t * t * t * p3.x,
                        u * u * u * p0.y + 3.0f * u * u * t * p1.y +
                            3.0f * u * t * t * p2.y + t * t * t * p3.y);
                    DrawText(
                        Upper(route->outcome),
                        mid.x,
                        mid.y - theme.routeLabelOffset,
                        theme.fontRouteLabel,
                        color,
                        cmd,
                        wi::font::WIFALIGN_CENTER);
                }
            }
        }

        void RenderJourneyOverview(const wi::graphics::CommandList cmd) const
        {
            if (!projection_.IsLoaded() || projection_.Cards().empty()) return;
            const auto& theme = StoryFlowVisualTheme::Get();
            const float width = std::min(
                theme.minimapWidth,
                std::max(150.0f, journeyViewport_.z * 0.28f));
            const float height = std::min(
                theme.minimapHeight,
                std::max(88.0f, journeyViewport_.w * 0.22f));
            const XMFLOAT4 panel(
                journeyViewport_.x + journeyViewport_.z - width - theme.shellPadding,
                journeyViewport_.y + journeyViewport_.w - height - theme.shellPadding,
                width,
                height);
            DrawPanel(panel, WithAlpha(theme.panel, 238), theme.border, 4.0f, cmd);
            DrawText("STORY OVERVIEW", panel.x + 9.0f, panel.y + 7.0f,
                theme.fontCardMeta, theme.muted, cmd);

            float minX = std::numeric_limits<float>::max();
            float minY = std::numeric_limits<float>::max();
            float maxX = std::numeric_limits<float>::lowest();
            float maxY = std::numeric_limits<float>::lowest();
            for (const auto& card : projection_.Cards())
            {
                const auto* offset = FindOffset(card.nodeId);
                const float x = static_cast<float>(card.columnIndex) *
                    theme.journeyColumnSpacing + (offset ? offset->offsetX : 0.0f);
                const float y = theme.journeyTrackTop +
                    static_cast<float>(card.trackIndex) * theme.journeyTrackSpacing +
                    (offset ? offset->offsetY : 0.0f);
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x + theme.journeyCardWidth);
                maxY = std::max(maxY, y + theme.journeyCardHeight);
            }
            if (minX > maxX || minY > maxY) return;

            const float contentX = panel.x + 9.0f;
            const float contentY = panel.y + 24.0f;
            const float contentW = panel.z - 18.0f;
            const float contentH = panel.w - 33.0f;
            const float spanX = std::max(1.0f, maxX - minX);
            const float spanY = std::max(1.0f, maxY - minY);
            const float scale = std::min(contentW / spanX, contentH / spanY);
            const float ox = contentX + (contentW - spanX * scale) * 0.5f;
            const float oy = contentY + (contentH - spanY * scale) * 0.5f;

            for (const auto& card : projection_.Cards())
            {
                const auto* offset = FindOffset(card.nodeId);
                const float x = static_cast<float>(card.columnIndex) *
                    theme.journeyColumnSpacing + (offset ? offset->offsetX : 0.0f);
                const float y = theme.journeyTrackTop +
                    static_cast<float>(card.trackIndex) * theme.journeyTrackSpacing +
                    (offset ? offset->offsetY : 0.0f);
                const auto* node = model_->FindNode(card.nodeId);
                DrawRoundedRect(
                    ox + (x - minX) * scale,
                    oy + (y - minY) * scale,
                    std::max(3.0f, theme.journeyCardWidth * scale),
                    std::max(2.0f, theme.journeyCardHeight * scale),
                    1.5f,
                    node ? DestinationColor(node->kind) : theme.routeOther,
                    cmd);
            }

            const float zoom = std::max(0.001f, layout_->journeyCanvas.zoom);
            const float visibleLeft = (-theme.shellPadding - layout_->journeyCanvas.panX) / zoom;
            const float visibleTop = (-theme.shellPadding - layout_->journeyCanvas.panY) / zoom;
            const float visibleRight = visibleLeft + journeyViewport_.z / zoom;
            const float visibleBottom = visibleTop + journeyViewport_.w / zoom;
            const XMFLOAT4 visible(
                ox + (visibleLeft - minX) * scale,
                oy + (visibleTop - minY) * scale,
                (visibleRight - visibleLeft) * scale,
                (visibleBottom - visibleTop) * scale);
            DrawOutline(visible, theme.selection, 1.0f, cmd);
        }

        void RenderInspector(
            const StoryFlowVisualTheme& theme,
            const wi::graphics::CommandList cmd) const
        {
            DrawRoundedRect(
                inspectorBounds_.x,
                inspectorBounds_.y,
                inspectorBounds_.z,
                inspectorBounds_.w,
                0.0f,
                theme.inspector,
                cmd);
            DrawLine(
                XMFLOAT2(inspectorBounds_.x, inspectorBounds_.y),
                XMFLOAT2(inspectorBounds_.x, inspectorBounds_.y + inspectorBounds_.w),
                1.0f,
                theme.border,
                cmd);

            const float x = inspectorBounds_.x + theme.shellPadding;
            const float width = std::max(1.0f,
                inspectorBounds_.z - theme.shellPadding * 2.0f);
            float y = inspectorBounds_.y + 12.0f;

            DrawText("INSPECTOR", x, y, theme.fontInspectorTitle,
                theme.textStrong, cmd);
            y += 28.0f;

            const auto* node = model_->FindNode(selectedNodeId_);
            const auto* route = model_->FindRoute(selectedRouteId_);
            if (node)
            {
                DrawDestinationHeader(*node, x, y, width, cmd);
                y += 72.0f;

                DrawSectionTitle("GENERAL", x, y, cmd);
                y += 19.0f;
                DrawKeyValue("Display Name", node->name, x, y, width, cmd);
                y += 24.0f;
                DrawKeyValue("Type", KindLabel(node->kind), x, y, width, cmd);
                y += 24.0f;
                if (node->kind == bridge::FlowNodeKind::Level)
                {
                    DrawKeyValue("Scene", Shorten(node->scenePathHint, 31), x, y, width, cmd);
                    y += 24.0f;
                }
                else if (node->kind == bridge::FlowNodeKind::Screen)
                {
                    DrawKeyValue("Screen", Shorten(node->screenPathHint, 31), x, y, width, cmd);
                    y += 24.0f;
                }

                y += 8.0f;
                DrawSectionTitle(
                    layout_->activeView == bridge::StoryFlowViewMode::Journey
                        ? "ACTIONS / EXITS  //  OPEN IN GRAPH"
                        : "CONNECTIONS",
                    x, y, cmd);
                y += 21.0f;
                const std::size_t maximum = std::min<std::size_t>(
                    node->outgoingRouteIds.size(), 6);
                for (std::size_t i = 0; i < maximum; ++i)
                {
                    const auto* outgoing = model_->FindRoute(node->outgoingRouteIds[i]);
                    if (!outgoing) continue;
                    const auto* destination = model_->FindNode(outgoing->destinationNodeId);
                    const wi::Color routeColor = RouteColorForRoute(*outgoing);
                    DrawRoundedRect(x, y + 5.0f, 7.0f, 7.0f, 3.5f, routeColor, cmd);
                    DrawText(
                        Shorten(outgoing->outcome.empty() ? "DEFAULT" :
                            Upper(outgoing->outcome), 15),
                        x + 14.0f,
                        y,
                        theme.fontInspectorBody,
                        theme.text,
                        cmd);
                    DrawText(
                        "→  " + Shorten(destination ? destination->name :
                            outgoing->destinationNodeId, 18),
                        x + width,
                        y,
                        theme.fontInspectorBody,
                        theme.muted,
                        cmd,
                        wi::font::WIFALIGN_RIGHT);
                    y += 22.0f;
                }
                if (maximum == 0)
                {
                    DrawText("No exits", x, y, theme.fontInspectorBody,
                        theme.muted, cmd);
                    y += 22.0f;
                }
            }
            else if (route)
            {
                DrawSectionTitle("ROUTE", x, y, cmd);
                y += 22.0f;
                const auto* source = model_->FindNode(route->sourceNodeId);
                const auto* destination = model_->FindNode(route->destinationNodeId);
                DrawKeyValue("From", source ? source->name : route->sourceNodeId,
                    x, y, width, cmd);
                y += 24.0f;
                DrawKeyValue("Outcome", route->outcome, x, y, width, cmd);
                y += 24.0f;
                DrawKeyValue("To", destination ? destination->name : route->destinationNodeId,
                    x, y, width, cmd);
                y += 24.0f;
                DrawKeyValue("Priority", std::to_string(route->priority),
                    x, y, width, cmd);
                y += 24.0f;
                DrawKeyValue("Conditions", std::to_string(route->conditionCount),
                    x, y, width, cmd);
                y += 24.0f;
            }
            else
            {
                DrawText(
                    layout_->activeView == bridge::StoryFlowViewMode::Journey
                        ? "Select a Journey card"
                        : "Select a Graph node or route",
                    x,
                    y,
                    theme.fontInspectorBody,
                    theme.muted,
                    cmd);
                y += 24.0f;
            }

            const float validationTop = std::min(
                inspectorBounds_.y + inspectorBounds_.w - 118.0f,
                std::max(y + 14.0f, inspectorBounds_.y + inspectorBounds_.w * 0.66f));
            DrawSectionTitle("VALIDATION", x, validationTop, cmd);
            const bool valid = model_->Diagnostics().empty();
            DrawRoundedRect(
                x,
                validationTop + 25.0f,
                8.0f,
                8.0f,
                4.0f,
                valid ? theme.success : theme.warning,
                cmd);
            DrawText(
                valid ? "Valid" :
                    (std::to_string(model_->Diagnostics().size()) + " issue(s)"),
                x + 15.0f,
                validationTop + 19.0f,
                theme.fontInspectorBody,
                valid ? theme.success : theme.warning,
                cmd);
            DrawText(
                session_ && session_->IsDirty() ? "UNSAVED CHANGES" : "SAVED",
                x + width,
                validationTop + 19.0f,
                theme.fontInspectorBody,
                session_ && session_->IsDirty() ? theme.warning : theme.success,
                cmd,
                wi::font::WIFALIGN_RIGHT);
        }

        void DrawDestinationHeader(
            const bridge::StoryFlowNodeView& node,
            const float x,
            const float y,
            const float width,
            const wi::graphics::CommandList cmd) const
        {
            const auto& theme = StoryFlowVisualTheme::Get();
            DrawPanel(XMFLOAT4(x, y, width, 60.0f), theme.panel,
                theme.borderSoft, 4.0f, cmd);
            DrawRoundedRect(
                x + 10.0f,
                y + 12.0f,
                34.0f,
                34.0f,
                4.0f,
                WithAlpha(DestinationColor(node.kind), 72),
                cmd);
            DrawOutline(XMFLOAT4(x + 10.0f, y + 12.0f, 34.0f, 34.0f),
                DestinationColor(node.kind), 1.0f, cmd);
            DrawText(
                Shorten(node.name, 25),
                x + 55.0f,
                y + 10.0f,
                theme.fontCardTitle,
                theme.textStrong,
                cmd);
            DrawText(
                KindLabel(node.kind),
                x + 55.0f,
                y + 31.0f,
                theme.fontInspectorBody,
                theme.muted,
                cmd);
        }

        void DrawSectionTitle(
            const std::string& title,
            const float x,
            const float y,
            const wi::graphics::CommandList cmd) const
        {
            const auto& theme = StoryFlowVisualTheme::Get();
            DrawText(title, x, y, theme.fontInspectorBody,
                theme.textStrong, cmd);
            DrawLine(
                XMFLOAT2(x, y + 16.0f),
                XMFLOAT2(inspectorBounds_.x + inspectorBounds_.z - theme.shellPadding,
                    y + 16.0f),
                1.0f,
                theme.borderSoft,
                cmd);
        }

        void DrawKeyValue(
            const std::string& key,
            const std::string& value,
            const float x,
            const float y,
            const float width,
            const wi::graphics::CommandList cmd) const
        {
            const auto& theme = StoryFlowVisualTheme::Get();
            DrawText(key, x, y, theme.fontInspectorBody, theme.muted, cmd);
            DrawText(Shorten(value, 28), x + width, y,
                theme.fontInspectorBody, theme.text, cmd,
                wi::font::WIFALIGN_RIGHT);
        }

        [[nodiscard]] wi::Color RouteColorForRoute(
            const bridge::StoryFlowRouteView& route) const
        {
            bridge::StoryFlowJourneyExit temporary;
            temporary.routeId = route.id;
            temporary.primaryContinuation = false;
            for (const auto& exit : projection_.Exits())
            {
                if (exit.routeId == route.id)
                    return RouteColor(exit);
            }
            return StoryFlowVisualTheme::Get().routeOther;
        }

        [[nodiscard]] wi::Color DestinationColor(
            const bridge::FlowNodeKind kind) const noexcept
        {
            const auto& theme = StoryFlowVisualTheme::Get();
            switch (kind)
            {
            case bridge::FlowNodeKind::GameStart: return theme.gameStart;
            case bridge::FlowNodeKind::Level: return theme.level;
            case bridge::FlowNodeKind::Screen: return theme.screen;
            case bridge::FlowNodeKind::CompleteGame: return theme.success;
            case bridge::FlowNodeKind::ReturnToMainMenu: return theme.routeOther;
            case bridge::FlowNodeKind::Quit: return theme.terminal;
            default: return theme.routeOther;
            }
        }

        static void DrawLine(
            const XMFLOAT2 a,
            const XMFLOAT2 b,
            const float thickness,
            const wi::Color color,
            const wi::graphics::CommandList cmd)
        {
            const float dx = b.x - a.x;
            const float dy = b.y - a.y;
            const float length = std::sqrt(dx * dx + dy * dy);
            if (length <= 0.001f || thickness <= 0.0f) return;
            wi::image::Params params;
            params.pos = XMFLOAT3(a.x, a.y, 0.0f);
            params.siz = XMFLOAT2(length, thickness);
            params.pivot = XMFLOAT2(0.0f, 0.5f);
            params.rotation = std::atan2(dy, dx);
            params.color = color;
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            wi::image::Draw(nullptr, params, cmd);
        }

        static void DrawRoundedRect(
            const float x,
            const float y,
            const float width,
            const float height,
            const float radius,
            const wi::Color color,
            const wi::graphics::CommandList cmd)
        {
            if (width <= 0.0f || height <= 0.0f) return;
            wi::image::Params params(x, y, width, height, color);
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            if (radius > 0.0f)
            {
                params.enableCornerRounding();
                for (auto& corner : params.corners_rounding)
                {
                    corner.radius = std::min(radius, std::min(width, height) * 0.5f);
                    corner.segments = 8;
                }
            }
            wi::image::Draw(nullptr, params, cmd);
        }

        static void DrawOutline(
            const XMFLOAT4 bounds,
            const wi::Color color,
            const float thickness,
            const wi::graphics::CommandList cmd)
        {
            DrawRoundedRect(bounds.x, bounds.y, bounds.z, thickness, 0.0f, color, cmd);
            DrawRoundedRect(bounds.x, bounds.y + bounds.w - thickness,
                bounds.z, thickness, 0.0f, color, cmd);
            DrawRoundedRect(bounds.x, bounds.y, thickness, bounds.w, 0.0f, color, cmd);
            DrawRoundedRect(bounds.x + bounds.z - thickness, bounds.y,
                thickness, bounds.w, 0.0f, color, cmd);
        }

        static void DrawPanel(
            const XMFLOAT4 bounds,
            const wi::Color fill,
            const wi::Color border,
            const float radius,
            const wi::graphics::CommandList cmd)
        {
            DrawRoundedRect(bounds.x, bounds.y, bounds.z, bounds.w, radius, border, cmd);
            DrawRoundedRect(bounds.x + 1.0f, bounds.y + 1.0f,
                std::max(0.0f, bounds.z - 2.0f),
                std::max(0.0f, bounds.w - 2.0f),
                std::max(0.0f, radius - 1.0f), fill, cmd);
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
            const auto& theme = StoryFlowVisualTheme::Get();
            wi::font::Params params(
                x,
                y,
                size,
                align,
                wi::font::WIFALIGN_TOP,
                color,
                wi::Color::Transparent());
            params.style = theme.fontStyle;
            params.spacingX = theme.fontTracking;
            params.bolden = theme.fontBolden;
            wi::font::Draw(text, params, cmd);
        }

        [[nodiscard]] static wi::Color WithAlpha(
            const wi::Color color,
            const std::uint8_t alpha) noexcept
        {
            return wi::Color(color.getR(), color.getG(), color.getB(), alpha);
        }

        [[nodiscard]] static const char* KindLabel(
            const bridge::FlowNodeKind kind) noexcept
        {
            switch (kind)
            {
            case bridge::FlowNodeKind::GameStart: return "Game Start";
            case bridge::FlowNodeKind::Level: return "Level";
            case bridge::FlowNodeKind::Screen: return "Screen";
            case bridge::FlowNodeKind::CompleteGame: return "Complete Game";
            case bridge::FlowNodeKind::ReturnToMainMenu: return "Return To Menu";
            case bridge::FlowNodeKind::Quit: return "Quit";
            default: return "Flow";
            }
        }

        [[nodiscard]] static std::string Upper(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](const unsigned char c)
                {
                    return static_cast<char>(std::toupper(c));
                });
            return value;
        }

        [[nodiscard]] static std::string Shorten(
            std::string value,
            const std::size_t maximum)
        {
            if (value.size() <= maximum) return value;
            if (maximum <= 3) return value.substr(0, maximum);
            value.resize(maximum - 3);
            value += "...";
            return value;
        }

        bridge::StoryFlowAuthoringSession* session_ = nullptr;
        bridge::StoryFlowAuthoringModel* model_ = nullptr;
        bridge::StoryFlowLayoutDocument* layout_ = nullptr;
        bridge::StoryFlowJourneyModel projection_;
        bridge::StableId selectedNodeId_;
        bridge::StableId selectedRouteId_;
        XMFLOAT4 journeyViewport_ = {};
        XMFLOAT4 inspectorBounds_ = {};
        std::size_t fingerprint_ = 0;
    };
}
