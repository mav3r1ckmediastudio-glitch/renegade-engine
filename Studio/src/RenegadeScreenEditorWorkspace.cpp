#include "RenegadeScreenEditorWorkspace.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

namespace
{
    constexpr float HeaderHeight = 64.0f;
    constexpr float FooterHeight = 44.0f;
    constexpr float HierarchyWidth = 260.0f;
    constexpr float InspectorWidth = 360.0f;
    constexpr float PanelGap = 14.0f;
    constexpr float HierarchyRowHeight = 32.0f;

    constexpr wi::Color Background(4, 7, 10, 255);
    constexpr wi::Color Surface(8, 13, 17, 255);
    constexpr wi::Color Raised(13, 21, 26, 255);
    constexpr wi::Color Border(34, 62, 73, 255);
    constexpr wi::Color Text(244, 244, 244, 255);
    constexpr wi::Color Muted(135, 151, 159, 255);
    constexpr wi::Color Cyan(39, 183, 222, 255);
    constexpr wi::Color Orange(222, 91, 29, 255);

    void Rect(
        const float x, const float y, const float width, const float height,
        const wi::Color color, const wi::graphics::CommandList cmd)
    {
        if (width <= 0.0f || height <= 0.0f) return;
        wi::image::Params params(x, y, width, height, color);
        params.blendFlag = wi::enums::BLENDMODE_ALPHA;
        wi::image::Draw(nullptr, params, cmd);
    }

    void Outline(
        const float x, const float y, const float width, const float height,
        const wi::Color color, const float thickness,
        const wi::graphics::CommandList cmd)
    {
        Rect(x, y, width, thickness, color, cmd);
        Rect(x, y + height - thickness, width, thickness, color, cmd);
        Rect(x, y, thickness, height, color, cmd);
        Rect(x + width - thickness, y, thickness, height, color, cmd);
    }

    void Label(
        const std::string& value, const float x, const float y,
        const int size, const wi::Color color,
        const wi::graphics::CommandList cmd)
    {
        wi::font::Params params(
            x, y, size, wi::font::WIFALIGN_LEFT,
            wi::font::WIFALIGN_TOP, color, wi::Color::Transparent());
        params.bolden = 0.12f;
        wi::font::Draw(value, params, cmd);
    }

    bool Contains(
        const renegade::bridge::ScreenRect& bounds,
        const XMFLOAT4& pointer) noexcept
    {
        return pointer.x >= bounds.x && pointer.y >= bounds.y &&
            pointer.x < bounds.x + bounds.width &&
            pointer.y < bounds.y + bounds.height;
    }

    std::string Trim(std::string value)
    {
        const auto whitespace = [](const unsigned char character)
        {
            return std::isspace(character) != 0;
        };
        while (!value.empty() && whitespace(value.front()))
            value.erase(value.begin());
        while (!value.empty() && whitespace(value.back()))
            value.pop_back();
        return value;
    }

    std::string Number(const float value)
    {
        std::ostringstream text;
        text << std::fixed << std::setprecision(1) << value;
        return text.str();
    }

    bool ParseNumber(const std::string& value, float& result)
    {
        try
        {
            std::size_t used = 0;
            result = std::stof(Trim(value), &used);
            return used == Trim(value).size() && std::isfinite(result);
        }
        catch (...)
        {
            return false;
        }
    }

    std::string Shorten(std::string value, const std::size_t limit)
    {
        if (value.size() <= limit) return value;
        value.resize(limit > 3 ? limit - 3 : limit);
        if (limit > 3) value += "...";
        return value;
    }
}

namespace renegade::studio
{
    void RenegadeScreenEditorWorkspace::Create()
    {
        SetName("Renegade Screen Editor workspace");
        SetShadowRadius(0.0f);
        CreateControls();
        SetLayout(width_, height_);
    }

    void RenegadeScreenEditorWorkspace::SetLayout(
        const float width,
        const float height)
    {
        width_ = std::max(1.0f, width);
        height_ = std::max(1.0f, height);
        SetSize(XMFLOAT2(width_, height_));
        LayoutControls();
    }

