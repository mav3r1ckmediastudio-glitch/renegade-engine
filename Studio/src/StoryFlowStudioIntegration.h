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
#include "renegade/bridge/ScreenFlowDestinationPlan.h"
#include "renegade/bridge/ScreenFlowAtomicSaveService.h"
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
#include <unordered_set>
#include <vector>

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
                screenLibraryDirty_ = true;
                screenLibraryActiveNode_.clear();
                screenLibraryActiveDocument_.clear();pendingLibraryDocumentId_.clear();screenLibraryCatalogue_={};
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

            // Reuse StoryFlow's one governed Screen handoff for project-wide
            // Screen navigation. This is a read-only catalogue; no second
            // Screen document index or per-frame Content scan is introduced.
            if (!screenLibraryHooked_)
            {
                screenEditor.OnOpenProjectScreen(
                    [this](const bridge::StableId& nodeId)
                    {
                        QueueOpenDestination(nodeId);
                    });
                screenEditor.OnOpenLibraryDocument([this](const bridge::StableId& documentId)
                {
                    pendingLibraryDocumentId_=documentId;
                    pendingScreenAction_=PendingScreenAction::OpenLibrary;
                });
                screenEditor.OnExternalFlowDirty([this]()
                {
                    return !screenEditorHandoff_.screenNodeId.empty() &&
                        authoringSession_.IsLoaded() && authoringSession_.IsDirty();
                });
                screenEditor.OnCoordinatedSave(
                    [this, &storyFlow](
                        bridge::ScreenAuthoringSession& screen,
                        std::string& error)
                    {
                        if (!screenEditorHandoff_.ready ||
                            activeProjectRoot_.empty() ||
                            !authoringSession_.IsLoaded())
                        {
                            error = "No active StoryFlow project is available for coordinated Save.";
                            return false;
                        }
                        if(screenEditorHandoff_.screenNodeId.empty())
                        {
                            if(!screen.Save(error))return false;
                            storyFlow.SetExternalStatus("LIBRARY SCREEN SAVED");return true;
                        }
                        const auto saved = bridge::ScreenFlowAtomicSaveService::Save(
                            screen, authoringSession_, activeProjectRoot_);
                        if (!saved.succeeded)
                        {
                            error = saved.message;
                            return false;
                        }
                        storyFlow.SetExternalStatus(
                            "SCREEN + STORYFLOW SAVED // COORDINATED TRANSACTION");
                        error.clear();
                        return true;
                    });
                screenEditor.OnOpenScreenRoute(
                    [this, &storyFlow](const bridge::ScreenDocument& screen,
                        const bridge::StableId& buttonId,
                        const bridge::StableId& destinationId,
                        bool& routeChanged,
                        std::string& error)
                    {
                        routeChanged = false;
                        if (!authoringSession_.IsLoaded() ||
                            !screenEditorHandoff_.ready ||
                            (screenEditorHandoff_.screenNodeId.empty() || screen.envelope.documentId!=screenEditorHandoff_.screenDocumentId ||
                             screen.envelope.projectId!=authoringSession_.Document().envelope.projectId))
                        {
                            error = "No authoritative StoryFlow Screen is open.";
                            return false;
                        }
                        const auto plan = bridge::PlanButtonOpenScreen(
                            screen, authoringSession_.Document(),
                            screenEditorHandoff_.screenNodeId,
                            buttonId, destinationId);
                        if (!plan.succeeded)
                        {
                            error = plan.message;
                            return false;
                        }
                        if (plan.noChanges)
                        {
                            error.clear();
                            return true;
                        }
                        // Fail closed if the button's action is new/changed in
                        // an unsaved Screen; Runtime uses persisted Screen IDs.
                        bridge::ScreenDocument persisted;
                        if (!bridge::ReadScreenDocument(
                                screenEditorHandoff_.resolvedPath,
                                screen.envelope.projectId, persisted, error))
                            return false;
                        const auto saved = std::find_if(
                            persisted.widgets.begin(), persisted.widgets.end(),
                            [&](const bridge::ScreenWidget& widget)
                            {
                                return widget.id == buttonId &&
                                    widget.kind == bridge::ScreenWidgetKind::Button;
                            });
                        if (saved == persisted.widgets.end() ||
                            saved->actionId != plan.actionId)
                        {
                            error = "Save the selected Button's Screen action before assigning its StoryFlow destination.";
                            return false;
                        }
                        const auto plannedRoute = std::find_if(
                            plan.candidateFlow.routes.begin(),
                            plan.candidateFlow.routes.end(),
                            [&](const bridge::FlowRoute& route)
                            {
                                return route.id == plan.routeId;
                            });
                        if (plannedRoute == plan.candidateFlow.routes.end())
                        {
                            error = "Validated destination plan lost its route.";
                            return false;
                        }
                        const bool changed = plan.updatingExisting
                            ? authoringSession_.UpdateRoute(
                                plan.routeId, *plannedRoute, error)
                            : [&]()
                            {
                                bridge::StableId createdId;
                                return authoringSession_.AddRoute(
                                    *plannedRoute, createdId, error);
                            }();
                        if (!changed) return false;
                        if (!storyFlow.RefreshAfterExternalRouteMutation())
                        {
                            std::string rollbackError;
                            if (!authoringSession_.Undo(rollbackError))
                                error = "Flow changed but presentation refresh failed and Undo failed: " + rollbackError;
                            else
                                error = "Flow route rolled back after presentation refresh failure.";
                            (void)storyFlow.RefreshAfterExternalRouteMutation();
                            return false;
                        }
                        routeChanged = true;
                        routeHistoryValid_ = true;
                        routeHistoryScreenNodeId_ = screenEditorHandoff_.screenNodeId;
                        routeHistoryExpectedUndo_ = authoringSession_.UndoCount();
                        routeHistoryExpectedRedo_ = authoringSession_.RedoCount();
                        storyFlow.SetExternalStatus(
                            "SCREEN DESTINATION UPDATED // UNSAVED STORYFLOW CHANGE");
                        error.clear();
                        return true;
                    });
                screenEditor.OnRouteHistoryAction(
                    [this, &storyFlow](const bool undo, std::string& error)
                    {
                        if (!routeHistoryValid_ ||
                            !authoringSession_.IsLoaded() ||
                            routeHistoryScreenNodeId_ !=
                                screenEditorHandoff_.screenNodeId ||
                            authoringSession_.UndoCount() !=
                                routeHistoryExpectedUndo_ ||
                            authoringSession_.RedoCount() !=
                                routeHistoryExpectedRedo_)
                        {
                            error = "StoryFlow changed outside this Screen editing timeline; return to StoryFlow to manage history.";
                            return false;
                        }
                        if (undo
                                ? !authoringSession_.Undo(error)
                                : !authoringSession_.Redo(error))
                            return false;
                        if (!storyFlow.RefreshAfterExternalRouteMutation())
                        {
                            std::string restoreError;
                            const bool restored = undo
                                ? authoringSession_.Redo(restoreError)
                                : authoringSession_.Undo(restoreError);
                            (void)storyFlow.RefreshAfterExternalRouteMutation();
                            error = restored
                                ? "StoryFlow presentation failed to refresh; route history was restored."
                                : "StoryFlow presentation failed and history restoration also failed: " + restoreError;
                            routeHistoryValid_ = restored;
                            return false;
                        }
                        routeHistoryExpectedUndo_ = authoringSession_.UndoCount();
                        routeHistoryExpectedRedo_ = authoringSession_.RedoCount();
                        storyFlow.SetExternalStatus(
                            undo ? "SCREEN DESTINATION UNDONE // UNSAVED FLOW"
                                 : "SCREEN DESTINATION REDONE // UNSAVED FLOW");
                        error.clear();
                        return true;
                    });
                screenLibraryHooked_ = true;
            }
            screenEditor.SetProjectDefaultHud(project.defaultHudId);
            const auto contextProject=project.projectId, contextDocument=screenEditorHandoff_.screenDocumentId;
            screenEditor.SetRouteContext(screenEditorHandoff_.screenNodeId,
                [this,contextProject,contextDocument](const bridge::StableId& id) {
                    if(!authoringSession_.IsLoaded() || authoringSession_.Document().envelope.projectId!=contextProject ||
                        screenEditorHandoff_.screenDocumentId!=contextDocument) return false;
                    for(const auto& node:authoringSession_.Document().nodes)
                        if(node.id==id && node.kind==bridge::FlowNodeKind::Screen && node.screenDocumentId==contextDocument)
                        { screenEditorHandoff_.screenNodeId=id; screenLibraryDirty_=true; return true; }
                    return false;
                });
            if (screenLibraryDirty_ || screenLibraryActiveNode_!=screenEditorHandoff_.screenNodeId ||
                screenLibraryActiveDocument_!=screenEditorHandoff_.screenDocumentId)
            {
                screenLibraryCatalogue_=bridge::EnumerateProjectScreenLibrary(project.rootPath,project.projectId);
                std::vector<ScreenEditorLibraryScreen> screens;
                std::unordered_set<bridge::StableId> referenced;
                const auto category=[](bridge::ScreenUsage usage)->std::string
                {
                    switch(usage){case bridge::ScreenUsage::Hud:return "HUDS";
                        case bridge::ScreenUsage::Overlay:return "OVERLAYS";
                        case bridge::ScreenUsage::Modal:return "MODALS";default:return "MENUS";}
                };
                for(const auto& node:authoringSession_.Document().nodes)
                {
                    if(node.kind!=bridge::FlowNodeKind::Screen)
                    {
                        if(node.kind!=bridge::FlowNodeKind::GameStart)
                        { ScreenEditorLibraryScreen target;target.nodeId=node.id;target.name=node.name;
                          target.category="FLOW DESTINATIONS";
                          if(node.kind==bridge::FlowNodeKind::Level){
                              const auto resolved=bridge::StoryFlowLevelReferenceService{}.ResolveLevel(project.rootPath,project.projectId,authoringSession_.Document(),node.id);
                              target.available=resolved.succeeded;target.diagnostic=resolved.message;
                          }
                          screens.push_back(std::move(target)); }
                        continue;
                    }
                    ScreenEditorLibraryScreen record;record.nodeId=node.id;record.name=node.name;
                    record.documentId=node.screenDocumentId;record.pathHint=node.screenPathHint;
                    record.current=node.id==screenEditorHandoff_.screenNodeId;
                    record.available=false;record.category="UNAVAILABLE";record.diagnostic="Screen document is missing or unavailable.";
                    const auto found=std::find_if(screenLibraryCatalogue_.screens.begin(),screenLibraryCatalogue_.screens.end(),
                        [&](const auto& entry){return entry.documentId==node.screenDocumentId;});
                    if(found!=screenLibraryCatalogue_.screens.end())
                    {
                        record.pathHint=found->pathHint;record.available=found->available;
                        record.enabledInGame=found->enabledInGame;record.diagnostic=found->diagnostic;
                        if(found->available)record.category=category(found->usage);
                        if(!found->enabledInGame)record.name+=" // OFF";
                    }
                    referenced.insert(record.documentId);screens.push_back(std::move(record));
                }
                for(const auto& entry:screenLibraryCatalogue_.screens)
                {
                    if(referenced.count(entry.documentId))continue;
                    ScreenEditorLibraryScreen record;record.name=entry.name;record.documentId=entry.documentId;
                    record.pathHint=entry.pathHint;record.available=entry.available;record.enabledInGame=entry.enabledInGame;
                    record.category=entry.available?category(entry.usage):"UNAVAILABLE";record.diagnostic=entry.diagnostic;
                    record.current=entry.documentId==screenEditorHandoff_.screenDocumentId;
                    if(!entry.enabledInGame)record.name+=" // OFF";
                    screens.push_back(std::move(record));
                }
                storyFlow.SetHudChoices(screenLibraryCatalogue_.screens);
                storyFlow.SetScreenLibrary(screenLibraryCatalogue_.screens);
                screenEditor.SetProjectScreenLibrary(std::move(screens));
                screenLibraryActiveNode_=screenEditorHandoff_.screenNodeId;
                screenLibraryActiveDocument_=screenEditorHandoff_.screenDocumentId;
                screenLibraryDirty_=false;
            }

            // Project-level actions belong to the project home, not the Level
            // Editor render path. Keep their Runtime/build lifecycle alive while
            // Story Flow remains the active surface.
            if (levelEditor.ConsumeWindowsGameBuildRequest())
            {
                std::string saveError;
                if (authoringSession_.IsDirty() &&
                    !authoringSession_.Save(saveError))
                {
                    const std::string message =
                        "BUILD GAME // BLOCKED // STORY FLOW SAVE FAILED // " +
                        saveError;
                    storyFlow.SetExternalStatus(message);
                    levelEditor.SetWindowsGameBuildStatus(message);
                }
                else if (buildGameBusy_ || pendingBuildGame_)
                {
                    levelEditor.SetWindowsGameBuildStatus(
                        "BUILD WINDOWS GAME // ALREADY RUNNING");
                }
                else
                {
                    pendingBuildGame_ = true;
                    buildGameBusy_ = true;
                    buildGameDispatchArmed_ = false;
                    storyFlow.SetProjectCommandState(
                        levelEditor.IsProjectPlayFromStoryFlowActive(), true);
                    storyFlow.SetExternalStatus(
                        "BUILD GAME // QUEUED // DIRTY DOCUMENTS SAVED");
                    levelEditor.SetWindowsGameBuildStatus(
                        "BUILD WINDOWS GAME // QUEUED // DIRTY DOCUMENTS SAVED");
                }
            }
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
            OpenLibrary,
            CreateStandalone,
            AdoptExisting,
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
                screenLibraryDirty_ = true;
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
            storyFlow.OnLibraryDocumentRequested([this](const bridge::StableId& documentId)
            {
                // A library document has standalone identity, regardless of
                // whether a StoryFlow node also references it.
                pendingLibraryDocumentId_ = documentId;
                pendingScreenAction_ = PendingScreenAction::OpenLibrary;
            });
            storyFlow.OnLibraryAdoptRequested([this](const bridge::StableId& documentId)
            {
                pendingLibraryDocumentId_ = documentId;
                pendingScreenAction_ = PendingScreenAction::AdoptExisting;
            });
            storyFlow.OnLibraryRefreshRequested([this]()
            {
                screenLibraryDirty_ = true;
            });
            storyFlow.OnLibraryCreateRequested([this](const bridge::ScreenUsage usage)
            {
                pendingStandaloneUsage_ = usage;
                pendingScreenAction_ = PendingScreenAction::CreateStandalone;
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
                        levelEditor.RequestWindowsGameBuild();
                        storyFlow.SetExternalStatus(
                            "BUILD GAME // SAVING DIRTY PROJECT DOCUMENTS");
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
            levelEditor.RegisterStoryFlowLifecycleControl(
                returnToStoryFlowButton_);

            lastLayoutWrite_ = std::chrono::steady_clock::now();
            attached_ = true;
        }

        template <typename LevelEditor>
        void LayoutReturnButton(LevelEditor& levelEditor)
        {
            if (!attached_) return;
            const XMFLOAT4 bounds = levelEditor.StoryFlowWorkspaceBounds();

            // The Scene chrome owns the first 230 px of the scene-tab strip and
            // the viewport starts immediately below it. Keep Story Flow return
            // in that strip, right-aligned before the Inspector, so it cannot
            // collide with the viewport's PERSPECTIVE/LIT/SHOW chips. The
            // registration helper also keeps it above the chrome in Wicked's
            // reverse top-level render order.
            constexpr float ReturnButtonWidth = 132.0f;
            constexpr float ReturnButtonHeight = 28.0f;
            constexpr float SceneTabSafeOffset = 242.0f;
            const float rightAlignedX =
                bounds.z - ReturnButtonWidth - 12.0f;
            const float x = std::max(
                bounds.x + SceneTabSafeOffset,
                rightAlignedX);
            const float y = std::max(0.0f, bounds.y - 31.0f);
            returnToStoryFlowButton_.SetPos(XMFLOAT2(x, y));
            returnToStoryFlowButton_.SetSize(XMFLOAT2(
                ReturnButtonWidth,
                ReturnButtonHeight));
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
                    levelEditor.SetWindowsGameBuildStatus(
                        "BUILD GAME // BLOCKED // TEST GAME RUNNING");
                }
                else
                {
                    storyFlow.SetExternalStatus(
                        "BUILD GAME // BUILDING WINDOWS GAME");
                    const WindowsGameBuildUiResult build =
                        BuildActiveWindowsGame();
                    buildGameBusy_ = false;
                    const std::string status = build.succeeded
                        ? "BUILD GAME // COMPLETE // " + build.finalOutputPath
                        : "BUILD GAME // FAILED // " + build.message;
                    storyFlow.SetExternalStatus(status);
                    levelEditor.SetWindowsGameBuildStatus(status);
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

            if (action == PendingScreenAction::AdoptExisting)
            {
                const auto catalogue = bridge::EnumerateProjectScreenLibrary(project.rootPath, project.projectId);
                const auto entry = std::find_if(catalogue.screens.begin(), catalogue.screens.end(),
                    [this](const auto& item) { return item.documentId == pendingLibraryDocumentId_; });
                if (entry == catalogue.screens.end() || !entry->available)
                { storyFlow.SetExternalStatus("ADD EXISTING FAILED // REFRESH UI LIBRARY"); return; }
                std::string error;
                if (!authoringSession_.Save(error))
                { storyFlow.SetExternalStatus("ADD EXISTING BLOCKED // " + error); return; }
                bridge::StoryFlowNewScreenRequest request;
                request.projectRoot = project.rootPath; request.projectId = project.projectId;
                request.flowPath = authoringSession_.FilePath(); request.flow = authoringSession_.Document();
                request.screenName = entry->name; request.screenPathHint = entry->pathHint;
                const auto result = bridge::StoryFlowScreenLifecycleService{}.AdoptExistingScreen(request);
                if (!result.succeeded)
                { storyFlow.SetExternalStatus("ADD EXISTING FAILED // " + result.message); return; }
                if (ReloadAfterContentMutation(project, storyFlow, result.screenNodeId, true))
                    storyFlow.SetExternalStatus("SCREEN ADDED + REVEALED // " + entry->name + " // CONNECT ROUTES EXPLICITLY");
                return;
            }

            if (action == PendingScreenAction::AddNew)
            {
                const std::string screenName = Trim(pendingScreenName_);
                if (screenName.empty())
                {
                    storyFlow.SetExternalStatus("SCREEN CREATE REJECTED // SCREEN NAME REQUIRED");
                    ReportSemanticFailure("Add Screen requires a Screen name.");
                    return;
                }

                std::string saveError;
                if (!authoringSession_.Save(saveError))
                { storyFlow.SetExternalStatus("SCREEN CREATE BLOCKED // " + saveError); return; }
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
                if (!ReloadAfterContentMutation(project, storyFlow, result.screenNodeId, true))
                    return;
                storyFlow.SetExternalStatus("SCREEN CREATED + REVEALED // " + screenName +
                    " // SAVED // CONNECT ROUTES EXPLICITLY");
                wi::backlog::post(
                    "Renegade Story Flow: created governed " +
                    std::string(bridge::StoryFlowScreenTemplateName(pendingScreenTemplate_)) +
                    " Screen '" + screenName + "' -> " + result.screenPathHint,
                    wi::backlog::LogLevel::Default);
                return;
            }

            if(action==PendingScreenAction::CreateStandalone)
            {
                const auto created=bridge::CreateProjectScreenLibraryDocument(
                    project.rootPath,project.projectId,pendingStandaloneUsage_);
                if(!created.succeeded)
                {
                    storyFlow.SetExternalStatus("CREATE UI DOCUMENT FAILED // "+created.error);
                    return;
                }
                StoryFlowScreenEditorHandoff handoff;
                std::string error;
                if(!BuildStandaloneScreenEditorHandoff(project.rootPath,project.projectId,
                    created.documentId,created.pathHint,handoff,error))
                {
                    storyFlow.SetExternalStatus("CREATED UI DOCUMENT CANNOT OPEN // "+error);
                    screenLibraryDirty_=true;
                    return;
                }
                screenEditorHandoff_=std::move(handoff);
                screenLibraryDirty_=true;
                storyFlow.SetExternalStatus("NEW PROJECT UI DOCUMENT CREATED // "+created.pathHint);
                if(screenEditorOpen_)screenEditorOpen_(screenEditorHandoff_);
                return;
            }

            if(action==PendingScreenAction::OpenLibrary)
            {
                screenLibraryCatalogue_=bridge::EnumerateProjectScreenLibrary(project.rootPath,project.projectId);
                screenLibraryDirty_=true;
                const auto found=std::find_if(screenLibraryCatalogue_.screens.begin(),screenLibraryCatalogue_.screens.end(),
                    [&](const auto& entry){return entry.documentId==pendingLibraryDocumentId_ && entry.available;});
                if(found==screenLibraryCatalogue_.screens.end())
                {storyFlow.SetExternalStatus("SCREEN LIBRARY ENTRY IS UNAVAILABLE");pendingLibraryDocumentId_.clear();return;}
                StoryFlowScreenEditorHandoff handoff;std::string error;
                if(!BuildStandaloneScreenEditorHandoff(project.rootPath,project.projectId,found->documentId,found->pathHint,handoff,error))
                {storyFlow.SetExternalStatus("OPEN LIBRARY SCREEN FAILED // "+error);pendingLibraryDocumentId_.clear();return;}
                (void)BindLibraryFlowContext(authoringSession_.Document(),handoff);
                if (!handoff.screenNodeId.empty()) storyFlow.SelectAndFocusNode(handoff.screenNodeId);
                screenEditorHandoff_=std::move(handoff);screenLibraryDirty_=true;pendingLibraryDocumentId_.clear();
                if(screenEditorOpen_)screenEditorOpen_(screenEditorHandoff_);
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
            const bridge::StableId& preferredNodeId, bool revealFailures = true)
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
                    if (revealFailures) storyFlow.SelectAndFocusNode(node.id);
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
            const bridge::StableId& createdNodeId, bool placeNewScreen = false)
        {
            const bool newIdentity = model_.FindNode(createdNodeId) == nullptr;
            std::string error;
            screenLibraryDirty_ = true;
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
            AuditScreenOutcomeParity(project, storyFlow, {}, false);
            if (placeNewScreen && newIdentity)
                storyFlow.PlaceNewNodeAtViewportCentre(createdNodeId);
            else
                storyFlow.SelectAndFocusNode(createdNodeId);
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
            AuditScreenOutcomeParity(project, storyFlow, {}, false);
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
            storyFlow.SetScreenLibrary({});
            layout_ = {};
            layoutPath_.clear();
            trackedProjectId_.clear();
            trackedFlowId_.clear();
            activeLevelNodeId_.clear();
            pendingLevelNodeId_.clear();
            pendingScreenNodeId_.clear();
            pendingLibraryDocumentId_.clear();
            screenLibraryCatalogue_ = {};
            screenLibraryDirty_ = true;
            screenLibraryActiveNode_.clear();
            screenLibraryActiveDocument_.clear();
            pendingExistingScenePath_.clear();
            pendingLevelAction_ = PendingLevelAction::None;
            pendingScreenAction_ = PendingScreenAction::None;
            pendingProjectPlay_ = false;
            pendingBuildGame_ = false;
            buildGameDispatchArmed_ = false;
            buildGameBusy_ = false;
            screenEditorHandoff_ = {};
            routeHistoryValid_ = false;
            routeHistoryScreenNodeId_.clear();
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
        bridge::StableId screenLibraryActiveNode_;
        bridge::StableId screenLibraryActiveDocument_;
        bridge::StableId pendingLibraryDocumentId_;
        bridge::ScreenLibraryCatalogue screenLibraryCatalogue_;
        bridge::StableId routeHistoryScreenNodeId_;
        std::size_t routeHistoryExpectedUndo_ = 0;
        std::size_t routeHistoryExpectedRedo_ = 0;
        bool routeHistoryValid_ = false;
        std::string layoutPath_;
        std::string pendingLevelName_;
        std::string pendingScreenName_;
        std::string pendingExistingScenePath_;
        std::string activeProjectRoot_;
        std::chrono::steady_clock::time_point lastLayoutWrite_{};
        Workspace desiredWorkspace_ = Workspace::StoryFlow;
        PendingLevelAction pendingLevelAction_ = PendingLevelAction::None;
        PendingScreenAction pendingScreenAction_ = PendingScreenAction::None;
        bridge::ScreenUsage pendingStandaloneUsage_ = bridge::ScreenUsage::Hud;
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
        bool screenLibraryDirty_ = true;
        bool screenLibraryHooked_ = false;
    };
}
