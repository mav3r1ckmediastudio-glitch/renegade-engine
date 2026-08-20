#pragma once

#include "RenegadeStoryFlowWorkspace.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

namespace renegade::studio
{
    // Gate 1 Studio lifecycle adapter for the native read-only Story Flow
    // workspace. The semantic Flow remains owned by the existing LP02
    // document/runtime contract; this adapter only resolves the current
    // project's startup_flow, builds the presentation model and persists the
    // separate editor layout document.
    class StoryFlowStudioIntegration final
    {
    public:
        ~StoryFlowStudioIntegration()
        {
            if (attached_ && gui_ != nullptr)
            {
                gui_->RemoveWidget(&workspace_);
            }
        }

        template <typename Renderer, typename Session>
        void Tick(Renderer& renderer, Session& session)
        {
            Attach(renderer);

            if (!session.Projects().HasProject())
            {
                FlushLayout(true);
                ResetActiveFlow();
                SetWorkspaceVisible(false);
                return;
            }

            // Project Hub and the project loading overlay own the surface while
            // visible. Keep the loaded Flow in memory so returning to the same
            // project is instant, but persist presentation changes before the
            // workspace disappears.
            if (renderer.IsProjectHubVisible() || renderer.IsProjectLoadBlocking())
            {
                FlushLayout(true);
                SetWorkspaceVisible(false);
                return;
            }

            const auto& project = session.Projects().CurrentProject();
            const bool hasStartupFlow =
                bridge::IsValidStableId(project.startupFlowId) &&
                !project.startupFlow.empty();
            if (!hasStartupFlow)
            {
                FlushLayout(true);
                ResetActiveFlow();
                SetWorkspaceVisible(false);
                return;
            }

            if (trackedProjectId_ != project.projectId ||
                trackedFlowId_ != project.startupFlowId)
            {
                FlushLayout(true);
                ResetActiveFlow();
                trackedProjectId_ = project.projectId;
                trackedFlowId_ = project.startupFlowId;
                loadAttempted_ = false;
            }

            if (!loadAttempted_)
            {
                loadAttempted_ = true;
                if (!LoadCurrentFlow(project))
                {
                    SetWorkspaceVisible(false);
                    return;
                }
            }

            if (!model_.IsLoaded())
            {
                SetWorkspaceVisible(false);
                return;
            }

            const XMFLOAT4 viewport = renderer.StoryFlowWorkspaceBounds();
            const float width = std::max(1.0f, viewport.z - viewport.x);
            const float height = std::max(1.0f, viewport.w - viewport.y);
            workspace_.SetPos(XMFLOAT2(viewport.x, viewport.y));
            workspace_.SetLayout(width, height);
            SetWorkspaceVisible(true);
            FlushLayout(false);
        }

    private:
        template <typename Renderer>
        void Attach(Renderer& renderer)
        {
            if (attached_)
                return;

            workspace_.Create();
            workspace_.SetVisible(false);
            workspace_.SetEnabled(false);
            workspace_.OnLayoutChanged([this]()
            {
                layoutDirty_ = true;
            });
            gui_ = &renderer.StoryFlowGui();
            gui_->AddWidget(&workspace_);
            lastLayoutWrite_ = std::chrono::steady_clock::now();
            attached_ = true;
        }