    void RenegadeScreenEditorWorkspace::Bind(
        bridge::ScreenAuthoringSession* session,
        screen::ScreenRenderer* renderer)
    {
        session_ = session;
        renderer_ = renderer;
        selectedWidgetId_.clear();
        EnsureSelection();
        status_ = session_ && session_->IsLoaded()
            ? "SCREEN READY // SHARED RUNTIME PREVIEW"
            : "NO SCREEN OPEN";
        RefreshInspector();
    }

    void RenegadeScreenEditorWorkspace::Clear() noexcept
    {
        session_ = nullptr;
        renderer_ = nullptr;
        selectedWidgetId_.clear();
        hierarchyScroll_ = 0;
        status_ = "NO SCREEN OPEN";
    }

    void RenegadeScreenEditorWorkspace::SelectWidget(
        const bridge::StableId& widgetId)
    {
        if (!session_ || !session_->FindWidget(widgetId)) return;
        selectedWidgetId_ = widgetId;
        RefreshInspector();
        SetStatus("ELEMENT SELECTED // INSPECTOR READY");
    }

    void RenegadeScreenEditorWorkspace::OnDocumentChanged(
        std::function<void()> callback)
    {
        documentChanged_ = std::move(callback);
    }

    void RenegadeScreenEditorWorkspace::OnReturnRequested(
        std::function<void()> callback)
    {
        returnRequested_ = std::move(callback);
    }

    bridge::ScreenRect RenegadeScreenEditorWorkspace::PreviewBounds() const noexcept
    {
        const float left = HierarchyWidth + PanelGap;
        const float top = HeaderHeight + PanelGap;
        return {
            left,
            top,
            std::max(1.0f, width_ - left - InspectorWidth - PanelGap),
            std::max(1.0f, height_ - top - FooterHeight - PanelGap),
        };
    }

