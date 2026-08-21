#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

#include <WickedEngine.h>

#include "RenegadeStoryFlowConditionEditor.h"
#include "RenegadeStoryFlowWorkspace.h"

namespace renegade::studio
{
    // Gate 3 promotes Story Flow out of the 3D Level Editor overlay and into
    // its own first-class 2D RenderPath. Wicked only updates and renders the
    // application's active RenderPath, so the Level Editor scene is dormant
    // while this path owns the application.
    class RenegadeStoryFlowRenderPath final : public wi::RenderPath2D
    {
    public:
        ~RenegadeStoryFlowRenderPath() override
        {
            if (loaded_)
            {
                GetGUI().RemoveWidget(&conditionEditor_);
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
            GetGUI().AddWidget(&workspace_);

            conditionEditor_.Create();
            conditionEditor_.SetVisible(false);
            conditionEditor_.SetEnabled(false);
            GetGUI().AddWidget(&conditionEditor_);

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
            conditionEditor_.SetVisible(workspaceActive_);
            conditionEditor_.SetEnabled(workspaceActive_);
        }

        void Stop() override
        {
            if (!loaded_)
                return;

            conditionEditor_.SetVisible(false);
            conditionEditor_.SetEnabled(false);
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
            workspace_.SetPos(XMFLOAT2(0.0f, 0.0f));
            workspace_.SetLayout(width, height);
            conditionEditor_.SetLayout(width, height);
        }

        RenegadeStoryFlowWorkspace workspace_;
        RenegadeStoryFlowConditionEditor conditionEditor_;
        bool loaded_ = false;
        bool workspaceActive_ = false;
        float lastLogicalWidth_ = -1.0f;
        float lastLogicalHeight_ = -1.0f;
    };
}
