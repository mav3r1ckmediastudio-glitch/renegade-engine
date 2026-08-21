#pragma once

#include "renegade/bridge/StoryFlowAuthoringModel.h"
#include "renegade/bridge/StoryFlowAuthoringSession.h"
#include "renegade/bridge/StoryFlowLayoutService.h"
#include "renegade/bridge/StoryFlowLevelLifecycleService.h"
#include "renegade/bridge/StoryFlowLevelReferenceService.h"
#include "RenegadeStoryFlowLevelPanel.h"
#include "RenegadeStudioChrome.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

namespace renegade::studio
{
    // Gate 4 lifecycle coordinator. Semantic Flow stays in the EngineBridge
    // authoring session while this adapter owns first-class workspace switches,
    // governed Level mutations and the explicit return path to Story Flow.
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
            Attach(levelEditor, storyFlow);
            storyFlow.SyncCanvas(application.canvas);
            levelPanel_.SetLayout(
                std::max(1.0f, storyFlow.GetLogicalWidth()),
                std::max(1.0f, storyFlow.GetLogicalHeight()));
            LayoutReturnButton(levelEditor);

            if (!session.Projects().HasProject())
            {
                FlushLayout(true);
                ResetActiveFlow(storyFlow);
                SetLevelControlsActive(false, false);
                desiredWorkspace_ = Workspace::LevelEditor;
                hubOwnedLastTick_ = true;
                EnsureActive(application, levelEditor);
                return;
            }

            const bool hubOwnsSurface =
                levelEditor.IsProjectHubVisible() ||
                levelEditor.IsProjectLoadBlocking();
            if (hubOwnsSurface)
            {
                FlushLayout(true);
                storyFlow.SetWorkspaceActive(false);
                SetLevelControlsActive(false, false);
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
                SetLevelControlsActive(false, false);
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
                activeLevelNodeId_.clear();
            }
            else if (hubOwnedLastTick_)
            {
                desiredWorkspace_ = Workspace::StoryFlow;
            }
            hubOwnedLastTick_ = false;

            if (!loadAttempted_)
            {
                loadAttempted_ = true;
                if (!LoadCurrentFlow(project, storyFlow))
                {
                    storyFlow.SetWorkspaceActive(false);
                    SetLevelControlsActive(false, false);
                    desiredWorkspace_ = Workspace::LevelEditor;
                    EnsureActive(application, levelEditor);
                    return;
                }
            }

            if (!authoringSession_.IsLoaded() || !model_.IsLoaded())
            {
                storyFlow.SetWorkspaceActive(false);
                SetLevelControlsActive(false, false);
                desiredWorkspace_ = Workspace::LevelEditor;
                EnsureActive(application, levelEditor);
                return;
            }

            ProcessPendingLevelAction(levelEditor, storyFlow, session, project);

            if (desiredWorkspace_ == Workspace::LevelEditor)
            {
                FlushLayout(true);
                storyFlow.SetWorkspaceActive(false);
                SetLevelControlsActive(false, !activeLevelNodeId_.empty());
                EnsureActive(application, levelEditor);
                return;
            }

