#pragma once

#include "RuntimeActions.h"
#include "renegade/bridge/ScreenService.h"
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
        void QueueFocused(RuntimeInputSource source);
        void QueueWidget(
            const std::string& widgetId,
            RuntimeInputSource source);
        void ApplyPointerFocus(RuntimeScreenController& controller);

        RuntimeScreenController* controller_ = nullptr;
        RequestSink requestSink_;
        screen::ScreenRenderer renderer_;
        std::uint64_t nextSequence_ = 1;
    };
}
