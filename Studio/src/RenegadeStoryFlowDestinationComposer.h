#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

#include <WickedEngine.h>

#include "RenegadeStudioChrome.h"
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/StoryFlowScreenLifecycleService.h"

namespace renegade::studio
{
    // Compact Journey-native destination sheet. This replaces the legacy
    // full-width Level and Screen management rows without replacing their
    // governed lifecycle services.
    class RenegadeStoryFlowDestinationComposer final : public wi::gui::Widget
    {
    public:
        enum class Mode
        {
            Closed,
            Level,
            Screen,
        };

        void Create()
        {
            SetName("Story Flow Add Destination sheet");
            SetShadowRadius(12.0f);

            destinationName_.Create("Story Flow Destination Name");
            destinationName_.SetDescription("NAME  ");
            destinationName_.SetPlaceholder("Destination name...");
            destinationName_.SetValue("");
            destinationName_.SetCancelInputEnabled(false);
            destinationName_.OnInput([this](const wi::gui::EventArgs& args)
            {
                nameDraft_ = args.sValue;
            });
            destinationName_.OnInputAccepted([this](const wi::gui::EventArgs& args)
            {
                nameDraft_ = args.sValue;
            });

            screenTemplate_.Create("Story Flow Screen Template");
            screenTemplate_.AddItem("CUSTOM",
                static_cast<int>(bridge::StoryFlowScreenTemplate::Custom));
            screenTemplate_.AddItem("TITLE / MAIN MENU",
                static_cast<int>(bridge::StoryFlowScreenTemplate::Title));
            screenTemplate_.AddItem("LOADING",
                static_cast<int>(bridge::StoryFlowScreenTemplate::Loading));
            screenTemplate_.AddItem("OPTIONS",
                static_cast<int>(bridge::StoryFlowScreenTemplate::Options));
            screenTemplate_.AddItem("PAUSE",
                static_cast<int>(bridge::StoryFlowScreenTemplate::Pause));
            screenTemplate_.AddItem("FAILURE / DEATH",
                static_cast<int>(bridge::StoryFlowScreenTemplate::Failure));
            screenTemplate_.AddItem("VICTORY",
                static_cast<int>(bridge::StoryFlowScreenTemplate::Victory));
            screenTemplate_.AddItem("SAVE / LOAD",
                static_cast<int>(bridge::StoryFlowScreenTemplate::SaveLoad));
            screenTemplate_.AddItem("CREDITS",
                static_cast<int>(bridge::StoryFlowScreenTemplate::Credits));
            screenTemplate_.SetSelectedWithoutCallback(1);
            screenTemplate_.OnSelect([this](const wi::gui::EventArgs& args)
            {
                selectedTemplate_ =
                    static_cast<bridge::StoryFlowScreenTemplate>(args.userdata);
            });

            create_.Create("Story Flow Create Destination");
            create_.SetText("CREATE");
            create_.SetTooltip(
                "Create this governed destination and add it to the current Journey.");
            create_.OnClick([this](const wi::gui::EventArgs&)
            {
                const std::string name = LiveName();
                if (mode_ == Mode::Level && createLevel_)
                    createLevel_(name);
                else if (mode_ == Mode::Screen && createScreen_)
                    createScreen_(name, selectedTemplate_);
            });

            adopt_.Create("Story Flow Adopt Existing Level");
            adopt_.SetText("ADD EXISTING...");
            adopt_.SetTooltip(
                "Choose an existing WISCENE and adopt it through the governed Level lifecycle.");
            adopt_.OnClick([this](const wi::gui::EventArgs&)
            {
                if (mode_ == Mode::Level && adoptLevel_)
                    adoptLevel_(LiveName());
            });

            openSelected_.Create("Story Flow Open Selected Destination");
            openSelected_.SetText("OPEN SELECTED");
            openSelected_.SetTooltip(
                "Open the selected Journey card in its governed editor.");
            openSelected_.OnClick([this](const wi::gui::EventArgs&)
            {
                if (openSelectedCallback_ && CanOpenSelection())
                    openSelectedCallback_(selectedNodeId_);
            });

            close_.Create("Story Flow Close Add Destination");
            close_.SetText("X");
            close_.SetTooltip("Close Add Destination.");
            close_.OnClick([this](const wi::gui::EventArgs&)
            {
                SetMode(Mode::Closed);
            });

            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&destinationName_),
                static_cast<wi::gui::Widget*>(&screenTemplate_),
                static_cast<wi::gui::Widget*>(&create_),
                static_cast<wi::gui::Widget*>(&adopt_),
                static_cast<wi::gui::Widget*>(&openSelected_),
                static_cast<wi::gui::Widget*>(&close_)})
            {
                widget->SetShadowRadius(4.0f);
            }

            SetMode(Mode::Closed);
            SetLayout(86.0f, 78.0f, 1280.0f);
        }

        void Attach(wi::gui::GUI& gui)
        {
            if (attached_) return;

            // Wicked renders registrations back-to-front. Register interactive
            // controls before the sheet so the sheet is behind its controls.
            gui.AddWidget(&close_);
            gui.AddWidget(&openSelected_);
            gui.AddWidget(&adopt_);
            gui.AddWidget(&create_);
            gui.AddWidget(&screenTemplate_);
            gui.AddWidget(&destinationName_);
            gui.AddWidget(this);
            attached_ = true;
        }

        void Detach(wi::gui::GUI& gui)
        {
            if (!attached_) return;
            gui.RemoveWidget(&close_);
            gui.RemoveWidget(&openSelected_);
            gui.RemoveWidget(&adopt_);
            gui.RemoveWidget(&create_);
            gui.RemoveWidget(&screenTemplate_);
            gui.RemoveWidget(&destinationName_);
            gui.RemoveWidget(this);
            attached_ = false;
        }

        void SetLayout(
            const float workspaceLeft,
            const float workspaceTop,
            const float viewportWidth)
        {
            const float sheetWidth = std::clamp(viewportWidth - 36.0f, 360.0f, 520.0f);
            const float x = workspaceLeft + 18.0f;
            const float y = workspaceTop + 16.0f;
            SetPos(XMFLOAT2(x, y));
            SetSize(XMFLOAT2(sheetWidth, 154.0f));

            close_.SetPos(XMFLOAT2(x + sheetWidth - 38.0f, y + 12.0f));
            close_.SetSize(XMFLOAT2(26.0f, 24.0f));

            destinationName_.SetPos(XMFLOAT2(x + 18.0f, y + 48.0f));
            destinationName_.SetSize(XMFLOAT2(sheetWidth - 36.0f, 28.0f));
            screenTemplate_.SetPos(XMFLOAT2(x + 18.0f, y + 84.0f));
            screenTemplate_.SetSize(XMFLOAT2(sheetWidth - 36.0f, 28.0f));

            const float buttonY = y + 118.0f;
            create_.SetPos(XMFLOAT2(x + 18.0f, buttonY));
            create_.SetSize(XMFLOAT2(96.0f, 26.0f));
            adopt_.SetPos(XMFLOAT2(x + 122.0f, buttonY));
            adopt_.SetSize(XMFLOAT2(132.0f, 26.0f));
            openSelected_.SetPos(XMFLOAT2(x + sheetWidth - 148.0f, buttonY));
            openSelected_.SetSize(XMFLOAT2(130.0f, 26.0f));
        }

        void SetMode(const Mode mode)
        {
            mode_ = mode;
            const bool active = workspaceActive_ && mode_ != Mode::Closed;
            SetVisible(active);
            SetEnabled(active);
            destinationName_.SetVisible(active);
            destinationName_.SetEnabled(active);
            screenTemplate_.SetVisible(active && mode_ == Mode::Screen);
            screenTemplate_.SetEnabled(active && mode_ == Mode::Screen);
            create_.SetVisible(active);
            create_.SetEnabled(active);
            create_.SetText(mode_ == Mode::Screen ? "CREATE SCREEN" : "CREATE LEVEL");
            adopt_.SetVisible(active && mode_ == Mode::Level);
            adopt_.SetEnabled(active && mode_ == Mode::Level);
            openSelected_.SetVisible(active);
            openSelected_.SetEnabled(active && CanOpenSelection());
            close_.SetVisible(active);
            close_.SetEnabled(active);
        }

        void SetWorkspaceActive(const bool active)
        {
            workspaceActive_ = active;
            SetMode(mode_);
        }

        void Toggle(const Mode mode)
        {
            SetMode(mode_ == mode ? Mode::Closed : mode);
        }

        void SetSelectedNode(bridge::StableId nodeId, const Mode destinationMode)
        {
            selectedNodeId_ = std::move(nodeId);
            selectedNodeMode_ = destinationMode;
            const bool active = workspaceActive_ && mode_ != Mode::Closed;
            openSelected_.SetEnabled(active && CanOpenSelection());
        }

        void Clear()
        {
            selectedNodeId_.clear();
            selectedNodeMode_ = Mode::Closed;
            destinationName_.SetValue("");
            nameDraft_.clear();
            SetMode(Mode::Closed);
        }

        [[nodiscard]] bool IsOpen() const noexcept
        {
            return mode_ != Mode::Closed;
        }

        void OnCreateLevel(std::function<void(const std::string&)> callback)
        {
            createLevel_ = std::move(callback);
        }

        void OnAdoptLevel(std::function<void(const std::string&)> callback)
        {
            adoptLevel_ = std::move(callback);
        }

        void OnCreateScreen(std::function<void(
            const std::string&, bridge::StoryFlowScreenTemplate)> callback)
        {
            createScreen_ = std::move(callback);
        }

        void OnOpenSelected(
            std::function<void(const bridge::StableId&)> callback)
        {
            openSelectedCallback_ = std::move(callback);
        }

        void Render(
            const wi::Canvas&,
            const wi::graphics::CommandList cmd) const override
        {
            if (!IsVisible()) return;

            wi::image::Params shadow(
                translation.x + 4.0f,
                translation.y + 7.0f,
                scale.x,
                scale.y,
                wi::Color(0, 0, 0, 118));
            shadow.blendFlag = wi::enums::BLENDMODE_ALPHA;
            wi::image::Draw(nullptr, shadow, cmd);

            wi::image::Params surface(
                translation.x,
                translation.y,
                scale.x,
                scale.y,
                wi::Color(14, 20, 25, 255));
            surface.blendFlag = wi::enums::BLENDMODE_ALPHA;
            wi::image::Draw(nullptr, surface, cmd);

            DrawLabel("ADD DESTINATION", translation.x + 18.0f,
                translation.y + 15.0f, 10, wi::Color(239, 242, 243, 255), cmd);
            DrawLabel(mode_ == Mode::Screen ? "SCREEN" : "LEVEL",
                translation.x + 148.0f, translation.y + 17.0f, 7,
                wi::Color(104, 180, 229, 255), cmd);
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStoryFlowDestinationComposer";
        }

    private:
        [[nodiscard]] bool CanOpenSelection() const noexcept
        {
            return !selectedNodeId_.empty() && selectedNodeMode_ == mode_;
        }

        [[nodiscard]] std::string LiveName()
        {
            const std::string live = destinationName_.GetCurrentInputValue();
            return live == destinationName_.GetValue() ? nameDraft_ : live;
        }

        static void DrawLabel(
            const std::string& value,
            const float x,
            const float y,
            const int size,
            const wi::Color color,
            const wi::graphics::CommandList cmd)
        {
            wi::font::Params params(
                x, y, size,
                wi::font::WIFALIGN_LEFT,
                wi::font::WIFALIGN_TOP,
                color,
                wi::Color::Transparent());
            params.bolden = 0.12f;
            wi::font::Draw(value, params, cmd);
        }

        RenegadeTextInputField destinationName_;
        RenegadeComboBox screenTemplate_;
        RenegadeButton create_;
        RenegadeButton adopt_;
        RenegadeButton openSelected_;
        RenegadeButton close_;
        Mode mode_ = Mode::Closed;
        Mode selectedNodeMode_ = Mode::Closed;
        bridge::StoryFlowScreenTemplate selectedTemplate_ =
            bridge::StoryFlowScreenTemplate::Title;
        bridge::StableId selectedNodeId_;
        std::string nameDraft_;
        std::function<void(const std::string&)> createLevel_;
        std::function<void(const std::string&)> adoptLevel_;
        std::function<void(
            const std::string&, bridge::StoryFlowScreenTemplate)> createScreen_;
        std::function<void(const bridge::StableId&)> openSelectedCallback_;
        bool attached_ = false;
        bool workspaceActive_ = false;
    };
}
