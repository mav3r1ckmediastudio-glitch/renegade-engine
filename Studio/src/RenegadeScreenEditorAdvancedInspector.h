#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <WickedEngine.h>

#include "RenegadeScreenChoicePicker.h"
#include "RenegadeScreenColorPicker.h"
#include "RenegadeScreenEditorWorkspace.h"
#include "RenegadeStudioChrome.h"
#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/bridge/ScreenCreatorResourceService.h"

namespace renegade::studio
{
    // Gate 8D's creator Inspector. It overlays the accepted 8C basic Inspector
    // only while expanded. All persistent changes still flow through
    // ScreenAuthoringSession and the shared Runtime renderer remains preview
    // authority.
    class RenegadeScreenEditorAdvancedInspector final : public wi::gui::Widget
    {
    public:
        void Create()
        {
            SetName("Renegade Screen Advanced Inspector");
            SetShadowRadius(0.0f);

            toggleButton_.Create("Screen Advanced Inspector Toggle");
            toggleButton_.SetText("ADVANCED");
            toggleButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                expanded_ = !expanded_;
                toggleButton_.SetText(expanded_ ? "< BASIC" : "ADVANCED");
                choicePicker_.Close();
                colorPicker_.Close();
                RefreshFromSelection(true);
            });

            stylePageButton_.Create("Screen Inspector Style Page");
            stylePageButton_.SetText("STYLE");
            stylePageButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                page_ = Page::Style;
                choicePicker_.Close();
                colorPicker_.Close();
                RefreshFromSelection(true);
            });
            textPageButton_.Create("Screen Inspector Text Page");
            textPageButton_.SetText("TEXT / FONT");
            textPageButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                page_ = Page::Text;
                choicePicker_.Close();
                colorPicker_.Close();
                RefreshFromSelection(true);
            });
            bindingPageButton_.Create("Screen Inspector Binding Page");
            bindingPageButton_.SetText("BIND / LAYOUT");
            bindingPageButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                page_ = Page::Binding;
                choicePicker_.Close();
                colorPicker_.Close();
                RefreshFromSelection(true);
            });

            stateButton_.Create("Screen Visual State");
            stateButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                state_ = static_cast<VisualState>(
                    (static_cast<int>(state_) + 1) % 5);
                RefreshFromSelection(true);
            });
            stateImageButton_.Create("Screen State Image Resource");
            stateImageButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                OpenStateImagePicker();
            });

            ConfigureInput(backgroundInput_, "Screen State Background", "BACKGROUND RGBA");
            ConfigureInput(foregroundInput_, "Screen State Foreground", "FOREGROUND RGBA");
            ConfigureInput(tintInput_, "Screen State Tint", "IMAGE TINT RGBA");
            ConfigureInput(borderColorInput_, "Screen Border Color", "BORDER RGBA");
            ConfigureInput(borderWidthInput_, "Screen Border Width", "BORDER WIDTH");
            ConfigureInput(cornerRadiusInput_, "Screen Corner Radius", "CORNER RADIUS");
            ConfigureInput(opacityInput_, "Screen Opacity", "OPACITY 0..1");
            ConfigureColorButton(backgroundColorButton_, "Screen Background Color Picker",
                [this]() { OpenStateColor(ColorTarget::Background); });
            ConfigureColorButton(foregroundColorButton_, "Screen Foreground Color Picker",
                [this]() { OpenStateColor(ColorTarget::Foreground); });
            ConfigureColorButton(tintColorButton_, "Screen Image Tint Color Picker",
                [this]() { OpenStateColor(ColorTarget::Tint); });
            ConfigureColorButton(borderColorButton_, "Screen Border Color Picker",
                [this]() { OpenStateColor(ColorTarget::Border); });
            applyStyleButton_.Create("Apply Screen Style");
            applyStyleButton_.SetText("APPLY STYLE STATE");
            applyStyleButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                ApplyStyle();
            });

            fontButton_.Create("Screen Font Resource");
            fontButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                OpenFontPicker();
            });
            ConfigureInput(fontSizeInput_, "Screen Font Size", "FONT SIZE");
            ConfigureInput(characterSpacingInput_, "Screen Character Spacing", "CHAR SPACING");
            ConfigureInput(lineSpacingInput_, "Screen Line Spacing", "LINE SPACING");
            ConfigureInput(softnessInput_, "Screen Font Softness", "SOFTNESS 0..1");
            ConfigureInput(boldenInput_, "Screen Font Bolden", "BOLDEN 0..1");
            ConfigureInput(shadowColorInput_, "Screen Shadow Color", "SHADOW RGBA");
            ConfigureInput(shadowXInput_, "Screen Shadow X", "SHADOW X");
            ConfigureInput(shadowYInput_, "Screen Shadow Y", "SHADOW Y");
            ConfigureInput(shadowSoftnessInput_, "Screen Shadow Softness", "SHADOW SOFTNESS");
            ConfigureInput(shadowBoldenInput_, "Screen Shadow Bolden", "SHADOW BOLDEN");
            ConfigureColorButton(shadowColorButton_, "Screen Shadow Color Picker",
                [this]() { OpenStateColor(ColorTarget::Shadow); });

            horizontalButton_.Create("Screen Horizontal Alignment");
            horizontalButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                const auto* widget = Selected();
                if (!widget) return;
                EnsureDraftStyle(*widget);
                auto& value = draftStyle_.text.horizontalAlignment;
                value = value == bridge::ScreenHorizontalAlignment::Left
                    ? bridge::ScreenHorizontalAlignment::Center
                    : value == bridge::ScreenHorizontalAlignment::Center
                        ? bridge::ScreenHorizontalAlignment::Right
                        : bridge::ScreenHorizontalAlignment::Left;
                horizontalButton_.SetText(HorizontalText(value));
            });
            verticalButton_.Create("Screen Vertical Alignment");
            verticalButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                const auto* widget = Selected();
                if (!widget) return;
                EnsureDraftStyle(*widget);
                auto& value = draftStyle_.text.verticalAlignment;
                value = value == bridge::ScreenVerticalAlignment::Top
                    ? bridge::ScreenVerticalAlignment::Center
                    : value == bridge::ScreenVerticalAlignment::Center
                        ? bridge::ScreenVerticalAlignment::Bottom
                        : bridge::ScreenVerticalAlignment::Top;
                verticalButton_.SetText(VerticalText(value));
            });
            wrapButton_.Create("Screen Text Wrap");
            wrapButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                const auto* widget = Selected();
                if (!widget) return;
                EnsureDraftStyle(*widget);
                draftStyle_.text.wrap = !draftStyle_.text.wrap;
                wrapButton_.SetText(draftStyle_.text.wrap ? "WRAP: YES" : "WRAP: NO");
            });
            applyTextButton_.Create("Apply Screen Typography");
            applyTextButton_.SetText("APPLY TYPOGRAPHY");
            applyTextButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                ApplyText();
            });

            primaryResourceButton_.Create("Screen Primary Image Resource");
            primaryResourceButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                OpenPrimaryImagePicker();
            });
            actionButton_.Create("Screen Action Binding");
            actionButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                OpenActionPicker();
            });
            ConfigureInput(actionIdInput_, "Screen Action ID", "ACTION ID");
            addActionButton_.Create("Add Screen Action");
            addActionButton_.SetText("+ ACTION");
            addActionButton_.OnClick([this](const wi::gui::EventArgs&) { AddAction(); });
            renameActionButton_.Create("Rename Screen Action");
            renameActionButton_.SetText("RENAME ACTION");
            renameActionButton_.OnClick([this](const wi::gui::EventArgs&) { RenameAction(); });
            deleteActionButton_.Create("Delete Screen Action");
            deleteActionButton_.SetText("DELETE ACTION");
            deleteActionButton_.OnClick([this](const wi::gui::EventArgs&) { DeleteAction(); });
            focusEarlierButton_.Create("Screen Focus Earlier");
            focusEarlierButton_.SetText("FOCUS UP");
            focusEarlierButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                MoveFocus(true);
            });
            focusLaterButton_.Create("Screen Focus Later");
            focusLaterButton_.SetText("FOCUS DOWN");
            focusLaterButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                MoveFocus(false);
            });

            parentButton_.Create("Screen Parent Binding");
            parentButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                OpenParentPicker();
            });
            layoutButton_.Create("Screen Layout Mode");
            layoutButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                draftLayout_ = draftLayout_ == bridge::ScreenLayoutMode::Absolute
                    ? bridge::ScreenLayoutMode::Anchored
                    : bridge::ScreenLayoutMode::Absolute;
                layoutButton_.SetText(
                    draftLayout_ == bridge::ScreenLayoutMode::Absolute
                        ? "LAYOUT: ABSOLUTE" : "LAYOUT: ANCHORED");
            });
            ConfigureInput(anchorMinXInput_, "Screen Anchor Min X", "ANCHOR MIN X");
            ConfigureInput(anchorMinYInput_, "Screen Anchor Min Y", "ANCHOR MIN Y");
            ConfigureInput(anchorMaxXInput_, "Screen Anchor Max X", "ANCHOR MAX X");
            ConfigureInput(anchorMaxYInput_, "Screen Anchor Max Y", "ANCHOR MAX Y");
            applyBindingButton_.Create("Apply Screen Binding Layout");
            applyBindingButton_.SetText("APPLY BIND / LAYOUT");
            applyBindingButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                ApplyBinding();
            });

            panelButton_.Create("Create Screen Panel Preset");
            panelButton_.SetText("+ PANEL");
            panelButton_.OnClick([this](const wi::gui::EventArgs&) { CreatePanel(); });
            headingButton_.Create("Create Screen Heading Preset");
            headingButton_.SetText("+ HEADING");
            headingButton_.OnClick([this](const wi::gui::EventArgs&) { CreateHeading(); });
            duplicateComponentButton_.Create("Duplicate Screen Component");
            duplicateComponentButton_.SetText("DUP COMPONENT");
            duplicateComponentButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                DuplicateComponent();
            });

            for (auto* widget : AllControls())
            {
                widget->SetShadowRadius(0.0f);
                widget->SetVisible(false);
            }
            created_ = true;
        }

        void Bind(
            bridge::ScreenAuthoringSession* session,
            RenegadeScreenEditorWorkspace* workspace,
            std::string projectRoot,
            std::function<void()> documentChanged)
        {
            session_ = session;
            workspace_ = workspace;
            projectRoot_ = std::move(projectRoot);
            documentChanged_ = std::move(documentChanged);
            selectedId_.clear();
            status_ = "ADVANCED INSPECTOR READY";
            RefreshResources();
            RefreshFromSelection(true);
        }

        void Clear() noexcept
        {
            choicePicker_.Close();
            colorPicker_.Close();
            session_ = nullptr;
            workspace_ = nullptr;
            projectRoot_.clear();
            documentChanged_ = {};
            selectedId_.clear();
            imageResources_.clear();
            fontResources_.clear();
            expanded_ = false;
            draftStyleValid_ = false;
            HideAll();
        }

        void SetLayout(const float width, const float height)
        {
            width_ = std::max(1.0f, width);
            height_ = std::max(1.0f, height);
            panelWidth_ = std::min(360.0f, width_);
            panelX_ = width_ - panelWidth_;
            panelY_ = 64.0f;
            panelHeight_ = std::max(1.0f, height_ - 108.0f);
            SetPos(XMFLOAT2(panelX_, panelY_));
            SetSize(XMFLOAT2(panelWidth_, panelHeight_));
            choicePicker_.SetLayout(
                panelX_ + 8.0f, panelY_ + 74.0f,
                std::max(220.0f, panelWidth_ - 16.0f),
                std::min(430.0f, std::max(180.0f, panelHeight_ - 92.0f)));
            colorPicker_.SetLayout(
                panelX_ + 12.0f, panelY_ + 82.0f,
                std::max(280.0f, panelWidth_ - 24.0f),
                std::min(340.0f, std::max(280.0f, panelHeight_ - 110.0f)));
            LayoutControls();
        }

        void Update(const wi::Canvas& canvas, const float dt) override
        {
            Widget::Update(canvas, dt);
            if (!created_ || !IsVisible() || !session_ || !session_->IsLoaded())
            {
                HideAll();
                choicePicker_.Close();
                colorPicker_.Close();
                return;
            }

            const bridge::StableId selection = workspace_
                ? workspace_->SelectedWidgetId() : bridge::StableId{};
            if (selection != selectedId_)
            {
                selectedId_ = selection;
                choicePicker_.Close();
                colorPicker_.Close();
                RefreshFromSelection(true);
            }

            LayoutControls();
            SyncVisibility();
            if (choicePicker_.IsOpen())
            {
                choicePicker_.Update();
                return;
            }
            if (colorPicker_.IsOpen())
            {
                colorPicker_.Update();
                return;
            }
            for (auto* widget : AllControls())
            {
                if (widget->IsVisible()) widget->Update(canvas, dt);
            }
        }

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            if (!created_ || !IsVisible() || !session_ || !session_->IsLoaded())
                return;

            if (expanded_)
            {
                DrawRect(panelX_, panelY_, panelWidth_, panelHeight_,
                    wi::Color(7, 11, 15, 255), cmd);
                DrawBorder(panelX_, panelY_, panelWidth_, panelHeight_,
                    wi::Color(39, 183, 222, 255), cmd);
                DrawLabel("ADVANCED SCREEN INSPECTOR",
                    panelX_ + 14.0f, panelY_ + 12.0f, 10,
                    wi::Color(244, 244, 244, 255), cmd);
                DrawLabel(status_, panelX_ + 14.0f,
                    panelY_ + panelHeight_ - 22.0f, 7,
                    wi::Color(135, 151, 159, 255), cmd);
            }
            for (const auto* widget : AllControlsConst())
            {
                if (widget->IsVisible()) widget->Render(canvas, cmd);
            }
            choicePicker_.Render(cmd);
            colorPicker_.Render(cmd);
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeScreenEditorAdvancedInspector";
        }

    private:
        enum class Page { Style, Text, Binding };
        enum class VisualState { Normal, Hover, Pressed, Focused, Disabled };
        enum class ColorTarget { Background, Foreground, Tint, Border, Shadow };

        static void ConfigureInput(
            RenegadeTextInputField& input,
            const char* name,
            const char* description)
        {
            input.Create(name);
            input.SetDescription((std::string(description) + "  ").c_str());
            input.SetCancelInputEnabled(false);
            input.SetShadowRadius(0.0f);
        }

        void ConfigureColorButton(
            RenegadeButton& button,
            const char* name,
            std::function<void()> callback)
        {
            button.Create(name);
            button.SetText("PICK");
            button.OnClick([callback = std::move(callback)](const wi::gui::EventArgs&)
            {
                if (callback) callback();
            });
        }

        static std::string Trim(std::string value)
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

        static std::string Number(const float value)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(3) << value;
            return stream.str();
        }

        static bool ParseNumber(const std::string& value, float& result)
        {
            try
            {
                std::size_t used = 0;
                const std::string trimmed = Trim(value);
                result = std::stof(trimmed, &used);
                return used == trimmed.size() && std::isfinite(result);
            }
            catch (...) { return false; }
        }

        static std::string ColorText(const bridge::ScreenColor& color)
        {
            std::ostringstream stream;
            stream << std::hex << std::uppercase << std::setfill('0')
                << std::setw(2) << static_cast<int>(color.red)
                << std::setw(2) << static_cast<int>(color.green)
                << std::setw(2) << static_cast<int>(color.blue)
                << std::setw(2) << static_cast<int>(color.alpha);
            return stream.str();
        }

        static bool ParseColor(std::string text, bridge::ScreenColor& color)
        {
            text = Trim(std::move(text));
            if (!text.empty() && text.front() == '#') text.erase(text.begin());
            if (text.size() != 8) return false;
            if (!std::all_of(text.begin(), text.end(), [](const unsigned char c)
                { return std::isxdigit(c) != 0; })) return false;
            try
            {
                const auto value = static_cast<std::uint32_t>(
                    std::stoul(text, nullptr, 16));
                color.red = static_cast<std::uint8_t>((value >> 24) & 0xFF);
                color.green = static_cast<std::uint8_t>((value >> 16) & 0xFF);
                color.blue = static_cast<std::uint8_t>((value >> 8) & 0xFF);
                color.alpha = static_cast<std::uint8_t>(value & 0xFF);
                return true;
            }
            catch (...) { return false; }
        }

        static const char* StateText(const VisualState state) noexcept
        {
            switch (state)
            {
            case VisualState::Normal: return "STATE: NORMAL";
            case VisualState::Hover: return "STATE: HOVER";
            case VisualState::Pressed: return "STATE: PRESSED";
            case VisualState::Focused: return "STATE: FOCUSED";
            case VisualState::Disabled: return "STATE: DISABLED";
            }
            return "STATE";
        }

        static const char* HorizontalText(
            const bridge::ScreenHorizontalAlignment value) noexcept
        {
            switch (value)
            {
            case bridge::ScreenHorizontalAlignment::Left: return "H: LEFT";
            case bridge::ScreenHorizontalAlignment::Center: return "H: CENTER";
            case bridge::ScreenHorizontalAlignment::Right: return "H: RIGHT";
            }
            return "H";
        }

        static const char* VerticalText(
            const bridge::ScreenVerticalAlignment value) noexcept
        {
            switch (value)
            {
            case bridge::ScreenVerticalAlignment::Top: return "V: TOP";
            case bridge::ScreenVerticalAlignment::Center: return "V: CENTER";
            case bridge::ScreenVerticalAlignment::Bottom: return "V: BOTTOM";
            }
            return "V";
        }

        static std::string Short(const std::string& value)
        {
            if (value.empty()) return "< NONE >";
            if (value.size() <= 30) return value;
            return "..." + value.substr(value.size() - 27);
        }

        static std::string ShortId(const bridge::StableId& value)
        {
            return value.size() <= 8 ? value : value.substr(0, 8);
        }

        const bridge::ScreenWidget* Selected() const noexcept
        {
            return session_ ? session_->FindWidget(selectedId_) : nullptr;
        }

        void EnsureDraftStyle(const bridge::ScreenWidget& widget)
        {
            if (!draftStyleValid_)
            {
                draftStyle_ = widget.style;
                draftStyleValid_ = true;
            }
        }

        bridge::ScreenVisualState* DraftVisual() noexcept
        {
            switch (state_)
            {
            case VisualState::Normal: return &draftStyle_.normal;
            case VisualState::Hover: return &draftStyle_.hover;
            case VisualState::Pressed: return &draftStyle_.pressed;
            case VisualState::Focused: return &draftStyle_.focused;
            case VisualState::Disabled: return &draftStyle_.disabled;
            }
            return &draftStyle_.normal;
        }

        const bridge::ScreenVisualState* Visual(
            const bridge::ScreenWidgetStyle& style) const noexcept
        {
            switch (state_)
            {
            case VisualState::Normal: return &style.normal;
            case VisualState::Hover: return &style.hover;
            case VisualState::Pressed: return &style.pressed;
            case VisualState::Focused: return &style.focused;
            case VisualState::Disabled: return &style.disabled;
            }
            return &style.normal;
        }

        void RefreshResources()
        {
            imageResources_.clear();
            fontResources_.clear();
            if (!session_ || !session_->IsLoaded()) return;
            const auto images = bridge::EnumerateScreenCreatorResources(
                projectRoot_, session_->ProjectId(),
                bridge::ScreenCreatorResourceKind::Image);
            if (images.succeeded)
                imageResources_ = images.choices;
            const auto fonts = bridge::EnumerateScreenCreatorResources(
                projectRoot_, session_->ProjectId(),
                bridge::ScreenCreatorResourceKind::Font);
            if (fonts.succeeded)
                fontResources_ = fonts.choices;
            if (!images.succeeded && !images.error.empty())
                status_ = "RESOURCE CATALOGUE // " + images.error;
            else if (!fonts.succeeded && !fonts.error.empty())
                status_ = "RESOURCE CATALOGUE // " + fonts.error;
        }

        void RefreshFromSelection(const bool overwriteDraft)
        {
            const auto* widget = Selected();
            if (!widget) return;
            if (overwriteDraft || !draftStyleValid_)
            {
                draftStyle_ = widget->style;
                draftStyleValid_ = true;
                draftPrimaryResource_ = widget->resourcePath;
                draftAction_ = widget->actionId;
                draftParent_ = widget->parentId;
                draftLayout_ = widget->layoutMode;
                draftAnchors_ = widget->anchors;
            }

            stateButton_.SetText(StateText(state_));
            const auto* state = Visual(draftStyle_);
            stateImageButton_.SetText("STATE IMAGE: " + Short(state->imageResourcePath));
            backgroundInput_.SetValue(ColorText(state->background));
            foregroundInput_.SetValue(ColorText(state->foreground));
            tintInput_.SetValue(ColorText(state->imageTint));
            borderColorInput_.SetValue(ColorText(draftStyle_.borderColor));
            borderWidthInput_.SetValue(Number(draftStyle_.borderWidth));
            cornerRadiusInput_.SetValue(Number(draftStyle_.cornerRadius));
            opacityInput_.SetValue(Number(draftStyle_.opacity));

            fontButton_.SetText("FONT: " + Short(draftStyle_.text.fontResource));
            fontSizeInput_.SetValue(Number(draftStyle_.text.fontSize));
            characterSpacingInput_.SetValue(Number(draftStyle_.text.characterSpacing));
            lineSpacingInput_.SetValue(Number(draftStyle_.text.lineSpacing));
            softnessInput_.SetValue(Number(draftStyle_.text.softness));
            boldenInput_.SetValue(Number(draftStyle_.text.bolden));
            shadowColorInput_.SetValue(ColorText(draftStyle_.text.shadowColor));
            shadowXInput_.SetValue(Number(draftStyle_.text.shadowOffsetX));
            shadowYInput_.SetValue(Number(draftStyle_.text.shadowOffsetY));
            shadowSoftnessInput_.SetValue(Number(draftStyle_.text.shadowSoftness));
            shadowBoldenInput_.SetValue(Number(draftStyle_.text.shadowBolden));
            horizontalButton_.SetText(HorizontalText(draftStyle_.text.horizontalAlignment));
            verticalButton_.SetText(VerticalText(draftStyle_.text.verticalAlignment));
            wrapButton_.SetText(draftStyle_.text.wrap ? "WRAP: YES" : "WRAP: NO");

            primaryResourceButton_.SetText("IMAGE: " + Short(draftPrimaryResource_));
            actionButton_.SetText("ACTION: " +
                (draftAction_.empty() ? std::string("< NONE >") : draftAction_));
            actionIdInput_.SetValue(draftAction_);
            parentButton_.SetText("PARENT: " + ParentLabel(draftParent_));
            layoutButton_.SetText(
                draftLayout_ == bridge::ScreenLayoutMode::Absolute
                    ? "LAYOUT: ABSOLUTE" : "LAYOUT: ANCHORED");
            anchorMinXInput_.SetValue(Number(draftAnchors_.minimumX));
            anchorMinYInput_.SetValue(Number(draftAnchors_.minimumY));
            anchorMaxXInput_.SetValue(Number(draftAnchors_.maximumX));
            anchorMaxYInput_.SetValue(Number(draftAnchors_.maximumY));
        }

        std::string ParentLabel(const bridge::StableId& parentId) const
        {
            if (parentId.empty()) return "CANVAS";
            const auto* parent = session_ ? session_->FindWidget(parentId) : nullptr;
            return parent ? Short(parent->name) : "< MISSING >";
        }

        bool IsDescendantOfSelection(const bridge::ScreenWidget& candidate) const
        {
            bridge::StableId parent = candidate.parentId;
            std::vector<bridge::StableId> visited;
            while (!parent.empty())
            {
                if (parent == selectedId_) return true;
                if (std::find(visited.begin(), visited.end(), parent) != visited.end())
                    return true;
                visited.push_back(parent);
                const auto* widget = session_->FindWidget(parent);
                if (!widget) break;
                parent = widget->parentId;
            }
            return false;
        }

        std::vector<RenegadeScreenChoicePicker::Choice> ImageChoices(
            const bool includeEmpty)
        {
            RefreshResources();
            std::vector<RenegadeScreenChoicePicker::Choice> choices;
            if (includeEmpty) choices.push_back({"< NONE >", ""});
            for (const auto& choice : imageResources_)
            {
                choices.push_back({
                    choice.projectRelativePath + "  [" + ShortId(choice.assetId) + "]",
                    choice.projectRelativePath});
            }
            return choices;
        }

        std::vector<RenegadeScreenChoicePicker::Choice> FontChoices()
        {
            RefreshResources();
            std::vector<RenegadeScreenChoicePicker::Choice> choices{
                {"BUILT-IN // Liberation Sans", bridge::BuiltinScreenFont}};
            for (const auto& choice : fontResources_)
            {
                choices.push_back({
                    choice.projectRelativePath + "  [" + ShortId(choice.assetId) + "]",
                    choice.projectRelativePath});
            }
            return choices;
        }

        void OpenStateImagePicker()
        {
            const auto* widget = Selected();
            if (!widget) return;
            EnsureDraftStyle(*widget);
            const std::string current = DraftVisual()->imageResourcePath;
            choicePicker_.Open("SELECT STATE IMAGE", ImageChoices(true), current,
                [this](const std::string& value)
                {
                    if (!Selected()) return;
                    DraftVisual()->imageResourcePath = value;
                    stateImageButton_.SetText("STATE IMAGE: " + Short(value));
                });
        }

        void OpenFontPicker()
        {
            const auto* widget = Selected();
            if (!widget || widget->kind == bridge::ScreenWidgetKind::Image) return;
            EnsureDraftStyle(*widget);
            choicePicker_.Open("SELECT FONT", FontChoices(),
                draftStyle_.text.fontResource,
                [this](const std::string& value)
                {
                    draftStyle_.text.fontResource = value;
                    fontButton_.SetText("FONT: " + Short(value));
                });
        }

        void OpenPrimaryImagePicker()
        {
            const auto* widget = Selected();
            if (!widget || widget->kind != bridge::ScreenWidgetKind::Image) return;
            choicePicker_.Open("SELECT IMAGE", ImageChoices(true), draftPrimaryResource_,
                [this](const std::string& value)
                {
                    draftPrimaryResource_ = value;
                    primaryResourceButton_.SetText("IMAGE: " + Short(value));
                });
        }

        void OpenActionPicker()
        {
            const auto* widget = Selected();
            if (!widget || widget->kind != bridge::ScreenWidgetKind::Button || !session_)
                return;
            std::vector<RenegadeScreenChoicePicker::Choice> choices;
            for (const auto& action : session_->Document().actions)
                choices.push_back({action.id, action.id});
            choicePicker_.Open("SELECT BUTTON ACTION", std::move(choices), draftAction_,
                [this](const std::string& value)
                {
                    draftAction_ = value;
                    actionIdInput_.SetValue(value);
                    actionButton_.SetText("ACTION: " + value);
                });
        }

        void OpenParentPicker()
        {
            const auto* widget = Selected();
            if (!widget || !session_) return;
            std::vector<RenegadeScreenChoicePicker::Choice> choices{
                {"CANVAS", ""}};
            for (const auto& candidate : session_->Document().widgets)
            {
                if (candidate.id == selectedId_ || IsDescendantOfSelection(candidate))
                    continue;
                choices.push_back({candidate.name + "  [" + ShortId(candidate.id) + "]",
                    candidate.id});
            }
            choicePicker_.Open("SELECT PARENT", std::move(choices), draftParent_,
                [this](const std::string& value)
                {
                    draftParent_ = value;
                    parentButton_.SetText("PARENT: " + ParentLabel(value));
                });
        }

        void OpenStateColor(const ColorTarget target)
        {
            const auto* widget = Selected();
            if (!widget) return;
            EnsureDraftStyle(*widget);
            bridge::ScreenColor value;
            std::string title;
            if (target == ColorTarget::Background)
            {
                value = DraftVisual()->background;
                title = "BACKGROUND RGBA";
            }
            else if (target == ColorTarget::Foreground)
            {
                value = DraftVisual()->foreground;
                title = "FOREGROUND RGBA";
            }
            else if (target == ColorTarget::Tint)
            {
                value = DraftVisual()->imageTint;
                title = "IMAGE TINT RGBA";
            }
            else if (target == ColorTarget::Border)
            {
                value = draftStyle_.borderColor;
                title = "BORDER RGBA";
            }
            else
            {
                value = draftStyle_.text.shadowColor;
                title = "SHADOW RGBA";
            }
            colorPicker_.Open(title, value,
                [this, target](const bridge::ScreenColor& color)
                {
                    if (!Selected()) return;
                    if (target == ColorTarget::Background)
                    {
                        DraftVisual()->background = color;
                        backgroundInput_.SetValue(ColorText(color));
                    }
                    else if (target == ColorTarget::Foreground)
                    {
                        DraftVisual()->foreground = color;
                        foregroundInput_.SetValue(ColorText(color));
                    }
                    else if (target == ColorTarget::Tint)
                    {
                        DraftVisual()->imageTint = color;
                        tintInput_.SetValue(ColorText(color));
                    }
                    else if (target == ColorTarget::Border)
                    {
                        draftStyle_.borderColor = color;
                        borderColorInput_.SetValue(ColorText(color));
                    }
                    else
                    {
                        draftStyle_.text.shadowColor = color;
                        shadowColorInput_.SetValue(ColorText(color));
                    }
                });
        }

        bool ReadStyleFields(bridge::ScreenWidgetStyle& style)
        {
            auto* state = [&]() -> bridge::ScreenVisualState*
            {
                switch (state_)
                {
                case VisualState::Normal: return &style.normal;
                case VisualState::Hover: return &style.hover;
                case VisualState::Pressed: return &style.pressed;
                case VisualState::Focused: return &style.focused;
                case VisualState::Disabled: return &style.disabled;
                }
                return &style.normal;
            }();
            if (!ParseColor(backgroundInput_.GetValue(), state->background))
            {
                status_ = "STYLE REJECTED // BACKGROUND RGBA IS INVALID";
                return false;
            }
            if (!ParseColor(foregroundInput_.GetValue(), state->foreground))
            {
                status_ = "STYLE REJECTED // FOREGROUND RGBA IS INVALID";
                return false;
            }
            if (!ParseColor(tintInput_.GetValue(), state->imageTint))
            {
                status_ = "STYLE REJECTED // IMAGE TINT RGBA IS INVALID";
                return false;
            }
            if (!ParseColor(borderColorInput_.GetValue(), style.borderColor))
            {
                status_ = "STYLE REJECTED // BORDER RGBA IS INVALID";
                return false;
            }
            if (!ParseNumber(borderWidthInput_.GetValue(), style.borderWidth))
            {
                status_ = "STYLE REJECTED // BORDER WIDTH IS INVALID";
                return false;
            }
            if (!ParseNumber(cornerRadiusInput_.GetValue(), style.cornerRadius))
            {
                status_ = "STYLE REJECTED // CORNER RADIUS IS INVALID";
                return false;
            }
            if (!ParseNumber(opacityInput_.GetValue(), style.opacity))
            {
                status_ = "STYLE REJECTED // OPACITY IS INVALID";
                return false;
            }
            return true;
        }

        bool ReadTextFields(bridge::ScreenWidgetStyle& style)
        {
            auto& text = style.text;
            if (!ParseNumber(fontSizeInput_.GetValue(), text.fontSize))
            {
                status_ = "TYPOGRAPHY REJECTED // FONT SIZE IS INVALID";
                return false;
            }
            if (!ParseNumber(characterSpacingInput_.GetValue(), text.characterSpacing))
            {
                status_ = "TYPOGRAPHY REJECTED // CHARACTER SPACING IS INVALID";
                return false;
            }
            if (!ParseNumber(lineSpacingInput_.GetValue(), text.lineSpacing))
            {
                status_ = "TYPOGRAPHY REJECTED // LINE SPACING IS INVALID";
                return false;
            }
            if (!ParseNumber(softnessInput_.GetValue(), text.softness))
            {
                status_ = "TYPOGRAPHY REJECTED // SOFTNESS IS INVALID";
                return false;
            }
            if (!ParseNumber(boldenInput_.GetValue(), text.bolden))
            {
                status_ = "TYPOGRAPHY REJECTED // BOLDEN IS INVALID";
                return false;
            }
            if (!ParseColor(shadowColorInput_.GetValue(), text.shadowColor))
            {
                status_ = "TYPOGRAPHY REJECTED // SHADOW RGBA IS INVALID";
                return false;
            }
            if (!ParseNumber(shadowXInput_.GetValue(), text.shadowOffsetX) ||
                !ParseNumber(shadowYInput_.GetValue(), text.shadowOffsetY))
            {
                status_ = "TYPOGRAPHY REJECTED // SHADOW OFFSET IS INVALID";
                return false;
            }
            if (!ParseNumber(shadowSoftnessInput_.GetValue(), text.shadowSoftness) ||
                !ParseNumber(shadowBoldenInput_.GetValue(), text.shadowBolden))
            {
                status_ = "TYPOGRAPHY REJECTED // SHADOW SOFTNESS/BOLDEN IS INVALID";
                return false;
            }
            return true;
        }

        void CommitCreatorEdit(
            bridge::ScreenWidgetCreatorEdit edit,
            const std::string& success)
        {
            std::string error;
            if (!session_->UpdateWidgetCreatorFields(selectedId_, std::move(edit), error))
            {
                status_ = "REJECTED // " + error;
                RefreshFromSelection(true);
                return;
            }
            draftStyleValid_ = false;
            status_ = success;
            if (documentChanged_) documentChanged_();
            RefreshFromSelection(true);
        }

        bridge::ScreenWidgetCreatorEdit CurrentCreatorEdit() const
        {
            const auto* widget = Selected();
            bridge::ScreenWidgetCreatorEdit edit;
            if (!widget) return edit;
            edit.resourcePath = draftPrimaryResource_;
            edit.actionId = draftAction_;
            edit.parentId = draftParent_;
            edit.layoutMode = draftLayout_;
            edit.anchors = draftAnchors_;
            edit.style = draftStyleValid_ ? draftStyle_ : widget->style;
            return edit;
        }

        void ApplyStyle()
        {
            const auto* widget = Selected();
            if (!widget) return;
            EnsureDraftStyle(*widget);
            if (!ReadStyleFields(draftStyle_)) return;
            CommitCreatorEdit(CurrentCreatorEdit(),
                "STYLE STATE UPDATED // UNSAVED SCREEN CHANGE");
        }

        void ApplyText()
        {
            const auto* widget = Selected();
            if (!widget || widget->kind == bridge::ScreenWidgetKind::Image) return;
            EnsureDraftStyle(*widget);
            if (!ReadTextFields(draftStyle_)) return;
            CommitCreatorEdit(CurrentCreatorEdit(),
                "TYPOGRAPHY UPDATED // UNSAVED SCREEN CHANGE");
        }

        void ApplyBinding()
        {
            const auto* widget = Selected();
            if (!widget) return;
            if (!ParseNumber(anchorMinXInput_.GetValue(), draftAnchors_.minimumX) ||
                !ParseNumber(anchorMinYInput_.GetValue(), draftAnchors_.minimumY) ||
                !ParseNumber(anchorMaxXInput_.GetValue(), draftAnchors_.maximumX) ||
                !ParseNumber(anchorMaxYInput_.GetValue(), draftAnchors_.maximumY))
            {
                status_ = "BINDING REJECTED // ANCHORS MUST BE NUMBERS";
                return;
            }
            CommitCreatorEdit(CurrentCreatorEdit(),
                "BINDING / LAYOUT UPDATED // UNSAVED SCREEN CHANGE");
        }

        void AddAction()
        {
            if (!session_) return;
            const std::string value = Trim(actionIdInput_.GetValue());
            std::string error;
            if (!session_->AddAction(value, error))
            {
                status_ = "ADD ACTION REJECTED // " + error;
                return;
            }
            draftAction_ = value;
            actionButton_.SetText("ACTION: " + value);
            status_ = "ACTION ADDED // APPLY BINDING TO USE IT";
            if (documentChanged_) documentChanged_();
        }

        void RenameAction()
        {
            if (!session_ || draftAction_.empty())
            {
                status_ = "RENAME ACTION REJECTED // SELECT AN ACTION FIRST";
                return;
            }
            const std::string replacement = Trim(actionIdInput_.GetValue());
            const std::string original = draftAction_;
            std::string error;
            if (!session_->RenameAction(original, replacement, error))
            {
                status_ = "RENAME ACTION REJECTED // " + error;
                return;
            }
            draftAction_ = replacement;
            actionButton_.SetText("ACTION: " + replacement);
            status_ = "ACTION RENAMED // BUTTON REFERENCES UPDATED";
            if (documentChanged_) documentChanged_();
            RefreshFromSelection(true);
        }

        void DeleteAction()
        {
            if (!session_ || draftAction_.empty())
            {
                status_ = "DELETE ACTION REJECTED // SELECT AN ACTION FIRST";
                return;
            }
            const std::string action = draftAction_;
            std::string error;
            if (!session_->DeleteAction(action, error))
            {
                status_ = "DELETE ACTION REJECTED // " + error;
                return;
            }
            status_ = "ACTION DELETED // UNSAVED SCREEN CHANGE";
            draftAction_.clear();
            actionIdInput_.SetValue("");
            actionButton_.SetText("ACTION: < NONE >");
            if (documentChanged_) documentChanged_();
            RefreshFromSelection(true);
        }

        void MoveFocus(const bool earlier)
        {
            const auto* widget = Selected();
            if (!session_ || !widget || widget->kind != bridge::ScreenWidgetKind::Button)
                return;
            std::string error;
            const bool moved = earlier
                ? session_->MoveButtonFocusEarlier(widget->id, error)
                : session_->MoveButtonFocusLater(widget->id, error);
            if (!moved)
            {
                status_ = "FOCUS ORDER REJECTED // " + error;
                return;
            }
            status_ = earlier
                ? "BUTTON MOVED EARLIER IN FOCUS ORDER"
                : "BUTTON MOVED LATER IN FOCUS ORDER";
            if (documentChanged_) documentChanged_();
        }

        void CreatePanel()
        {
            if (!session_ || !session_->IsLoaded()) return;
            const auto& document = session_->Document();
            bridge::ScreenWidget widget;
            widget.kind = bridge::ScreenWidgetKind::Image;
            widget.name = "Panel";
            widget.visible = true;
            widget.enabled = false;
            widget.rect = {
                document.designWidth * 0.25f,
                document.designHeight * 0.25f,
                document.designWidth * 0.5f,
                document.designHeight * 0.5f,
            };
            widget.style = bridge::MakeScreenWidgetStyleTemplate(
                widget.kind, widget.rect.height);
            widget.style.normal.background = {24, 30, 36, 230};
            widget.style.hover = widget.style.normal;
            widget.style.pressed = widget.style.normal;
            widget.style.focused = widget.style.normal;
            widget.style.disabled = widget.style.normal;
            bridge::StableId created;
            std::string error;
            if (!session_->CreateWidget(std::move(widget), false, created, error))
            {
                status_ = "PANEL CREATE REJECTED // " + error;
                return;
            }
            workspace_->SelectWidget(created);
            status_ = "PANEL CREATED // ALL APPEARANCE REMAINS EDITABLE";
            if (documentChanged_) documentChanged_();
        }

        void CreateHeading()
        {
            if (!session_ || !session_->IsLoaded()) return;
            const auto& document = session_->Document();
            bridge::ScreenWidget widget;
            widget.kind = bridge::ScreenWidgetKind::Text;
            widget.name = "Heading";
            widget.text = "HEADING";
            widget.visible = true;
            widget.enabled = false;
            widget.rect = {
                document.designWidth * 0.15f,
                document.designHeight * 0.12f,
                document.designWidth * 0.7f,
                88.0f,
            };
            widget.style = bridge::MakeScreenWidgetStyleTemplate(
                widget.kind, widget.rect.height);
            widget.style.text.fontSize = 48.0f;
            widget.style.text.horizontalAlignment =
                bridge::ScreenHorizontalAlignment::Center;
            bridge::StableId created;
            std::string error;
            if (!session_->CreateWidget(std::move(widget), false, created, error))
            {
                status_ = "HEADING CREATE REJECTED // " + error;
                return;
            }
            workspace_->SelectWidget(created);
            status_ = "HEADING CREATED // PRESET VALUES ARE FULLY EDITABLE";
            if (documentChanged_) documentChanged_();
        }

        void DuplicateComponent()
        {
            if (!session_ || selectedId_.empty()) return;
            bridge::StableId duplicated;
            std::string error;
            if (!session_->DuplicateWidgetTree(selectedId_, duplicated, error))
            {
                status_ = "COMPONENT DUPLICATE REJECTED // " + error;
                return;
            }
            workspace_->SelectWidget(duplicated);
            status_ = "COMPONENT SUBTREE DUPLICATED // UNSAVED SCREEN CHANGE";
            if (documentChanged_) documentChanged_();
        }

        void LayoutControls()
        {
            toggleButton_.SetPos(XMFLOAT2(width_ - 122.0f, 14.0f));
            toggleButton_.SetSize(XMFLOAT2(108.0f, 32.0f));
            if (!expanded_) return;

            const float x = panelX_ + 12.0f;
            const float w = panelWidth_ - 24.0f;
            float y = panelY_ + 40.0f;
            const float third = (w - 8.0f) / 3.0f;
            stylePageButton_.SetPos(XMFLOAT2(x, y));
            stylePageButton_.SetSize(XMFLOAT2(third, 28.0f));
            textPageButton_.SetPos(XMFLOAT2(x + third + 4.0f, y));
            textPageButton_.SetSize(XMFLOAT2(third, 28.0f));
            bindingPageButton_.SetPos(XMFLOAT2(x + (third + 4.0f) * 2.0f, y));
            bindingPageButton_.SetSize(XMFLOAT2(third, 28.0f));
            y += 38.0f;

            const auto place = [&](wi::gui::Widget& widget, const float h = 28.0f)
            {
                widget.SetPos(XMFLOAT2(x, y));
                widget.SetSize(XMFLOAT2(w, h));
                y += h + 7.0f;
            };
            const auto pair = [&](wi::gui::Widget& a, wi::gui::Widget& b)
            {
                a.SetPos(XMFLOAT2(x, y));
                a.SetSize(XMFLOAT2(w * 0.70f - 3.0f, 28.0f));
                b.SetPos(XMFLOAT2(x + w * 0.70f + 3.0f, y));
                b.SetSize(XMFLOAT2(w * 0.30f - 3.0f, 28.0f));
                y += 35.0f;
            };
            const auto halves = [&](wi::gui::Widget& a, wi::gui::Widget& b)
            {
                a.SetPos(XMFLOAT2(x, y));
                a.SetSize(XMFLOAT2(w * 0.5f - 3.0f, 28.0f));
                b.SetPos(XMFLOAT2(x + w * 0.5f + 3.0f, y));
                b.SetSize(XMFLOAT2(w * 0.5f - 3.0f, 28.0f));
                y += 35.0f;
            };

            if (page_ == Page::Style)
            {
                place(stateButton_);
                place(stateImageButton_);
                pair(backgroundInput_, backgroundColorButton_);
                pair(foregroundInput_, foregroundColorButton_);
                pair(tintInput_, tintColorButton_);
                pair(borderColorInput_, borderColorButton_);
                halves(borderWidthInput_, cornerRadiusInput_);
                place(opacityInput_);
                place(applyStyleButton_, 32.0f);
            }
            else if (page_ == Page::Text)
            {
                place(fontButton_);
                halves(fontSizeInput_, characterSpacingInput_);
                halves(lineSpacingInput_, softnessInput_);
                place(boldenInput_);
                pair(shadowColorInput_, shadowColorButton_);
                halves(shadowXInput_, shadowYInput_);
                halves(shadowSoftnessInput_, shadowBoldenInput_);
                halves(horizontalButton_, verticalButton_);
                place(wrapButton_);
                place(applyTextButton_, 32.0f);
            }
            else
            {
                place(primaryResourceButton_);
                place(actionButton_);
                place(actionIdInput_);
                halves(addActionButton_, renameActionButton_);
                place(deleteActionButton_);
                halves(focusEarlierButton_, focusLaterButton_);
                place(parentButton_);
                place(layoutButton_);
                halves(anchorMinXInput_, anchorMinYInput_);
                halves(anchorMaxXInput_, anchorMaxYInput_);
                place(applyBindingButton_, 32.0f);
                y += 4.0f;
                halves(panelButton_, headingButton_);
                place(duplicateComponentButton_, 32.0f);
            }
        }

        void SyncVisibility()
        {
            HideAll();
            toggleButton_.SetVisible(true);
            if (!expanded_ || !Selected()) return;
            stylePageButton_.SetVisible(true);
            textPageButton_.SetVisible(true);
            bindingPageButton_.SetVisible(true);

            if (page_ == Page::Style)
            {
                for (auto* widget : StyleControls()) widget->SetVisible(true);
            }
            else if (page_ == Page::Text)
            {
                const bool textCapable = Selected()->kind != bridge::ScreenWidgetKind::Image;
                for (auto* widget : TextControls()) widget->SetVisible(textCapable);
            }
            else
            {
                for (auto* widget : BindingControls()) widget->SetVisible(true);
                const bool image = Selected()->kind == bridge::ScreenWidgetKind::Image;
                const bool button = Selected()->kind == bridge::ScreenWidgetKind::Button;
                primaryResourceButton_.SetEnabled(image);
                actionButton_.SetEnabled(button);
                focusEarlierButton_.SetEnabled(button);
                focusLaterButton_.SetEnabled(button);
            }
        }

        std::vector<wi::gui::Widget*> StyleControls()
        {
            return {&stateButton_, &stateImageButton_,
                &backgroundInput_, &backgroundColorButton_,
                &foregroundInput_, &foregroundColorButton_,
                &tintInput_, &tintColorButton_,
                &borderColorInput_, &borderColorButton_,
                &borderWidthInput_, &cornerRadiusInput_, &opacityInput_,
                &applyStyleButton_};
        }

        std::vector<wi::gui::Widget*> TextControls()
        {
            return {&fontButton_, &fontSizeInput_, &characterSpacingInput_,
                &lineSpacingInput_, &softnessInput_, &boldenInput_,
                &shadowColorInput_, &shadowColorButton_, &shadowXInput_,
                &shadowYInput_, &shadowSoftnessInput_, &shadowBoldenInput_,
                &horizontalButton_, &verticalButton_, &wrapButton_,
                &applyTextButton_};
        }

        std::vector<wi::gui::Widget*> BindingControls()
        {
            return {&primaryResourceButton_, &actionButton_, &actionIdInput_,
                &addActionButton_, &renameActionButton_, &deleteActionButton_,
                &focusEarlierButton_, &focusLaterButton_, &parentButton_,
                &layoutButton_, &anchorMinXInput_, &anchorMinYInput_,
                &anchorMaxXInput_, &anchorMaxYInput_, &applyBindingButton_,
                &panelButton_, &headingButton_, &duplicateComponentButton_};
        }

        std::vector<wi::gui::Widget*> AllControls()
        {
            auto result = std::vector<wi::gui::Widget*>{
                &toggleButton_, &stylePageButton_, &textPageButton_,
                &bindingPageButton_};
            const auto append = [&result](const std::vector<wi::gui::Widget*>& values)
            {
                result.insert(result.end(), values.begin(), values.end());
            };
            append(StyleControls());
            append(TextControls());
            append(BindingControls());
            return result;
        }

        std::vector<const wi::gui::Widget*> AllControlsConst() const
        {
            std::vector<const wi::gui::Widget*> result;
            auto* self = const_cast<RenegadeScreenEditorAdvancedInspector*>(this);
            for (auto* widget : self->AllControls()) result.push_back(widget);
            return result;
        }

        void HideAll()
        {
            for (auto* widget : AllControls()) widget->SetVisible(false);
        }

        static void DrawRect(
            const float x, const float y, const float w, const float h,
            const wi::Color color, const wi::graphics::CommandList cmd)
        {
            wi::image::Params params(x, y, w, h, color);
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            wi::image::Draw(nullptr, params, cmd);
        }

        static void DrawBorder(
            const float x, const float y, const float w, const float h,
            const wi::Color color, const wi::graphics::CommandList cmd)
        {
            DrawRect(x, y, w, 1.0f, color, cmd);
            DrawRect(x, y + h - 1.0f, w, 1.0f, color, cmd);
            DrawRect(x, y, 1.0f, h, color, cmd);
            DrawRect(x + w - 1.0f, y, 1.0f, h, color, cmd);
        }

        static void DrawLabel(
            const std::string& value, const float x, const float y,
            const int size, const wi::Color color,
            const wi::graphics::CommandList cmd)
        {
            wi::font::Params params(x, y, size,
                wi::font::WIFALIGN_LEFT, wi::font::WIFALIGN_TOP,
                color, wi::Color::Transparent());
            params.bolden = 0.12f;
            wi::font::Draw(value, params, cmd);
        }

        bridge::ScreenAuthoringSession* session_ = nullptr;
        RenegadeScreenEditorWorkspace* workspace_ = nullptr;
        std::function<void()> documentChanged_;
        std::string projectRoot_;
        bridge::StableId selectedId_;
        bool created_ = false;
        bool expanded_ = false;
        Page page_ = Page::Style;
        VisualState state_ = VisualState::Normal;
        float width_ = 1.0f;
        float height_ = 1.0f;
        float panelX_ = 0.0f;
        float panelY_ = 64.0f;
        float panelWidth_ = 360.0f;
        float panelHeight_ = 1.0f;
        std::string status_;

        bridge::ScreenWidgetStyle draftStyle_;
        bool draftStyleValid_ = false;
        std::string draftPrimaryResource_;
        std::string draftAction_;
        bridge::StableId draftParent_;
        bridge::ScreenLayoutMode draftLayout_ = bridge::ScreenLayoutMode::Absolute;
        bridge::ScreenAnchors draftAnchors_;
        std::vector<bridge::ScreenCreatorResourceChoice> imageResources_;
        std::vector<bridge::ScreenCreatorResourceChoice> fontResources_;
        RenegadeScreenChoicePicker choicePicker_;
        RenegadeScreenColorPicker colorPicker_;

        RenegadeButton toggleButton_;
        RenegadeButton stylePageButton_;
        RenegadeButton textPageButton_;
        RenegadeButton bindingPageButton_;
        RenegadeButton stateButton_;
        RenegadeButton stateImageButton_;
        RenegadeTextInputField backgroundInput_;
        RenegadeButton backgroundColorButton_;
        RenegadeTextInputField foregroundInput_;
        RenegadeButton foregroundColorButton_;
        RenegadeTextInputField tintInput_;
        RenegadeButton tintColorButton_;
        RenegadeTextInputField borderColorInput_;
        RenegadeButton borderColorButton_;
        RenegadeTextInputField borderWidthInput_;
        RenegadeTextInputField cornerRadiusInput_;
        RenegadeTextInputField opacityInput_;
        RenegadeButton applyStyleButton_;
        RenegadeButton fontButton_;
        RenegadeTextInputField fontSizeInput_;
        RenegadeTextInputField characterSpacingInput_;
        RenegadeTextInputField lineSpacingInput_;
        RenegadeTextInputField softnessInput_;
        RenegadeTextInputField boldenInput_;
        RenegadeTextInputField shadowColorInput_;
        RenegadeButton shadowColorButton_;
        RenegadeTextInputField shadowXInput_;
        RenegadeTextInputField shadowYInput_;
        RenegadeTextInputField shadowSoftnessInput_;
        RenegadeTextInputField shadowBoldenInput_;
        RenegadeButton horizontalButton_;
        RenegadeButton verticalButton_;
        RenegadeButton wrapButton_;
        RenegadeButton applyTextButton_;
        RenegadeButton primaryResourceButton_;
        RenegadeButton actionButton_;
        RenegadeTextInputField actionIdInput_;
        RenegadeButton addActionButton_;
        RenegadeButton renameActionButton_;
        RenegadeButton deleteActionButton_;
        RenegadeButton focusEarlierButton_;
        RenegadeButton focusLaterButton_;
        RenegadeButton parentButton_;
        RenegadeButton layoutButton_;
        RenegadeTextInputField anchorMinXInput_;
        RenegadeTextInputField anchorMinYInput_;
        RenegadeTextInputField anchorMaxXInput_;
        RenegadeTextInputField anchorMaxYInput_;
        RenegadeButton applyBindingButton_;
        RenegadeButton panelButton_;
        RenegadeButton headingButton_;
        RenegadeButton duplicateComponentButton_;
    };
}