    void RenegadeScreenEditorWorkspace::CreateControls()
    {
        returnButton_.Create("Screen Editor Return");
        returnButton_.SetText("< STORY FLOW");
        returnButton_.SetTooltip("Return to Story Flow after saving the Screen.");
        returnButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ReturnToStoryFlow();
        });

        saveButton_.Create("Screen Editor Save");
        saveButton_.SetText("SAVE");
        saveButton_.OnClick([this](const wi::gui::EventArgs&) { SaveScreen(); });

        undoButton_.Create("Screen Editor Undo");
        undoButton_.SetText("UNDO");
        undoButton_.OnClick([this](const wi::gui::EventArgs&) { UndoScreen(); });

        redoButton_.Create("Screen Editor Redo");
        redoButton_.SetText("REDO");
        redoButton_.OnClick([this](const wi::gui::EventArgs&) { RedoScreen(); });

        const auto configureInput = [](RenegadeTextInputField& input,
                                       const char* name,
                                       const char* placeholder)
        {
            input.Create(name);
            input.SetPlaceholder(placeholder);
            input.SetCancelInputEnabled(false);
            input.SetShadowRadius(0.0f);
        };
        configureInput(nameInput_, "Screen Element Name", "Element name");
        configureInput(textInput_, "Screen Element Text", "Displayed text");
        configureInput(xInput_, "Screen Element X", "X");
        configureInput(yInput_, "Screen Element Y", "Y");
        configureInput(widthInput_, "Screen Element Width", "Width");
        configureInput(heightInput_, "Screen Element Height", "Height");

        applyButton_.Create("Screen Editor Apply Element");
        applyButton_.SetText("APPLY ELEMENT");
        applyButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ApplySelectedWidget();
        });
        textInput_.OnInputAccepted([this](const wi::gui::EventArgs&)
        {
            ApplySelectedWidget();
        });
        heightInput_.OnInputAccepted([this](const wi::gui::EventArgs&)
        {
            ApplySelectedWidget();
        });

        visibleButton_.Create("Screen Editor Visible Toggle");
        visibleButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ToggleVisible();
        });
        enabledButton_.Create("Screen Editor Enabled Toggle");
        enabledButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ToggleEnabled();
        });

        for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&returnButton_),
                static_cast<wi::gui::Widget*>(&saveButton_),
                static_cast<wi::gui::Widget*>(&undoButton_),
                static_cast<wi::gui::Widget*>(&redoButton_),
                static_cast<wi::gui::Widget*>(&nameInput_),
                static_cast<wi::gui::Widget*>(&textInput_),
                static_cast<wi::gui::Widget*>(&xInput_),
                static_cast<wi::gui::Widget*>(&yInput_),
                static_cast<wi::gui::Widget*>(&widthInput_),
                static_cast<wi::gui::Widget*>(&heightInput_),
                static_cast<wi::gui::Widget*>(&applyButton_),
                static_cast<wi::gui::Widget*>(&visibleButton_),
                static_cast<wi::gui::Widget*>(&enabledButton_)})
        {
            widget->SetShadowRadius(0.0f);
            widget->SetVisible(false);
        }
    }

    void RenegadeScreenEditorWorkspace::LayoutControls()
    {
        float x = 14.0f;
        const auto header = [&x](wi::gui::Widget& widget, const float width)
        {
            widget.SetPos(XMFLOAT2(x, 14.0f));
            widget.SetSize(XMFLOAT2(width, 32.0f));
            x += width + 7.0f;
        };
        header(returnButton_, 126.0f);
        header(undoButton_, 64.0f);
        header(redoButton_, 64.0f);
        header(saveButton_, 64.0f);

        const float inspectorX = width_ - InspectorWidth + 14.0f;
        const float fieldWidth = InspectorWidth - 28.0f;
        float y = HeaderHeight + 72.0f;
        const auto field = [&](wi::gui::Widget& widget, const float height = 30.0f)
        {
            widget.SetPos(XMFLOAT2(inspectorX, y));
            widget.SetSize(XMFLOAT2(fieldWidth, height));
            y += height + 34.0f;
        };
        field(nameInput_);
        field(textInput_);

        xInput_.SetPos(XMFLOAT2(inspectorX, y));
        xInput_.SetSize(XMFLOAT2(fieldWidth * 0.5f - 4.0f, 30.0f));
        yInput_.SetPos(XMFLOAT2(
            inspectorX + fieldWidth * 0.5f + 4.0f, y));
        yInput_.SetSize(XMFLOAT2(fieldWidth * 0.5f - 4.0f, 30.0f));
        y += 64.0f;
        widthInput_.SetPos(XMFLOAT2(inspectorX, y));
        widthInput_.SetSize(XMFLOAT2(fieldWidth * 0.5f - 4.0f, 30.0f));
        heightInput_.SetPos(XMFLOAT2(
            inspectorX + fieldWidth * 0.5f + 4.0f, y));
        heightInput_.SetSize(XMFLOAT2(fieldWidth * 0.5f - 4.0f, 30.0f));
        y += 52.0f;

        visibleButton_.SetPos(XMFLOAT2(inspectorX, y));
        visibleButton_.SetSize(XMFLOAT2(fieldWidth * 0.5f - 4.0f, 30.0f));
        enabledButton_.SetPos(XMFLOAT2(
            inspectorX + fieldWidth * 0.5f + 4.0f, y));
        enabledButton_.SetSize(XMFLOAT2(fieldWidth * 0.5f - 4.0f, 30.0f));
        y += 42.0f;
        applyButton_.SetPos(XMFLOAT2(inspectorX, y));
        applyButton_.SetSize(XMFLOAT2(fieldWidth, 34.0f));
    }

    void RenegadeScreenEditorWorkspace::UpdateControls(
        const wi::Canvas& canvas,
        const float dt)
    {
        LayoutControls();
        const bool loaded = session_ && session_->IsLoaded();
        const auto* selected = loaded
            ? session_->FindWidget(selectedWidgetId_) : nullptr;

        returnButton_.SetVisible(loaded);
        saveButton_.SetVisible(loaded);
        undoButton_.SetVisible(loaded);
        redoButton_.SetVisible(loaded);
        saveButton_.SetEnabled(loaded && session_->IsDirty());
        undoButton_.SetEnabled(loaded && session_->CanUndo());
        redoButton_.SetEnabled(loaded && session_->CanRedo());

        const bool selectedReady = selected != nullptr;
        for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&nameInput_),
                static_cast<wi::gui::Widget*>(&textInput_),
                static_cast<wi::gui::Widget*>(&xInput_),
                static_cast<wi::gui::Widget*>(&yInput_),
                static_cast<wi::gui::Widget*>(&widthInput_),
                static_cast<wi::gui::Widget*>(&heightInput_),
                static_cast<wi::gui::Widget*>(&applyButton_),
                static_cast<wi::gui::Widget*>(&visibleButton_),
                static_cast<wi::gui::Widget*>(&enabledButton_)})
        {
            widget->SetVisible(selectedReady);
        }
        textInput_.SetEnabled(selectedReady &&
            selected->kind != bridge::ScreenWidgetKind::Image);
        enabledButton_.SetEnabled(selectedReady &&
            selected->kind == bridge::ScreenWidgetKind::Button);

        for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&returnButton_),
                static_cast<wi::gui::Widget*>(&saveButton_),
                static_cast<wi::gui::Widget*>(&undoButton_),
                static_cast<wi::gui::Widget*>(&redoButton_),
                static_cast<wi::gui::Widget*>(&nameInput_),
                static_cast<wi::gui::Widget*>(&textInput_),
                static_cast<wi::gui::Widget*>(&xInput_),
                static_cast<wi::gui::Widget*>(&yInput_),
                static_cast<wi::gui::Widget*>(&widthInput_),
                static_cast<wi::gui::Widget*>(&heightInput_),
                static_cast<wi::gui::Widget*>(&applyButton_),
                static_cast<wi::gui::Widget*>(&visibleButton_),
                static_cast<wi::gui::Widget*>(&enabledButton_)})
        {
            if (widget->IsVisible()) widget->Update(canvas, dt);
        }
    }

    void RenegadeScreenEditorWorkspace::RenderControls(
        const wi::Canvas& canvas,
        const wi::graphics::CommandList cmd) const
    {
        for (const wi::gui::Widget* widget : {
                static_cast<const wi::gui::Widget*>(&returnButton_),
                static_cast<const wi::gui::Widget*>(&saveButton_),
                static_cast<const wi::gui::Widget*>(&undoButton_),
                static_cast<const wi::gui::Widget*>(&redoButton_),
                static_cast<const wi::gui::Widget*>(&nameInput_),
                static_cast<const wi::gui::Widget*>(&textInput_),
                static_cast<const wi::gui::Widget*>(&xInput_),
                static_cast<const wi::gui::Widget*>(&yInput_),
                static_cast<const wi::gui::Widget*>(&widthInput_),
                static_cast<const wi::gui::Widget*>(&heightInput_),
                static_cast<const wi::gui::Widget*>(&applyButton_),
                static_cast<const wi::gui::Widget*>(&visibleButton_),
                static_cast<const wi::gui::Widget*>(&enabledButton_)})
        {
            if (widget->IsVisible()) widget->Render(canvas, cmd);
        }
    }

    void RenegadeScreenEditorWorkspace::EnsureSelection()
    {
        if (!session_ || !session_->IsLoaded())
        {
            selectedWidgetId_.clear();
            return;
        }
        if (session_->FindWidget(selectedWidgetId_)) return;
        selectedWidgetId_ = session_->Document().widgets.empty()
            ? bridge::StableId{} : session_->Document().widgets.front().id;
    }

    void RenegadeScreenEditorWorkspace::RefreshInspector()
    {
        EnsureSelection();
        const auto* widget = session_
            ? session_->FindWidget(selectedWidgetId_) : nullptr;
        if (!widget) return;
        bridge::ScreenRect rect;
        std::string error;
        if (!bridge::ResolveScreenWidgetRect(
                session_->Document(), widget->id, rect, error))
        {
            SetStatus("LAYOUT ERROR // " + error);
            return;
        }
        nameInput_.SetValue(widget->name);
        textInput_.SetValue(widget->text);
        xInput_.SetValue(Number(rect.x));
        yInput_.SetValue(Number(rect.y));
        widthInput_.SetValue(Number(rect.width));
        heightInput_.SetValue(Number(rect.height));
        visibleButton_.SetText(widget->visible ? "VISIBLE: YES" : "VISIBLE: NO");
        enabledButton_.SetText(widget->enabled ? "ENABLED: YES" : "ENABLED: NO");
    }

    void RenegadeScreenEditorWorkspace::ApplySelectedWidget()
    {
        if (!session_) return;
        const auto* widget = session_->FindWidget(selectedWidgetId_);
        if (!widget) return;

        bridge::ScreenWidgetAuthoringEdit edit;
        edit.name = Trim(nameInput_.GetValue());
        edit.text = widget->kind == bridge::ScreenWidgetKind::Image
            ? widget->text : textInput_.GetValue();
        edit.visible = widget->visible;
        edit.enabled = widget->enabled;
        if (!ParseNumber(xInput_.GetValue(), edit.resolvedRect.x) ||
            !ParseNumber(yInput_.GetValue(), edit.resolvedRect.y) ||
            !ParseNumber(widthInput_.GetValue(), edit.resolvedRect.width) ||
            !ParseNumber(heightInput_.GetValue(), edit.resolvedRect.height))
        {
            SetStatus("EDIT REJECTED // POSITION AND SIZE MUST BE NUMBERS");
            RefreshInspector();
            return;
        }

        std::string error;
        if (!session_->UpdateWidget(selectedWidgetId_, std::move(edit), error))
        {
            SetStatus("EDIT REJECTED // " + error);
            RefreshInspector();
            return;
        }
        RefreshAfterMutation("ELEMENT UPDATED // UNSAVED SCREEN CHANGE");
    }

    void RenegadeScreenEditorWorkspace::SaveScreen()
    {
        if (!session_) return;
        std::string error;
        if (!session_->Save(error))
        {
            SetStatus("SAVE FAILED // " + error);
            return;
        }
        SetStatus("SCREEN SAVED // TRANSACTION COMMITTED");
    }

    void RenegadeScreenEditorWorkspace::UndoScreen()
    {
        if (!session_) return;
        std::string error;
        if (!session_->Undo(error))
        {
            SetStatus("UNDO REFUSED // " + error);
            return;
        }
        RefreshAfterMutation("SCREEN UNDO // VALID STATE RESTORED");
    }

    void RenegadeScreenEditorWorkspace::RedoScreen()
    {
        if (!session_) return;
        std::string error;
        if (!session_->Redo(error))
        {
            SetStatus("REDO REFUSED // " + error);
            return;
        }
        RefreshAfterMutation("SCREEN REDO // VALID STATE RESTORED");
    }

    void RenegadeScreenEditorWorkspace::ToggleVisible()
    {
        const auto* widget = session_
            ? session_->FindWidget(selectedWidgetId_) : nullptr;
        if (!widget) return;
        bridge::ScreenRect rect;
        std::string error;
        if (!bridge::ResolveScreenWidgetRect(
                session_->Document(), widget->id, rect, error)) return;
        bridge::ScreenWidgetAuthoringEdit edit{
            widget->name, widget->text, rect, !widget->visible, widget->enabled};
        if (!session_->UpdateWidget(widget->id, std::move(edit), error))
        {
            SetStatus("VISIBILITY REJECTED // " + error);
            return;
        }
        RefreshAfterMutation("VISIBILITY UPDATED // UNSAVED SCREEN CHANGE");
    }

    void RenegadeScreenEditorWorkspace::ToggleEnabled()
    {
        const auto* widget = session_
            ? session_->FindWidget(selectedWidgetId_) : nullptr;
        if (!widget || widget->kind != bridge::ScreenWidgetKind::Button) return;
        bridge::ScreenRect rect;
        std::string error;
        if (!bridge::ResolveScreenWidgetRect(
                session_->Document(), widget->id, rect, error)) return;
        bridge::ScreenWidgetAuthoringEdit edit{
            widget->name, widget->text, rect, widget->visible, !widget->enabled};
        if (!session_->UpdateWidget(widget->id, std::move(edit), error))
        {
            SetStatus("ENABLED STATE REJECTED // " + error);
            return;
        }
        RefreshAfterMutation("ENABLED STATE UPDATED // UNSAVED SCREEN CHANGE");
    }

    void RenegadeScreenEditorWorkspace::ReturnToStoryFlow()
    {
        if (session_ && session_->IsDirty())
        {
            SetStatus("RETURN BLOCKED // SAVE OR UNDO SCREEN CHANGES FIRST");
            return;
        }
        if (returnRequested_) returnRequested_();
    }

    void RenegadeScreenEditorWorkspace::RefreshAfterMutation(
        std::string status)
    {
        EnsureSelection();
        RefreshInspector();
        if (documentChanged_) documentChanged_();
        SetStatus(std::move(status));
    }

    void RenegadeScreenEditorWorkspace::SetStatus(std::string status)
    {
        status_ = std::move(status);
    }

    void RenegadeScreenEditorWorkspace::Update(
        const wi::Canvas& canvas,
        const float dt)
    {
        Widget::Update(canvas, dt);
        UpdateControls(canvas, dt);
        if (!IsVisible() || !IsEnabled() || !session_ ||
            !session_->IsLoaded()) return;

        const XMFLOAT4 pointer = wi::input::GetPointer();
        const float rowTop = HeaderHeight + 50.0f;
        const std::size_t visibleRows = static_cast<std::size_t>(std::max(
            1.0f, std::floor((height_ - rowTop - FooterHeight) /
                HierarchyRowHeight)));
        const auto& widgets = session_->Document().widgets;

        if (pointer.x < HierarchyWidth && pointer.y >= HeaderHeight &&
            pointer.y < height_ - FooterHeight)
        {
            if (pointer.z != 0.0f && widgets.size() > visibleRows)
            {
                if (pointer.z < 0.0f)
                    hierarchyScroll_ = std::min(
                        hierarchyScroll_ + 1, widgets.size() - visibleRows);
                else if (hierarchyScroll_ > 0)
                    --hierarchyScroll_;
            }
            if (wi::input::Press(wi::input::MOUSE_BUTTON_LEFT) &&
                pointer.y >= rowTop)
            {
                const std::size_t row = static_cast<std::size_t>(
                    (pointer.y - rowTop) / HierarchyRowHeight);
                const std::size_t index = hierarchyScroll_ + row;
                if (row < visibleRows && index < widgets.size())
                    SelectWidget(widgets[index].id);
            }
            return;
        }

        const auto preview = PreviewBounds();
        if (!Contains(preview, pointer) ||
            !wi::input::Press(wi::input::MOUSE_BUTTON_LEFT) || !renderer_)
            return;

        for (auto iterator = widgets.rbegin(); iterator != widgets.rend(); ++iterator)
        {
            const auto* bounds = renderer_->LogicalRect(iterator->id);
            if (bounds && Contains(*bounds, pointer))
            {
                SelectWidget(iterator->id);
                return;
            }
        }
    }

    void RenegadeScreenEditorWorkspace::Render(
        const wi::Canvas& canvas,
        const wi::graphics::CommandList cmd) const
    {
        if (!IsVisible()) return;

        const auto preview = PreviewBounds();
        Rect(0.0f, 0.0f, width_, HeaderHeight, Surface, cmd);
        Rect(0.0f, HeaderHeight, HierarchyWidth, height_ - HeaderHeight,
            Surface, cmd);
        Rect(width_ - InspectorWidth, HeaderHeight, InspectorWidth,
            height_ - HeaderHeight, Surface, cmd);
        Rect(0.0f, height_ - FooterHeight, width_, FooterHeight,
            Background, cmd);
        Outline(preview.x - 1.0f, preview.y - 1.0f,
            preview.width + 2.0f, preview.height + 2.0f, Border, 1.0f, cmd);

        Label("SCREEN EDITOR", 360.0f, 18.0f, 18, Text, cmd);
        Label("LIVE RUNTIME PREVIEW", preview.x, HeaderHeight - 20.0f,
            11, Muted, cmd);
        Label("HIERARCHY", 16.0f, HeaderHeight + 16.0f, 13, Text, cmd);
        Label("INSPECTOR", width_ - InspectorWidth + 14.0f,
            HeaderHeight + 16.0f, 13, Text, cmd);

        if (session_ && session_->IsLoaded())
        {
            const auto& widgets = session_->Document().widgets;
            const float rowTop = HeaderHeight + 50.0f;
            const std::size_t visibleRows = static_cast<std::size_t>(std::max(
                1.0f, std::floor((height_ - rowTop - FooterHeight) /
                    HierarchyRowHeight)));
            for (std::size_t row = 0; row < visibleRows; ++row)
            {
                const std::size_t index = hierarchyScroll_ + row;
                if (index >= widgets.size()) break;
                const auto& widget = widgets[index];
                const float y = rowTop + static_cast<float>(row) *
                    HierarchyRowHeight;
                if (widget.id == selectedWidgetId_)
                    Rect(8.0f, y, HierarchyWidth - 16.0f,
                        HierarchyRowHeight - 3.0f, Raised, cmd);
                Label(std::to_string(index + 1), 16.0f, y + 7.0f,
                    10, Muted, cmd);
                Label(Shorten(widget.name, 24), 44.0f, y + 5.0f,
                    12, widget.id == selectedWidgetId_ ? Text : Muted, cmd);
                Label(bridge::ScreenWidgetKindName(widget.kind),
                    178.0f, y + 7.0f, 9, Muted, cmd);
            }

            if (const auto* widget = session_->FindWidget(selectedWidgetId_))
            {
                const float inspectorX = width_ - InspectorWidth + 14.0f;
                Label(bridge::ScreenWidgetKindName(widget->kind), inspectorX,
                    HeaderHeight + 42.0f, 10, Orange, cmd);
                Label("NAME", inspectorX, HeaderHeight + 58.0f, 9, Muted, cmd);
                Label("TEXT", inspectorX, HeaderHeight + 122.0f, 9, Muted, cmd);
                Label("POSITION  X / Y", inspectorX,
                    HeaderHeight + 186.0f, 9, Muted, cmd);
                Label("SIZE  WIDTH / HEIGHT", inspectorX,
                    HeaderHeight + 250.0f, 9, Muted, cmd);
                Label(widget->layoutMode == bridge::ScreenLayoutMode::Anchored
                        ? "LAYOUT // ANCHORED (OFFSETS PRESERVED)"
                        : "LAYOUT // ABSOLUTE",
                    inspectorX, HeaderHeight + 410.0f, 9, Muted, cmd);
            }
        }

        if (renderer_ && !selectedWidgetId_.empty())
        {
            if (const auto* selected = renderer_->LogicalRect(selectedWidgetId_))
            {
                Outline(selected->x - 2.0f, selected->y - 2.0f,
                    selected->width + 4.0f, selected->height + 4.0f,
                    Cyan, 2.0f, cmd);
            }
        }

        Label(status_, 14.0f, height_ - FooterHeight + 14.0f,
            11, session_ && session_->IsDirty() ? Orange : Muted, cmd);
        RenderControls(canvas, cmd);
    }
}
