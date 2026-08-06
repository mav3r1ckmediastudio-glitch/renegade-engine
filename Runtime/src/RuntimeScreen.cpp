#include "RuntimeScreen.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
    constexpr std::size_t NoFocus = std::numeric_limits<std::size_t>::max();

    wi::Color ButtonIdleColor()
    {
        return wi::Color(18, 27, 42, 235);
    }

    wi::Color ButtonFocusColor()
    {
        return wi::Color(0, 196, 235, 255);
    }

    wi::Color ButtonActiveColor()
    {
        return wi::Color(220, 250, 255, 255);
    }
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

        document_ = document;
        controller_ = &controller;
        requestSink_ = std::move(requestSink);

        for (const auto& widget : document_.widgets)
        {
            const std::string widgetName = widget.name + "#" + widget.id;
            switch (widget.kind)
            {
            case bridge::ScreenWidgetKind::Image:
            {
                std::string resourcePath;
                if (!bridge::ResolveScreenResourcePath(
                        projectRoot,
                        widget.resourcePath,
                        resourcePath,
                        error))
                {
                    Reset(renderPath);
                    return false;
                }
                wi::Resource resource = wi::resourcemanager::Load(resourcePath);
                if (!resource.IsValid())
                {
                    error = "Wicked could not load Runtime screen image: " +
                        resourcePath;
                    Reset(renderPath);
                    return false;
                }

                auto image = std::make_unique<wi::gui::Image>();
                image->Create(widgetName);
                image->SetImage(resource);
                image->SetVisible(widget.visible);
                image->SetEnabled(false);
                renderPath.GetGUI().AddWidget(image.get());
                visuals_.push_back({widget.id, widget.rect, image.get()});
                images_.push_back(std::move(image));
                break;
            }

            case bridge::ScreenWidgetKind::Text:
            {
                auto label = std::make_unique<wi::gui::Label>();
                label->Create(widgetName);
                label->SetText(widget.text);
                label->SetWrapEnabled(true);
                label->SetVisible(widget.visible);
                label->SetEnabled(false);
                label->SetColor(wi::Color(0, 0, 0, 0));
                label->font.params.color = wi::Color(238, 250, 255, 255);
                label->font.params.shadowColor = wi::Color(0, 0, 0, 210);
                label->font.params.size = static_cast<int>(
                    std::clamp(widget.rect.height * 0.55f, 18.0f, 64.0f));
                label->font.params.h_align = wi::font::WIFALIGN_CENTER;
                label->font.params.v_align = wi::font::WIFALIGN_CENTER;
                renderPath.GetGUI().AddWidget(label.get());
                visuals_.push_back({widget.id, widget.rect, label.get()});
                labels_.push_back(std::move(label));
                break;
            }

            case bridge::ScreenWidgetKind::Button:
            {
                auto button = std::make_unique<wi::gui::Button>();
                button->Create(widgetName);
                button->SetText(widget.text);
                button->SetVisible(widget.visible);
                button->SetEnabled(widget.enabled);
                button->SetColor(ButtonIdleColor(), wi::gui::WIDGET_ID_IDLE);
                button->SetColor(ButtonFocusColor(), wi::gui::WIDGET_ID_FOCUS);
                button->SetColor(ButtonActiveColor(), wi::gui::WIDGET_ID_ACTIVE);
                button->font.params.color = wi::Color(240, 252, 255, 255);
                button->font.params.size = static_cast<int>(
                    std::clamp(widget.rect.height * 0.42f, 18.0f, 42.0f));
                const std::string widgetId = widget.id;
                button->OnClick(
                    [this, widgetId](const wi::gui::EventArgs&)
                    {
                        if (controller_ != nullptr)
                        {
                            (void)controller_->FocusWidget(widgetId);
                        }
                        QueueWidget(widgetId, RuntimeInputSource::Mouse);
                    });
                renderPath.GetGUI().AddWidget(button.get());
                buttons_.emplace(widget.id, button.get());
                visuals_.push_back({widget.id, widget.rect, button.get()});
                buttonStorage_.push_back(std::move(button));
                break;
            }
            }
        }

        loaded_ = true;
        ApplyLayout(renderPath);
        RefreshVisualFocus(controller);
        error.clear();
        return true;
    }

    void RuntimeScreenPresenter::UpdateInput(
        wi::RenderPath2D& renderPath,
        RuntimeScreenController& controller)
    {
        if (!loaded_)
        {
            return;
        }

        ApplyLayout(renderPath);

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
        const std::string focusedId = focused == nullptr ? std::string{} : focused->id;

        for (const auto& [widgetId, button] : buttons_)
        {
            const auto* widget = controller.FindWidget(widgetId);
            if (widget != nullptr)
            {
                button->SetVisible(widget->visible);
                button->SetEnabled(widget->enabled);
            }
            button->SetColor(
                widgetId == focusedId ? ButtonFocusColor() : ButtonIdleColor(),
                wi::gui::WIDGET_ID_IDLE);
        }
    }

    void RuntimeScreenPresenter::Reset(wi::RenderPath2D& renderPath) noexcept
    {
        for (const auto& visual : visuals_)
        {
            if (visual.widget != nullptr)
            {
                renderPath.GetGUI().RemoveWidget(visual.widget);
            }
        }

        visuals_.clear();
        buttons_.clear();
        logicalRects_.clear();
        images_.clear();
        labels_.clear();
        buttonStorage_.clear();
        requestSink_ = {};
        controller_ = nullptr;
        document_ = {};
        nextSequence_ = 1;
        loaded_ = false;
    }

    bool RuntimeScreenPresenter::IsLoaded() const noexcept
    {
        return loaded_;
    }

    void RuntimeScreenPresenter::ApplyLayout(wi::RenderPath2D& renderPath)
    {
        const float logicalWidth = renderPath.GetLogicalWidth();
        const float logicalHeight = renderPath.GetLogicalHeight();
        if (logicalWidth <= 0.0f || logicalHeight <= 0.0f ||
            document_.designWidth <= 0.0f || document_.designHeight <= 0.0f)
        {
            return;
        }

        const float scale = std::min(
            logicalWidth / document_.designWidth,
            logicalHeight / document_.designHeight);
        const float offsetX =
            (logicalWidth - document_.designWidth * scale) * 0.5f;
        const float offsetY =
            (logicalHeight - document_.designHeight * scale) * 0.5f;

        logicalRects_.clear();
        for (auto& visual : visuals_)
        {
            const bridge::ScreenRect logical{
                offsetX + visual.designRect.x * scale,
                offsetY + visual.designRect.y * scale,
                visual.designRect.width * scale,
                visual.designRect.height * scale,
            };
            logicalRects_.emplace(visual.widgetId, logical);
            visual.widget->SetPos(XMFLOAT2(logical.x, logical.y));
            visual.widget->SetSize(XMFLOAT2(logical.width, logical.height));
        }
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
        for (auto iterator = document_.focusOrder.rbegin();
            iterator != document_.focusOrder.rend();
            ++iterator)
        {
            const auto rect = logicalRects_.find(*iterator);
            const auto* widget = controller.FindWidget(*iterator);
            if (rect == logicalRects_.end() || widget == nullptr ||
                !widget->visible || !widget->enabled)
            {
                continue;
            }

            const auto& area = rect->second;
            if (pointer.x >= area.x && pointer.x <= area.x + area.width &&
                pointer.y >= area.y && pointer.y <= area.y + area.height)
            {
                (void)controller.FocusWidget(*iterator);
                return;
            }
        }
    }
}
