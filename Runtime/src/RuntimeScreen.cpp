#include "RuntimeScreen.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
    constexpr std::size_t NoFocus = std::numeric_limits<std::size_t>::max();
}

namespace renegade::runtime
{
    bool RuntimeScreenController::Initialize(
        bridge::ScreenDocument document,
        std::string& error)
    {
        if (!bridge::ValidateScreenDocument(
                document,
                document.envelope.projectId,
                error))
        {
            return false;
        }

        document_ = std::move(document);
        widgetIndex_.clear();
        for (std::size_t index = 0; index < document_.widgets.size(); ++index)
        {
            widgetIndex_.emplace(document_.widgets[index].id, index);
        }
        focusIndex_ = NoFocus;
        initialized_ = true;
        RepairFocus();
        error.clear();
        return true;
    }

    bool RuntimeScreenController::SetWidgetState(
        const std::string& widgetId,
        const bool visible,
        const bool enabled,
        std::string& error)
    {
        const auto found = widgetIndex_.find(widgetId);
        if (found == widgetIndex_.end())
        {
            error = "Runtime screen widget does not exist: " + widgetId;
            return false;
        }

        auto& widget = document_.widgets[found->second];
        widget.visible = visible;
        widget.enabled = enabled;
        RepairFocus();
        error.clear();
        return true;
    }

    bool RuntimeScreenController::FocusNext() noexcept
    {
        if (!initialized_ || document_.focusOrder.empty())
        {
            return false;
        }

        const std::size_t start = focusIndex_ == NoFocus ?
            document_.focusOrder.size() - 1 : focusIndex_;
        for (std::size_t offset = 1; offset <= document_.focusOrder.size(); ++offset)
        {
            const std::size_t candidate =
                (start + offset) % document_.focusOrder.size();
            const auto* widget = FindWidget(document_.focusOrder[candidate]);
            if (widget != nullptr && IsFocusable(*widget))
            {
                focusIndex_ = candidate;
                return true;
            }
        }

        focusIndex_ = NoFocus;
        return false;
    }

    bool RuntimeScreenController::FocusPrevious() noexcept
    {
        if (!initialized_ || document_.focusOrder.empty())
        {
            return false;
        }

        const std::size_t start = focusIndex_ == NoFocus ?
            0 : focusIndex_;
        for (std::size_t offset = 1; offset <= document_.focusOrder.size(); ++offset)
        {
            const std::size_t candidate =
                (start + document_.focusOrder.size() - offset) %
                document_.focusOrder.size();
            const auto* widget = FindWidget(document_.focusOrder[candidate]);
            if (widget != nullptr && IsFocusable(*widget))
            {
                focusIndex_ = candidate;
                return true;
            }
        }

        focusIndex_ = NoFocus;
        return false;
    }

    bool RuntimeScreenController::FocusWidget(
        const std::string& widgetId) noexcept
    {
        const auto focus = std::find(
            document_.focusOrder.begin(),
            document_.focusOrder.end(),
            widgetId);
        if (focus == document_.focusOrder.end())
        {
            return false;
        }

        const auto* widget = FindWidget(widgetId);
        if (widget == nullptr || !IsFocusable(*widget))
        {
            return false;
        }

        focusIndex_ = static_cast<std::size_t>(
            std::distance(document_.focusOrder.begin(), focus));
        return true;
    }

    const bridge::ScreenWidget* RuntimeScreenController::FocusedWidget() const noexcept
    {
        if (focusIndex_ == NoFocus ||
            focusIndex_ >= document_.focusOrder.size())
        {
            return nullptr;
        }
        return FindWidget(document_.focusOrder[focusIndex_]);
    }

    const bridge::ScreenWidget* RuntimeScreenController::FindWidget(
        const std::string& widgetId) const noexcept
    {
        const auto found = widgetIndex_.find(widgetId);
        if (found == widgetIndex_.end())
        {
            return nullptr;
        }
        return &document_.widgets[found->second];
    }

