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

#include "RenegadeScreenEditorWorkspace.h"
#include "RenegadeStudioChrome.h"
#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/bridge/ScreenCreatorResourceService.h"

namespace renegade::studio
{
    // Gate 8D's complete schema-v2 Inspector. It overlays the accepted 8C
    // basic Inspector only while expanded; closing it reveals the original
    // position/size/name/text workflow unchanged.
    class RenegadeScreenEditorAdvancedInspector final : public wi::gui::Widget
    {
    public:
        void Create()
        {
            SetName("Renegade Screen advanced Inspector");
            SetShadowRadius(0.0f);

            toggleButton_.Create("Renegade Screen advanced Inspector Toggle");
            toggleButton_.SetText("ADVANCED");
            toggleButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                expanded_ = !expanded_;
                toggleButton_.SetText(expanded_ ? "< BASIC" : "ADVANCED");
                RefreshFromSelection(true);
            });

            stylePageButton_.Create("Screen Inspector Style Page");
            stylePageButton_.SetText("STYLE");
            stylePageButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                page_ = Page::Style;
                RefreshFromSelection(true);
            });
            textPageButton_.Create("Screen Inspector Text Page");
            textPageButton_.SetText("TEXT / FONT");
            textPageButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                page_ = Page::Text;
                RefreshFromSelection(true);
            });
            bindingPageButton_.Create("Screen Inspector Binding Page");
            bindingPageButton_.SetText("BIND / LAYOUT");
            bindingPageButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                page_ = Page::Binding;
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
                CycleStateImage();
            });

            ConfigureInput(backgroundInput_, "Screen State Background", "BACKGROUND RGBA");
            ConfigureInput(foregroundInput_, "Screen State Foreground", "FOREGROUND RGBA");
            ConfigureInput(tintInput_, "Screen State Tint", "IMAGE TINT RGBA");
            ConfigureInput(borderColorInput_, "Screen Border Color", "BORDER RGBA");
            ConfigureInput(borderWidthInput_, "Screen Border Width", "BORDER WIDTH");
            ConfigureInput(cornerRadiusInput_, "Screen Corner Radius", "CORNER RADIUS");
            ConfigureInput(opacityInput_, "Screen Opacity", "OPACITY 0..1");
            applyStyleButton_.Create("Apply Screen Style");
            applyStyleButton_.SetText("APPLY STYLE STATE");
            applyStyleButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                ApplyStyle();
            });

            fontButton_.Create("Screen Font Resource");
            fontButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                CycleFont();
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
            horizontalButton_.Create("Screen Horizontal Alignment");
            horizontalButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                const auto* widget = Selected();
                if (!widget) return;
                draftStyle_ = widget->style;
                auto& value = draftStyle_.text.horizontalAlignment;
                value = value == bridge::ScreenHorizontalAlignment::Left
                    ? bridge::ScreenHorizontalAlignment::Center
                    : value == bridge::ScreenHorizontalAlignment::Center
                        ? bridge::ScreenHorizontalAlignment::Right
                        : bridge::ScreenHorizontalAlignment::Left;
                draftStyleValid_ = true;
                horizontalButton_.SetText(HorizontalText(value));
            });
            verticalButton_.Create("Screen Vertical Alignment");
            verticalButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                const auto* widget = Selected();
                if (!widget) return;
                if (!draftStyleValid_) draftStyle_ = widget->style;
                auto& value = draftStyle_.text.verticalAlignment;
                value = value == bridge::ScreenVerticalAlignment::Top
                    ? bridge::ScreenVerticalAlignment::Center
                    : value == bridge::ScreenVerticalAlignment::Center
                        ? bridge::ScreenVerticalAlignment::Bottom
                        : bridge::ScreenVerticalAlignment::Top;
                draftStyleValid_ = true;
                verticalButton_.SetText(VerticalText(value));
            });
            wrapButton_.Create("Screen Text Wrap");
            wrapButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                const auto* widget = Selected();
                if (!widget) return;
                if (!draftStyleValid_) draftStyle_ = widget->style;
                draftStyle_.text.wrap = !draftStyle_.text.wrap;
                draftStyleValid_ = true;
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
                CyclePrimaryResource();
            });
            actionButton_.Create("Screen Action Binding");
            actionButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                CycleAction();
            });
            parentButton_.Create("Screen Parent Binding");
            parentButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                CycleParent();
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
            panelButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                CreatePanel();
            });
            headingButton_.Create("Create Screen Heading Preset");
            headingButton_.SetText("+ HEADING");
            headingButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                CreateHeading();
            });
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
            session_ = nullptr;
            workspace_ = nullptr;
            projectRoot_.clear();
            documentChanged_ = {};
            selectedId_.clear();
            imageResources_.clear();
            fontResources_.clear();
            expanded_ = false;
            HideAll();
        }

        void SetLayout(const float width, const float height)
        {
            width_ = std::max(1.0f, width);
            height_ = std::max(1.0f, height);
            panelWidth_ = std::min(390.0f, std::max(320.0f, width_ * 0.31f));
            panelX_ = width_ - panelWidth_;
            panelY_ = 64.0f;
            panelHeight_ = std::max(1.0f, height_ - 108.0f);
            SetPos(XMFLOAT2(panelX_, panelY_));
            SetSize(XMFLOAT2(panelWidth_, panelHeight_));
            LayoutControls();
        }

        void Update(const wi::Canvas& canvas, const float dt) override
        {
            Widget::Update(canvas, dt);
            if (!created_ || !IsVisible() || !session_ || !session_->IsLoaded())
            {
                HideAll();
                return;
            }
            const bridge::StableId selection = workspace_
                ? workspace_->SelectedWidgetId() : bridge::StableId{};
            if (selection != selectedId_)
            {
                selectedId_ = selection;
                RefreshFromSelection(true);
            }
            LayoutControls();
            SyncVisibility();
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
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeScreenEditorAdvancedInspector";
        }

    private:
        enum class Page { Style, Text, Binding };
        enum class VisualState { Normal, Hover, Pressed, Focused, Disabled };

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
                result = std::stof(value, &used);
                return used == value.size() && std::isfinite(result);
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

        const bridge::ScreenWidget* Selected() const noexcept
        {
            return session_ ? session_->FindWidget(selectedId_) : nullptr;
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
            const auto images = bridge::EnumerateScreenCreatorResources(
                projectRoot_, bridge::ScreenCreatorResourceKind::Image);
            if (images.succeeded) imageResources_ = images.choices;
            const auto fonts = bridge::EnumerateScreenCreatorResources(
                projectRoot_, bridge::ScreenCreatorResourceKind::Font);
            if (fonts.succeeded) fontResources_ = fonts.choices;
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
            stateImageButton_.SetText(
                "STATE IMAGE: " + Short(state->imageResourcePath));
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

            primaryResourceButton_.SetText(
                "IMAGE: " + Short(draftPrimaryResource_));
            actionButton_.SetText("ACTION: " +
                (draftAction_.empty() ? std::string("< NONE >") : draftAction_));
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
            while (!parent.empty())
            {
                if (parent == selectedId_) return true;
                const auto* widget = session_->FindWidget(parent);
                if (!widget) break;
                parent = widget->parentId;
            }
            return false;
        }

        void CycleStringChoice(
            std::string& current,
            const std::vector<std::string>& choices,
            const bool includeEmpty)
        {
            std::vector<std::string> values;
            if (includeEmpty) values.emplace_back();
            values.insert(values.end(), choices.begin(), choices.end());
            if (values.empty()) return;
            const auto found = std::find(values.begin(), values.end(), current);
            const std::size_t index = found == values.end()
                ? 0 : (static_cast<std::size_t>(
                    std::distance(values.begin(), found)) + 1) % values.size();
            current = values[index];
        }

        std::vector<std::string> ImagePaths() const
        {
            std::vector<std::string> values;
            for (const auto& choice : imageResources_)
                values.push_back(choice.projectRelativePath);
            return values;
        }

        std::vector<std::string> FontPaths() const
        {
            std::vector<std::string> values{bridge::BuiltinScreenFont};
            for (const auto& choice : fontResources_)
                values.push_back(choice.projectRelativePath);
            return values;
        }

        void CycleStateImage()
        {
            const auto* widget = Selected();
            if (!widget) return;
            if (!draftStyleValid_) draftStyle_ = widget->style;
            auto* state = DraftVisual();
            auto values = ImagePaths();
            CycleStringChoice(state->imageResourcePath, values, true);
            stateImageButton_.SetText(
                "STATE IMAGE: " + Short(state->imageResourcePath));
        }

        void CycleFont()
        {
            const auto* widget = Selected();
            if (!widget) return;
            if (!draftStyleValid_) draftStyle_ = widget->style;
            const auto values = FontPaths();
            CycleStringChoice(draftStyle_.text.fontResource, values, false);
            fontButton_.SetText("FONT: " + Short(draftStyle_.text.fontResource));
        }

        void CyclePrimaryResource()
        {
            const auto* widget = Selected();
            if (!widget || widget->kind != bridge::ScreenWidgetKind::Image) return;
            const auto values = ImagePaths();
            CycleStringChoice(draftPrimaryResource_, values, true);
            primaryResourceButton_.SetText(
                "IMAGE: " + Short(draftPrimaryResource_));
        }

        void CycleAction()
        {
            const auto* widget = Selected();
            if (!widget || widget->kind != bridge::ScreenWidgetKind::Button) return;
            std::vector<std::string> values;
            for (const auto& action : session_->Document().actions)
                values.push_back(action.id);
            CycleStringChoice(draftAction_, values, false);
            actionButton_.SetText("ACTION: " + draftAction_);
        }

        void CycleParent()
        {
            const auto* widget = Selected();
            if (!widget) return;
            std::vector<std::string> values;
            values.emplace_back();
            for (const auto& candidate : session_->Document().widgets)
            {
                if (candidate.id == selectedId_ || IsDescendantOfSelection(candidate))
                    continue;
                values.push_back(candidate.id);
            }
            if (values.empty()) return;
            const auto found = std::find(values.begin(), values.end(), draftParent_);
            const std::size_t index = found == values.end()
                ? 0 : (static_cast<std::size_t>(
                    std::distance(values.begin(), found)) + 1) % values.size();
            draftParent_ = values[index];
            parentButton_.SetText("PARENT: " + ParentLabel(draftParent_));
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
            if (!ParseColor(backgroundInput_.GetValue(), state->background) ||
                !ParseColor(foregroundInput_.GetValue(), state->foreground) ||
                !ParseColor(tintInput_.GetValue(), state->imageTint) ||
                !ParseColor(borderColorInput_.GetValue(), style.borderColor) ||
                !ParseNumber(borderWidthInput_.GetValue(), style.borderWidth) ||
                !ParseNumber(cornerRadiusInput_.GetValue(), style.cornerRadius) ||
                !ParseNumber(opacityInput_.GetValue(), style.opacity))
            {
                status_ = "STYLE REJECTED // CHECK RGBA AND NUMBER FIELDS";
                return false;
            }
            return true;
        }

        bool ReadTextFields(bridge::ScreenWidgetStyle& style)
        {
            auto& text = style.text;
            if (!ParseNumber(fontSizeInput_.GetValue(), text.fontSize) ||
                !ParseNumber(characterSpacingInput_.GetValue(), text.characterSpacing) ||
                !ParseNumber(lineSpacingInput_.GetValue(), text.lineSpacing) ||
                !ParseNumber(softnessInput_.GetValue(), text.softness) ||
                !ParseNumber(boldenInput_.GetValue(), text.bolden) ||
                !ParseColor(shadowColorInput_.GetValue(), text.shadowColor) ||
                !ParseNumber(shadowXInput_.GetValue(), text.shadowOffsetX) ||
                !ParseNumber(shadowYInput_.GetValue(), text.shadowOffsetY) ||
                !ParseNumber(shadowSoftnessInput_.GetValue(), text.shadowSoftness) ||
                !ParseNumber(shadowBoldenInput_.GetValue(), text.shadowBolden))
            {
                status_ = "TYPOGRAPHY REJECTED // CHECK RGBA AND NUMBER FIELDS";
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
            if (!draftStyleValid_) draftStyle_ = widget->style;
            if (!ReadStyleFields(draftStyle_)) return;
            draftStyleValid_ = true;
            CommitCreatorEdit(CurrentCreatorEdit(),
                "STYLE STATE UPDATED // UNSAVED SCREEN CHANGE");
        }

        void ApplyText()
        {
            const auto* widget = Selected();
            if (!widget || widget->kind == bridge::ScreenWidgetKind::Image) return;
            if (!draftStyleValid_) draftStyle_ = widget->style;
            if (!ReadTextFields(draftStyle_)) return;
            draftStyleValid_ = true;
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

            auto place = [&](wi::gui::Widget& widget, const float h = 28.0f)
            {
                widget.SetPos(XMFLOAT2(x, y));
                widget.SetSize(XMFLOAT2(w, h));
                y += h + 7.0f;
            };
            auto pair = [&](wi::gui::Widget& a, wi::gui::Widget& b)
            {
                a.SetPos(XMFLOAT2(x, y));
                a.SetSize(XMFLOAT2(w * 0.5f - 3.0f, 28.0f));
                b.SetPos(XMFLOAT2(x + w * 0.5f + 3.0f, y));
                b.SetSize(XMFLOAT2(w * 0.5f - 3.0f, 28.0f));
                y += 35.0f;
            };

            if (page_ == Page::Style)
            {
                place(stateButton_); place(stateImageButton_);
                place(backgroundInput_); place(foregroundInput_); place(tintInput_);
                place(borderColorInput_); pair(borderWidthInput_, cornerRadiusInput_);
                place(opacityInput_); place(applyStyleButton_, 32.0f);
            }
            else if (page_ == Page::Text)
            {
                place(fontButton_); pair(fontSizeInput_, characterSpacingInput_);
                pair(lineSpacingInput_, softnessInput_); pair(boldenInput_, shadowColorInput_);
                pair(shadowXInput_, shadowYInput_);
                pair(shadowSoftnessInput_, shadowBoldenInput_);
                pair(horizontalButton_, verticalButton_); place(wrapButton_);
                place(applyTextButton_, 32.0f);
            }
            else
            {
                place(primaryResourceButton_); place(actionButton_); place(parentButton_);
                place(layoutButton_); pair(anchorMinXInput_, anchorMinYInput_);
                pair(anchorMaxXInput_, anchorMaxYInput_); place(applyBindingButton_, 32.0f);
                y += 4.0f; pair(panelButton_, headingButton_);
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
                primaryResourceButton_.SetEnabled(
                    Selected()->kind == bridge::ScreenWidgetKind::Image);
                actionButton_.SetEnabled(
                    Selected()->kind == bridge::ScreenWidgetKind::Button);
            }
        }

        std::vector<wi::gui::Widget*> StyleControls()
        {
            return {&stateButton_, &stateImageButton_, &backgroundInput_,
                &foregroundInput_, &tintInput_, &borderColorInput_,
                &borderWidthInput_, &cornerRadiusInput_, &opacityInput_,
                &applyStyleButton_};
        }
        std::vector<wi::gui::Widget*> TextControls()
        {
            return {&fontButton_, &fontSizeInput_, &characterSpacingInput_,
                &lineSpacingInput_, &softnessInput_, &boldenInput_,
                &shadowColorInput_, &shadowXInput_, &shadowYInput_,
                &shadowSoftnessInput_, &shadowBoldenInput_, &horizontalButton_,
                &verticalButton_, &wrapButton_, &applyTextButton_};
        }
        std::vector<wi::gui::Widget*> BindingControls()
        {
            return {&primaryResourceButton_, &actionButton_, &parentButton_,
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
            append(StyleControls()); append(TextControls()); append(BindingControls());
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
        float panelWidth_ = 390.0f;
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

        RenegadeButton toggleButton_;
        RenegadeButton stylePageButton_;
        RenegadeButton textPageButton_;
        RenegadeButton bindingPageButton_;
        RenegadeButton stateButton_;
        RenegadeButton stateImageButton_;
        RenegadeTextInputField backgroundInput_;
        RenegadeTextInputField foregroundInput_;
        RenegadeTextInputField tintInput_;
        RenegadeTextInputField borderColorInput_;
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
