#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <utility>

#include <WickedEngine.h>

#include "RenegadeStudioChrome.h"
#include "RenegadeStoryFlowWorkspace.h"
#include "renegade/bridge/StoryFlowAuthoringModel.h"
#include "renegade/bridge/StoryFlowAuthoringSession.h"

namespace renegade::studio
{
    // Gate 3D keeps condition editing on the same native Story Flow surface
    // while preserving the semantic boundary: edits are applied only through
    // StoryFlowAuthoringSession::UpdateRoute(), never by mutating FlowDocument
    // in place. The panel intentionally owns no layout or runtime state.
    class RenegadeStoryFlowConditionEditor final : public wi::gui::Widget
    {
    public:
        void Create()
        {
            SetName("Renegade Story Flow route condition editor");
            SetShadowRadius(0.0f);

            previousButton_.Create("Previous Story Flow condition");
            previousButton_.SetText("<");
            previousButton_.SetTooltip("Select the previous condition on this route.");
            previousButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                StepCondition(-1);
            });

            nextButton_.Create("Next Story Flow condition");
            nextButton_.SetText(">");
            nextButton_.SetTooltip("Select the next condition on this route.");
            nextButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                StepCondition(1);
            });

            addButton_.Create("Add Story Flow condition");
            addButton_.SetText("+ CONDITION");
            addButton_.SetTooltip(
                "Draft a new route condition. It is not committed until APPLY CONDITION succeeds.");
            addButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                BeginNewCondition();
            });

            keyInput_.Create("Story Flow condition key");
            keyInput_.SetDescription("KEY  ");
            keyInput_.SetPlaceholder("quest.complete / player.has_key...");
            keyInput_.SetCancelInputEnabled(false);

            operatorCombo_.Create("Story Flow condition operator");
            operatorCombo_.AddItem("EQUALS", 0);
            operatorCombo_.AddItem("NOT EQUALS", 1);
            operatorCombo_.AddItem("EXISTS", 2);
            operatorCombo_.AddItem("MISSING", 3);
            operatorCombo_.SetSelectedWithoutCallback(0);
            operatorCombo_.OnSelect([this](const wi::gui::EventArgs& args)
            {
                draftOperation_ = OperationFromUserData(args.userdata);
                SyncValueControlState();
            });

            valueInput_.Create("Story Flow condition value");
            valueInput_.SetDescription("VALUE  ");
            valueInput_.SetPlaceholder("true / opened / 3...");
            valueInput_.SetCancelInputEnabled(false);
            valueInput_.OnInputAccepted([this](const wi::gui::EventArgs&)
            {
                ApplyCondition();
            });

            applyButton_.Create("Apply Story Flow condition");
            applyButton_.SetText("APPLY CONDITION");
            applyButton_.SetTooltip(
                "Validate the complete Flow candidate and commit this condition to Story Flow history.");
            applyButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                ApplyCondition();
            });

            deleteButton_.Create("Delete Story Flow condition");
            deleteButton_.SetText("DELETE CONDITION");
            deleteButton_.SetTooltip(
                "Delete the selected committed route condition. A new uncommitted draft is simply cancelled.");
            deleteButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                DeleteCondition();
            });

            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&previousButton_),
                static_cast<wi::gui::Widget*>(&nextButton_),
                static_cast<wi::gui::Widget*>(&addButton_),
                static_cast<wi::gui::Widget*>(&keyInput_),
                static_cast<wi::gui::Widget*>(&operatorCombo_),
                static_cast<wi::gui::Widget*>(&valueInput_),
                static_cast<wi::gui::Widget*>(&applyButton_),
                static_cast<wi::gui::Widget*>(&deleteButton_)})
            {
                widget->SetShadowRadius(0.0f);
                widget->SetVisible(false);
            }

            created_ = true;
            SetLayout(width_, height_);
        }

        void Bind(
            bridge::StoryFlowAuthoringSession* session,
            bridge::StoryFlowAuthoringModel* model,
            RenegadeStoryFlowWorkspace* workspace)
        {
            session_ = session;
            model_ = model;
            workspace_ = workspace;
            activeRouteId_.clear();
            selectedConditionIndex_ = 0;
            editingNewCondition_ = false;
            observedUndoCount_ = InvalidCount;
            observedRedoCount_ = InvalidCount;
            statusMessage_ = "SELECT A ROUTE";
            RefreshFromAuthoritativeRoute(true);
        }

        void Clear() noexcept
        {
            session_ = nullptr;
            model_ = nullptr;
            workspace_ = nullptr;
            activeRouteId_.clear();
            selectedConditionIndex_ = 0;
            editingNewCondition_ = false;
            observedUndoCount_ = InvalidCount;
            observedRedoCount_ = InvalidCount;
            statusMessage_ = "SELECT A ROUTE";
            HideControls();
        }

        void SetLayout(const float width, const float height)
        {
            width_ = std::max(1.0f, width);
            height_ = std::max(1.0f, height);

            const float inspectorWidth = std::min(320.0f, width_ * 0.42f);
            panelWidth_ = std::max(120.0f, inspectorWidth - 20.0f);
            panelHeight_ = std::min(206.0f, std::max(152.0f, height_ * 0.29f));
            panelX_ = width_ - inspectorWidth + 10.0f;
            panelY_ = std::max(58.0f, height_ - panelHeight_ - 36.0f);
            SetPos(XMFLOAT2(panelX_, panelY_));
            SetSize(XMFLOAT2(panelWidth_, panelHeight_));
            LayoutControls();
        }

        void Update(const wi::Canvas& canvas, const float dt) override
        {
            Widget::Update(canvas, dt);
            if (!created_ || !IsVisible() || !IsEnabled())
            {
                HideControls();
                return;
            }

            const bridge::StableId selectedRoute =
                workspace_ ? workspace_->SelectedRouteId() : bridge::StableId{};
            const bool historyChanged = session_ && session_->IsLoaded() &&
                (session_->UndoCount() != observedUndoCount_ ||
                 session_->RedoCount() != observedRedoCount_);
            if (selectedRoute != activeRouteId_ || historyChanged)
            {
                // Any authoritative history movement cancels an uncommitted
                // condition draft rather than silently reapplying stale input.
                editingNewCondition_ = false;
                activeRouteId_ = selectedRoute;
                RefreshFromAuthoritativeRoute(true);
            }

            const auto* route = ActiveRoute();
            if (!route)
            {
                HideControls();
                return;
            }

            LayoutControls();
            ShowControls();

            const bool hasCommittedCondition =
                !editingNewCondition_ && selectedConditionIndex_ < route->conditions.size();
            previousButton_.SetEnabled(hasCommittedCondition && selectedConditionIndex_ > 0);
            nextButton_.SetEnabled(
                hasCommittedCondition && selectedConditionIndex_ + 1 < route->conditions.size());
            addButton_.SetEnabled(!editingNewCondition_);
            deleteButton_.SetEnabled(editingNewCondition_ || hasCommittedCondition);
            SyncValueControlState();

            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&previousButton_),
                static_cast<wi::gui::Widget*>(&nextButton_),
                static_cast<wi::gui::Widget*>(&addButton_),
                static_cast<wi::gui::Widget*>(&keyInput_),
                static_cast<wi::gui::Widget*>(&operatorCombo_),
                static_cast<wi::gui::Widget*>(&valueInput_),
                static_cast<wi::gui::Widget*>(&applyButton_),
                static_cast<wi::gui::Widget*>(&deleteButton_)})
            {
                if (widget->IsVisible())
                    widget->Update(canvas, dt);
            }
        }

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            if (!created_ || !IsVisible() || !ActiveRoute())
                return;

            DrawRect(panelX_, panelY_, panelWidth_, panelHeight_,
                wi::Color(6, 10, 13, 252), cmd);
            DrawBorder(panelX_, panelY_, panelWidth_, panelHeight_,
                wi::Color(52, 67, 76, 255), cmd);

            DrawLabel("ROUTE CONDITIONS", panelX_ + 10.0f, panelY_ + 8.0f,
                9, wi::Color(244, 244, 244, 255), cmd);
            DrawLabel(ConditionCounterLabel(), panelX_ + 126.0f, panelY_ + 8.0f,
                8, wi::Color(210, 91, 29, 255), cmd);

            for (const wi::gui::Widget* widget : {
                static_cast<const wi::gui::Widget*>(&previousButton_),
                static_cast<const wi::gui::Widget*>(&nextButton_),
                static_cast<const wi::gui::Widget*>(&addButton_),
                static_cast<const wi::gui::Widget*>(&keyInput_),
                static_cast<const wi::gui::Widget*>(&operatorCombo_),
                static_cast<const wi::gui::Widget*>(&valueInput_),
                static_cast<const wi::gui::Widget*>(&applyButton_),
                static_cast<const wi::gui::Widget*>(&deleteButton_)})
            {
                if (widget->IsVisible())
                    widget->Render(canvas, cmd);
            }

            DrawLabel(
                Shorten(statusMessage_, 45),
                panelX_ + 10.0f,
                panelY_ + panelHeight_ - 18.0f,
                7,
                IsErrorStatus(statusMessage_)
                    ? wi::Color(229, 92, 92, 255)
                    : wi::Color(132, 143, 149, 255),
                cmd);
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStoryFlowConditionEditor";
        }

    private:
        static constexpr std::size_t InvalidCount = static_cast<std::size_t>(-1);

        [[nodiscard]] const bridge::FlowRoute* ActiveRoute() const noexcept
        {
            if (!session_ || !session_->IsLoaded() || activeRouteId_.empty())
                return nullptr;
            const auto& routes = session_->Document().routes;
            const auto found = std::find_if(
                routes.begin(),
                routes.end(),
                [this](const bridge::FlowRoute& route)
                {
                    return route.id == activeRouteId_;
                });
            return found == routes.end() ? nullptr : &*found;
        }

        static bridge::FlowConditionOperator OperationFromUserData(
            const std::uint64_t value) noexcept
        {
            switch (value)
            {
            case 1: return bridge::FlowConditionOperator::NotEquals;
            case 2: return bridge::FlowConditionOperator::Exists;
            case 3: return bridge::FlowConditionOperator::Missing;
            default: return bridge::FlowConditionOperator::Equals;
            }
        }

        static int ComboIndexForOperation(
            const bridge::FlowConditionOperator operation) noexcept
        {
            switch (operation)
            {
            case bridge::FlowConditionOperator::NotEquals: return 1;
            case bridge::FlowConditionOperator::Exists: return 2;
            case bridge::FlowConditionOperator::Missing: return 3;
            default: return 0;
            }
        }

        static std::string Trim(std::string value)
        {
            const auto whitespace = [](const unsigned char c)
            {
                return std::isspace(c) != 0;
            };
            while (!value.empty() && whitespace(value.front()))
                value.erase(value.begin());
            while (!value.empty() && whitespace(value.back()))
                value.pop_back();
            return value;
        }

        static std::string Shorten(std::string value, const std::size_t maximum)
        {
            if (value.size() <= maximum)
                return value;
            if (maximum <= 3)
                return value.substr(0, maximum);
            value.resize(maximum - 3);
            value += "...";
            return value;
        }

        static bool IsErrorStatus(const std::string& value)
        {
            return value.find("REJECTED") != std::string::npos ||
                value.find("FAILED") != std::string::npos ||
                value.find("ERROR") != std::string::npos;
        }

        static void DrawRect(
            const float x,
            const float y,
            const float width,
            const float height,
            const wi::Color color,
            const wi::graphics::CommandList cmd)
        {
            if (width <= 0.0f || height <= 0.0f)
                return;
            wi::image::Params params(x, y, width, height, color);
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            wi::image::Draw(nullptr, params, cmd);
        }

        static void DrawBorder(
            const float x,
            const float y,
            const float width,
            const float height,
            const wi::Color color,
            const wi::graphics::CommandList cmd)
        {
            DrawRect(x, y, width, 1.0f, color, cmd);
            DrawRect(x, y + height - 1.0f, width, 1.0f, color, cmd);
            DrawRect(x, y, 1.0f, height, color, cmd);
            DrawRect(x + width - 1.0f, y, 1.0f, height, color, cmd);
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
                x,
                y,
                size,
                wi::font::WIFALIGN_LEFT,
                wi::font::WIFALIGN_TOP,
                color,
                wi::Color::Transparent());
            params.bolden = 0.12f;
            wi::font::Draw(value, params, cmd);
        }

        void LayoutControls()
        {
            const float left = panelX_ + 10.0f;
            const float innerWidth = std::max(80.0f, panelWidth_ - 20.0f);
            const float navY = panelY_ + 25.0f;
            previousButton_.SetPos(XMFLOAT2(left, navY));
            previousButton_.SetSize(XMFLOAT2(28.0f, 24.0f));
            nextButton_.SetPos(XMFLOAT2(left + 33.0f, navY));
            nextButton_.SetSize(XMFLOAT2(28.0f, 24.0f));
            addButton_.SetPos(XMFLOAT2(left + 68.0f, navY));
            addButton_.SetSize(XMFLOAT2(innerWidth - 68.0f, 24.0f));

            const float keyY = navY + 31.0f;
            keyInput_.SetPos(XMFLOAT2(left, keyY));
            keyInput_.SetSize(XMFLOAT2(innerWidth, 27.0f));

            const float opY = keyY + 34.0f;
            const float opWidth = std::min(112.0f, innerWidth * 0.39f);
            operatorCombo_.SetPos(XMFLOAT2(left, opY));
            operatorCombo_.SetSize(XMFLOAT2(opWidth, 27.0f));
            valueInput_.SetPos(XMFLOAT2(left + opWidth + 6.0f, opY));
            valueInput_.SetSize(XMFLOAT2(innerWidth - opWidth - 6.0f, 27.0f));

            const float actionY = opY + 34.0f;
            applyButton_.SetPos(XMFLOAT2(left, actionY));
            applyButton_.SetSize(XMFLOAT2(innerWidth * 0.55f - 3.0f, 27.0f));
            deleteButton_.SetPos(XMFLOAT2(left + innerWidth * 0.55f + 3.0f, actionY));
            deleteButton_.SetSize(XMFLOAT2(innerWidth * 0.45f - 3.0f, 27.0f));
        }

        void ShowControls()
        {
            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&previousButton_),
                static_cast<wi::gui::Widget*>(&nextButton_),
                static_cast<wi::gui::Widget*>(&addButton_),
                static_cast<wi::gui::Widget*>(&keyInput_),
                static_cast<wi::gui::Widget*>(&operatorCombo_),
                static_cast<wi::gui::Widget*>(&valueInput_),
                static_cast<wi::gui::Widget*>(&applyButton_),
                static_cast<wi::gui::Widget*>(&deleteButton_)})
            {
                widget->SetVisible(true);
            }
        }

        void HideControls() noexcept
        {
            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&previousButton_),
                static_cast<wi::gui::Widget*>(&nextButton_),
                static_cast<wi::gui::Widget*>(&addButton_),
                static_cast<wi::gui::Widget*>(&keyInput_),
                static_cast<wi::gui::Widget*>(&operatorCombo_),
                static_cast<wi::gui::Widget*>(&valueInput_),
                static_cast<wi::gui::Widget*>(&applyButton_),
                static_cast<wi::gui::Widget*>(&deleteButton_)})
            {
                widget->SetVisible(false);
            }
        }

        void SyncValueControlState()
        {
            const bool valueRequired =
                draftOperation_ == bridge::FlowConditionOperator::Equals ||
                draftOperation_ == bridge::FlowConditionOperator::NotEquals;
            valueInput_.SetEnabled(valueRequired);
            if (!valueRequired)
                valueInput_.SetValue("");
        }

        void RefreshFromAuthoritativeRoute(const bool force)
        {
            if (!session_ || !session_->IsLoaded())
            {
                HideControls();
                return;
            }

            observedUndoCount_ = session_->UndoCount();
            observedRedoCount_ = session_->RedoCount();
            const auto* route = ActiveRoute();
            if (!route)
            {
                selectedConditionIndex_ = 0;
                editingNewCondition_ = false;
                statusMessage_ = "SELECT A ROUTE";
                HideControls();
                return;
            }

            if (!force && editingNewCondition_)
                return;

            editingNewCondition_ = false;
            if (route->conditions.empty())
            {
                selectedConditionIndex_ = 0;
                keyInput_.SetValue("");
                draftOperation_ = bridge::FlowConditionOperator::Equals;
                operatorCombo_.SetSelectedWithoutCallback(0);
                valueInput_.SetValue("");
                statusMessage_ = "NO CONDITIONS // ADD ONE WHEN ROUTING DEPENDS ON STATE";
                SyncValueControlState();
                return;
            }

            selectedConditionIndex_ = std::min(
                selectedConditionIndex_, route->conditions.size() - 1);
            LoadCondition(route->conditions[selectedConditionIndex_]);
            statusMessage_ = "CONDITION LOADED // EDITS REQUIRE APPLY";
        }

        void LoadCondition(const bridge::FlowCondition& condition)
        {
            keyInput_.SetValue(condition.key);
            draftOperation_ = condition.operation;
            operatorCombo_.SetSelectedWithoutCallback(
                ComboIndexForOperation(condition.operation));
            valueInput_.SetValue(condition.value);
            SyncValueControlState();
        }

        void StepCondition(const int direction)
        {
            const auto* route = ActiveRoute();
            if (!route || route->conditions.empty())
                return;
            editingNewCondition_ = false;
            if (direction < 0 && selectedConditionIndex_ > 0)
                --selectedConditionIndex_;
            else if (direction > 0 && selectedConditionIndex_ + 1 < route->conditions.size())
                ++selectedConditionIndex_;
            LoadCondition(route->conditions[selectedConditionIndex_]);
            statusMessage_ = "CONDITION " + std::to_string(selectedConditionIndex_ + 1) +
                " OF " + std::to_string(route->conditions.size());
        }

        void BeginNewCondition()
        {
            if (!ActiveRoute())
                return;
            editingNewCondition_ = true;
            keyInput_.SetValue("");
            draftOperation_ = bridge::FlowConditionOperator::Equals;
            operatorCombo_.SetSelectedWithoutCallback(0);
            valueInput_.SetValue("");
            SyncValueControlState();
            statusMessage_ = "NEW CONDITION DRAFT // NOT YET IN FLOW HISTORY";
        }

        void ApplyCondition()
        {
            if (!session_ || !model_ || activeRouteId_.empty())
                return;
            const auto* current = ActiveRoute();
            if (!current)
                return;

            bridge::FlowCondition condition;
            condition.key = Trim(keyInput_.GetValue());
            condition.operation = draftOperation_;
            if (condition.operation == bridge::FlowConditionOperator::Equals ||
                condition.operation == bridge::FlowConditionOperator::NotEquals)
            {
                condition.value = Trim(valueInput_.GetValue());
            }

            bridge::FlowRoute candidate = *current;
            std::size_t resultingIndex = selectedConditionIndex_;
            if (editingNewCondition_)
            {
                candidate.conditions.push_back(std::move(condition));
                resultingIndex = candidate.conditions.size() - 1;
            }
            else if (selectedConditionIndex_ < candidate.conditions.size())
            {
                candidate.conditions[selectedConditionIndex_] = std::move(condition);
            }
            else
            {
                statusMessage_ = "CONDITION EDIT REJECTED // SELECTION IS STALE";
                RefreshFromAuthoritativeRoute(true);
                return;
            }

            std::string error;
            if (!session_->UpdateRoute(activeRouteId_, std::move(candidate), error))
            {
                statusMessage_ = "CONDITION EDIT REJECTED // " + error;
                RefreshFromAuthoritativeRoute(true);
                return;
            }
            if (!model_->Load(session_->Document(), session_->ProjectId(), error))
            {
                statusMessage_ = "CONDITION MODEL REFRESH FAILED // " + error;
                return;
            }

            selectedConditionIndex_ = resultingIndex;
            editingNewCondition_ = false;
            observedUndoCount_ = session_->UndoCount();
            observedRedoCount_ = session_->RedoCount();
            RefreshFromAuthoritativeRoute(true);
            statusMessage_ = "CONDITION COMMITTED // UNSAVED FLOW CHANGE";
        }

        void DeleteCondition()
        {
            if (editingNewCondition_)
            {
                editingNewCondition_ = false;
                RefreshFromAuthoritativeRoute(true);
                statusMessage_ = "NEW CONDITION DRAFT CANCELLED";
                return;
            }
            if (!session_ || !model_ || activeRouteId_.empty())
                return;
            const auto* current = ActiveRoute();
            if (!current || selectedConditionIndex_ >= current->conditions.size())
                return;

            bridge::FlowRoute candidate = *current;
            candidate.conditions.erase(
                candidate.conditions.begin() +
                    static_cast<std::ptrdiff_t>(selectedConditionIndex_));

            std::string error;
            if (!session_->UpdateRoute(activeRouteId_, std::move(candidate), error))
            {
                statusMessage_ = "CONDITION DELETE REJECTED // " + error;
                RefreshFromAuthoritativeRoute(true);
                return;
            }
            if (!model_->Load(session_->Document(), session_->ProjectId(), error))
            {
                statusMessage_ = "CONDITION MODEL REFRESH FAILED // " + error;
                return;
            }

            const auto* updated = ActiveRoute();
            if (!updated || updated->conditions.empty())
                selectedConditionIndex_ = 0;
            else
                selectedConditionIndex_ = std::min(
                    selectedConditionIndex_, updated->conditions.size() - 1);
            observedUndoCount_ = session_->UndoCount();
            observedRedoCount_ = session_->RedoCount();
            RefreshFromAuthoritativeRoute(true);
            statusMessage_ = "CONDITION DELETED // UNSAVED FLOW CHANGE";
        }

        [[nodiscard]] std::string ConditionCounterLabel() const
        {
            const auto* route = ActiveRoute();
            if (!route)
                return {};
            if (editingNewCondition_)
                return "NEW // " + std::to_string(route->conditions.size()) + " COMMITTED";
            if (route->conditions.empty())
                return "NONE";
            return std::to_string(selectedConditionIndex_ + 1) + " / " +
                std::to_string(route->conditions.size());
        }

        bridge::StoryFlowAuthoringSession* session_ = nullptr;
        bridge::StoryFlowAuthoringModel* model_ = nullptr;
        RenegadeStoryFlowWorkspace* workspace_ = nullptr;
        bridge::StableId activeRouteId_;
        std::size_t selectedConditionIndex_ = 0;
        std::size_t observedUndoCount_ = InvalidCount;
        std::size_t observedRedoCount_ = InvalidCount;
        bool editingNewCondition_ = false;
        bool created_ = false;
        bridge::FlowConditionOperator draftOperation_ =
            bridge::FlowConditionOperator::Equals;
        std::string statusMessage_ = "SELECT A ROUTE";
        float width_ = 1.0f;
        float height_ = 1.0f;
        float panelX_ = 0.0f;
        float panelY_ = 0.0f;
        float panelWidth_ = 1.0f;
        float panelHeight_ = 1.0f;

        RenegadeButton previousButton_;
        RenegadeButton nextButton_;
        RenegadeButton addButton_;
        RenegadeTextInputField keyInput_;
        RenegadeComboBox operatorCombo_;
        RenegadeTextInputField valueInput_;
        RenegadeButton applyButton_;
        RenegadeButton deleteButton_;
    };
}
