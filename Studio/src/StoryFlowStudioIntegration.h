#pragma once

#include "renegade/bridge/StoryFlowAuthoringModel.h"
#include "renegade/bridge/StoryFlowAuthoringSession.h"
#include "renegade/bridge/StoryFlowLayoutService.h"
#include "renegade/bridge/StoryFlowLevelLifecycleService.h"
#include "renegade/bridge/StoryFlowLevelReferenceService.h"
#include "renegade/bridge/StoryFlowScreenLifecycleService.h"
#include "renegade/bridge/StoryFlowScreenReferenceService.h"
#include "RenegadeStoryFlowLevelPanel.h"
#include "RenegadeStoryFlowScreenPanel.h"
#include "StoryFlowScreenEditorHandoff.h"
#include "RenegadeStudioChrome.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>
#include <utility>

namespace renegade::studio
{
    // Story Flow lifecycle coordinator. Semantic Flow stays in the EngineBridge
    // authoring session while this adapter owns first-class Studio content
    // lifecycle actions and workspace boundaries. Gate 5 adds governed Screen
    // creation/resolution without constructing Gate 8's visual Screen Editor.
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

        void OnScreenEditorOpen(
            std::function<void(const StoryFlowScreenEditorHandoff&)> callback)
        {
            screenEditorOpen_ = std::move(callback);
        }

        [[nodiscard]] bool HasScreenEditorHandoff() const noexcept
        {
            return screenEditorHandoff_.ready;
        }

        [[nodiscard]] const StoryFlowScreenEditorHandoff&
        ScreenEditorHandoff() const noexcept
        {
            return screenEditorHandoff_;
        }

        void ClearScreenEditorHandoff() noexcept
        {
            screenEditorHandoff_ = {};
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
            const float storyWidth = std::max(1.0f, storyFlow.GetLogicalWidth());
            const float storyHeight = std::max(1.0f, storyFlow.GetLogicalHeight());
            levelPanel_.SetLayout(storyWidth, storyHeight);
            screenPanel_.SetLayout(storyWidth, storyHeight);
            LayoutReturnButton(levelEditor);

            if (!session.Projects().HasProject())
            {
                FlushLayout(true);
                ResetActiveFlow(storyFlow);
                SetContentControlsActive(false, false);
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
                SetContentControlsActive(false, false);
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
                SetContentControlsActive(false, false);
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
                screenEditorHandoff_ = {};
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
                    SetContentControlsActive(false, false);
                    desiredWorkspace_ = Workspace::LevelEditor;
                    EnsureActive(application, levelEditor);
                    return;
                }
            }

            if (!authoringSession_.IsLoaded() || !model_.IsLoaded())
            {
                storyFlow.SetWorkspaceActive(false);
                SetContentControlsActive(false, false);
                desiredWorkspace_ = Workspace::LevelEditor;
                EnsureActive(application, levelEditor);
                return;
            }

            ProcessPendingLevelAction(levelEditor, storyFlow, session, project);
            ProcessPendingScreenAction(storyFlow, project);

            if (desiredWorkspace_ == Workspace::LevelEditor)
            {
                FlushLayout(true);
                storyFlow.SetWorkspaceActive(false);
                SetContentControlsActive(false, !activeLevelNodeId_.empty());
                EnsureActive(application, levelEditor);
                return;
            }

            storyFlow.SetWorkspaceActive(true);
            SetContentControlsActive(true, false);
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

        enum class PendingScreenAction
        {
            None,
            AddNew,
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
                bridge::StableId screenId;
                if (model_.IsLoaded())
                {
                    const auto* node = model_.FindNode(nodeId);
                    if (node && node->kind == bridge::FlowNodeKind::Level)
                        levelId = nodeId;
                    else if (node && node->kind == bridge::FlowNodeKind::Screen)
                        screenId = nodeId;
                }
                levelPanel_.SetSelectedLevelNode(std::move(levelId));
                screenPanel_.SetSelectedScreenNode(std::move(screenId));
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

