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
    // Native wiGUI host for the ImNodes Graph canvas. This gives Graph a stable
    // place in the same z/input stack as the rest of Story Flow instead of
    // composing it as a second full-screen overlay after wiGUI.
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

            // Opaque ownership of the Graph viewport means the retired legacy
            // Graph projection can never leak through if ImNodes has no frame.
            wi::image::Params background(
                translation.x,
                translation.y,
                scale.x,
                scale.y,
                wi::Color(8, 12, 16, 255));
            background.blendFlag = wi::enums::BLENDMODE_OPAQUE;
            wi::image::Draw(nullptr, background, cmd);

            if (editor_)
                editor_->Render(cmd);
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStoryFlowGraphLayer";
        }

    private:
        RenegadeStoryFlowGraphEditor* editor_ = nullptr;
        XMFLOAT4 bounds_ = {};
    };

    // Story Flow is a first-class 2D RenderPath. Journey remains the native 9B
    // card/reel presentation. Graph has one visible and interactive canvas:
    // ImNodes hosted by RenegadeStoryFlowGraphLayer. Native lifecycle/header/
    // inspector controls remain wiGUI and always sit above that canvas.
    class RenegadeStoryFlowRenderPath final : public wi::RenderPath2D
    {
    public:
        ~RenegadeStoryFlowRenderPath() override
        {
            if (loaded_)
            {
                GetGUI().RemoveWidget(&deleteSelectionButton_);
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

            deleteSelectionButton_.Create("Story Flow Delete Selection");
            deleteSelectionButton_.SetText("DELETE");
            deleteSelectionButton_.SetTooltip(
                "Delete the selected Story Flow route or non-Game-Start node. Delete also works from the keyboard.");
            deleteSelectionButton_.SetShadowRadius(0.0f);
            deleteSelectionButton_.SetVisible(false);
            deleteSelectionButton_.SetEnabled(false);
            deleteSelectionButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                workspace_.DeleteSelection();
            });

            // GUI renders back-to-front. Before lifecycle controls are attached,
            // establish the invariant order: workspace -> Graph -> Journey ->
            // condition modal -> delete command. Attach() later moves only the
            // three background layers behind lifecycle controls exactly once.
            GetGUI().AddWidget(&deleteSelectionButton_);
            GetGUI().AddWidget(&conditionEditor_);
            GetGUI().AddWidget(&journeyChrome_);
            GetGUI().AddWidget(&graphLayer_);
            GetGUI().AddWidget(&workspace_);

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
            workspace_.SetVisible(workspaceActive_);
            workspace_.SetEnabled(workspaceActive_);
            journeyChrome_.SetVisible(workspaceActive_);
            conditionEditor_.SetVisible(workspaceActive_);
            conditionEditor_.SetEnabled(workspaceActive_);
            const bool graphActive = workspaceActive_ &&
                workspace_.ActiveView() == bridge::StoryFlowViewMode::Graph;
            graphLayer_.SetVisible(graphActive);
            graphLayer_.SetEnabled(graphActive);
            deleteSelectionButton_.SetVisible(
                workspaceActive_ && workspace_.CanDeleteSelection());
            deleteSelectionButton_.SetEnabled(
                workspaceActive_ && workspace_.CanDeleteSelection());
        }

        void Stop() override
        {
            if (!loaded_)
                return;

            graphEditor_.ResetTransientInteractionState();
            graphInputBlockedLastFrame_ = false;
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

            const bool canDeleteBeforeUpdate =
                workspaceActive_ && workspace_.CanDeleteSelection();
            deleteSelectionButton_.SetVisible(canDeleteBeforeUpdate);
            deleteSelectionButton_.SetEnabled(canDeleteBeforeUpdate);

            // One stable wiGUI update. The shared workspace still contains the
            // pre-9C Graph hit-testing code used by Journey/Inspector chrome,
            // so suppress only that compatibility surface while the pointer is
            // inside the ImNodes viewport. Header and Inspector remain native
            // and available whenever the pointer is outside the Graph canvas.
            const XMFLOAT4 pointerBeforeGui = wi::input::GetPointer();
            const bool graphOwnsCanvas = graphBeforeUpdate &&
                graphEditor_.ContainsPointer(pointerBeforeGui);
            workspace_.SetEnabled(workspaceActive_ && !graphOwnsCanvas);
            wi::RenderPath2D::Update(dt);
            workspace_.SetEnabled(workspaceActive_);

            const bool graphActive = workspaceActive_ &&
                workspace_.ActiveView() == bridge::StoryFlowViewMode::Graph;
            graphLayer_.SetVisible(graphActive);
            graphLayer_.SetEnabled(graphActive);

            const XMFLOAT4 pointer = wi::input::GetPointer();
            const bool pointerInsideGraph = graphActive &&
                graphEditor_.ContainsPointer(pointer);
            const bool graphInputBlocked = pointerInsideGraph &&
                !graphLayer_.OwnsPointer();

            if (graphActive)
            {
                if (graphInputBlocked)
                {
                    // A higher native control (notably the Screen template
                    // dropdown) owns this pointer. Keep the last Graph frame
                    // visible underneath it and cancel any transient Graph
                    // gesture exactly once when ownership changes.
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

                if (workspaceActive_ &&
                    workspace_.ActiveView() == bridge::StoryFlowViewMode::Journey &&
                    !GetGUI().IsTyping() &&
                    wi::input::Press(wi::input::KEYBOARD_BUTTON_DELETE))
                {
                    workspace_.DeleteSelection();
                }
            }
            graphInputBlockedLastFrame_ = graphInputBlocked;

            const bool canDeleteAfterUpdate =
                workspaceActive_ && workspace_.CanDeleteSelection();
            deleteSelectionButton_.SetVisible(canDeleteAfterUpdate);
            deleteSelectionButton_.SetEnabled(canDeleteAfterUpdate);
        }

        void Compose(const wi::graphics::CommandList cmd) const override
        {
            // Graph is now a wiGUI layer and is composed in the same stable stack
            // as native Story Flow controls. Never compose a second overlay here.
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
            }
        }

        void FitToContent()
        {
            EnsureLoaded();
            workspace_.FitToContent();
        }

        void CenterOnGameStart()
        {
            EnsureLoaded();
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

            workspace_.SetVisible(active);
            workspace_.SetEnabled(active);
            journeyChrome_.SetVisible(active);
            conditionEditor_.SetVisible(active);
            conditionEditor_.SetEnabled(active);
            const bool graphActive = active &&
                workspace_.ActiveView() == bridge::StoryFlowViewMode::Graph;
            graphLayer_.SetVisible(graphActive);
            graphLayer_.SetEnabled(graphActive);
            const bool canDelete = active && workspace_.CanDeleteSelection();
            deleteSelectionButton_.SetVisible(canDelete);
            deleteSelectionButton_.SetEnabled(canDelete);
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
            // Leave condition/delete at the front. Move only background/shared
            // layers after the lifecycle widgets that Attach() has registered.
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
        RenegadeButton deleteSelectionButton_;
        bool loaded_ = false;
        bool workspaceActive_ = false;
        bool lifecycleLayeringReady_ = false;
        bool graphInputBlockedLastFrame_ = false;
        float lastLogicalWidth_ = -1.0f;
        float lastLogicalHeight_ = -1.0f;
    };
}
