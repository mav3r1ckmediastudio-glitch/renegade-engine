#pragma once

#include "renegade/bridge/ScreenService.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace wi
{
    class RenderPath2D;
}

namespace renegade::screen
{
    enum class ScreenInteractionState
    {
        Normal,
        Hover,
        Pressed,
        Focused,
        Disabled,
    };

    struct ScreenRenderItem
    {
        bridge::StableId widgetId;
        bridge::ScreenWidgetKind kind = bridge::ScreenWidgetKind::Text;
        bridge::ScreenRect logicalRect;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        ScreenInteractionState interaction = ScreenInteractionState::Normal;
        bridge::ScreenVisualState visual;
        bridge::ScreenColor borderColor;
        float borderWidth = 0.0f;
        float cornerRadius = 0.0f;
        float opacity = 1.0f;
        float fontSize = 0.0f;
        float characterSpacing = 0.0f;
        float lineSpacing = 0.0f;
        float shadowOffsetX = 0.0f;
        float shadowOffsetY = 0.0f;
        bool visible = true;
        bool enabled = true;
    };

    // Deterministic, renderer-independent evidence for the exact frame that
    // Runtime and Screen Editor preview ask the renderer to draw.
    [[nodiscard]] bool BuildScreenRenderItems(
        const bridge::ScreenDocument& document,
        float outputWidth,
        float outputHeight,
        const std::unordered_map<bridge::StableId, ScreenInteractionState>&
            interactions,
        std::vector<ScreenRenderItem>& items,
        std::string& error);

    // Produces the same frame inside an editor-owned logical viewport. This is
    // a translation of the Runtime contract, not a second preview renderer.
    [[nodiscard]] bool BuildScreenRenderItemsInViewport(
        const bridge::ScreenDocument& document,
        const bridge::ScreenRect& viewport,
        const std::unordered_map<bridge::StableId, ScreenInteractionState>&
            interactions,
        std::vector<ScreenRenderItem>& items,
        std::string& error);

    // Renegade's sole Wicked-backed Screen presentation implementation.
    // Runtime and Gate 8C Screen Editor preview instantiate this same class
    // rather than reproducing its drawing rules.
    class ScreenRenderer
    {
    public:
        using ActivateSink = std::function<void(const bridge::StableId&)>;

        ScreenRenderer();
        ~ScreenRenderer();
        ScreenRenderer(ScreenRenderer&&) noexcept;
        ScreenRenderer& operator=(ScreenRenderer&&) noexcept;
        ScreenRenderer(const ScreenRenderer&) = delete;
        ScreenRenderer& operator=(const ScreenRenderer&) = delete;

        [[nodiscard]] bool Load(
            const bridge::ScreenDocument& document,
            const std::string& projectRoot,
            wi::RenderPath2D& renderPath,
            ActivateSink activateSink,
            std::string& error);
        // Studio preview can confine the exact Runtime presentation to its
        // central canvas. Runtime leaves this unset and continues to use the
        // full RenderPath logical extent.
        void SetViewport(const bridge::ScreenRect& viewport) noexcept;
        void ClearViewport() noexcept;
        void ApplyLayout(wi::RenderPath2D& renderPath);
        void SetFocusedWidget(const bridge::StableId& widgetId);
        void SetWidgetState(
            const bridge::StableId& widgetId,
            bool visible,
            bool enabled);
        void Reset(wi::RenderPath2D& renderPath) noexcept;

        [[nodiscard]] const bridge::ScreenRect* LogicalRect(
            const bridge::StableId& widgetId) const noexcept;
        [[nodiscard]] bool IsLoaded() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
