#include "RuntimeScreen.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
    constexpr std::size_t NoFocus = std::numeric_limits<std::size_t>::max();

    wi::Color ToWickedColor(
        const renegade::bridge::ScreenColor& color,
        const float opacity = 1.0f)
    {
        return wi::Color(
            color.red,
            color.green,
            color.blue,
            static_cast<std::uint8_t>(std::clamp(
                static_cast<float>(color.alpha) * opacity,
                0.0f,
                255.0f)));
    }

    wi::font::Alignment ToWickedAlignment(
        const renegade::bridge::ScreenHorizontalAlignment alignment)
    {
        using renegade::bridge::ScreenHorizontalAlignment;
        switch (alignment)
        {
        case ScreenHorizontalAlignment::Left: return wi::font::WIFALIGN_LEFT;
        case ScreenHorizontalAlignment::Center: return wi::font::WIFALIGN_CENTER;
        case ScreenHorizontalAlignment::Right: return wi::font::WIFALIGN_RIGHT;
        default: return wi::font::WIFALIGN_LEFT;
        }
    }

    wi::font::Alignment ToWickedAlignment(
        const renegade::bridge::ScreenVerticalAlignment alignment)
    {
        using renegade::bridge::ScreenVerticalAlignment;
        switch (alignment)
        {
        case ScreenVerticalAlignment::Top: return wi::font::WIFALIGN_TOP;
        case ScreenVerticalAlignment::Center: return wi::font::WIFALIGN_CENTER;
        case ScreenVerticalAlignment::Bottom: return wi::font::WIFALIGN_BOTTOM;
        default: return wi::font::WIFALIGN_TOP;
        }
    }

    void ApplyTextStyle(
        wi::gui::Widget& widget,
        const renegade::bridge::ScreenWidgetStyle& style,
        const int fontStyle)
    {
        const auto& text = style.text;
        widget.font.params.style = fontStyle;
        widget.font.params.color = ToWickedColor(
            style.normal.foreground, style.opacity);
        widget.font.params.shadowColor = ToWickedColor(
            text.shadowColor, style.opacity);
        widget.font.params.size = static_cast<int>(std::round(text.fontSize));
        widget.font.params.spacingX = text.characterSpacing;
        widget.font.params.spacingY = text.lineSpacing;
        widget.font.params.softness = text.softness;
        widget.font.params.bolden = text.bolden;
        widget.font.params.shadow_offset_x = text.shadowOffsetX;
        widget.font.params.shadow_offset_y = text.shadowOffsetY;
        widget.font.params.shadow_softness = text.shadowSoftness;
        widget.font.params.shadow_bolden = text.shadowBolden;
        widget.font.params.h_align = ToWickedAlignment(text.horizontalAlignment);
        widget.font.params.v_align = ToWickedAlignment(text.verticalAlignment);
    }

    void ApplyCornerRadius(wi::gui::Widget& widget, const float radius)
    {
        for (auto& sprite : widget.sprites)
        {
            if (radius > 0.0f)
            {
                sprite.params.enableCornerRounding();
                for (auto& corner : sprite.params.corners_rounding)
                    corner.radius = radius;
            }
            else
            {
                sprite.params.disableCornerRounding();
            }
        }
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

        const auto resolveFontStyle = [&projectRoot, &error](
            const bridge::ScreenTextStyle& text,
            int& fontStyle)
        {
            if (text.fontResource == bridge::BuiltinScreenFont)
            {
                fontStyle = 0;
                return true;
            }
            std::string fontPath;
            if (!bridge::ResolveScreenResourcePath(
                    projectRoot, text.fontResource, fontPath, error))
            {
                return false;
            }
            fontStyle = wi::font::AddFontStyle(fontPath);
            if (fontStyle < 0)
            {
                error = "Wicked could not load Runtime screen font: " + fontPath;
                return false;
            }
            return true;
        };

        const auto addVisual = [this](
            const bridge::ScreenWidget& widget,
            const bridge::ScreenRect& designRect,
            wi::gui::Widget* wickedWidget)
        {
            VisualWidget visual;
            visual.widgetId = widget.id;
            visual.designRect = designRect;
            visual.designFontSize = widget.style.text.fontSize;
            visual.designCharacterSpacing = widget.style.text.characterSpacing;
            visual.designLineSpacing = widget.style.text.lineSpacing;
            visual.designShadowOffsetX = widget.style.text.shadowOffsetX;
            visual.designShadowOffsetY = widget.style.text.shadowOffsetY;
            visual.designCornerRadius = widget.style.cornerRadius;
            visual.widget = wickedWidget;
            visuals_.push_back(std::move(visual));
        };

        // Wicked's GUI stores widgets in insertion order but renders that
        // storage in reverse. Iterate the authored back-to-front document
        // order in reverse so backgrounds render first and controls remain
        // visible above them.
        for (auto iterator = document_.widgets.rbegin();
             iterator != document_.widgets.rend();
             ++iterator)
        {
            const auto& widget = *iterator;
            const std::string widgetName = widget.name + "#" + widget.id;
            bridge::ScreenRect designRect;
            if (!bridge::ResolveScreenWidgetRect(
                    document_, widget.id, designRect, error))
            {
                Reset(renderPath);
                return false;
            }
            switch (widget.kind)
            {
            case bridge::ScreenWidgetKind::Image:
            {
                std::string resourcePath;
                const std::string& authoredImage =
                    widget.style.normal.imageResourcePath.empty() ?
                    widget.resourcePath : widget.style.normal.imageResourcePath;
                if (!bridge::ResolveScreenResourcePath(
                        projectRoot,
                        authoredImage,
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
                image->SetColor(ToWickedColor(
                    widget.style.normal.imageTint, widget.style.opacity));
                image->SetVisible(widget.visible);
                image->SetEnabled(false);
                ApplyCornerRadius(*image, widget.style.cornerRadius);
                renderPath.GetGUI().AddWidget(image.get());
                addVisual(widget, designRect, image.get());
                images_.push_back(std::move(image));
                break;
            }

            case bridge::ScreenWidgetKind::Text:
            {
                auto label = std::make_unique<wi::gui::Label>();
                label->Create(widgetName);
                label->SetText(widget.text);
                label->SetWrapEnabled(widget.style.text.wrap);
                label->SetVisible(widget.visible);
                label->SetEnabled(false);
                label->SetColor(ToWickedColor(
                    widget.style.normal.background, widget.style.opacity));
                int fontStyle = 0;
                if (!resolveFontStyle(widget.style.text, fontStyle))
                {
                    Reset(renderPath);
                    return false;
                }
                ApplyTextStyle(*label, widget.style, fontStyle);
                ApplyCornerRadius(*label, widget.style.cornerRadius);
                renderPath.GetGUI().AddWidget(label.get());
                addVisual(widget, designRect, label.get());
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
                const bool hasButtonImage =
                    !widget.style.normal.imageResourcePath.empty();
                const auto stateColor = [&widget, hasButtonImage](
                    const bridge::ScreenVisualState& state)
                {
                    return ToWickedColor(
                        (hasButtonImage || !state.imageResourcePath.empty()) ?
                            state.imageTint : state.background,
                        widget.style.opacity);
                };
                button->SetColor(stateColor(widget.style.normal),
                    wi::gui::WIDGET_ID_IDLE);
                button->SetColor(stateColor(widget.style.hover),
                    wi::gui::WIDGET_ID_FOCUS);
                button->SetColor(stateColor(widget.style.pressed),
                    wi::gui::WIDGET_ID_ACTIVE);
                button->SetColor(stateColor(widget.style.normal),
                    wi::gui::WIDGET_ID_DEACTIVATING);
                ButtonStateResources stateResources;
                const auto applyStateImage = [
                    &projectRoot, &error, &button](
                    const bridge::ScreenVisualState& state,
                    const int stateId,
                    wi::Resource* retained)
                {
                    if (state.imageResourcePath.empty()) return true;
                    std::string path;
                    if (!bridge::ResolveScreenResourcePath(
                            projectRoot, state.imageResourcePath, path, error))
                        return false;
                    wi::Resource resource = wi::resourcemanager::Load(path);
                    if (!resource.IsValid())
                    {
                        error = "Wicked could not load Runtime screen state image: " + path;
                        return false;
                    }
                    button->SetImage(resource, stateId);
                    if (retained != nullptr) *retained = resource;
                    return true;
                };
                if (!applyStateImage(widget.style.normal, wi::gui::WIDGET_ID_IDLE,
                        &stateResources.normal) ||
                    !applyStateImage(widget.style.hover, wi::gui::WIDGET_ID_FOCUS,
                        nullptr) ||
                    !applyStateImage(widget.style.pressed, wi::gui::WIDGET_ID_ACTIVE,
                        nullptr) ||
                    !applyStateImage(
                        widget.style.normal, wi::gui::WIDGET_ID_DEACTIVATING,
                        nullptr) ||
                    !applyStateImage(widget.style.focused, wi::gui::WIDGET_ID_IDLE,
                        &stateResources.focused) ||
                    !applyStateImage(widget.style.disabled, wi::gui::WIDGET_ID_IDLE,
                        &stateResources.disabled))
                {
                    Reset(renderPath);
                    return false;
                }
                int fontStyle = 0;
                if (!resolveFontStyle(widget.style.text, fontStyle))
                {
                    Reset(renderPath);
                    return false;
                }
                ApplyTextStyle(*button, widget.style, fontStyle);
                ApplyCornerRadius(*button, widget.style.cornerRadius);
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
                buttonStateResources_.emplace(
                    widget.id, std::move(stateResources));
                addVisual(widget, designRect, button.get());
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
                const bridge::ScreenVisualState& state = !widget->enabled ?
                    widget->style.disabled :
                    (widgetId == focusedId ? widget->style.focused :
                        widget->style.normal);
                const bridge::ScreenColor& background =
                    (widget->style.normal.imageResourcePath.empty() &&
                     state.imageResourcePath.empty()) ?
                        state.background : state.imageTint;
                button->SetColor(
                    ToWickedColor(background, widget->style.opacity),
                    wi::gui::WIDGET_ID_IDLE);
                button->font.params.color = ToWickedColor(
                    state.foreground, widget->style.opacity);
                const auto resources = buttonStateResources_.find(widgetId);
                if (resources != buttonStateResources_.end())
                {
                    const wi::Resource* resource = &resources->second.normal;
                    if (!widget->enabled && resources->second.disabled.IsValid())
                        resource = &resources->second.disabled;
                    else if (widgetId == focusedId &&
                        resources->second.focused.IsValid())
                        resource = &resources->second.focused;
                    if (resource->IsValid())
                        button->SetImage(*resource, wi::gui::WIDGET_ID_IDLE);
                }
            }
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
        buttonStateResources_.clear();
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

        bridge::ScreenCanvasTransform canvas;
        std::string layoutError;
        if (!bridge::ResolveScreenCanvasTransform(
                document_, logicalWidth, logicalHeight, canvas, layoutError))
        {
            return;
        }
        const float fontScale = std::min(canvas.scaleX, canvas.scaleY);

        logicalRects_.clear();
        for (auto& visual : visuals_)
        {
            const bridge::ScreenRect logical{
                canvas.offsetX + visual.designRect.x * canvas.scaleX,
                canvas.offsetY + visual.designRect.y * canvas.scaleY,
                visual.designRect.width * canvas.scaleX,
                visual.designRect.height * canvas.scaleY,
            };
            logicalRects_.emplace(visual.widgetId, logical);
            visual.widget->SetPos(XMFLOAT2(logical.x, logical.y));
            visual.widget->SetSize(XMFLOAT2(logical.width, logical.height));
            visual.widget->font.params.size = std::max(
                1, static_cast<int>(std::round(
                    visual.designFontSize * fontScale)));
            visual.widget->font.params.spacingX =
                visual.designCharacterSpacing * fontScale;
            visual.widget->font.params.spacingY =
                visual.designLineSpacing * fontScale;
            visual.widget->font.params.shadow_offset_x =
                visual.designShadowOffsetX * fontScale;
            visual.widget->font.params.shadow_offset_y =
                visual.designShadowOffsetY * fontScale;
            ApplyCornerRadius(
                *visual.widget, visual.designCornerRadius * fontScale);
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
