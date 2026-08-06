#pragma once

#include "RuntimeActions.h"
#include "renegade/bridge/ScreenService.h"

#include <WickedEngine.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace renegade::runtime
{
    class RuntimeScreenController
    {
    public:
        [[nodiscard]] bool Initialize(
            bridge::ScreenDocument document,
            std::string& error);
        [[nodiscard]] bool SetWidgetState(
            const std::string& widgetId,
            bool visible,
            bool enabled,
            std::string& error);

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

        bridge::ScreenDocument document_;
        std::unordered_map<std::string, std::size_t> widgetIndex_;
        std::size_t focusIndex_ = static_cast<std::size_t>(-1);
        bool initialized_ = false;
    };

    class RuntimeScreenPresenter
    {
    public:
        using RequestSink = std::function<void(RuntimeActionRequest)>;

        [[nodiscard]] bool Load(
            const bridge::ScreenDocument& document,
            const std::string& projectRoot,
            wi::RenderPath2D& renderPath,
            RuntimeScreenController& controller,
            RequestSink requestSink,
            std::string& error);
        void UpdateInput(
            wi::RenderPath2D& renderPath,
            RuntimeScreenController& controller);
        void RefreshVisualFocus(
            const RuntimeScreenController& controller);
        void Reset(wi::RenderPath2D& renderPath) noexcept;

        [[nodiscard]] bool IsLoaded() const noexcept;

    private:
        struct VisualWidget
        {
            std::string widgetId;
            bridge::ScreenRect designRect;
            wi::gui::Widget* widget = nullptr;
        };

        void ApplyLayout(wi::RenderPath2D& renderPath);
        void QueueFocused(RuntimeInputSource source);
        void QueueWidget(
            const std::string& widgetId,
            RuntimeInputSource source);
        void ApplyPointerFocus(RuntimeScreenController& controller);

        bridge::ScreenDocument document_;
        RuntimeScreenController* controller_ = nullptr;
        RequestSink requestSink_;
        std::vector<VisualWidget> visuals_;
        std::unordered_map<std::string, wi::gui::Button*> buttons_;
        std::unordered_map<std::string, bridge::ScreenRect> logicalRects_;
        std::vector<std::unique_ptr<wi::gui::Image>> images_;
        std::vector<std::unique_ptr<wi::gui::Label>> labels_;
        std::vector<std::unique_ptr<wi::gui::Button>> buttonStorage_;
        std::uint64_t nextSequence_ = 1;
        bool loaded_ = false;
    };
}
