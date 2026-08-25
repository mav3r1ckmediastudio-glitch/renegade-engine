#pragma once

#include "renegade/bridge/StoryFlowAuthoringModel.h"
#include "renegade/bridge/StoryFlowAuthoringSession.h"
#include "renegade/bridge/StoryFlowInteractionPolicy.h"
#include "renegade/bridge/StoryFlowLayoutService.h"
#include "renegade/bridge/StoryFlowLevelLifecycleService.h"
#include "renegade/bridge/StoryFlowLevelReferenceService.h"
#include "renegade/bridge/StoryFlowProjectHomeService.h"
#include "renegade/bridge/StoryFlowScreenLifecycleService.h"
#include "renegade/bridge/StoryFlowScreenReferenceService.h"
#include "StoryFlowScreenEditorHandoff.h"
#include "RenegadeStudioChrome.h"
#include "WindowsGameBuildController.h"

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
    // lifecycle actions and workspace boundaries. Gate 6 adds shared
    // Journey/Graph activation and creation focus without constructing Gate 8's
    // visual Screen Editor.
    class StoryFlowStudioIntegration final
    {
    public:
        enum class Workspace
        {
            LevelEditor,
            StoryFlow,
            ScreenEditor,
        };

        void RequestStoryFlow() noexcept
        {
            if (desiredWorkspace_ == Workspace::ScreenEditor)
                screenOutcomeAuditPending_ = true;
            desiredWorkspace_ = Workspace::StoryFlow;
        }

        void RequestLevelEditor() noexcept
        {
            desiredWorkspace_ = Workspace::LevelEditor;
        }

        void RequestScreenEditor() noexcept
        {
            desiredWorkspace_ = Workspace::ScreenEditor;
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

        template <typename Application, typename LevelEditor, typename StoryFlowPath,
            typename ScreenEditorPath, typename Session>
        void Tick(
            Application& application,
            LevelEditor& levelEditor,
            StoryFlowPath& storyFlow,
            ScreenEditorPath& screenEditor,
            Session& session)
        {
            Attach(levelEditor, storyFlow);
            storyFlow.SyncCanvas(application.canvas);
            screenEditor.SyncCanvas(application.canvas);
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
            storyFlow.SetProjectName(project.name);
            const bool hasStartupFlow =
                bridge::IsValidStableId(project.startupFlowId) &&
                !project.startupFlow.empty();
            if (!hasStartupFlow)
            {
                // Story Flow is the Renegade project home. Scene-first projects
                // are legacy state to migrate, never a reason to route creators
                // back into the Level Editor. Bootstrap a canonical Flow,
                // adopt the existing startup scene as its first Level, update
                // the descriptor transactionally, then refresh ProjectService.
                bridge::StoryFlowProjectHomeService projectHome;
                const auto ensured = projectHome.Ensure(project);
                if (!ensured.succeeded)
                {
                    FlushLayout(true);
                    ResetActiveFlow(storyFlow);
                    storyFlow.SetWorkspaceActive(false);
                    SetContentControlsActive(false, false);
                    ReportSemanticFailure(
                        "could not establish Story Flow project home: " +
                        ensured.message);
                    desiredWorkspace_ = Workspace::LevelEditor;
                    hubOwnedLastTick_ = false;
                    EnsureActive(application, levelEditor);
                    return;
                }

                if (!session.Projects().RefreshCurrentProject())
                {
                    FlushLayout(true);
                    ResetActiveFlow(storyFlow);
                    storyFlow.SetWorkspaceActive(false);
                    SetContentControlsActive(false, false);
                    ReportSemanticFailure(
                        "Story Flow project home was committed but the project "
                        "descriptor could not be refreshed: " +
                        session.Projects().LastError());
                    desiredWorkspace_ = Workspace::LevelEditor;
                    hubOwnedLastTick_ = false;
                    EnsureActive(application, levelEditor);
                    return;
                }

                const auto& refreshedProject =
                    session.Projects().CurrentProject();
                if (refreshedProject.startupFlowId != ensured.flowDocumentId ||
                    refreshedProject.startupFlow != ensured.flowPathHint)
                {
                    FlushLayout(true);
                    ResetActiveFlow(storyFlow);
                    storyFlow.SetWorkspaceActive(false);
                    SetContentControlsActive(false, false);
                    ReportSemanticFailure(
                        "Story Flow project home was committed but the active "
                        "project did not adopt its stable Flow identity.");
                    desiredWorkspace_ = Workspace::LevelEditor;
                    hubOwnedLastTick_ = false;
                    EnsureActive(application, levelEditor);
                    return;
                }

                FlushLayout(true);
                ResetActiveFlow(storyFlow);
                desiredWorkspace_ = Workspace::StoryFlow;
                hubOwnedLastTick_ = false;
                wi::backlog::post(
                    ensured.adoptedStartupScene
                        ? "Renegade Story Flow: project home created; existing startup Level adopted."
                        : "Renegade Story Flow: project home created.",
                    wi::backlog::LogLevel::Default);
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
                screenEditor.Clear();
            }
            else if (hubOwnedLastTick_)
            {
                desiredWorkspace_ = Workspace::StoryFlow;
            }
            hubOwnedLastTick_ = false;
            activeProjectRoot_ = project.rootPath;

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

            // Project-level actions belong to the project home, not the Level
            // Editor render path. Keep their Runtime/build lifecycle alive while
            // Story Flow remains the active surface.
            ProcessProjectCommands(levelEditor, storyFlow);
            ProcessPendingLevelAction(levelEditor, storyFlow, session, project);
            ProcessPendingScreenAction(storyFlow, project);

            if (screenOutcomeAuditPending_ &&
                desiredWorkspace_ == Workspace::StoryFlow)
            {
                screenOutcomeAuditPending_ = false;
                AuditScreenOutcomeParity(
                    project, storyFlow, screenEditorHandoff_.screenNodeId);
            }

            if (desiredWorkspace_ == Workspace::ScreenEditor)
            {
                FlushLayout(true);
                storyFlow.SetWorkspaceActive(false);
                SetContentControlsActive(false, false);
                EnsureActive(application, screenEditor);
                return;
            }

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
            storyFlow.OnSelectionChanged([](const bridge::StableId&) {});
            storyFlow.OnSemanticChanged([this]()
            {
                screenOutcomeAuditPending_ = true;
            });
            storyFlow.OnScreenOutcomeQuery([this](
                const bridge::StableId& nodeId,
                std::vector<std::string>& outcomes,
                std::string& error)
            {
                outcomes.clear();
                if (activeProjectRoot_.empty() || !authoringSession_.IsLoaded())
                {
                    error = "No active Story Flow project is available for Screen outcome resolution.";
                    return false;
                }
                bridge::StoryFlowScreenReferenceService service;
                const auto resolved = service.ResolveScreen(
                    activeProjectRoot_,
                    authoringSession_.ProjectId(),
                    authoringSession_.Document(),
                    nodeId);
                if (!resolved.succeeded)
                {
                    error = resolved.message;
                    return false;
                }
                outcomes = resolved.actionIds;
                error.clear();
                return true;
            });
            storyFlow.OnNodeActivated([this](const bridge::StableId& nodeId)
            {
                QueueOpenDestination(nodeId);
            });
            storyFlow.OnJourneyShellAction(
                [this, &levelEditor, &storyFlow](
                    const RenegadeStoryFlowJourneyChrome::Action action)
                {
                    using Action = RenegadeStoryFlowJourneyChrome::Action;
                    switch (action)
                    {
                    case Action::Hub:
                    case Action::ProjectSelector:
                        levelEditor.RequestProjectHubFromStoryFlow();
                        desiredWorkspace_ = Workspace::LevelEditor;
                        break;
                    case Action::Assets:
                        levelEditor.RequestAssetBrowserFromStoryFlow();
                        desiredWorkspace_ = Workspace::LevelEditor;
                        break;
                    case Action::Variables:
                        storyFlow.SetExternalStatus(
                            "VARIABLES // PROJECT VARIABLE WORKSPACE NOT YET AVAILABLE");
                        break;
                    case Action::TestPlay:
                        pendingProjectPlay_ = true;
                        storyFlow.SetExternalStatus(
                            levelEditor.IsProjectPlayFromStoryFlowActive()
                                ? "TEST GAME // STOP REQUESTED"
                                : "TEST GAME // QUEUED // SAVED STORY FLOW");
                        break;
                    case Action::BuildGame:
                        pendingBuildGame_ = true;
                        buildGameBusy_ = true;
                        buildGameDispatchArmed_ = false;
                        storyFlow.SetProjectCommandState(
                            levelEditor.IsProjectPlayFromStoryFlowActive(), true);
                        storyFlow.SetExternalStatus(
                            "BUILD GAME // QUEUED // SAVED STORY FLOW");
                        break;
                    case Action::Settings:
                        storyFlow.SetExternalStatus(
                            "SETTINGS // NO JOURNEY-SPECIFIC SETTINGS YET");
                        break;
                    case Action::MainMenu:
                        storyFlow.SetExternalStatus(
                            "MAIN MENU // GLOBAL MENU SURFACE NOT YET AVAILABLE");
                        break;
                    default:
                        break;
                    }
                });

            storyFlow.OnCreateLevel([this](const std::string& name)
            {
                pendingLevelName_ = name;
                pendingLevelAction_ = PendingLevelAction::AddNew;
            });
            storyFlow.OnAdoptLevel([this](const std::string& name)
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
            storyFlow.OnCreateScreen([this](
                const std::string& name,
                const bridge::StoryFlowScreenTemplate screenTemplate)
            {
                pendingScreenName_ = name;
                pendingScreenTemplate_ = screenTemplate;
                pendingScreenAction_ = PendingScreenAction::AddNew;
            });
            storyFlow.OnOpenSelectedDestination(
                [this](const bridge::StableId& nodeId)
            {
                QueueOpenDestination(nodeId);
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
            const bool,
            const bool levelEditorActive)
        {
            returnToStoryFlowButton_.SetVisible(levelEditorActive);
            returnToStoryFlowButton_.SetEnabled(levelEditorActive);
        }

        void QueueOpenDestination(const bridge::StableId& nodeId)
        {
            if (!model_.IsLoaded()) return;
            const auto* node = model_.FindNode(nodeId);
            if (!node) return;

            const auto target =
                bridge::StoryFlowActivationTargetForKind(node->kind);
            if (target == bridge::StoryFlowActivationTarget::LevelEditor)
            {
                pendingLevelNodeId_ = nodeId;
                pendingLevelAction_ = PendingLevelAction::Open;
            }
            else if (target == bridge::StoryFlowActivationTarget::ScreenEditor)
            {
                pendingScreenNodeId_ = nodeId;
                pendingScreenAction_ = PendingScreenAction::Open;
            }
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

        template <typename LevelEditor, typename StoryFlowPath>
        void ProcessProjectCommands(
            LevelEditor& levelEditor,
            StoryFlowPath& storyFlow)
        {
            if (pendingProjectPlay_)
            {
                pendingProjectPlay_ = false;
                if (levelEditor.IsProjectPlayFromStoryFlowActive())
                {
                    levelEditor.StopProjectPlayFromStoryFlowNow();
                    storyFlow.SetExternalStatus("TEST GAME // STOPPED");
                }
                else
                {
                    storyFlow.SetExternalStatus(
                        "TEST GAME // STARTING GOVERNED PROJECT RUNTIME");
                    levelEditor.StartProjectPlayFromStoryFlowNow();
                    if (!levelEditor.IsProjectPlayFromStoryFlowActive())
                    {
                        const auto& result =
                            levelEditor.ProjectPlayFromStoryFlowResult();
                        storyFlow.SetExternalStatus(
                            result.message.empty()
                                ? "TEST GAME // LAUNCH FAILED"
                                : "TEST GAME // LAUNCH FAILED // " +
                                    result.message);
                    }
                }
            }

            if (levelEditor.IsProjectPlayFromStoryFlowActive())
            {
                levelEditor.PollProjectPlayFromStoryFlow();
                if (levelEditor.IsProjectPlayFromStoryFlowActive())
                {
                    storyFlow.SetExternalStatus(
                        levelEditor.IsProjectPlayFromStoryFlowRunning()
                            ? "TEST GAME // RUNNING // CLICK STOP GAME TO END"
                            : "TEST GAME // STARTING");
                }
                else
                {
                    const auto& result =
                        levelEditor.ProjectPlayFromStoryFlowResult();
                    storyFlow.SetExternalStatus(
                        result.succeeded
                            ? "TEST GAME // COMPLETED"
                            : (result.message.empty()
                                ? "TEST GAME // FAILED"
                                : "TEST GAME // FAILED // " + result.message));
                }
            }

            // Give BUILD GAME one complete rendered frame in its queued/busy
            // state before starting the synchronous existing build controller.
            // This avoids another apparently dead control during a long build.
            if (pendingBuildGame_ && !buildGameDispatchArmed_)
            {
                buildGameDispatchArmed_ = true;
                storyFlow.SetProjectCommandState(
                    levelEditor.IsProjectPlayFromStoryFlowActive(), true);
                storyFlow.SetExternalStatus(
                    "BUILD GAME // QUEUED // STARTING NEXT FRAME");
                return;
            }

            if (pendingBuildGame_ && buildGameDispatchArmed_)
            {
                pendingBuildGame_ = false;
                buildGameDispatchArmed_ = false;
                if (levelEditor.IsProjectPlayFromStoryFlowActive())
                {
                    buildGameBusy_ = false;
                    storyFlow.SetExternalStatus(
                        "BUILD GAME // BLOCKED // TEST GAME RUNNING");
                }
                else
                {
                    storyFlow.SetExternalStatus(
                        "BUILD GAME // BUILDING WINDOWS GAME");
                    const WindowsGameBuildUiResult build =
                        BuildActiveWindowsGame();
                    buildGameBusy_ = false;
                    storyFlow.SetExternalStatus(
                        build.succeeded
                            ? "BUILD GAME // COMPLETE // " +
                                build.finalOutputPath
                            : "BUILD GAME // FAILED // " + build.message);
                }
            }

            storyFlow.SetProjectCommandState(
                levelEditor.IsProjectPlayFromStoryFlowActive(),
                buildGameBusy_);
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
                    storyFlow.SetExternalStatus("LEVEL CREATE REJECTED // LEVEL NAME REQUIRED");
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

                storyFlow.SetExternalStatus("CREATING LEVEL // " + levelName);
                bridge::StoryFlowLevelLifecycleService service;
                const auto result = service.CreateNewLevel(request);
                if (!result.succeeded)
                {
                    storyFlow.SetExternalStatus("LEVEL CREATE FAILED // " + result.message);
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
                    storyFlow.SetExternalStatus("LEVEL ADOPT REJECTED // LEVEL NAME REQUIRED");
                    ReportSemanticFailure("Add Existing Level requires a Level name.");
                    return;
                }
                if (pendingExistingScenePath_.empty())
                {
                    storyFlow.SetExternalStatus("LEVEL ADOPT FAILED // WISCENE PATH MISSING");
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

                storyFlow.SetExternalStatus("ADOPTING LEVEL // " + levelName);
                bridge::StoryFlowLevelReferenceService service;
                const auto result = service.AddExistingLevel(request);
                if (!result.succeeded)
                {
                    storyFlow.SetExternalStatus("LEVEL ADOPT FAILED // " + result.message);
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
                    storyFlow.SetExternalStatus("OPEN LEVEL FAILED // " + resolved.message);
                    ReportSemanticFailure("Open Level failed: " + resolved.message);
                    pendingLevelNodeId_.clear();
                    return;
                }

                FlushLayout(true);
                if (!session.LoadScene(resolved.resolvedPath))
                {
                    storyFlow.SetExternalStatus(
                        "OPEN LEVEL FAILED // " + session.Scenes().LastError());
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
                    storyFlow.SetExternalStatus("SCREEN CREATE REJECTED // SCREEN NAME REQUIRED");
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

                storyFlow.SetExternalStatus("CREATING SCREEN // " + screenName);
                bridge::StoryFlowScreenLifecycleService service;
                const auto result = service.CreateNewScreen(request);
                if (!result.succeeded)
                {
                    storyFlow.SetExternalStatus("SCREEN CREATE FAILED // " + result.message);
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
                    storyFlow.SetExternalStatus("OPEN SCREEN FAILED // " + resolved.message);
                    ReportSemanticFailure("Open Screen failed: " + resolved.message);
                    pendingScreenNodeId_.clear();
                    return;
                }

                StoryFlowScreenEditorHandoff handoff;
                std::string error;
                if (!BuildStoryFlowScreenEditorHandoff(resolved, handoff, error))
                {
                    storyFlow.SetExternalStatus("OPEN SCREEN FAILED // " + error);
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
                        : "Renegade Story Flow: Screen Editor handoff opened the governed Screen.",
                    resolved.pathHintMoved
                        ? wi::backlog::LogLevel::Warning
                        : wi::backlog::LogLevel::Default);
            }
        }

        template <typename Project, typename StoryFlowPath>
        bool AuditScreenOutcomeParity(
            const Project& project,
            StoryFlowPath& storyFlow,
            const bridge::StableId& preferredNodeId)
        {
            if (!authoringSession_.IsLoaded()) return false;

            bridge::StoryFlowScreenReferenceService service;
            bool sawScreen = false;
            for (const auto& node : authoringSession_.Document().nodes)
            {
                if (node.kind != bridge::FlowNodeKind::Screen) continue;
                sawScreen = true;
                const auto audit = service.AuditScreenOutcomes(
                    project.rootPath,
                    project.projectId,
                    authoringSession_.Document(),
                    node.id);
                if (!audit.succeeded)
                {
                    storyFlow.SelectAndFocusNode(node.id);
                    storyFlow.SetExternalStatus(
                        "SCREEN ROUTING INVALID // " + audit.message);
                    ReportSemanticFailure(
                        "Screen outcome parity failed for '" + node.name +
                        "': " + audit.message);
                    return false;
                }
            }

            if (sawScreen)
            {
                if (!preferredNodeId.empty())
                    storyFlow.SelectAndFocusNode(preferredNodeId);
                storyFlow.SetExternalStatus(
                    "SCREEN OUTCOMES SYNCHRONIZED // STORY FLOW ROUTING READY");
            }
            return true;
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
                const std::string message =
                    "Content transaction committed but Story Flow reload failed: " + error;
                storyFlow.SetExternalStatus("CONTENT RELOAD FAILED // " + error);
                ReportSemanticFailure(message);
                return false;
            }
            if (!model_.Load(authoringSession_.Document(), project.projectId, error))
            {
                const std::string message =
                    "Content transaction committed but Story Flow model reload failed: " + error;
                storyFlow.SetExternalStatus("CONTENT MODEL RELOAD FAILED // " + error);
                ReportSemanticFailure(message);
                return false;
            }

            const auto* createdNode = model_.FindNode(createdNodeId);
            if (!createdNode)
            {
                storyFlow.SetExternalStatus(
                    "CONTENT CREATE FAILED // COMMITTED NODE MISSING AFTER RELOAD");
                ReportSemanticFailure(
                    "Content transaction reported success but the created Story Flow node was missing after reload: " +
                    createdNodeId);
                return false;
            }

            if (!bridge::ReconcileStoryFlowLayout(
                    model_, project.projectId, project.startupFlowId, layout_, error))
            {
                layout_ = bridge::BuildDeterministicStoryFlowLayout(
                    model_, project.projectId, project.startupFlowId);
            }
            storyFlow.Bind(&authoringSession_, &model_, &layout_);
            storyFlow.SelectAndFocusNode(createdNodeId);
            AuditScreenOutcomeParity(project, storyFlow, createdNodeId);
            layoutDirty_ = true;
            FlushLayout(true);

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
            AuditScreenOutcomeParity(project, storyFlow, {});
            if (!restoredSavedLayout)
                storyFlow.FitToContent();
            layoutDirty_ = !restoredSavedLayout;
            if (layoutDirty_) FlushLayout(true);

            wi::backlog::post(
                "Renegade Story Flow: synchronized Journey/Graph session ready for " +
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
            pendingProjectPlay_ = false;
            pendingBuildGame_ = false;
            buildGameDispatchArmed_ = false;
            buildGameBusy_ = false;
            screenEditorHandoff_ = {};
            activeProjectRoot_.clear();
            screenOutcomeAuditPending_ = false;
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
        std::string activeProjectRoot_;
        std::chrono::steady_clock::time_point lastLayoutWrite_{};
        Workspace desiredWorkspace_ = Workspace::StoryFlow;
        PendingLevelAction pendingLevelAction_ = PendingLevelAction::None;
        PendingScreenAction pendingScreenAction_ = PendingScreenAction::None;
        bridge::StoryFlowScreenTemplate pendingScreenTemplate_ =
            bridge::StoryFlowScreenTemplate::Title;
        bool pendingProjectPlay_ = false;
        bool pendingBuildGame_ = false;
        bool buildGameDispatchArmed_ = false;
        bool buildGameBusy_ = false;
        bool attached_ = false;
        bool loadAttempted_ = false;
        bool layoutDirty_ = false;
        bool hubOwnedLastTick_ = true;
        bool screenOutcomeAuditPending_ = false;
    };
}
