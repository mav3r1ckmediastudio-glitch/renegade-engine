#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

#include <WickedEngine.h>

#include "RenegadeStudioChrome.h"
#include "renegade/bridge/StoryFlowScreenLifecycleService.h"

namespace renegade::studio
{
    // Native Story Flow surface for governed Screen creation and explicit
    // Screen Editor handoff. The panel owns the user's live draft name so
    // creation never depends on a TextInputField focus/commit transition.
    class RenegadeStoryFlowScreenPanel final : public wi::gui::Widget
    {
    public:
        void Create()
        {
            SetName("Story Flow Screen lifecycle");
            SetShadowRadius(0.0f);

            screenName_.Create("Story Flow Screen Name");
            screenName_.SetDescription("SCREEN  ");
            screenName_.SetPlaceholder("Screen name...");
            screenName_.SetValue("");
            screenName_.SetCancelInputEnabled(false);
            screenName_.OnInput([this](const wi::gui::EventArgs& args)
            {
                screenNameDraft_ = args.sValue;
            });
            screenName_.OnInputAccepted([this](const wi::gui::EventArgs& args)
            {
                screenNameDraft_ = args.sValue;
            });

            screenTemplate_.Create("Story Flow Screen Template");
            screenTemplate_.AddItem("CUSTOM", static_cast<int>(bridge::StoryFlowScreenTemplate::Custom));
            screenTemplate_.AddItem("TITLE / MAIN MENU", static_cast<int>(bridge::StoryFlowScreenTemplate::Title));
            screenTemplate_.AddItem("LOADING", static_cast<int>(bridge::StoryFlowScreenTemplate::Loading));
            screenTemplate_.AddItem("OPTIONS", static_cast<int>(bridge::StoryFlowScreenTemplate::Options));
            screenTemplate_.AddItem("PAUSE", static_cast<int>(bridge::StoryFlowScreenTemplate::Pause));
            screenTemplate_.AddItem("FAILURE / DEATH", static_cast<int>(bridge::StoryFlowScreenTemplate::Failure));
            screenTemplate_.AddItem("VICTORY", static_cast<int>(bridge::StoryFlowScreenTemplate::Victory));
            screenTemplate_.AddItem("SAVE / LOAD", static_cast<int>(bridge::StoryFlowScreenTemplate::SaveLoad));
            screenTemplate_.AddItem("CREDITS", static_cast<int>(bridge::StoryFlowScreenTemplate::Credits));
            screenTemplate_.SetSelectedWithoutCallback(1);
            selectedTemplate_ = bridge::StoryFlowScreenTemplate::Title;
            screenTemplate_.OnSelect([this](const wi::gui::EventArgs& args)
            {
                selectedTemplate_ = static_cast<bridge::StoryFlowScreenTemplate>(args.userdata);
            });

            newScreen_.Create("Story Flow Add Screen");
            newScreen_.SetText("+ SCREEN");
            newScreen_.SetTooltip(
                "Create a governed Runtime Screen document and add it to Story Flow atomically.");
            newScreen_.OnClick([this](const wi::gui::EventArgs&)
            {
                if (!addNew_) return;
                const std::string live = screenName_.GetCurrentInputValue();
                addNew_(live == screenName_.GetValue() ? screenNameDraft_ : live,
                    selectedTemplate_);
            });

            openScreen_.Create("Story Flow Open Screen");
            openScreen_.SetText("OPEN SCREEN");
            openScreen_.SetTooltip(
                "Resolve this Screen by stable identity and prepare the Screen Editor handoff.");
            openScreen_.OnClick([this](const wi::gui::EventArgs&)
            {
                if (open_ && !selectedScreenNodeId_.empty())
                    open_(selectedScreenNodeId_);
            });

            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&screenName_),
                static_cast<wi::gui::Widget*>(&screenTemplate_),
                static_cast<wi::gui::Widget*>(&newScreen_),
                static_cast<wi::gui::Widget*>(&openScreen_)})
            {
                widget->SetShadowRadius(0.0f);
            }

            SetLayout(1.0f, 1.0f);
        }

        void Attach(wi::gui::GUI& gui)
        {
            if (attached_) return;
            gui.AddWidget(&screenName_);
            gui.AddWidget(&screenTemplate_);
            gui.AddWidget(&newScreen_);
            gui.AddWidget(&openScreen_);
            attached_ = true;
        }

        void SetLayout(const float width, const float height)
        {
            width_ = width;
            height_ = height;
            const float y = 88.0f;
            const float x = std::clamp(width * 0.27f, 230.0f, 520.0f);
            screenName_.SetPos(XMFLOAT2(x, y));
            screenName_.SetSize(XMFLOAT2(220.0f, 27.0f));
            screenTemplate_.SetPos(XMFLOAT2(x + 230.0f, y));
            screenTemplate_.SetSize(XMFLOAT2(170.0f, 27.0f));
            newScreen_.SetPos(XMFLOAT2(x + 408.0f, y));
            newScreen_.SetSize(XMFLOAT2(86.0f, 27.0f));
            openScreen_.SetPos(XMFLOAT2(x + 502.0f, y));
            openScreen_.SetSize(XMFLOAT2(116.0f, 27.0f));
        }

        void SetActive(const bool active)
        {
            active_ = active;
            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&screenName_),
                static_cast<wi::gui::Widget*>(&screenTemplate_),
                static_cast<wi::gui::Widget*>(&newScreen_),
                static_cast<wi::gui::Widget*>(&openScreen_)})
            {
                widget->SetVisible(active);
                widget->SetEnabled(active);
            }
            openScreen_.SetEnabled(active && !selectedScreenNodeId_.empty());
        }

        void SetSelectedScreenNode(bridge::StableId nodeId)
        {
            selectedScreenNodeId_ = std::move(nodeId);
            openScreen_.SetEnabled(active_ && !selectedScreenNodeId_.empty());
        }

        void OnAddNew(std::function<void(
            const std::string&, bridge::StoryFlowScreenTemplate)> callback)
        {
            addNew_ = std::move(callback);
        }

        void OnOpen(std::function<void(const bridge::StableId&)> callback)
        {
            open_ = std::move(callback);
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStoryFlowScreenPanel";
        }

    private:
        RenegadeTextInputField screenName_;
        RenegadeComboBox screenTemplate_;
        RenegadeButton newScreen_;
        RenegadeButton openScreen_;
        std::string screenNameDraft_;
        bridge::StableId selectedScreenNodeId_;
        bridge::StoryFlowScreenTemplate selectedTemplate_ =
            bridge::StoryFlowScreenTemplate::Title;
        std::function<void(
            const std::string&, bridge::StoryFlowScreenTemplate)> addNew_;
        std::function<void(const bridge::StableId&)> open_;
        float width_ = 1.0f;
        float height_ = 1.0f;
        bool attached_ = false;
        bool active_ = false;
    };
}
