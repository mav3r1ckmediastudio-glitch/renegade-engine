#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

#include <WickedEngine.h>

#include "RenegadeStoryFlowConditionEditor.h"
#include "RenegadeStoryFlowGraphEditor.h"
#include "RenegadeStoryFlowJourneyChrome.h"
#include "RenegadeStoryFlowWorkspace.h"
#include "StoryFlowGuiLayerPolicy.h"

namespace renegade::studio
{
    // Native wiGUI host for the ImNodes Graph canvas. Graph rendering and input
    // live in one stable GUI layer; the shared workspace never interprets Graph
    // canvas gestures underneath it.
    class RenegadeStoryFlowGraphLayer final : public wi::gui::Widget
    {
    public:
        void Create(RenegadeStoryFlowGraphEditor* editor)
        {
            editor_ = editor;
            SetName("Story Flow Graph canvas");
            SetShadowRadius(0.0f);
            SetVisible(false);
            SetEnabled(false);
        }

        void SetViewport(const XMFLOAT4& bounds)
        {
            bounds_ = bounds;
            SetPos(XMFLOAT2(bounds.x, bounds.y));
            SetSize(XMFLOAT2(
                std::max(1.0f, bounds.z),
                std::max(1.0f, bounds.w)));
        }

        [[nodiscard]] bool OwnsPointer() const noexcept
        {
            return IsVisible() && IsEnabled() && GetState() > wi::gui::IDLE;
        }

        void Update(const wi::Canvas& canvas, const float dt) override
        {
            wi::gui::Widget::Update(canvas, dt);
            if (!IsVisible() || !IsEnabled() || force_disable)
            {
                state = wi::gui::IDLE;
                return;
            }

            const XMFLOAT4 pointer = wi::input::GetPointer();
            const bool inside = pointer.x >= bounds_.x &&
                pointer.y >= bounds_.y &&
                pointer.x < bounds_.x + bounds_.z &&
                pointer.y < bounds_.y + bounds_.w;
            if (!inside)
            {
                state = wi::gui::IDLE;
                return;
            }

            const bool mouseDown =
                wi::input::Down(wi::input::MOUSE_BUTTON_LEFT) ||
                wi::input::Down(wi::input::MOUSE_BUTTON_RIGHT) ||
                wi::input::Down(wi::input::MOUSE_BUTTON_MIDDLE);
            state = mouseDown ? wi::gui::ACTIVE : wi::gui::FOCUS;
        }

        void Render(
            const wi::Canvas&,
            const wi::graphics::CommandList cmd) const override
        {
            if (!IsVisible())
                return;

            wi::image::Params background(
                translation.x,
                translation.y,
                scale.x,
                scale.y,
                wi::Color(8, 12, 16, 255));
            background.blendFlag = wi::enums::BLENDMODE_OPAQUE;
            wi::image::Draw(nullptr, background, cmd);

            if (editor_)
            {
                editor_->Render(cmd);
                editor_->RenderOverview(cmd);
            }
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStoryFlowGraphLayer";
        }

    private:
        RenegadeStoryFlowGraphEditor* editor_ = nullptr;
        XMFLOAT4 bounds_ = {};
    };

    // Story Flow is a first-class 2D RenderPath. Journey remains the native
    // card/reel presentation. Graph has exactly one canvas implementation:
    // ImNodes hosted by RenegadeStoryFlowGraphLayer. Lifecycle/header/search
    // controls are native wiGUI objects above both presentations.
    class RenegadeStoryFlowRenderPath final : public wi::RenderPath2D
    {
    public:
        ~RenegadeStoryFlowRenderPath() override
        {
            if (loaded_)
            {
                GetGUI().RemoveWidget(&deleteSelectionButton_);
                GetGUI().RemoveWidget(&findButton_);
                GetGUI().RemoveWidget(&findInput_);
                GetGUI().RemoveWidget(&startButton_);
                GetGUI().RemoveWidget(&fitButton_);
                GetGUI().RemoveWidget(&graphViewButton_);
                GetGUI().RemoveWidget(&journeyViewButton_);
                GetGUI().RemoveWidget(&conditionEditor_);
                GetGUI().RemoveWidget(&journeyChrome_);
                GetGUI().RemoveWidget(&graphLayer_);
                GetGUI().RemoveWidget(&workspace_);
            }
        }

