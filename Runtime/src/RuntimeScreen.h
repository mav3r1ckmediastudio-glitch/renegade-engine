#pragma once
#include "renegade/bridge/InventoryPagedScreenAdapter.h"

#include "RuntimeActions.h"
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/ScreenInteractionPlan.h"
#include "renegade/bridge/ScreenBindingFrame.h"
#include "renegade/screen/ScreenRenderer.h"

#include <WickedEngine.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace renegade::runtime
{
    class RuntimeScreenController
    {
    public:
        [[nodiscard]] bool Initialize(
            bridge::ScreenDocument document,
            std::string& error);
        [[nodiscard]] bool ApplyBindings(const bridge::ScreenBindingSnapshot& snapshot,
            const bridge::ScreenBindingContext& context, std::string& error);
        [[nodiscard]] const bridge::ScreenBindingFrame& BindingFrame() const noexcept
        { return bindingFrame_; }
        [[nodiscard]] const bridge::ScreenDocument& AuthoredDocument() const noexcept
        { return authoredDocument_; }

        [[nodiscard]] bool SetWidgetState(
            const std::string& widgetId,
            bool visible,
            bool enabled,
            std::string& error);
        // A transient gameplay binding can update presentation text without
        // rewriting the editor's authored Screen document.
        [[nodiscard]] bool SetWidgetDisplayText(
            const std::string& widgetId,std::string text,std::string& error);

        // Local UI state only. Gameplay changes require a separate governed command.
        [[nodiscard]] bool SetControlValue(const std::string& id,bridge::GameplayAttributeValue value,std::string& error);

        [[nodiscard]] bool ExecuteInteractions(const std::string& id,bridge::ScreenInteractionEvent event,std::string& error);

        [[nodiscard]] bool FocusNext() noexcept;
        [[nodiscard]] bool FocusPrevious() noexcept;
        [[nodiscard]] bool FocusWidget(
            const std::string& widgetId) noexcept;

        [[nodiscard]] const bridge::ScreenWidget* FocusedWidget() const noexcept;
        [[nodiscard]] const bridge::ScreenWidget* FindWidget(
            const std::string& widgetId) const noexcept;
        [[nodiscard]] bool MakeFocusedActionRequest(
            RuntimeInputSource source,
            std::uint64_t sequence,
            RuntimeActionRequest& request,
            std::string& error) const;
        [[nodiscard]] bool MakeActionRequest(
            const std::string& widgetId,
            RuntimeInputSource source,
            std::uint64_t sequence,
            RuntimeActionRequest& request,
            std::string& error) const;

        [[nodiscard]] const bridge::ScreenDocument& Document() const noexcept;
        [[nodiscard]] bool HasFocus() const noexcept;

    private:
        [[nodiscard]] bool IsFocusable(
            const bridge::ScreenWidget& widget) const noexcept;
        void RepairFocus() noexcept;

        bool CommitInteractionPlan(bridge::ScreenInteractionPlan plan,std::string& error);
        void ApplyResolvedFrame(bridge::ScreenBindingFrame frame);
        bridge::ScreenDocument authoredDocument_;
        bridge::ScreenWidgetStateOverrides stateOverrides_;
        // Runtime-only, per-screen presentation text. Restored after any
        // resolved state/binding frame so changing one slot cannot reset text
        // already refreshed on another slot. Cleared on Initialize.
        std::unordered_map<std::string,std::string> displayTextOverrides_;
        bridge::ScreenBindingSnapshot bindingSnapshot_;
        bridge::ScreenBindingContext bindingContext_;
        bridge::ScreenBindingFrame bindingFrame_;
        bool hasBindingSnapshot_ = false;
        bridge::ScreenDocument document_;
        std::unordered_map<std::string, std::size_t> widgetIndex_;
        std::size_t focusIndex_ = static_cast<std::size_t>(-1);
        bool initialized_ = false;
    };

    // Inventory-local requests are only admitted by the separate scoped inventory host.
    inline bool ShouldDispatchScreenAction(const bridge::ScreenWidget& widget,bool inventoryHost) {
        if(bridge::IsInventoryPagedLocalAction(widget.actionId))return inventoryHost;
        return !widget.localOnly;
    }
    class RuntimeScreenPresenter
    {
    public:
        using RequestSink = std::function<void(RuntimeActionRequest)>;
        using PointerDragSink = std::function<void(
            const bridge::StableId&,bool begin,float x,float y)>;

        [[nodiscard]] bool Load(
            const bridge::ScreenDocument& document,
            const std::string& projectRoot,
            wi::RenderPath2D& renderPath,
            RuntimeScreenController& controller,
            RequestSink requestSink,
            std::string& error,PointerDragSink dragSink={},bool inventoryHost=false);
        void UpdateInput(
            wi::RenderPath2D& renderPath,
            RuntimeScreenController& controller, bool navigationEnabled = true, float uiDeltaTime = 0);
        void SetPresentationVisible(bool visible) noexcept;
        bool BeginExit(const std::vector<bridge::ScreenAnimationTrack>* overrideTracks,std::string& error);
        bool OverrideEnterTransition(const std::vector<bridge::ScreenAnimationTrack>& tracks,wi::RenderPath2D& renderPath,std::string& error);
        void CancelExit() noexcept;
        bool ExitComplete() const noexcept {return transition_.ExitComplete();}
        bridge::ScreenTransitionPhase TransitionPhase() const noexcept {return transition_.Phase();}
        void SetReducedMotion(bool enabled) noexcept {reducedMotion_=enabled;}
        void SetAccessibilitySettings(bridge::ScreenAccessibilitySettings settings) noexcept {renderer_.SetAccessibilitySettings(settings);}
        [[nodiscard]] bool ReducedMotion() const noexcept {return reducedMotion_;}
        void AttachPresentation(wi::RenderPath2D& renderPath);
        void DetachPresentation(wi::RenderPath2D& renderPath) noexcept;
        void SetInputEnabled(bool enabled) noexcept;
        void RefreshVisualFocus(
            const RuntimeScreenController& controller);
        void Reset(wi::RenderPath2D& renderPath) noexcept;

        [[nodiscard]] bool IsLoaded() const noexcept;
        // Hit-test actual shared Screen layout, including disabled empty slots.
        [[nodiscard]] std::string InventorySlotAt(float x,float y) const;
        [[nodiscard]] std::string InventoryContextAt(float x,float y) const;
        [[nodiscard]] bool PagedScrollbarGeometry(std::size_t panel,
            bridge::ScreenRect& track,bridge::ScreenRect& thumb) const;
        void SetPagedScrollbar(std::size_t panel,float position,float fraction);
        bool SetVisualImageResource(const bridge::StableId& widgetId,
            const std::string& resourcePath,std::string& error);

    private:
        void QueueFocused(RuntimeInputSource source);
        void QueueWidget(
            const std::string& widgetId,
            RuntimeInputSource source);
        void ApplyPointerFocus(RuntimeScreenController& controller);

        RuntimeScreenController* controller_ = nullptr;
        RequestSink requestSink_;
        bool inventoryHost_=false;
        screen::ScreenRenderer renderer_;
        std::uint64_t nextSequence_ = 1;
        double animationTime_=0;
        bool presentationVisible_=true;
        bool reducedMotion_=false;
        bool requestedInput_=true;
        bridge::ScreenTransitionPair transitionPolicy_;
        bridge::ScreenTransitionPlayer transition_;
    };
}
