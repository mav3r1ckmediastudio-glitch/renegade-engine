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

            // Wicked paints widgets in reverse registration order. Keep the
            // semantic workspace at the back, then native Journey chrome, then
            // the modal condition editor. The Graph canvas is composed later by
            // RenegadeStoryFlowGraphEditor and never registers a full-screen
            // wiGUI widget.
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
        }

        void Stop() override
        {
            if (!loaded_)
                return;

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

            graphEditor_.Update(*this, dt, graphActive);

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

            conditionEditor_.SetLayout(width, height);
        }

        RenegadeStoryFlowWorkspace workspace_;
        RenegadeStoryFlowGraphEditor graphEditor_;
        RenegadeStoryFlowJourneyChrome journeyChrome_;
        RenegadeStoryFlowConditionEditor conditionEditor_;
        bool loaded_ = false;
        bool workspaceActive_ = false;
        bool lifecycleLayeringReady_ = false;
        float lastLogicalWidth_ = -1.0f;
        float lastLogicalHeight_ = -1.0f;
    };
}
