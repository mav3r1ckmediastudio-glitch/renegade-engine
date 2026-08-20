#pragma once

#include "renegade/bridge/StoryFlowAuthoringModel.h"
#include "renegade/bridge/StoryFlowLayoutService.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

namespace renegade::studio
{
    // Gate 3 lifecycle adapter. Semantic Flow remains owned by EngineBridge;
    // this class decides which first-class Studio RenderPath owns the next
    // frame and persists presentation-only Story Flow layout state.
    class StoryFlowStudioIntegration final
    {
    public:
        enum class Workspace
        {
            LevelEditor,
            StoryFlow,
        };

        void RequestStoryFlow() noexcept
        {
            desiredWorkspace_ = Workspace::StoryFlow;
        }

        // Gate 4 uses this seam before a Level is explicitly opened. Without
        // it, the presence of startup_flow would force the application back to
        // Story Flow on the next frame.
        void RequestLevelEditor() noexcept
        {
            desiredWorkspace_ = Workspace::LevelEditor;
        }

        template <typename Application, typename LevelEditor, typename StoryFlowPath, typename Session>
        void Tick(
            Application& application,
            LevelEditor& levelEditor,
            StoryFlowPath& storyFlow,
            Session& session)
        {
            Attach(storyFlow);
            storyFlow.SyncCanvas(application.canvas);

            if (!session.Projects().HasProject())
            {
                FlushLayout(true);
                ResetActiveFlow(storyFlow);
                storyFlow.SetWorkspaceActive(false);
                desiredWorkspace_ = Workspace::LevelEditor;
                hubOwnedLastTick_ = true;
                EnsureActive(application, levelEditor);
                return;
            }

            // Hub and the project-loading overlay remain owned by the existing
            // 3D Studio path. Story Flow is only activated after those surfaces
            // relinquish ownership.
            const bool hubOwnsSurface =
                levelEditor.IsProjectHubVisible() ||
                levelEditor.IsProjectLoadBlocking();
            if (hubOwnsSurface)
            {
                FlushLayout(true);
                storyFlow.SetWorkspaceActive(false);
                desiredWorkspace_ = Workspace::LevelEditor;
                hubOwnedLastTick_ = true;
                EnsureActive(application, levelEditor);
                return;
            }

            const auto& project = session.Projects().CurrentProject();
            const bool hasStartupFlow =
                bridge::IsValidStableId(project.startupFlowId) &&
                !project.startupFlow.empty();
            if (!hasStartupFlow)
            {
                FlushLayout(true);
                ResetActiveFlow(storyFlow);
                storyFlow.SetWorkspaceActive(false);
                desiredWorkspace_ = Workspace::LevelEditor;
                hubOwnedLastTick_ = false;
                EnsureActive(application, levelEditor);
                return;
            }

            const bool projectOrFlowChanged =
                trackedProjectId_ != project.projectId ||
                trackedFlowId_ != project.startupFlowId;
            if (projectOrFlowChanged)
            {
                FlushLayout(true);
                ResetActiveFlow(storyFlow);
                trackedProjectId_ = project.projectId;
                trackedFlowId_ = project.startupFlowId;
                loadAttempted_ = false;
                desiredWorkspace_ = Workspace::StoryFlow;
            }
            else if (hubOwnedLastTick_)
            {
                // Open/Continue Project enters Story Flow as the project home.
                desiredWorkspace_ = Workspace::StoryFlow;
            }
            hubOwnedLastTick_ = false;

            if (!loadAttempted_)
            {
                loadAttempted_ = true;
                if (!LoadCurrentFlow(project, storyFlow))
                {
                    storyFlow.SetWorkspaceActive(false);
                    desiredWorkspace_ = Workspace::LevelEditor;
                    EnsureActive(application, levelEditor);
                    return;
                }
            }

            if (!model_.IsLoaded())
            {
                storyFlow.SetWorkspaceActive(false);
                desiredWorkspace_ = Workspace::LevelEditor;
                EnsureActive(application, levelEditor);
                return;
            }

            if (desiredWorkspace_ == Workspace::LevelEditor)
            {
                storyFlow.SetWorkspaceActive(false);
                EnsureActive(application, levelEditor);
                return;
            }

            storyFlow.SetWorkspaceActive(true);
            EnsureActive(application, storyFlow);
            FlushLayout(false);
        }

    private:
        template <typename StoryFlowPath>
        void Attach(StoryFlowPath& storyFlow)
        {
            if (attached_)
                return;

            storyFlow.EnsureLoaded();
            storyFlow.OnLayoutChanged([this]()
            {
                layoutDirty_ = true;
            });
            lastLayoutWrite_ = std::chrono::steady_clock::now();
            attached_ = true;
        }

        template <typename Application, typename Path>
        static void EnsureActive(Application& application, Path& path)
        {
            if (application.GetActivePath() != &path)
            {
                application.ActivatePath(&path);
            }
        }

        template <typename Project, typename StoryFlowPath>
        bool LoadCurrentFlow(const Project& project, StoryFlowPath& storyFlow)
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
                ReportLayoutWarning(
                    "layout reconciliation failed and was rebuilt: " + error);
                layout_ = bridge::BuildDeterministicStoryFlowLayout(
                    model_,
                    project.projectId,
                    project.startupFlowId);
            }

            // SyncCanvas() has already given the dedicated path the current
            // application dimensions, so first-open FitToContent() frames
            // against the real window instead of the old 3D viewport host.
            storyFlow.Bind(&model_, &layout_);
            if (!restoredSavedLayout)
            {
                storyFlow.FitToContent();
            }
            layoutDirty_ = !restoredSavedLayout;
            if (layoutDirty_)
            {
                FlushLayout(true);
            }

            wi::backlog::post(
                "Renegade Story Flow: dedicated render path ready for " +
                    resolvedFlowPath,
                wi::backlog::LogLevel::Default);
            return true;
        }

        template <typename StoryFlowPath>
        void ResetActiveFlow(StoryFlowPath& storyFlow)
        {
            storyFlow.Clear();
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

        static void ReportSemanticFailure(const std::string& message)
        {
            wi::backlog::post(
                "Renegade Story Flow: " + message,
                wi::backlog::LogLevel::Error);
        }

        static void ReportLayoutWarning(const std::string& message)
        {
            wi::backlog::post(
                "Renegade Story Flow presentation: " + message,
                wi::backlog::LogLevel::Warning);
        }

        bridge::StoryFlowAuthoringModel model_;
        bridge::StoryFlowLayoutDocument layout_;
        bridge::StableId trackedProjectId_;
        bridge::StableId trackedFlowId_;
        std::string layoutPath_;
        std::chrono::steady_clock::time_point lastLayoutWrite_{};
        Workspace desiredWorkspace_ = Workspace::StoryFlow;
        bool attached_ = false;
        bool loadAttempted_ = false;
        bool layoutDirty_ = false;
        bool hubOwnedLastTick_ = true;
    };
}