            screenPanel_.Create();
            screenPanel_.Attach(storyFlow.GetGUI());
            screenPanel_.SetActive(false);
            screenPanel_.OnAddNew([this](
                const std::string& name,
                const bridge::StoryFlowScreenTemplate screenTemplate)
            {
                pendingScreenName_ = name;
                pendingScreenTemplate_ = screenTemplate;
                pendingScreenAction_ = PendingScreenAction::AddNew;
            });
            screenPanel_.OnOpen([this](const bridge::StableId& nodeId)
            {
                pendingScreenNodeId_ = nodeId;
                pendingScreenAction_ = PendingScreenAction::Open;
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

        void SetContentControlsActive(
            const bool storyFlowActive,
            const bool levelEditorActive)
        {
            levelPanel_.SetActive(storyFlowActive);
            screenPanel_.SetActive(storyFlowActive);
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

        static std::string SafeFileStem(
            std::string name,
            const char* fallback)
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
            if (result.empty()) result = fallback;
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
                    SafeFileStem(levelName, "Level") + ".wiscene";

                bridge::StoryFlowLevelLifecycleService service;
                const auto result = service.CreateNewLevel(request);
                if (!result.succeeded)
                {
                    ReportSemanticFailure("Add New Level failed: " + result.message);
                    return;
                }
                if (!ReloadAfterContentMutation(project, storyFlow, result.levelNodeId))
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
                if (!ReloadAfterContentMutation(project, storyFlow, result.levelNodeId))
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

        template <typename StoryFlowPath, typename Project>
        void ProcessPendingScreenAction(
            StoryFlowPath& storyFlow,
            const Project& project)
        {
            const PendingScreenAction action = pendingScreenAction_;
            if (action == PendingScreenAction::None) return;
            pendingScreenAction_ = PendingScreenAction::None;

            if (action == PendingScreenAction::AddNew)
            {
                const std::string screenName = Trim(pendingScreenName_);
                if (screenName.empty())
                {
                    ReportSemanticFailure("Add Screen requires a Screen name.");
                    return;
                }

                bridge::StoryFlowNewScreenRequest request;
                request.projectRoot = project.rootPath;
                request.projectId = project.projectId;
                request.flowPath = authoringSession_.FilePath();
                request.flow = authoringSession_.Document();
                request.screenName = screenName;
                request.screenPathHint = "Content/Screens/" +
                    SafeFileStem(screenName, "Screen") + ".renegade-screen";
                request.screenTemplate = pendingScreenTemplate_;

                bridge::StoryFlowScreenLifecycleService service;
                const auto result = service.CreateNewScreen(request);
                if (!result.succeeded)
                {
                    ReportSemanticFailure("Add Screen failed: " + result.message);
                    return;
                }
                if (!ReloadAfterContentMutation(project, storyFlow, result.screenNodeId))
                    return;

                wi::backlog::post(
                    "Renegade Story Flow: created governed " +
                    std::string(bridge::StoryFlowScreenTemplateName(pendingScreenTemplate_)) +
                    " Screen '" + screenName + "' -> " + result.screenPathHint,
                    wi::backlog::LogLevel::Default);
                return;
            }

            if (action == PendingScreenAction::Open)
            {
                bridge::StoryFlowScreenReferenceService service;
                const auto resolved = service.ResolveScreen(
                    project.rootPath,
                    project.projectId,
                    authoringSession_.Document(),
                    pendingScreenNodeId_);
                if (!resolved.succeeded)
                {
                    ReportSemanticFailure("Open Screen failed: " + resolved.message);
                    pendingScreenNodeId_.clear();
                    return;
                }

                StoryFlowScreenEditorHandoff handoff;
                std::string error;
                if (!BuildStoryFlowScreenEditorHandoff(resolved, handoff, error))
                {
                    ReportSemanticFailure("Open Screen handoff failed: " + error);
                    pendingScreenNodeId_.clear();
                    return;
                }

                screenEditorHandoff_ = std::move(handoff);
                pendingScreenNodeId_.clear();
                if (screenEditorOpen_)
                    screenEditorOpen_(screenEditorHandoff_);

                wi::backlog::post(
                    resolved.pathHintMoved
                        ? "Renegade Story Flow: Screen Editor handoff resolved by stable identity after move; stored path hint is stale."
                        : "Renegade Story Flow: Screen Editor handoff ready. Gate 8 will consume this boundary.",
                    resolved.pathHintMoved
                        ? wi::backlog::LogLevel::Warning
                        : wi::backlog::LogLevel::Default);
            }
        }

        template <typename Project, typename StoryFlowPath>
        bool ReloadAfterContentMutation(
            const Project& project,
            StoryFlowPath& storyFlow,
            const bridge::StableId& createdNodeId)
        {
            std::string error;
            if (!authoringSession_.Open(
                    authoringSession_.FilePath(), project.projectId, error))
            {
                ReportSemanticFailure(
                    "Content transaction committed but Story Flow reload failed: " + error);
                return false;
            }
            if (!model_.Load(authoringSession_.Document(), project.projectId, error))
            {
                ReportSemanticFailure(
                    "Content transaction committed but Story Flow model reload failed: " + error);
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
            screenPanel_.SetSelectedScreenNode(
                node && node->kind == bridge::FlowNodeKind::Screen
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
            pendingScreenNodeId_.clear();
            pendingExistingScenePath_.clear();
            pendingLevelAction_ = PendingLevelAction::None;
            pendingScreenAction_ = PendingScreenAction::None;
            screenEditorHandoff_ = {};
            loadAttempted_ = false;
            layoutDirty_ = false;
            levelPanel_.SetSelectedLevelNode({});
            screenPanel_.SetSelectedScreenNode({});
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
        RenegadeStoryFlowScreenPanel screenPanel_;
        RenegadeButton returnToStoryFlowButton_;
        StoryFlowScreenEditorHandoff screenEditorHandoff_;
        std::function<void(const StoryFlowScreenEditorHandoff&)> screenEditorOpen_;
        bridge::StableId trackedProjectId_;
        bridge::StableId trackedFlowId_;
        bridge::StableId activeLevelNodeId_;
        bridge::StableId pendingLevelNodeId_;
        bridge::StableId pendingScreenNodeId_;
        std::string layoutPath_;
        std::string pendingLevelName_;
        std::string pendingScreenName_;
        std::string pendingExistingScenePath_;
        std::chrono::steady_clock::time_point lastLayoutWrite_{};
        Workspace desiredWorkspace_ = Workspace::StoryFlow;
        PendingLevelAction pendingLevelAction_ = PendingLevelAction::None;
        PendingScreenAction pendingScreenAction_ = PendingScreenAction::None;
        bridge::StoryFlowScreenTemplate pendingScreenTemplate_ =
            bridge::StoryFlowScreenTemplate::Title;
        bool attached_ = false;
        bool loadAttempted_ = false;
        bool layoutDirty_ = false;
        bool hubOwnedLastTick_ = true;
    };
}