        template <typename Project>
        bool LoadCurrentFlow(const Project& project)
        {
            std::string resolvedFlowPath;
            std::string error;
            if (!bridge::ResolveStoryFlowDocumentPath(
                    project.rootPath,
                    project.projectId,
                    project.startupFlowId,
                    project.startupFlow,
                    resolvedFlowPath,
                    error))
            {
                ReportSemanticFailure(
                    "could not resolve startup_flow '" + project.startupFlow +
                    "': " + error);
                return false;
            }

            bridge::FlowDocument document;
            if (!bridge::ReadFlowDocument(
                    resolvedFlowPath,
                    project.projectId,
                    document,
                    error))
            {
                ReportSemanticFailure(
                    "could not read startup_flow '" + resolvedFlowPath +
                    "': " + error);
                return false;
            }

            if (!model_.Load(std::move(document), project.projectId, error))
            {
                ReportSemanticFailure(
                    "startup_flow failed Story Flow authoring validation: " +
                    error);
                return false;
            }

            layoutPath_ = bridge::ResolveStoryFlowLayoutPath(
                project.rootPath,
                project.startupFlowId);

            bool restoredSavedLayout = false;
            if (!layoutPath_.empty())
            {
                std::error_code existsError;
                const bool layoutExists = std::filesystem::exists(
                    std::filesystem::u8path(layoutPath_),
                    existsError);
                if (!existsError && layoutExists)
                {
                    bridge::StoryFlowLayoutDocument saved;
                    std::string layoutError;
                    if (bridge::ReadStoryFlowLayout(
                            layoutPath_,
                            project.projectId,
                            project.startupFlowId,
                            saved,
                            layoutError) &&
                        bridge::ReconcileStoryFlowLayout(
                            model_,
                            project.projectId,
                            project.startupFlowId,
                            saved,
                            layoutError))
                    {
                        layout_ = std::move(saved);
                        restoredSavedLayout = true;
                    }
                    else
                    {
                        ReportLayoutWarning(
                            "saved layout was ignored and rebuilt: " +
                            layoutError);
                    }
                }
                else if (existsError)
                {
                    ReportLayoutWarning(
                        "could not inspect saved layout; using deterministic "
                        "layout: " + existsError.message());
                }
            }

            if (!restoredSavedLayout)
            {
                layout_ = bridge::BuildDeterministicStoryFlowLayout(
                    model_,
                    project.projectId,
                    project.startupFlowId);
            }

            if (!bridge::ReconcileStoryFlowLayout(
                    model_,
                    project.projectId,
                    project.startupFlowId,
                    layout_,
                    error))
            {
                // A presentation-only document must never stop a valid
                // semantic Flow from opening. Rebuild once from deterministic
                // metadata and leave Runtime/Flow content untouched.
                ReportLayoutWarning(
                    "layout reconciliation failed and was rebuilt: " + error);
                layout_ = bridge::BuildDeterministicStoryFlowLayout(
                    model_,
                    project.projectId,
                    project.startupFlowId);
            }

            workspace_.Bind(&model_, &layout_);
            if (!restoredSavedLayout)
            {
                workspace_.FitToContent();
            }
            layoutDirty_ = !restoredSavedLayout;
            if (layoutDirty_)
            {
                FlushLayout(true);
            }

            wi::backlog::post(
                "Renegade Story Flow: opened startup_flow " + resolvedFlowPath,
                wi::backlog::LogLevel::Default);
            return true;
        }

        void SetWorkspaceVisible(const bool visible)
        {
            if (!attached_)
                return;
            workspace_.SetVisible(visible);
            workspace_.SetEnabled(visible);
        }

        void ResetActiveFlow()
        {
            workspace_.Clear();
            model_.Clear();
            layout_ = {};
            layoutPath_.clear();
            trackedProjectId_.clear();
            trackedFlowId_.clear();
            loadAttempted_ = false;
            layoutDirty_ = false;
        }

        void FlushLayout(const bool force)
        {
            if (!layoutDirty_ || layoutPath_.empty() || !model_.IsLoaded())
                return;

            const auto now = std::chrono::steady_clock::now();
            if (!force &&
                now - lastLayoutWrite_ < std::chrono::milliseconds(250))
            {
                return;
            }

            std::string error;
            if (!bridge::WriteStoryFlowLayout(layoutPath_, layout_, error))
            {
                ReportLayoutWarning("could not persist layout: " + error);
                lastLayoutWrite_ = now;
                return;
            }

            layoutDirty_ = false;
            lastLayoutWrite_ = now;
        }

        void ReportSemanticFailure(const std::string& message)
        {
            wi::backlog::post(
                "Renegade Story Flow: " + message,
                wi::backlog::LogLevel::Error);
        }

        void ReportLayoutWarning(const std::string& message)
        {
            wi::backlog::post(
                "Renegade Story Flow presentation: " + message,
                wi::backlog::LogLevel::Warning);
        }

        RenegadeStoryFlowWorkspace workspace_;
        bridge::StoryFlowAuthoringModel model_;
        bridge::StoryFlowLayoutDocument layout_;
        bridge::StableId trackedProjectId_;
        bridge::StableId trackedFlowId_;
        std::string layoutPath_;
        wi::gui::GUI* gui_ = nullptr;
        std::chrono::steady_clock::time_point lastLayoutWrite_{};
        bool attached_ = false;
        bool loadAttempted_ = false;
        bool layoutDirty_ = false;
    };
}