        void EnsureLoaded()
        {
            if (loaded_)
                return;

            workspace_.Create();
            workspace_.SetVisible(false);
            workspace_.SetEnabled(false);

            graphLayer_.Create(&graphEditor_);

            journeyChrome_.Create();
            journeyChrome_.SetVisible(false);
            journeyChrome_.SetEnabled(false);

            conditionEditor_.Create();
            conditionEditor_.SetVisible(false);
            conditionEditor_.SetEnabled(false);

            journeyViewButton_.Create("Story Flow Journey View");
            journeyViewButton_.SetText("JOURNEY");
            journeyViewButton_.SetTooltip("Show the high-level player journey presentation.");
            journeyViewButton_.SetShadowRadius(0.0f);
            journeyViewButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                graphEditor_.ResetTransientInteractionState();
                workspace_.ActivateView(bridge::StoryFlowViewMode::Journey);
                graphInputBlockedLastFrame_ = false;
            });

            graphViewButton_.Create("Story Flow Graph View");
            graphViewButton_.SetText("GRAPH");
            graphViewButton_.SetTooltip("Show exact Story Flow topology and route wiring.");
            graphViewButton_.SetShadowRadius(0.0f);
            graphViewButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                workspace_.ActivateView(bridge::StoryFlowViewMode::Graph);
                graphInputBlockedLastFrame_ = false;
            });

            fitButton_.Create("Story Flow Fit");
            fitButton_.SetText("FIT");
            fitButton_.SetTooltip("Frame all content in the active Story Flow view.");
            fitButton_.SetShadowRadius(0.0f);
            fitButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                FitToContent();
            });

            startButton_.Create("Story Flow Start");
            startButton_.SetText("START");
            startButton_.SetTooltip("Center the permanent Game Start destination.");
            startButton_.SetShadowRadius(0.0f);
            startButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                CenterOnGameStart();
            });

            findInput_.Create("Story Flow Find Node");
            findInput_.SetDescription("FIND  ");
            findInput_.SetPlaceholder("Node name...");
            findInput_.SetCancelInputEnabled(false);
            findInput_.SetShadowRadius(0.0f);
            findInput_.OnInput([this](const wi::gui::EventArgs& args)
            {
                findDraft_ = args.sValue;
            });
            findInput_.OnInputAccepted([this](const wi::gui::EventArgs& args)
            {
                findDraft_ = args.sValue;
                RunFind();
            });

            findButton_.Create("Story Flow Find");
            findButton_.SetText("FIND");
            findButton_.SetTooltip("Focus the first exact or partial Story Flow node-name match.");
            findButton_.SetShadowRadius(0.0f);
            findButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                RunFind();
            });

            deleteSelectionButton_.Create("Story Flow Delete Selection");
            deleteSelectionButton_.SetText("DELETE");
            deleteSelectionButton_.SetTooltip(
                "Delete the selected Graph route or non-Game-Start node. Delete also works from the keyboard in Graph View.");
            deleteSelectionButton_.SetShadowRadius(0.0f);
            deleteSelectionButton_.SetVisible(false);
            deleteSelectionButton_.SetEnabled(false);
            deleteSelectionButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                workspace_.DeleteSelection();
            });

            // Wicked updates in registration order and renders in reverse. Keep
            // modal/native commands at the front; background presentation layers
            // are moved behind lifecycle controls once those controls attach.
            GetGUI().AddWidget(&deleteSelectionButton_);
            GetGUI().AddWidget(&conditionEditor_);
            GetGUI().AddWidget(&findButton_);
            GetGUI().AddWidget(&findInput_);
            GetGUI().AddWidget(&startButton_);
            GetGUI().AddWidget(&fitButton_);
            GetGUI().AddWidget(&graphViewButton_);
            GetGUI().AddWidget(&journeyViewButton_);
            GetGUI().AddWidget(&journeyChrome_);
            GetGUI().AddWidget(&graphLayer_);
            GetGUI().AddWidget(&workspace_);

            SetNativeControlsActive(false);
            loaded_ = true;
            LayoutWorkspace();
        }

        void Load() override
        {
            EnsureLoaded();
        }

        void Start() override
        {
            EnsureLoaded();
            LayoutWorkspace();
            const bool graphActive = workspaceActive_ &&
                workspace_.ActiveView() == bridge::StoryFlowViewMode::Graph;
            workspace_.SetVisible(workspaceActive_);
            workspace_.SetEnabled(workspaceActive_ && !graphActive);
            journeyChrome_.SetVisible(workspaceActive_);
            conditionEditor_.SetVisible(workspaceActive_);
            conditionEditor_.SetEnabled(workspaceActive_);
            graphLayer_.SetVisible(graphActive);
            graphLayer_.SetEnabled(graphActive);
            SetNativeControlsActive(workspaceActive_);
            UpdateDeleteControl(graphActive);
        }

        void Stop() override
        {
            if (!loaded_)
                return;

            graphEditor_.ResetTransientInteractionState();
            graphInputBlockedLastFrame_ = false;
            SetNativeControlsActive(false);
            deleteSelectionButton_.SetVisible(false);
            deleteSelectionButton_.SetEnabled(false);
            conditionEditor_.SetVisible(false);
            conditionEditor_.SetEnabled(false);
            journeyChrome_.SetVisible(false);
            graphLayer_.SetVisible(false);
            graphLayer_.SetEnabled(false);
            workspace_.SetVisible(false);
            workspace_.SetEnabled(false);
        }

        void ResizeLayout() override
        {
            wi::RenderPath2D::ResizeLayout();
            LayoutWorkspace();
        }

        void Update(const float dt) override
        {
            EnsureLoaded();
            LayoutWorkspace();

            const bool graphBeforeUpdate = workspaceActive_ &&
                workspace_.ActiveView() == bridge::StoryFlowViewMode::Graph;
            graphLayer_.SetVisible(graphBeforeUpdate);
            graphLayer_.SetEnabled(graphBeforeUpdate);
            workspace_.SetEnabled(workspaceActive_ && !graphBeforeUpdate);
            SetNativeControlsActive(workspaceActive_);
            UpdateDeleteControl(graphBeforeUpdate);

            // One GUI update. In Graph View the shared workspace remains visible
            // for its header/Inspector rendering but is disabled as a canvas
            // interaction surface. This removes the old Graph hit-test/drag/
            // connect path from runtime ownership instead of trying to arbitrate
            // two canvas implementations by pointer position.
            wi::RenderPath2D::Update(dt);

            const bool graphActive = workspaceActive_ &&
                workspace_.ActiveView() == bridge::StoryFlowViewMode::Graph;
            graphLayer_.SetVisible(graphActive);
            graphLayer_.SetEnabled(graphActive);
            workspace_.SetEnabled(workspaceActive_ && !graphActive);

            const XMFLOAT4 pointer = wi::input::GetPointer();
            const bool pointerInsideGraph = graphActive &&
                graphEditor_.ContainsPointer(pointer);
            const bool graphInputBlocked = pointerInsideGraph &&
                !graphLayer_.OwnsPointer();

            if (graphActive)
            {
                if (graphInputBlocked)
                {
                    // A higher native control (notably a lifecycle dropdown)
                    // owns the pointer. Keep the last Graph frame visible and
                    // terminate only transient ImNodes interaction state once.
                    if (!graphInputBlockedLastFrame_)
                        graphEditor_.ResetTransientInteractionState();
                }
                else
                {
                    graphEditor_.Update(*this, dt, true);
                    graphEditor_.ReconcileHostInteractions(!GetGUI().IsTyping());
                    graphEditor_.RecoverTransientInteractionIfIdle();
                }
            }
            else
            {
                if (graphBeforeUpdate)
                    graphEditor_.ResetTransientInteractionState();
                graphEditor_.Update(*this, dt, false);
            }

            graphInputBlockedLastFrame_ = graphInputBlocked;
            UpdateDeleteControl(graphActive);
        }

        void Compose(const wi::graphics::CommandList cmd) const override
        {
            wi::RenderPath2D::Compose(cmd);
        }

        void SyncCanvas(const wi::Canvas& canvas)
        {
            init(canvas);
            EnsureLoaded();
            LayoutWorkspace();
        }

        void Bind(
            bridge::StoryFlowAuthoringSession* session,
            bridge::StoryFlowAuthoringModel* model,
            bridge::StoryFlowLayoutDocument* layout)
        {
            EnsureLoaded();
            workspace_.Bind(session, model, layout);
            graphEditor_.Bind(session, model, layout, &workspace_);
            graphEditor_.ResetTransientInteractionState();
            conditionEditor_.Bind(session, model, &workspace_);
        }

        void Clear() noexcept
        {
            if (loaded_)
            {
                conditionEditor_.Clear();
                graphEditor_.Clear();
                workspace_.Clear();
                graphLayer_.SetVisible(false);
                graphLayer_.SetEnabled(false);
                deleteSelectionButton_.SetVisible(false);
                deleteSelectionButton_.SetEnabled(false);
                graphInputBlockedLastFrame_ = false;
                findDraft_.clear();
                findInput_.SetValue("");
            }
        }

        void FitToContent()
        {
            EnsureLoaded();
            if (workspace_.ActiveView() == bridge::StoryFlowViewMode::Graph)
                graphEditor_.FitToContent();
            else
                workspace_.FitToContent();
        }

        void CenterOnGameStart()
        {
            EnsureLoaded();
            if (workspace_.ActiveView() == bridge::StoryFlowViewMode::Graph)
                graphEditor_.CenterOnGameStart();
            else
                workspace_.CenterOnGameStart();
        }

        void SetWorkspaceActive(const bool active)
        {
            const bool wasActive = workspaceActive_;
            workspaceActive_ = active;
            if (!loaded_)
                return;

            if (wasActive && !active)
                graphEditor_.ResetTransientInteractionState();

            const bool graphActive = active &&
                workspace_.ActiveView() == bridge::StoryFlowViewMode::Graph;
            workspace_.SetVisible(active);
            workspace_.SetEnabled(active && !graphActive);
            journeyChrome_.SetVisible(active);
            conditionEditor_.SetVisible(active);
            conditionEditor_.SetEnabled(active);
            graphLayer_.SetVisible(graphActive);
            graphLayer_.SetEnabled(graphActive);
            SetNativeControlsActive(active);
            UpdateDeleteControl(graphActive);
            if (!active)
                graphInputBlockedLastFrame_ = false;
        }

        void OnLayoutChanged(std::function<void()> callback)
        {
            EnsureLoaded();
            workspace_.OnLayoutChanged(std::move(callback));
        }

        void OnSelectionChanged(
            std::function<void(const bridge::StableId&)> callback)
        {
            EnsureLoaded();
            workspace_.OnSelectionChanged(std::move(callback));
        }

        void OnSemanticChanged(std::function<void()> callback)
        {
            EnsureLoaded();
            workspace_.OnSemanticChanged(std::move(callback));
        }

        void OnScreenOutcomeQuery(
            RenegadeStoryFlowWorkspace::ScreenOutcomeQuery callback)
        {
            EnsureLoaded();
            graphEditor_.OnScreenOutcomeQuery(callback);
            workspace_.OnScreenOutcomeQuery(std::move(callback));
        }

        void SetExternalStatus(std::string message)
        {
            EnsureLoaded();
            workspace_.SetExternalStatus(std::move(message));
        }

        void OnNodeActivated(
            std::function<void(const bridge::StableId&)> callback)
        {
            EnsureLoaded();
            workspace_.OnNodeActivated(std::move(callback));
        }

        void SelectAndFocusNode(const bridge::StableId& nodeId)
        {
            EnsureLoaded();
            workspace_.SelectAndFocusNode(nodeId);
        }

        void PlaceWorkspaceBehindLifecycleControls()
        {
            EnsureLoaded();
            if (lifecycleLayeringReady_)
                return;

            auto& gui = GetGUI();
            // Native header/search/modal commands stay in front. Move only the
            // three presentation backgrounds behind lifecycle widgets once.
            gui.RemoveWidget(&journeyChrome_);
            gui.RemoveWidget(&graphLayer_);
            gui.RemoveWidget(&workspace_);
            gui.AddWidget(&journeyChrome_);
            gui.AddWidget(&graphLayer_);
            gui.AddWidget(&workspace_);
            lifecycleLayeringReady_ = true;
        }

        [[nodiscard]] bool IsLifecycleLayeringReady() const noexcept
        {
            return lifecycleLayeringReady_;
        }

        [[nodiscard]] const bridge::StableId& SelectedNodeId() const noexcept
        {
            return workspace_.SelectedNodeId();
        }

        [[nodiscard]] bool IsWorkspaceActive() const noexcept
        {
            return workspaceActive_;
        }

    private:
        void SetNativeControlsActive(const bool active)
        {
            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&journeyViewButton_),
                static_cast<wi::gui::Widget*>(&graphViewButton_),
                static_cast<wi::gui::Widget*>(&fitButton_),
                static_cast<wi::gui::Widget*>(&startButton_),
                static_cast<wi::gui::Widget*>(&findInput_),
                static_cast<wi::gui::Widget*>(&findButton_)})
            {
                widget->SetVisible(active);
                widget->SetEnabled(active);
            }
        }

        void UpdateDeleteControl(const bool graphActive)
        {
            const bool canDelete = workspaceActive_ && graphActive &&
                workspace_.CanDeleteSelection();
            deleteSelectionButton_.SetVisible(canDelete);
            deleteSelectionButton_.SetEnabled(canDelete);
        }

        void RunFind()
        {
            const std::string live = findInput_.GetCurrentInputValue();
            if (!live.empty())
                findDraft_ = live;
            (void)graphEditor_.FocusNodeByName(findDraft_);
        }

        void LayoutWorkspace()
        {
            if (!loaded_)
                return;

            const float width = std::max(1.0f, GetLogicalWidth());
            const float height = std::max(1.0f, GetLogicalHeight());
            if (std::abs(width - lastLogicalWidth_) < 0.01f &&
                std::abs(height - lastLogicalHeight_) < 0.01f)
            {
                return;
            }

            lastLogicalWidth_ = width;
            lastLogicalHeight_ = height;

            journeyChrome_.SetPos(XMFLOAT2(0.0f, 0.0f));
            journeyChrome_.SetLayout(width, height);

            const float workspaceLeft = journeyChrome_.WorkspaceLeftInset();
            const float workspaceWidth = std::max(1.0f, width - workspaceLeft);
            workspace_.SetPos(XMFLOAT2(workspaceLeft, 0.0f));
            workspace_.SetLayout(workspaceWidth, height);

            const float inspectorWidth = std::min(
                RenegadeStoryFlowJourneyChrome::PreferredInspectorWidth,
                workspaceWidth * 0.42f);
            const float graphWidth = std::max(1.0f, workspaceWidth - inspectorWidth);
            const XMFLOAT4 graphViewport(
                workspaceLeft,
                RenegadeStoryFlowJourneyChrome::WorkspaceHeaderHeight,
                graphWidth,
                std::max(
                    1.0f,
                    height - RenegadeStoryFlowJourneyChrome::WorkspaceHeaderHeight));
            graphEditor_.SetViewport(graphViewport);
            graphLayer_.SetViewport(graphViewport);

            journeyViewButton_.SetPos(XMFLOAT2(workspaceLeft + 112.0f, 10.0f));
            journeyViewButton_.SetSize(XMFLOAT2(58.0f, 28.0f));
            graphViewButton_.SetPos(XMFLOAT2(workspaceLeft + 174.0f, 10.0f));
            graphViewButton_.SetSize(XMFLOAT2(44.0f, 28.0f));

            // Canvas navigation lives in the existing lower-left navigation
            // host area, away from Level/Screen lifecycle controls in the two
            // header rows. This remains stable even at the 1280x720 owner size.
            const float canvasNavY = std::max(
                RenegadeStoryFlowJourneyChrome::WorkspaceHeaderHeight + 18.0f,
                height - 52.0f);
            fitButton_.SetPos(XMFLOAT2(workspaceLeft + 18.0f, canvasNavY));
            fitButton_.SetSize(XMFLOAT2(52.0f, 28.0f));
            startButton_.SetPos(XMFLOAT2(workspaceLeft + 76.0f, canvasNavY));
            startButton_.SetSize(XMFLOAT2(64.0f, 28.0f));

            const float inspectorX = workspaceLeft + graphWidth + 14.0f;
            const float findY = std::max(
                RenegadeStoryFlowJourneyChrome::WorkspaceHeaderHeight + 300.0f,
                height - 66.0f);
            const float findButtonWidth = 62.0f;
            const float findFieldWidth = std::max(
                94.0f,
                inspectorWidth - 28.0f - findButtonWidth - 6.0f);
            findInput_.SetPos(XMFLOAT2(inspectorX, findY));
            findInput_.SetSize(XMFLOAT2(findFieldWidth, 27.0f));
            findButton_.SetPos(XMFLOAT2(
                inspectorX + findFieldWidth + 6.0f, findY));
            findButton_.SetSize(XMFLOAT2(findButtonWidth, 27.0f));

            deleteSelectionButton_.SetPos(XMFLOAT2(
                workspaceLeft + graphWidth + std::max(0.0f, inspectorWidth - 88.0f),
                RenegadeStoryFlowJourneyChrome::WorkspaceHeaderHeight + 9.0f));
            deleteSelectionButton_.SetSize(XMFLOAT2(74.0f, 26.0f));

            conditionEditor_.SetLayout(width, height);
        }

        RenegadeStoryFlowWorkspace workspace_;
        RenegadeStoryFlowGraphEditor graphEditor_;
        RenegadeStoryFlowGraphLayer graphLayer_;
        RenegadeStoryFlowJourneyChrome journeyChrome_;
        RenegadeStoryFlowConditionEditor conditionEditor_;
        RenegadeButton journeyViewButton_;
        RenegadeButton graphViewButton_;
        RenegadeButton fitButton_;
        RenegadeButton startButton_;
        RenegadeTextInputField findInput_;
        RenegadeButton findButton_;
        RenegadeButton deleteSelectionButton_;
        std::string findDraft_;
        bool loaded_ = false;
        bool workspaceActive_ = false;
        bool lifecycleLayeringReady_ = false;
        bool graphInputBlockedLastFrame_ = false;
        float lastLogicalWidth_ = -1.0f;
        float lastLogicalHeight_ = -1.0f;
    };
}
