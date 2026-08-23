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
    // Story Flow is a first-class 2D RenderPath. wiGUI remains Renegade's
    // production editor UI. Gate 9C embeds ImNodes only over the Graph canvas;
    // Journey View remains the native 9B card/reel surface with no wires.
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

            // Wicked paints widgets in reverse registration order. Keep the
            // semantic workspace at the back, then native Journey chrome, then
            // the modal condition editor. The compact delete command is added
            // before these layers so it remains a real native control above the
            // shared Story Flow surface in both Journey and Graph.
            GetGUI().AddWidget(&deleteSelectionButton_);
            GetGUI().AddWidget(&conditionEditor_);
            GetGUI().AddWidget(&journeyChrome_);
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
            deleteSelectionButton_.SetVisible(
                workspaceActive_ && workspace_.CanDeleteSelection());
            deleteSelectionButton_.SetEnabled(
                workspaceActive_ && workspace_.CanDeleteSelection());
        }

        void Stop() override
        {
            if (!loaded_)
                return;

            deleteSelectionButton_.SetVisible(false);
            deleteSelectionButton_.SetEnabled(false);
            conditionEditor_.SetVisible(false);
            conditionEditor_.SetEnabled(false);
            journeyChrome_.SetVisible(false);
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

            const bool canDeleteBeforeUpdate =
                workspaceActive_ && workspace_.CanDeleteSelection();
            deleteSelectionButton_.SetVisible(canDeleteBeforeUpdate);
            deleteSelectionButton_.SetEnabled(canDeleteBeforeUpdate);

            const XMFLOAT4 pointer = wi::input::GetPointer();
            const bool graphActive = workspaceActive_ &&
                workspace_.ActiveView() == bridge::StoryFlowViewMode::Graph;
            const bool graphOwnsPointer = graphActive &&
                graphEditor_.ContainsPointer(pointer);

            // The legacy Graph implementation is still part of the shared
            // workspace because Journey/Inspector/header depend on that class.
            // While the pointer is inside the Graph canvas, disable workspace
            // canvas input for this frame so only ImNodes can interpret socket
            // and link gestures. Header/Inspector input remains available when
            // the pointer is outside the Graph viewport.
            workspace_.SetEnabled(workspaceActive_ && !graphOwnsPointer);
            wi::RenderPath2D::Update(dt);
            workspace_.SetEnabled(workspaceActive_);

            // A native Wicked ComboBox opens down into the Graph viewport. The
            // ImNodes canvas is composed after wiGUI, so rendering or accepting
            // Graph input while that popup is active would cover/intercept the
            // dropdown. Suspend the Graph overlay for the popup's active span;
            // the synchronized legacy projection remains underneath for those
            // few frames and the native dropdown stays authoritative.
            bool nativePopupActive = false;
            if (auto* widget = GetGUI().GetWidget("Story Flow Screen Template"))
            {
                nativePopupActive = widget->IsVisible() &&
                    widget->GetState() >= wi::gui::ACTIVE;
            }

            graphEditor_.Update(*this, dt, graphActive && !nativePopupActive);
            if (graphActive && !nativePopupActive)
            {
                graphEditor_.ReconcileHostInteractions(!GetGUI().IsTyping());
            }
            else if (workspaceActive_ &&
                workspace_.ActiveView() == bridge::StoryFlowViewMode::Journey &&
                !GetGUI().IsTyping() &&
                wi::input::Press(wi::input::KEYBOARD_BUTTON_DELETE))
            {
                workspace_.DeleteSelection();
            }

            const bool canDeleteAfterUpdate =
                workspaceActive_ && workspace_.CanDeleteSelection();
            deleteSelectionButton_.SetVisible(canDeleteAfterUpdate);
            deleteSelectionButton_.SetEnabled(canDeleteAfterUpdate);

            if (lifecycleLayeringReady_)
                PlaceJourneyLayersBehindLifecycleControls();
        }

        void Compose(const wi::graphics::CommandList cmd) const override
        {
            wi::RenderPath2D::Compose(cmd);
            graphEditor_.Render(cmd);
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
            conditionEditor_.Bind(session, model, &workspace_);
        }

        void Clear() noexcept
        {
            if (loaded_)
            {
                conditionEditor_.Clear();
                graphEditor_.Clear();
                workspace_.Clear();
                deleteSelectionButton_.SetVisible(false);
                deleteSelectionButton_.SetEnabled(false);
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
            workspaceActive_ = active;
            if (!loaded_)
                return;

            workspace_.SetVisible(active);
            workspace_.SetEnabled(active);
            journeyChrome_.SetVisible(active);
            conditionEditor_.SetVisible(active);
            conditionEditor_.SetEnabled(active);
            const bool canDelete = active && workspace_.CanDeleteSelection();
            deleteSelectionButton_.SetVisible(canDelete);
            deleteSelectionButton_.SetEnabled(canDelete);
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
            PlaceJourneyLayersBehindLifecycleControls();
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
        void PlaceJourneyLayersBehindLifecycleControls()
        {
            if (!loaded_)
                return;

            auto& gui = GetGUI();
            gui.RemoveWidget(&conditionEditor_);
            gui.RemoveWidget(&journeyChrome_);
            gui.RemoveWidget(&workspace_);
            gui.AddWidget(&conditionEditor_);
            gui.AddWidget(&journeyChrome_);
            gui.AddWidget(&workspace_);
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
            graphEditor_.SetViewport(XMFLOAT4(
                workspaceLeft,
                RenegadeStoryFlowJourneyChrome::WorkspaceHeaderHeight,
                graphWidth,
                std::max(
                    1.0f,
                    height - RenegadeStoryFlowJourneyChrome::WorkspaceHeaderHeight)));

            deleteSelectionButton_.SetPos(XMFLOAT2(
                workspaceLeft + graphWidth + std::max(0.0f, inspectorWidth - 88.0f),
                RenegadeStoryFlowJourneyChrome::WorkspaceHeaderHeight + 9.0f));
            deleteSelectionButton_.SetSize(XMFLOAT2(74.0f, 26.0f));

            conditionEditor_.SetLayout(width, height);
        }

        RenegadeStoryFlowWorkspace workspace_;
        RenegadeStoryFlowGraphEditor graphEditor_;
        RenegadeStoryFlowJourneyChrome journeyChrome_;
        RenegadeStoryFlowConditionEditor conditionEditor_;
        RenegadeButton deleteSelectionButton_;
        bool loaded_ = false;
        bool workspaceActive_ = false;
        bool lifecycleLayeringReady_ = false;
        float lastLogicalWidth_ = -1.0f;
        float lastLogicalHeight_ = -1.0f;
    };
}