    bool RuntimeScreenController::MakeFocusedActionRequest(
        const RuntimeInputSource source,
        const std::uint64_t sequence,
        RuntimeActionRequest& request,
        std::string& error) const
    {
        const auto* focused = FocusedWidget();
        if (focused == nullptr)
        {
            error = "The Runtime screen has no focused action control.";
            return false;
        }
        return MakeActionRequest(focused->id, source, sequence, request, error);
    }

    bool RuntimeScreenController::MakeActionRequest(
        const std::string& widgetId,
        const RuntimeInputSource source,
        const std::uint64_t sequence,
        RuntimeActionRequest& request,
        std::string& error) const
    {
        const auto* widget = FindWidget(widgetId);
        if (widget == nullptr || widget->kind != bridge::ScreenWidgetKind::Button)
        {
            error = "Runtime screen activation target is missing or is not a button.";
            return false;
        }
        if (!IsFocusable(*widget))
        {
            error = "Runtime screen activation target is hidden or disabled.";
            return false;
        }

        request.actionId = widget->actionId;
        request.widgetId = widget->id;
        request.inputSource = source;
        request.sequence = sequence;
        error.clear();
        return true;
    }

    const bridge::ScreenDocument& RuntimeScreenController::Document() const noexcept
    {
        return document_;
    }

    bool RuntimeScreenController::HasFocus() const noexcept
    {
        return FocusedWidget() != nullptr;
    }

    bool RuntimeScreenController::IsFocusable(
        const bridge::ScreenWidget& widget) const noexcept
    {
        return widget.kind == bridge::ScreenWidgetKind::Button &&
            widget.visible && widget.enabled;
    }

    void RuntimeScreenController::RepairFocus() noexcept
    {
        if (focusIndex_ != NoFocus &&
            focusIndex_ < document_.focusOrder.size())
        {
            const auto* current = FindWidget(document_.focusOrder[focusIndex_]);
            if (current != nullptr && IsFocusable(*current))
            {
                return;
            }
        }

        focusIndex_ = NoFocus;
        (void)FocusNext();
    }

    bool RuntimeScreenPresenter::Load(
        const bridge::ScreenDocument& document,
        const std::string& projectRoot,
        wi::RenderPath2D& renderPath,
        RuntimeScreenController& controller,
        RequestSink requestSink,
        std::string& error)
    {
        Reset(renderPath);
        if (!requestSink)
        {
            error = "Runtime screen presenter requires an action request sink.";
            return false;
        }

        controller_ = &controller;
        requestSink_ = std::move(requestSink);
        if (!renderer_.Load(
                document,
                projectRoot,
                renderPath,
                [this](const bridge::StableId& widgetId)
                {
                    if (controller_ != nullptr)
                        (void)controller_->FocusWidget(widgetId);
                    QueueWidget(widgetId, RuntimeInputSource::Mouse);
                },
                error))
        {
            controller_ = nullptr;
            requestSink_ = {};
            return false;
        }
        RefreshVisualFocus(controller);
        error.clear();
        return true;
    }

