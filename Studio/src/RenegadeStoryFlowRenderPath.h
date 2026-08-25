#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

#include <WickedEngine.h>

#include "RenegadeStoryFlowConditionEditor.h"
#include "RenegadeStoryFlowDestinationComposer.h"
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
        enum class NativeCommand
        {
            None,
            Journey,
            Graph,
            Fit,
            Start,
            Find,
            DeleteSelection,
            Undo,
            Redo,
            ZoomOut,
            ZoomIn,
            Preview,
            Filter,
        };

    public:
        ~RenegadeStoryFlowRenderPath() override
        {
            if (loaded_)
            {
                destinationComposer_.Detach(GetGUI());
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
            journeyChrome_.OnAction(
                [this](const RenegadeStoryFlowJourneyChrome::Action action)
                {
                    using Action = RenegadeStoryFlowJourneyChrome::Action;
                    switch (action)
                    {
                    case Action::StoryFlow:
                    case Action::Select:
                        pendingNativeCommand_ = NativeCommand::Journey;
                        break;
                    case Action::Arrange:
                        pendingNativeCommand_ = NativeCommand::Fit;
                        break;
                    case Action::Search:
                        searchOpen_ = !searchOpen_;
                        break;
                    case Action::Validate:
                        workspace_.SetExternalStatus(
                            "JOURNEY VALIDATION // SEE INSPECTOR DIAGNOSTICS");
                        break;
                    case Action::Undo:
                        pendingNativeCommand_ = NativeCommand::Undo;
                        break;
                    case Action::Redo:
                        pendingNativeCommand_ = NativeCommand::Redo;
                        break;
                    case Action::ZoomOut:
                        pendingNativeCommand_ = NativeCommand::ZoomOut;
                        break;
                    case Action::ZoomIn:
                        pendingNativeCommand_ = NativeCommand::ZoomIn;
                        break;
                    case Action::TestPlay:
                    case Action::Preview:
                        pendingNativeCommand_ = NativeCommand::Preview;
                        break;
                    case Action::Fit:
                        pendingNativeCommand_ = NativeCommand::Fit;
                        break;
                    case Action::Start:
                        pendingNativeCommand_ = NativeCommand::Start;
                        break;
                    case Action::Filter:
                        if (workspace_.ActiveView() ==
                            bridge::StoryFlowViewMode::Graph)
                        {
                            workspace_.SetExternalStatus(
                                "FILTER UNAVAILABLE // JOURNEY VIEW ONLY");
                        }
                        else
                        {
                            pendingNativeCommand_ = NativeCommand::Filter;
                        }
                        break;
                    case Action::Hub:
                    case Action::Assets:
                    case Action::Variables:
                    case Action::ProjectSelector:
                    case Action::Settings:
                    case Action::MainMenu:
                        if (journeyShellAction_)
                            journeyShellAction_(action);
                        break;
                    case Action::Levels:
                        pendingNativeCommand_ = NativeCommand::Journey;
                        destinationComposer_.Toggle(
                            RenegadeStoryFlowDestinationComposer::Mode::Level);
                        workspace_.SetExternalStatus(
                            "ADD DESTINATION // LEVEL LIFECYCLE");
                        break;
                    case Action::Screens:
                        pendingNativeCommand_ = NativeCommand::Journey;
                        destinationComposer_.Toggle(
                            RenegadeStoryFlowDestinationComposer::Mode::Screen);
                        workspace_.SetExternalStatus(
                            "ADD DESTINATION // SCREEN LIFECYCLE");
                        break;
                    }
                });
            journeyChrome_.OnZoomRequested([this](const float zoom)
            {
                workspace_.SetJourneyZoom(zoom);
            });

            destinationComposer_.Create();
            destinationComposer_.Attach(GetGUI());

            conditionEditor_.Create();
            conditionEditor_.SetVisible(false);
            conditionEditor_.SetEnabled(false);

            journeyViewButton_.Create("Story Flow Journey View");
            journeyViewButton_.SetText("JOURNEY");
            journeyViewButton_.SetTooltip("Show the high-level player journey presentation.");
            journeyViewButton_.SetShadowRadius(0.0f);
            journeyViewButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                pendingNativeCommand_ = NativeCommand::Journey;
            });

            graphViewButton_.Create("Story Flow Graph View");
            graphViewButton_.SetText("GRAPH");
            graphViewButton_.SetTooltip("Show exact Story Flow topology and route wiring.");
            graphViewButton_.SetShadowRadius(0.0f);
            graphViewButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                pendingNativeCommand_ = NativeCommand::Graph;
            });

            fitButton_.Create("Story Flow Fit");
            fitButton_.SetText("FIT");
            fitButton_.SetTooltip("Frame all content in the active Story Flow view.");
            fitButton_.SetShadowRadius(0.0f);
            fitButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                pendingNativeCommand_ = NativeCommand::Fit;
            });

            startButton_.Create("Story Flow Start");
            startButton_.SetText("START");
            startButton_.SetTooltip("Center the permanent Game Start destination.");
            startButton_.SetShadowRadius(0.0f);
            startButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                pendingNativeCommand_ = NativeCommand::Start;
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
                pendingNativeCommand_ = NativeCommand::Find;
            });

            findButton_.Create("Story Flow Find");
            findButton_.SetText("FIND");
            findButton_.SetTooltip("Focus the first exact or partial Story Flow node-name match.");
            findButton_.SetShadowRadius(0.0f);
            findButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                pendingNativeCommand_ = NativeCommand::Find;
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
                pendingNativeCommand_ = NativeCommand::DeleteSelection;
            });

            // Wicked updates in registration order and renders in reverse. Keep
            // native commands in front of the background presentation layers.
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
            workspace_.SetEnabled(
                workspaceActive_ && !graphActive && !destinationComposer_.IsOpen());
            journeyChrome_.SetVisible(workspaceActive_);
            journeyChrome_.SetEnabled(workspaceActive_);
            destinationComposer_.SetWorkspaceActive(workspaceActive_ && !graphActive);
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
            pendingNativeCommand_ = NativeCommand::None;
            SetNativeControlsActive(false);
            destinationComposer_.SetWorkspaceActive(false);
            deleteSelectionButton_.SetVisible(false);
            deleteSelectionButton_.SetEnabled(false);
            conditionEditor_.SetVisible(false);
            conditionEditor_.SetEnabled(false);
            journeyChrome_.SetVisible(false);
            journeyChrome_.SetEnabled(false);
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
            journeyChrome_.SetProjectContext(
                projectName_,
                workspace_.IsDirty(),
                workspace_.CanUndo(),
                workspace_.CanRedo(),
                workspace_.JourneyZoom(),
                workspace_.JourneyFilterActive());

            const bool graphBeforeUpdate = workspaceActive_ &&
                workspace_.ActiveView() == bridge::StoryFlowViewMode::Graph;
            journeyChrome_.SetGraphViewActive(graphBeforeUpdate);
            graphLayer_.SetVisible(graphBeforeUpdate);
            graphLayer_.SetEnabled(graphBeforeUpdate);
            SetNativeControlsActive(workspaceActive_);
            UpdateDeleteControl(graphBeforeUpdate);

            // FIT/START live in the lower-left canvas navigation host. Journey
            // uses raw canvas input, so the shared workspace must not receive the
            // same press when one of those native controls owns the pointer.
            // Graph already disables the shared workspace entirely.
            const XMFLOAT4 pointerBeforeGui = wi::input::GetPointer();
            const bool nativeCanvasNavigationOwnsPointer =
                workspaceActive_ && !graphBeforeUpdate &&
                journeyChrome_.CanvasOverlayOwnsPointer(pointerBeforeGui);
            workspace_.SetEnabled(
                workspaceActive_ && !graphBeforeUpdate &&
                !nativeCanvasNavigationOwnsPointer &&
                !destinationComposer_.IsOpen());

            // Native callbacks queue intent only. Execute the command after the
            // complete wiGUI update so Story Flow never mutates canvas/view/
            // semantic state re-entrantly while Wicked is traversing widgets.
            wi::RenderPath2D::Update(dt);
            ProcessPendingNativeCommand();

            const bool graphActive = workspaceActive_ &&
                workspace_.ActiveView() == bridge::StoryFlowViewMode::Graph;
            journeyChrome_.SetGraphViewActive(graphActive);
            graphLayer_.SetVisible(graphActive);
            graphLayer_.SetEnabled(graphActive);
            destinationComposer_.SetWorkspaceActive(workspaceActive_ && !graphActive);
            workspace_.SetEnabled(
                workspaceActive_ && !graphActive && !destinationComposer_.IsOpen());

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
            boundModel_ = model;
            workspace_.Bind(session, model, layout);
            UpdateDestinationSelection(workspace_.SelectedNodeId());
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
                pendingNativeCommand_ = NativeCommand::None;
                findDraft_.clear();
                findInput_.SetValue("");
                destinationComposer_.Clear();
                boundModel_ = nullptr;
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
            destinationComposer_.SetWorkspaceActive(active && !graphActive);
            workspace_.SetEnabled(
                active && !graphActive && !destinationComposer_.IsOpen());
            journeyChrome_.SetVisible(active);
            journeyChrome_.SetEnabled(active);
            conditionEditor_.SetVisible(active);
            conditionEditor_.SetEnabled(active);
            graphLayer_.SetVisible(graphActive);
            graphLayer_.SetEnabled(graphActive);
            SetNativeControlsActive(active);
            UpdateDeleteControl(graphActive);
            if (!active)
            {
                graphInputBlockedLastFrame_ = false;
                pendingNativeCommand_ = NativeCommand::None;
            }
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
            workspace_.OnSelectionChanged(
                [this, callback = std::move(callback)](
                    const bridge::StableId& nodeId)
                {
                    UpdateDestinationSelection(nodeId);
                    if (callback) callback(nodeId);
                });
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

        void SetProjectName(std::string name)
        {
            projectName_ = std::move(name);
        }

        void OnNodeActivated(
            std::function<void(const bridge::StableId&)> callback)
        {
            EnsureLoaded();
            workspace_.OnNodeActivated(std::move(callback));
        }

        void OnJourneyShellAction(std::function<void(
            RenegadeStoryFlowJourneyChrome::Action)> callback)
        {
            journeyShellAction_ = std::move(callback);
        }

        void OnCreateLevel(std::function<void(const std::string&)> callback)
        {
            EnsureLoaded();
            destinationComposer_.OnCreateLevel(std::move(callback));
        }

        void OnAdoptLevel(std::function<void(const std::string&)> callback)
        {
            EnsureLoaded();
            destinationComposer_.OnAdoptLevel(std::move(callback));
        }

        void OnCreateScreen(std::function<void(
            const std::string&, bridge::StoryFlowScreenTemplate)> callback)
        {
            EnsureLoaded();
            destinationComposer_.OnCreateScreen(std::move(callback));
        }

        void OnOpenSelectedDestination(
            std::function<void(const bridge::StableId&)> callback)
        {
            EnsureLoaded();
            destinationComposer_.OnOpenSelected(std::move(callback));
        }

        void SelectAndFocusNode(const bridge::StableId& nodeId)
        {
            EnsureLoaded();
            workspace_.SelectAndFocusNode(nodeId);
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
        [[nodiscard]] static bool Contains(
            const XMFLOAT4& bounds,
            const XMFLOAT4& pointer) noexcept
        {
            return pointer.x >= bounds.x &&
                pointer.y >= bounds.y &&
                pointer.x < bounds.x + bounds.z &&
                pointer.y < bounds.y + bounds.w;
        }

        void ProcessPendingNativeCommand()
        {
            const NativeCommand command = std::exchange(
                pendingNativeCommand_, NativeCommand::None);
            switch (command)
            {
            case NativeCommand::Journey:
                workspace_.ActivateView(bridge::StoryFlowViewMode::Journey);
                graphInputBlockedLastFrame_ = false;
                break;
            case NativeCommand::Graph:
                workspace_.ActivateView(bridge::StoryFlowViewMode::Graph);
                graphInputBlockedLastFrame_ = false;
                break;
            case NativeCommand::Fit:
                FitToContent();
                break;
            case NativeCommand::Start:
                CenterOnGameStart();
                break;
            case NativeCommand::Find:
                RunFind();
                break;
            case NativeCommand::DeleteSelection:
                workspace_.DeleteSelection();
                break;
            case NativeCommand::Undo:
                workspace_.UndoJourney();
                break;
            case NativeCommand::Redo:
                workspace_.RedoJourney();
                break;
            case NativeCommand::ZoomOut:
                workspace_.AdjustJourneyZoom(1.0f / 1.1f);
                break;
            case NativeCommand::ZoomIn:
                workspace_.AdjustJourneyZoom(1.1f);
                break;
            case NativeCommand::Preview:
                if (workspace_.IsDirty())
                    workspace_.SaveJourney();
                if (workspace_.IsDirty())
                {
                    workspace_.SetExternalStatus(
                        "PREVIEW BLOCKED // STORY FLOW SAVE FAILED");
                    break;
                }
                if (journeyShellAction_)
                {
                    journeyShellAction_(
                        RenegadeStoryFlowJourneyChrome::Action::Preview);
                }
                break;
            case NativeCommand::Filter:
                workspace_.ToggleJourneyFilter();
                break;
            case NativeCommand::None:
            default:
                break;
            }
        }

        void SetNativeControlsActive(const bool active)
        {
            // Graph remains the existing synchronized topology editor. The
            // compact switch restores deliberate access without making Graph
            // part of the Journey visual-recovery scope.
            journeyViewButton_.SetVisible(active);
            journeyViewButton_.SetEnabled(active);
            graphViewButton_.SetVisible(active);
            graphViewButton_.SetEnabled(active);
            fitButton_.SetVisible(false);
            fitButton_.SetEnabled(false);
            startButton_.SetVisible(false);
            startButton_.SetEnabled(false);
            findInput_.SetVisible(active && searchOpen_);
            findInput_.SetEnabled(active && searchOpen_);
            findButton_.SetVisible(active && searchOpen_);
            findButton_.SetEnabled(active && searchOpen_);
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
            if (workspace_.ActiveView() == bridge::StoryFlowViewMode::Journey)
                (void)workspace_.FindAndFocusJourneyNode(findDraft_);
            else
                (void)graphEditor_.FocusNodeByName(findDraft_);
        }

        void UpdateDestinationSelection(const bridge::StableId& nodeId)
        {
            using Mode = RenegadeStoryFlowDestinationComposer::Mode;
            Mode mode = Mode::Closed;
            if (boundModel_)
            {
                const auto* node = boundModel_->FindNode(nodeId);
                if (node && node->kind == bridge::FlowNodeKind::Level)
                    mode = Mode::Level;
                else if (node && node->kind == bridge::FlowNodeKind::Screen)
                    mode = Mode::Screen;
            }
            destinationComposer_.SetSelectedNode(nodeId, mode);
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
            destinationComposer_.SetLayout(
                workspaceLeft,
                RenegadeStoryFlowJourneyChrome::WorkspaceHeaderHeight,
                graphWidth);
            const XMFLOAT4 graphViewport(
                workspaceLeft,
                RenegadeStoryFlowJourneyChrome::WorkspaceHeaderHeight,
                graphWidth,
                std::max(
                    1.0f,
                    height - RenegadeStoryFlowJourneyChrome::WorkspaceHeaderHeight));
            graphEditor_.SetViewport(graphViewport);
            graphLayer_.SetViewport(graphViewport);

            const auto& shell = journeyChrome_.ShellLayout();
            const float viewSwitchY =
                shell.workspaceTitle.Bottom() - 34.0f;
            journeyViewButton_.SetPos(XMFLOAT2(
                shell.workspaceTitle.x + 20.0f, viewSwitchY));
            journeyViewButton_.SetSize(XMFLOAT2(82.0f, 26.0f));
            graphViewButton_.SetPos(XMFLOAT2(
                shell.workspaceTitle.x + 108.0f, viewSwitchY));
            graphViewButton_.SetSize(XMFLOAT2(62.0f, 26.0f));

            // Canvas navigation lives in the existing lower-left navigation
            // host area, away from Level/Screen lifecycle controls in the two
            // header rows. The complete host is reserved from Journey raw input.
            const float canvasNavY = std::max(
                RenegadeStoryFlowJourneyChrome::WorkspaceHeaderHeight + 18.0f,
                height - 52.0f);
            fitButton_.SetPos(XMFLOAT2(workspaceLeft + 18.0f, canvasNavY));
            fitButton_.SetSize(XMFLOAT2(52.0f, 28.0f));
            startButton_.SetPos(XMFLOAT2(workspaceLeft + 76.0f, canvasNavY));
            startButton_.SetSize(XMFLOAT2(64.0f, 28.0f));
            canvasNavigationBounds_ = XMFLOAT4(
                workspaceLeft + 18.0f,
                canvasNavY,
                122.0f,
                28.0f);

            const float inspectorX = workspaceLeft + graphWidth + 14.0f;
            const float findY = shell.workspaceTitle.y + 22.0f;
            const float findButtonWidth = 62.0f;
            const float findFieldWidth = std::clamp(
                graphWidth * 0.28f, 210.0f, 320.0f);
            const float findX = workspaceLeft +
                std::max(220.0f, graphWidth - findFieldWidth - findButtonWidth - 30.0f);
            findInput_.SetPos(XMFLOAT2(findX, findY));
            findInput_.SetSize(XMFLOAT2(findFieldWidth, 27.0f));
            findButton_.SetPos(XMFLOAT2(
                findX + findFieldWidth + 6.0f, findY));
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
        RenegadeStoryFlowDestinationComposer destinationComposer_;
        RenegadeStoryFlowConditionEditor conditionEditor_;
        bridge::StoryFlowAuthoringModel* boundModel_ = nullptr;
        RenegadeButton journeyViewButton_;
        RenegadeButton graphViewButton_;
        RenegadeButton fitButton_;
        RenegadeButton startButton_;
        RenegadeTextInputField findInput_;
        RenegadeButton findButton_;
        RenegadeButton deleteSelectionButton_;
        std::function<void(RenegadeStoryFlowJourneyChrome::Action)>
            journeyShellAction_;
        std::string findDraft_;
        std::string projectName_ = "Renegade Project";
        XMFLOAT4 canvasNavigationBounds_ = {};
        NativeCommand pendingNativeCommand_ = NativeCommand::None;
        bool loaded_ = false;
        bool workspaceActive_ = false;
        bool graphInputBlockedLastFrame_ = false;
        bool searchOpen_ = false;
        float lastLogicalWidth_ = -1.0f;
        float lastLogicalHeight_ = -1.0f;
    };
}