            storyFlow.SetWorkspaceActive(true);
            SetLevelControlsActive(true, false);
            EnsureActive(application, storyFlow);
            FlushLayout(false);
        }

    private:
        enum class PendingLevelAction
        {
            None,
            AddNew,
            AddExisting,
            Open,
        };

        template <typename LevelEditor, typename StoryFlowPath>
        void Attach(LevelEditor& levelEditor, StoryFlowPath& storyFlow)
        {
            if (attached_)
                return;

            storyFlow.EnsureLoaded();
            storyFlow.OnLayoutChanged([this]()
            {
                layoutDirty_ = true;
            });
            storyFlow.OnSelectionChanged([this](const bridge::StableId& nodeId)
            {
                bridge::StableId levelId;
                if (model_.IsLoaded())
                {
                    const auto* node = model_.FindNode(nodeId);
                    if (node && node->kind == bridge::FlowNodeKind::Level)
                        levelId = nodeId;
                }
                levelPanel_.SetSelectedLevelNode(std::move(levelId));
            });

            levelPanel_.Create();
            levelPanel_.Attach(storyFlow.GetGUI());
            levelPanel_.SetActive(false);
            levelPanel_.OnAddNew([this](const std::string& name)
            {
                pendingLevelName_ = name;
                pendingLevelAction_ = PendingLevelAction::AddNew;
            });
            levelPanel_.OnAddExisting([this](const std::string& name)
            {
                pendingLevelName_ = name;
                wi::helper::FileDialogParams params;
                params.type = wi::helper::FileDialogParams::OPEN;
                params.description = "Add Existing Renegade Level";
                params.extensions.push_back("wiscene");
                wi::helper::FileDialog(params,
                    [this](const std::string& selectedPath)
                    {
                        if (selectedPath.empty()) return;
                        pendingExistingScenePath_ = selectedPath;
                        pendingLevelAction_ = PendingLevelAction::AddExisting;
                    });
            });
            levelPanel_.OnOpen([this](const bridge::StableId& nodeId)
            {
                pendingLevelNodeId_ = nodeId;
                pendingLevelAction_ = PendingLevelAction::Open;
            });

            returnToStoryFlowButton_.Create("Return to Story Flow");
            returnToStoryFlowButton_.SetText("< STORY FLOW");
            returnToStoryFlowButton_.SetTooltip(
                "Save this Level normally, then return to the same Story Flow authoring session.");
            returnToStoryFlowButton_.SetShadowRadius(0.0f);
            returnToStoryFlowButton_.SetVisible(false);
            returnToStoryFlowButton_.SetEnabled(false);
            returnToStoryFlowButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                pendingLevelAction_ = PendingLevelAction::None;
                desiredWorkspace_ = Workspace::StoryFlow;
            });
            levelEditor.StoryFlowGui().AddWidget(&returnToStoryFlowButton_);

            lastLayoutWrite_ = std::chrono::steady_clock::now();
            attached_ = true;
        }

        template <typename LevelEditor>
        void LayoutReturnButton(LevelEditor& levelEditor)
        {
            if (!attached_) return;
            const XMFLOAT4 bounds = levelEditor.StoryFlowWorkspaceBounds();
            returnToStoryFlowButton_.SetPos(XMFLOAT2(
                bounds.x + 12.0f,
                bounds.y + 12.0f));
            returnToStoryFlowButton_.SetSize(XMFLOAT2(132.0f, 30.0f));
        }

        void SetLevelControlsActive(
            const bool storyFlowActive,
            const bool levelEditorActive)
        {
            levelPanel_.SetActive(storyFlowActive);
            returnToStoryFlowButton_.SetVisible(levelEditorActive);
            returnToStoryFlowButton_.SetEnabled(levelEditorActive);
        }

        template <typename Application, typename Path>
        static void EnsureActive(Application& application, Path& path)
        {
            if (application.GetActivePath() != &path)
                application.ActivatePath(&path);
        }

        static std::string Trim(std::string value)
        {
            const auto whitespace = [](const unsigned char c)
            {
                return std::isspace(c) != 0;
            };
            while (!value.empty() && whitespace(value.front())) value.erase(value.begin());
            while (!value.empty() && whitespace(value.back())) value.pop_back();
            return value;
        }

        static std::string SafeLevelFileStem(std::string name)
        {
            name = Trim(std::move(name));
            std::string result;
            bool previousDash = false;
            for (const unsigned char c : name)
            {
                if (std::isalnum(c))
                {
                    result.push_back(static_cast<char>(c));
                    previousDash = false;
                }
                else if (!previousDash && !result.empty())
                {
                    result.push_back('-');
                    previousDash = true;
                }
            }
            while (!result.empty() && result.back() == '-') result.pop_back();
            if (result.empty()) result = "Level";
            return result;
        }

        template <typename LevelEditor, typename StoryFlowPath, typename Session, typename Project>
        void ProcessPendingLevelAction(
            LevelEditor& levelEditor,
            StoryFlowPath& storyFlow,
            Session& session,
            const Project& project)
        {
            const PendingLevelAction action = pendingLevelAction_;
            if (action == PendingLevelAction::None) return;
            pendingLevelAction_ = PendingLevelAction::None;

            if (action == PendingLevelAction::AddNew)
            {
                const std::string levelName = Trim(pendingLevelName_);
                if (levelName.empty())
                {
                    ReportSemanticFailure("Add New Level requires a Level name.");
                    return;
                }

                bridge::StoryFlowNewLevelRequest request;
                request.projectRoot = project.rootPath;
                request.projectId = project.projectId;
                request.flowPath = authoringSession_.FilePath();
                request.flow = authoringSession_.Document();
                request.levelName = levelName;
                request.scenePathHint = "Content/Scenes/" +
                    SafeLevelFileStem(levelName) + ".wiscene";

                bridge::StoryFlowLevelLifecycleService service;
                const auto result = service.CreateNewLevel(request);
                if (!result.succeeded)
                {
                    ReportSemanticFailure("Add New Level failed: " + result.message);
                    return;
                }
                if (!ReloadAfterLevelMutation(project, storyFlow, result.levelNodeId))
                    return;
                wi::backlog::post(
                    "Renegade Story Flow: created governed Level '" + levelName +
                    "' -> " + result.scenePathHint,
                    wi::backlog::LogLevel::Default);
                return;
            }

            if (action == PendingLevelAction::AddExisting)
            {
                const std::string levelName = Trim(pendingLevelName_);
                if (levelName.empty())
                {
                    ReportSemanticFailure("Add Existing Level requires a Level name.");
                    return;
                }
                if (pendingExistingScenePath_.empty())
                {
                    ReportSemanticFailure("Add Existing Level did not receive a WISCENE path.");
                    return;
                }

                bridge::StoryFlowExistingLevelRequest request;
                request.projectRoot = project.rootPath;
                request.projectId = project.projectId;
                request.flowPath = authoringSession_.FilePath();
                request.flow = authoringSession_.Document();
                request.levelName = levelName;
                request.scenePath = pendingExistingScenePath_;
                pendingExistingScenePath_.clear();

                bridge::StoryFlowLevelReferenceService service;
                const auto result = service.AddExistingLevel(request);
                if (!result.succeeded)
                {
                    ReportSemanticFailure("Add Existing Level failed: " + result.message);
                    return;
                }
                if (!ReloadAfterLevelMutation(project, storyFlow, result.levelNodeId))
                    return;
                wi::backlog::post(
                    "Renegade Story Flow: adopted governed Level '" + levelName +
                    "' -> " + result.scenePathHint,
                    wi::backlog::LogLevel::Default);
                return;
            }

            if (action == PendingLevelAction::Open)
            {
                bridge::StoryFlowLevelReferenceService service;
                const auto resolved = service.ResolveLevel(
                    project.rootPath,
                    project.projectId,
                    authoringSession_.Document(),
                    pendingLevelNodeId_);
                if (!resolved.succeeded)
                {
                    ReportSemanticFailure("Open Level failed: " + resolved.message);
                    pendingLevelNodeId_.clear();
                    return;
                }

                FlushLayout(true);
                if (!session.LoadScene(resolved.resolvedPath))
                {
                    ReportSemanticFailure(
                        "Open Level failed while loading WISCENE: " +
                        session.Scenes().LastError());
                    pendingLevelNodeId_.clear();
                    return;
                }

                activeLevelNodeId_ = pendingLevelNodeId_;
                pendingLevelNodeId_.clear();
                desiredWorkspace_ = Workspace::LevelEditor;
                levelEditor.RefreshHierarchy();
                levelEditor.RefreshInspector();
                levelEditor.RefreshStatus();
                wi::backlog::post(
                    resolved.pathHintMoved
                        ? "Renegade Story Flow: Level opened by stable identity after move; stored path hint is stale."
                        : "Renegade Story Flow: Level opened in the 3D Level Editor.",
                    resolved.pathHintMoved
                        ? wi::backlog::LogLevel::Warning
                        : wi::backlog::LogLevel::Default);
            }
        }

        template <typename Project, typename StoryFlowPath>
        bool ReloadAfterLevelMutation(
            const Project& project,
            StoryFlowPath& storyFlow,
            const bridge::StableId& createdNodeId)
        {
            std::string error;
            if (!authoringSession_.Open(
                    authoringSession_.FilePath(), project.projectId, error))
            {
                ReportSemanticFailure(
                    "Level transaction committed but Story Flow reload failed: " + error);
                return false;
            }
            if (!model_.Load(authoringSession_.Document(), project.projectId, error))
            {
                ReportSemanticFailure(
                    "Level transaction committed but Story Flow model reload failed: " + error);
                return false;
            }
            if (!bridge::ReconcileStoryFlowLayout(
                    model_, project.projectId, project.startupFlowId, layout_, error))
            {
                layout_ = bridge::BuildDeterministicStoryFlowLayout(
                    model_, project.projectId, project.startupFlowId);
            }
            storyFlow.Bind(&authoringSession_, &model_, &layout_);
            layoutDirty_ = true;
            FlushLayout(true);
            const auto* node = model_.FindNode(createdNodeId);
            levelPanel_.SetSelectedLevelNode(
                node && node->kind == bridge::FlowNodeKind::Level
                    ? createdNodeId : bridge::StableId{});
            return true;
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

            if (!authoringSession_.Open(
                    resolvedFlowPath,
                    project.projectId,
                    error))
            {
                ReportSemanticFailure(
                    "could not open startup_flow authoring session '" +
                    resolvedFlowPath + "': " + error);
                return false;
            }

            if (!model_.Load(
                    authoringSession_.Document(),
                    project.projectId,
                    error))
            {
                ReportSemanticFailure(
                    "startup_flow failed Story Flow presentation validation: " +
                    error);
                authoringSession_.Clear();
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
                        "could not inspect saved layout; using deterministic layout: " +
                        existsError.message());
                }
            }

            if (!restoredSavedLayout)
            {
                layout_ = bridge::BuildDeterministicStoryFlowLayout(
                    model_, project.projectId, project.startupFlowId);
            }

            if (!bridge::ReconcileStoryFlowLayout(
                    model_, project.projectId, project.startupFlowId,
                    layout_, error))
            {
                ReportLayoutWarning(
                    "layout reconciliation failed and was rebuilt: " + error);
                layout_ = bridge::BuildDeterministicStoryFlowLayout(
                    model_, project.projectId, project.startupFlowId);
            }

            storyFlow.Bind(&authoringSession_, &model_, &layout_);
            if (!restoredSavedLayout)
                storyFlow.FitToContent();
            layoutDirty_ = !restoredSavedLayout;
            if (layoutDirty_) FlushLayout(true);

            wi::backlog::post(
                "Renegade Story Flow: editable Graph session ready for " +
                    resolvedFlowPath,
                wi::backlog::LogLevel::Default);
            return true;
        }

        template <typename StoryFlowPath>
        void ResetActiveFlow(StoryFlowPath& storyFlow)
        {
            storyFlow.Clear();
            authoringSession_.Clear();
            model_.Clear();
            layout_ = {};
            layoutPath_.clear();
            trackedProjectId_.clear();
            trackedFlowId_.clear();
            activeLevelNodeId_.clear();
            pendingLevelNodeId_.clear();
            pendingExistingScenePath_.clear();
            pendingLevelAction_ = PendingLevelAction::None;
            loadAttempted_ = false;
            layoutDirty_ = false;
            levelPanel_.SetSelectedLevelNode({});
        }

        void FlushLayout(const bool force)
        {
            if (!layoutDirty_ || layoutPath_.empty() || !model_.IsLoaded())
                return;

            const auto now = std::chrono::steady_clock::now();
            if (!force &&
                now - lastLayoutWrite_ < std::chrono::milliseconds(250))
                return;

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

        bridge::StoryFlowAuthoringSession authoringSession_;
        bridge::StoryFlowAuthoringModel model_;
        bridge::StoryFlowLayoutDocument layout_;
        RenegadeStoryFlowLevelPanel levelPanel_;
        RenegadeButton returnToStoryFlowButton_;
        bridge::StableId trackedProjectId_;
        bridge::StableId trackedFlowId_;
        bridge::StableId activeLevelNodeId_;
        bridge::StableId pendingLevelNodeId_;
        std::string layoutPath_;
        std::string pendingLevelName_;
        std::string pendingExistingScenePath_;
        std::chrono::steady_clock::time_point lastLayoutWrite_{};
        Workspace desiredWorkspace_ = Workspace::StoryFlow;
        PendingLevelAction pendingLevelAction_ = PendingLevelAction::None;
        bool attached_ = false;
        bool loadAttempted_ = false;
        bool layoutDirty_ = false;
        bool hubOwnedLastTick_ = true;
    };
}