    void RuntimeScreenPresenter::UpdateInput(
        wi::RenderPath2D& renderPath,
        RuntimeScreenController& controller)
    {
        if (!renderer_.IsLoaded())
        {
            return;
        }

        renderer_.ApplyLayout(renderPath);

        const bool gamepadPrevious =
            wi::input::Press(wi::input::GAMEPAD_BUTTON_UP) ||
            wi::input::Press(wi::input::GAMEPAD_BUTTON_LEFT) ||
            wi::input::Press(
                wi::input::GAMEPAD_ANALOG_THUMBSTICK_L_AS_BUTTON_UP) ||
            wi::input::Press(
                wi::input::GAMEPAD_ANALOG_THUMBSTICK_L_AS_BUTTON_LEFT);
        const bool gamepadNext =
            wi::input::Press(wi::input::GAMEPAD_BUTTON_DOWN) ||
            wi::input::Press(wi::input::GAMEPAD_BUTTON_RIGHT) ||
            wi::input::Press(
                wi::input::GAMEPAD_ANALOG_THUMBSTICK_L_AS_BUTTON_DOWN) ||
            wi::input::Press(
                wi::input::GAMEPAD_ANALOG_THUMBSTICK_L_AS_BUTTON_RIGHT);
        const bool gamepadConfirm =
            wi::input::Press(wi::input::GAMEPAD_BUTTON_2);

        if (gamepadPrevious)
        {
            (void)controller.FocusPrevious();
        }
        else if (gamepadNext)
        {
            (void)controller.FocusNext();
        }
        else if (gamepadConfirm)
        {
            QueueFocused(RuntimeInputSource::Gamepad);
        }
        else
        {
            const bool keyboardPrevious =
                wi::input::Press(wi::input::KEYBOARD_BUTTON_UP) ||
                wi::input::Press(wi::input::KEYBOARD_BUTTON_LEFT);
            const bool keyboardNext =
                wi::input::Press(wi::input::KEYBOARD_BUTTON_DOWN) ||
                wi::input::Press(wi::input::KEYBOARD_BUTTON_RIGHT) ||
                wi::input::Press(wi::input::KEYBOARD_BUTTON_TAB);
            const bool keyboardConfirm =
                wi::input::Press(wi::input::KEYBOARD_BUTTON_ENTER) ||
                wi::input::Press(wi::input::KEYBOARD_BUTTON_SPACE);

            if (keyboardPrevious)
            {
                (void)controller.FocusPrevious();
            }
            else if (keyboardNext)
            {
                (void)controller.FocusNext();
            }
            else if (keyboardConfirm)
            {
                QueueFocused(RuntimeInputSource::Keyboard);
            }
            else
            {
                ApplyPointerFocus(controller);
            }
        }

        RefreshVisualFocus(controller);
    }

    void RuntimeScreenPresenter::RefreshVisualFocus(
        const RuntimeScreenController& controller)
    {
        const auto* focused = controller.FocusedWidget();
        renderer_.SetFocusedWidget(
            focused == nullptr ? bridge::StableId{} : focused->id);
        for (const auto& widget : controller.Document().widgets)
            renderer_.SetWidgetState(widget.id, widget.visible, widget.enabled);
    }

    void RuntimeScreenPresenter::Reset(wi::RenderPath2D& renderPath) noexcept
    {
        renderer_.Reset(renderPath);
        requestSink_ = {};
        controller_ = nullptr;
        nextSequence_ = 1;
    }

    bool RuntimeScreenPresenter::IsLoaded() const noexcept
    {
        return renderer_.IsLoaded();
    }

    void RuntimeScreenPresenter::QueueFocused(const RuntimeInputSource source)
    {
        if (controller_ == nullptr || !requestSink_)
        {
            return;
        }

        RuntimeActionRequest request;
        std::string error;
        if (controller_->MakeFocusedActionRequest(
                source,
                nextSequence_++,
                request,
                error))
        {
            requestSink_(std::move(request));
        }
    }

    void RuntimeScreenPresenter::QueueWidget(
        const std::string& widgetId,
        const RuntimeInputSource source)
    {
        if (controller_ == nullptr || !requestSink_)
        {
            return;
        }

        RuntimeActionRequest request;
        std::string error;
        if (controller_->MakeActionRequest(
                widgetId,
                source,
                nextSequence_++,
                request,
                error))
        {
            requestSink_(std::move(request));
        }
    }

    void RuntimeScreenPresenter::ApplyPointerFocus(
        RuntimeScreenController& controller)
    {
        const auto& mouse = wi::input::GetMouseState();
        if (mouse.delta_position.x == 0.0f && mouse.delta_position.y == 0.0f)
        {
            return;
        }

        const XMFLOAT4 pointer = wi::input::GetPointer();
        const auto& document = controller.Document();
        for (auto iterator = document.focusOrder.rbegin();
            iterator != document.focusOrder.rend();
            ++iterator)
        {
            const auto* rect = renderer_.LogicalRect(*iterator);
            const auto* widget = controller.FindWidget(*iterator);
            if (rect == nullptr || widget == nullptr ||
                !widget->visible || !widget->enabled)
            {
                continue;
            }

            const auto& area = *rect;
            if (pointer.x >= area.x && pointer.x <= area.x + area.width &&
                pointer.y >= area.y && pointer.y <= area.y + area.height)
            {
                (void)controller.FocusWidget(*iterator);
                return;
            }
        }
    }
}
