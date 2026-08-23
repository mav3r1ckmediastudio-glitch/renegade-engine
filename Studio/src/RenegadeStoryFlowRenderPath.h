#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

#include <WickedEngine.h>

#include "RenegadeStoryFlowConditionEditor.h"
#include "RenegadeStoryFlowJourneyChrome.h"
#include "RenegadeStoryFlowJourneyRoutingOverlay.h"
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
    // Gate 9C adds a Renegade-owned visual routing layer over the same
    // authoritative Story Flow session/model. No concept bitmap or stock Wicked
    // Editor route UX is used.
    class RenegadeStoryFlowRenderPath final : public wi::RenderPath2D
    {
    public:
        ~RenegadeStoryFlowRenderPath() override
        {
            if (loaded_)
            {
                GetGUI().RemoveWidget(&conditionEditor_);
                GetGUI().RemoveWidget(&journeyChrome_);
                GetGUI().RemoveWidget(&routingOverlay_);
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

            routingOverlay_.Create();
            routingOverlay_.SetVisible(false);

            journeyChrome_.Create();
            journeyChrome_.SetVisible(false);
            // Gate 9A chrome is visual structure only. It must not intercept
            // the established Story Flow authoring/lifecycle input paths.
            journeyChrome_.SetEnabled(false);

            conditionEditor_.Create();
            conditionEditor_.SetVisible(false);
            conditionEditor_.SetEnabled(false);

            // Wicked paints widgets in reverse registration order. Register
            // the semantic workspace last so it paints first, then the Gate 9C
            // route overlay, then Journey chrome, then the modal condition
            // editor. External lifecycle controls attach later and stay frontmost.
            GetGUI().AddWidget(&conditionEditor_);
            GetGUI().AddWidget(&journeyChrome_);
            GetGUI().AddWidget(&routingOverlay_);
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
            routingOverlay_.SetVisible(workspaceActive_);
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
            routingOverlay_.SetVisible(false);
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

            // The route overlay intentionally skips normal GUI input update so
            // the established workspace processes its interaction first. This
            // explicit second phase lets route/port hits refine that result
            // without a second competing GUI focus surface.
            routingOverlay_.UpdateRouting(dt, GetGUI().IsTyping());

            if (lifecycleLayeringReady_)
            {
                // Widget interaction can reprioritize Wicked GUI registrations.
                // Reassert: semantic workspace -> visual routing -> Journey
                // chrome -> condition editor -> lifecycle controls.
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
            routingOverlay_.Bind(session, model, layout, &workspace_);
            conditionEditor_.Bind(session, model, &workspace_);
        }

        void Clear() noexcept
        {
            if (loaded_)
            {
                conditionEditor_.Clear();
                routingOverlay_.Clear();
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
            routingOverlay_.SetVisible(active);
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
            workspace_.OnSemanticChanged(
                [this, callback = std::move(callback)]() mutable
                {
                    routingOverlay_.MarkProjectionDirty();
                    if (callback)
                        callback();
                });
        }

        void OnScreenOutcomeQuery(
            RenegadeStoryFlowWorkspace::ScreenOutcomeQuery callback)
        {
            EnsureLoaded();
            routingOverlay_.OnScreenOutcomeQuery(callback);
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
        // so the opaque semantic workspace remains at the back, the Gate 9C
        // routing overlay sits over cards/wires, Journey chrome frames it, and
        // lifecycle controls remain frontmost.
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
            gui.RemoveWidget(&routingOverlay_);
            gui.RemoveWidget(&workspace_);
            gui.AddWidget(&conditionEditor_);
            gui.AddWidget(&journeyChrome_);
            gui.AddWidget(&routingOverlay_);
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

            routingOverlay_.SetPos(XMFLOAT2(workspaceLeft, 0.0f));
            routingOverlay_.SetLayout(workspaceWidth, height);

            // The condition editor remains full-canvas because it is a modal
            // authoring surface, not part of the Journey rail geometry.
            conditionEditor_.SetLayout(width, height);
        }

        RenegadeStoryFlowWorkspace workspace_;
        RenegadeStoryFlowJourneyRoutingOverlay routingOverlay_;
        RenegadeStoryFlowJourneyChrome journeyChrome_;
        RenegadeStoryFlowConditionEditor conditionEditor_;
        bool loaded_ = false;
        bool workspaceActive_ = false;
        bool lifecycleLayeringReady_ = false;
        float lastLogicalWidth_ = -1.0f;
        float lastLogicalHeight_ = -1.0f;
    };
}
