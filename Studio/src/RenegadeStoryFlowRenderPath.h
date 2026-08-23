#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

#include <WickedEngine.h>

#include "RenegadeStoryFlowConditionEditor.h"
#include "RenegadeStoryFlowJourneyChrome.h"
#include "RenegadeStoryFlowWorkspace.h"
#include "StoryFlowGuiLayerPolicy.h"

namespace renegade::studio
{
    // Gate 3 promotes Story Flow out of the 3D Level Editor overlay and into
    // its own first-class 2D RenderPath. Wicked only updates and renders the
    // application's active RenderPath, so the Level Editor scene is dormant
    // while this path owns the application.
    //
    // Gate 9A adds a native Journey chrome layer around that proven workspace.
    // The approved concept remains reference-only media: the render path never
    // loads it. The rail/frame/chrome are real Wicked GUI objects and the
    // existing semantic workspace is inset into that shell without changing
    // Story Flow document/runtime authority.
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
            // Gate 9A chrome is visual structure only. It must not intercept
            // the established Story Flow authoring/lifecycle input paths.
            journeyChrome_.SetEnabled(false);

            conditionEditor_.Create();
            conditionEditor_.SetVisible(false);
            conditionEditor_.SetEnabled(false);

            // Wicked paints widgets in reverse registration order. Register
            // the semantic workspace last so it paints first, then the native
            // Journey chrome, then the condition editor. External lifecycle
            // controls are attached later and remain frontmost.
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
            wi::RenderPath2D::Update(dt);
            if (lifecycleLayeringReady_)
            {
                // Widget interaction can reprioritize Wicked GUI registrations.
                // Reassert the Gate 9A order after input/update and before
                // Render: semantic workspace -> Journey chrome -> condition
                // editor -> lifecycle controls.
                PlaceJourneyLayersBehindLifecycleControls();
            }
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
            conditionEditor_.Bind(session, model, &workspace_);
        }

        void Clear() noexcept
        {
            if (loaded_)
            {
                conditionEditor_.Clear();
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

        // External Level/Screen lifecycle controls are added after this path
        // is loaded. Re-register the Story Flow layers in reverse paint order
        // so the opaque semantic workspace remains at the back, the Gate 9A
        // native Journey chrome sits above it, and lifecycle controls stay
        // frontmost.
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
            workspace_.SetPos(XMFLOAT2(workspaceLeft, 0.0f));
            workspace_.SetLayout(
                std::max(1.0f, width - workspaceLeft),
                height);

            // The condition editor remains full-canvas because it is a modal
            // authoring surface, not part of the Journey rail geometry.
            conditionEditor_.SetLayout(width, height);
        }

        RenegadeStoryFlowWorkspace workspace_;
        RenegadeStoryFlowJourneyChrome journeyChrome_;
        RenegadeStoryFlowConditionEditor conditionEditor_;
        bool loaded_ = false;
        bool workspaceActive_ = false;
        bool lifecycleLayeringReady_ = false;
        float lastLogicalWidth_ = -1.0f;
        float lastLogicalHeight_ = -1.0f;
    };
}
