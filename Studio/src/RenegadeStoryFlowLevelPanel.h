#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

#include <WickedEngine.h>

#include "RenegadeStudioChrome.h"
#include "renegade/bridge/FlowService.h"

namespace renegade::studio
{
    // Native Story Flow Level lifecycle surface. The panel owns the user's live
    // draft name so creation/adoption never depends on a focus/commit transition.
    class RenegadeStoryFlowLevelPanel final : public wi::gui::Widget
    {
    public:
        void Create()
        {
            SetName("Story Flow Level lifecycle");
            SetShadowRadius(0.0f);

            levelName_.Create("Story Flow Level Name");
            levelName_.SetDescription("LEVEL  ");
            levelName_.SetPlaceholder("Level name...");
            levelName_.SetValue("");
            levelName_.SetCancelInputEnabled(false);
            levelName_.OnInput([this](const wi::gui::EventArgs& args)
            {
                levelNameDraft_ = args.sValue;
            });
            levelName_.OnInputAccepted([this](const wi::gui::EventArgs& args)
            {
                levelNameDraft_ = args.sValue;
            });

            newLevel_.Create("Story Flow Add New Level");
            newLevel_.SetText("+ NEW LEVEL");
            newLevel_.OnClick([this](const wi::gui::EventArgs&)
            {
                if (addNew_)
                {
                    const std::string live = levelName_.GetCurrentInputValue();
                    addNew_(live == levelName_.GetValue() ? levelNameDraft_ : live);
                }
            });

            existingLevel_.Create("Story Flow Add Existing Level");
            existingLevel_.SetText("+ EXISTING...");
            existingLevel_.OnClick([this](const wi::gui::EventArgs&)
            {
                if (addExisting_)
                {
                    const std::string live = levelName_.GetCurrentInputValue();
                    addExisting_(live == levelName_.GetValue() ? levelNameDraft_ : live);
                }
            });

            openLevel_.Create("Story Flow Open Level");
            openLevel_.SetText("OPEN LEVEL");
            openLevel_.OnClick([this](const wi::gui::EventArgs&)
            {
                if (open_ && !selectedLevelNodeId_.empty())
                    open_(selectedLevelNodeId_);
            });

            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&levelName_),
                static_cast<wi::gui::Widget*>(&newLevel_),
                static_cast<wi::gui::Widget*>(&existingLevel_),
                static_cast<wi::gui::Widget*>(&openLevel_)})
            {
                widget->SetShadowRadius(0.0f);
            }

            SetLayout(1.0f, 1.0f);
        }

        void Attach(wi::gui::GUI& gui)
        {
            if (attached_) return;
            gui.AddWidget(&levelName_);
            gui.AddWidget(&newLevel_);
            gui.AddWidget(&existingLevel_);
            gui.AddWidget(&openLevel_);
            attached_ = true;
        }

        void SetLayout(const float width, const float height)
        {
            width_ = width;
            height_ = height;
            const float y = 88.0f;
            const float x = std::clamp(width * 0.30f, 250.0f, 560.0f);
            levelName_.SetPos(XMFLOAT2(x, y));
            levelName_.SetSize(XMFLOAT2(220.0f, 27.0f));
            newLevel_.SetPos(XMFLOAT2(x + 230.0f, y));
            newLevel_.SetSize(XMFLOAT2(108.0f, 27.0f));
            existingLevel_.SetPos(XMFLOAT2(x + 346.0f, y));
            existingLevel_.SetSize(XMFLOAT2(124.0f, 27.0f));
            openLevel_.SetPos(XMFLOAT2(x + 478.0f, y));
            openLevel_.SetSize(XMFLOAT2(108.0f, 27.0f));
        }

        void SetActive(const bool active)
        {
            active_ = active;
            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&levelName_),
                static_cast<wi::gui::Widget*>(&newLevel_),
                static_cast<wi::gui::Widget*>(&existingLevel_),
                static_cast<wi::gui::Widget*>(&openLevel_)})
            {
                widget->SetVisible(active);
                widget->SetEnabled(active);
            }
            openLevel_.SetEnabled(active && !selectedLevelNodeId_.empty());
        }

        void SetSelectedLevelNode(bridge::StableId nodeId)
        {
            selectedLevelNodeId_ = std::move(nodeId);
            openLevel_.SetEnabled(active_ && !selectedLevelNodeId_.empty());
        }

        void OnAddNew(std::function<void(const std::string&)> callback)
        {
            addNew_ = std::move(callback);
        }

        void OnAddExisting(std::function<void(const std::string&)> callback)
        {
            addExisting_ = std::move(callback);
        }

        void OnOpen(std::function<void(const bridge::StableId&)> callback)
        {
            open_ = std::move(callback);
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStoryFlowLevelPanel";
        }

    private:
        RenegadeTextInputField levelName_;
        RenegadeButton newLevel_;
        RenegadeButton existingLevel_;
        RenegadeButton openLevel_;
        std::string levelNameDraft_;
        bridge::StableId selectedLevelNodeId_;
        std::function<void(const std::string&)> addNew_;
        std::function<void(const std::string&)> addExisting_;
        std::function<void(const bridge::StableId&)> open_;
        float width_ = 1.0f;
        float height_ = 1.0f;
        bool attached_ = false;
        bool active_ = false;
    };
}
