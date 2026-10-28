#pragma once

#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/ScreenAccessibility.h"

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
        float fill = 0.5f;
        bridge::ScreenFillDirection fillDirection = bridge::ScreenFillDirection::LeftToRight;
        bridge::ScreenRect fillRect;
        bridge::GameplayAttributeValue controlValue=0.0;
        float controlFraction=0;
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
        std::string& error,float layoutWidth=0);

    // Produces the same frame inside an editor-owned logical viewport. This is
    // a translation of the Runtime contract, not a second preview renderer.
    [[nodiscard]] bool BuildScreenRenderItemsInViewport(
        const bridge::ScreenDocument& document,
        const bridge::ScreenRect& viewport,
        const std::unordered_map<bridge::StableId, ScreenInteractionState>&
            interactions,
        std::vector<ScreenRenderItem>& items,
        std::string& error,float layoutWidth=0);

    // Shared deterministic tracks, evaluated after layout and before drawing/hit geometry.
    [[nodiscard]] bool BuildAnimatedScreenRenderItems(const bridge::ScreenDocument& document,
        const bridge::ScreenRect& viewport,double seconds,bool reducedMotion,
        const std::unordered_map<bridge::StableId,ScreenInteractionState>& interactions,
        std::vector<ScreenRenderItem>& items,std::string& error,float layoutWidth=0);

    [[nodiscard]] bool ApplyScreenTransitionFrame(const bridge::ScreenDocument& document,
        const bridge::ScreenRect& viewport,const bridge::ScreenTransitionFrame& frame,
        std::vector<ScreenRenderItem>& items,std::string& error);

    // Accessibility shares the exact animated drawing/hit frame. UI scale
    // changes logical design space so anchors/containers/breakpoints reflow.
    [[nodiscard]] bool BuildAccessibleScreenRenderItems(const bridge::ScreenDocument& document,
        const bridge::ScreenRect& viewport,const bridge::ScreenAccessibilitySettings& settings,
        const bridge::ScreenAnimationPreview& animation,const bridge::ScreenTransitionFrame& transition,
        const std::unordered_map<bridge::StableId,ScreenInteractionState>& interactions,
        std::vector<ScreenRenderItem>& items,std::string& error,float layoutWidth=0);

    // Renegade's sole Wicked-backed Screen presentation implementation.
    // Runtime and Gate 8C Screen Editor preview instantiate this same class
    // rather than reproducing its drawing rules.
    class ScreenRenderer
    {
    public:
        using ActivateSink = std::function<void(const bridge::StableId&)>;
        using ControlSink = std::function<bool(const bridge::StableId&,const bridge::GameplayAttributeValue&)>;
        // Optional pointer gesture callback shared by Runtime and authoring
        // clients. It owns no gameplay actions or item semantics.
        using DragSink = std::function<void(const bridge::StableId&,bool begin,float x,float y)>;

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
            std::string& error, ControlSink controlSink = {},
            DragSink dragSink = {});
        // Studio preview can confine the exact Runtime presentation to its
        // central canvas. Runtime leaves this unset and continues to use the
        // full RenderPath logical extent.
        void SetViewport(const bridge::ScreenRect& viewport) noexcept;
        void ClearViewport() noexcept;
        // Optional editor clip is independent of the projected document viewport.
        // Runtime leaves this unset; authored geometry is never modified.
        void SetClipViewport(const bridge::ScreenRect& clip) noexcept;
        void ClearClipViewport() noexcept;
        void ApplyLayout(wi::RenderPath2D& renderPath);
        bool SetWidgetControlValue(const bridge::StableId& id,const bridge::GameplayAttributeValue& value);
        bool ActivateControl(const bridge::StableId& id);
        bool AdjustControl(const bridge::StableId& id,int direction);
        [[nodiscard]] bool ConsumesTextInput() const noexcept;
        void SetAnimationPreview(bridge::ScreenAnimationPreview preview) noexcept;
        void SetTransitionFrame(bridge::ScreenTransitionFrame frame) noexcept;
        void SetAccessibilitySettings(bridge::ScreenAccessibilitySettings settings) noexcept;
        [[nodiscard]] bool IsWidgetInteractive(const bridge::StableId& id) const noexcept;
        // Runtime keeps renderer surfaces interactive. Screen Editor disables
        // only GUI input ownership so the authoring workspace exclusively owns
        // selection, drag and resize while drawing the same authored surfaces.
        void SetInputEnabled(bool enabled) noexcept;
        void SetPresentationVisible(bool visible) noexcept;
        void AttachPresentation(wi::RenderPath2D& renderPath);
        void DetachPresentation(wi::RenderPath2D& renderPath) noexcept;
        [[nodiscard]] bool IsInputEnabled() const noexcept;
        void SetFocusedWidget(const bridge::StableId& widgetId);
        void SetWidgetState(
            const bridge::StableId& widgetId,
            bool visible,
            bool enabled);
        // Runtime-only vertical scrollbar thumb geometry, scaled by the SAME
        // accessible layout pass as all authored widgets. No persisted edits.
        void SetScrollbarThumb(const bridge::StableId& thumbId,
            const bridge::StableId& trackId,float position,float fraction);
        void SetWidgetFill(const bridge::StableId& widgetId, float fill);
        void SetWidgetText(const bridge::StableId& widgetId, const std::string& text);
        // Transient image for UI-bound icon widgets. Only project-contained
        // Image resources may be loaded, never an arbitrary filesystem path.
        bool SetWidgetImageResource(const bridge::StableId& widgetId,
            const std::string& resourcePath,std::string& error);
        void Reset(wi::RenderPath2D& renderPath) noexcept;

        [[nodiscard]] const bridge::ScreenRect* LogicalRect(
            const bridge::StableId& widgetId) const noexcept;
        [[nodiscard]] bool IsLoaded() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
