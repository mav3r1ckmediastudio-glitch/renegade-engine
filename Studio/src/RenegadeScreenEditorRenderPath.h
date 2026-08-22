#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

#include <WickedEngine.h>

#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/screen/ScreenRenderer.h"
#include "RenegadeScreenEditorWorkspace.h"
#include "StoryFlowScreenEditorHandoff.h"

namespace renegade::studio
{
    // First-class Gate 8C Screen Editor path. The inactive Story Flow and 3D
    // Level Editor paths do not tick or render while this path owns Studio.
    class RenegadeScreenEditorRenderPath final : public wi::RenderPath2D
    {
    public:
        ~RenegadeScreenEditorRenderPath() override
        {
            if (loaded_)
            {
                renderer_.Reset(*this);
                GetGUI().RemoveWidget(&workspace_);
            }
        }

        void EnsureLoaded()
        {
            if (loaded_) return;
            workspace_.Create();
            workspace_.SetVisible(false);
            workspace_.SetEnabled(false);
            workspace_.OnDocumentChanged([this]()
            {
                previewDirty_ = true;
            });
            workspace_.OnReturnRequested([this]()
            {
                if (returnRequested_) returnRequested_();
            });
            // Register the shell before preview surfaces. Wicked renders GUI
            // storage in reverse, so the shell's panels and selection outline
            // remain above the shared preview without covering its canvas.
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
            workspace_.SetVisible(open_);
            workspace_.SetEnabled(open_);
        }

        void Stop() override
        {
            if (!loaded_) return;
            workspace_.SetVisible(false);
            workspace_.SetEnabled(false);
        }

        void ResizeLayout() override
        {
            wi::RenderPath2D::ResizeLayout();
            LayoutWorkspace();
            renderer_.ApplyLayout(*this);
        }

        void Update(const float dt) override
        {
            EnsureLoaded();
            LayoutWorkspace();
            if (previewDirty_)
            {
                previewDirty_ = false;
                std::string error;
                if (!ReloadPreview(error))
                {
                    wi::backlog::post(
                        "Renegade Screen Editor preview: " + error,
                        wi::backlog::LogLevel::Error);
                }
            }
            renderer_.ApplyLayout(*this);
            wi::RenderPath2D::Update(dt);
        }

        void SyncCanvas(const wi::Canvas& canvas)
        {
            init(canvas);
            EnsureLoaded();
            LayoutWorkspace();
            renderer_.ApplyLayout(*this);
        }

        [[nodiscard]] bool OpenScreen(
            const StoryFlowScreenEditorHandoff& handoff,
            const std::string& projectRoot,
            const bridge::StableId& projectId,
            std::string& error)
        {
            EnsureLoaded();
            if (!handoff.ready || handoff.resolvedPath.empty() ||
                !bridge::IsValidStableId(handoff.screenDocumentId))
            {
                error = "Screen Editor received an incomplete Story Flow handoff.";
                return false;
            }

            bridge::ScreenAuthoringSession candidate;
            if (!candidate.Open(handoff.resolvedPath, projectId, error))
                return false;
            if (candidate.Document().envelope.documentId !=
                handoff.screenDocumentId)
            {
                error = "Screen Editor handoff identity does not match the opened document.";
                return false;
            }

            session_ = std::move(candidate);
            projectRoot_ = projectRoot;
            open_ = true;
            workspace_.Bind(&session_, &renderer_);
            workspace_.SetVisible(true);
            workspace_.SetEnabled(true);
            LayoutWorkspace();
            if (!ReloadPreview(error))
            {
                Clear();
                return false;
            }
            wi::backlog::post(
                "Renegade Screen Editor: authoring shell opened " +
                    handoff.resolvedPath,
                wi::backlog::LogLevel::Default);
            return true;
        }

        void Clear() noexcept
        {
            if (loaded_) renderer_.Reset(*this);
            session_.Clear();
            projectRoot_.clear();
            workspace_.Clear();
            if (loaded_)
            {
                workspace_.SetVisible(false);
                workspace_.SetEnabled(false);
            }
            previewDirty_ = false;
            open_ = false;
        }

        void OnReturnRequested(std::function<void()> callback)
        {
            returnRequested_ = std::move(callback);
        }

        [[nodiscard]] bool IsOpen() const noexcept
        {
            return open_ && session_.IsLoaded();
        }

        [[nodiscard]] bool IsDirty() const noexcept
        {
            return session_.IsDirty();
        }

    private:
        void LayoutWorkspace()
        {
            if (!loaded_) return;
            const float width = std::max(1.0f, GetLogicalWidth());
            const float height = std::max(1.0f, GetLogicalHeight());
            if (std::abs(width - lastWidth_) < 0.01f &&
                std::abs(height - lastHeight_) < 0.01f)
                return;
            lastWidth_ = width;
            lastHeight_ = height;
            workspace_.SetPos(XMFLOAT2(0.0f, 0.0f));
            workspace_.SetLayout(width, height);
            renderer_.SetViewport(workspace_.PreviewBounds());
        }

        [[nodiscard]] bool ReloadPreview(std::string& error)
        {
            if (!open_ || !session_.IsLoaded())
            {
                error = "No Screen document is open for preview.";
                return false;
            }
            renderer_.SetViewport(workspace_.PreviewBounds());
            const bool loaded = renderer_.Load(
                session_.Document(), projectRoot_, *this,
                [this](const bridge::StableId& widgetId)
                {
                    workspace_.SelectWidget(widgetId);
                },
                error);
            if (loaded)
                renderer_.ApplyLayout(*this);
            return loaded;
        }

        bridge::ScreenAuthoringSession session_;
        screen::ScreenRenderer renderer_;
        RenegadeScreenEditorWorkspace workspace_;
        std::function<void()> returnRequested_;
        std::string projectRoot_;
        bool loaded_ = false;
        bool open_ = false;
        bool previewDirty_ = false;
        float lastWidth_ = -1.0f;
        float lastHeight_ = -1.0f;
    };
}
