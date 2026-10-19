#include "RuntimeFlow.h"
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/ScreenFlowDestinationPlan.h"
#include "renegade/bridge/StoryFlowScreenReferenceService.h"
#include "renegade/bridge/StoryFlowAuthoringSession.h"
#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/bridge/ScreenFlowAtomicSaveService.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;
    using namespace renegade::runtime;

    int failures = 0;

    void Check(const bool condition, const char* message)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    bool Contains(
        const std::vector<std::string>& values,
        const std::string& value)
    {
        return std::find(values.begin(), values.end(), value) != values.end();
    }

    struct TemporaryDirectory
    {
        fs::path path;
        ~TemporaryDirectory()
        {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    };

    ScreenWidget MakeButton(
        const std::string& name,
        const std::string& action,
        const float y)
    {
        ScreenWidget button;
        button.id = GenerateStableId();
        button.kind = ScreenWidgetKind::Button;
        button.name = name;
        button.rect = {440.0f, y, 400.0f, 64.0f};
        button.visible = true;
        button.enabled = true;
        button.text = name;
        button.actionId = action;
        button.style = MakeScreenWidgetStyleTemplate(
            button.kind, button.rect.height);
        return button;
    }

    ScreenDocument MakeScreen(
        const StableId& projectId,
        const StableId& documentId)
    {
        ScreenDocument screen;
        screen.envelope = CreateDocumentEnvelope(
            projectId,
            RuntimeScreenDocumentType,
            "Content/Screens/Title.renegade-screen",
            "Story Flow Gate 8E outcome parity tests");
        screen.envelope.documentId = documentId;
        screen.designWidth = 1280.0f;
        screen.designHeight = 720.0f;
        screen.actions = {{"new_game"}, {"options"}, {"unused_declared"}};
        screen.widgets = {
            MakeButton("NEW GAME", "new_game", 260.0f),
            MakeButton("OPTIONS", "options", 350.0f),
        };
        screen.focusOrder = {
            screen.widgets[0].id,
            screen.widgets[1].id,
        };
        return screen;
    }

    FlowNode MakeNode(
        const StableId& id,
        const FlowNodeKind kind,
        const std::string& name)
    {
        FlowNode node;
        node.id = id;
        node.kind = kind;
        node.name = name;
        return node;
    }

    FlowRoute MakeRoute(
        const StableId& id,
        const StableId& source,
        const std::string& outcome,
        const StableId& destination)
    {
        FlowRoute route;
        route.id = id;
        route.sourceNodeId = source;
        route.outcome = outcome;
        route.destinationNodeId = destination;
        return route;
    }

    FlowDocument MakeFlow(
        const StableId& projectId,
        const StableId& flowId,
        const StableId& startId,
        const StableId& screenNodeId,
        const StableId& screenDocumentId,
        const StableId& completeId,
        const StableId& quitId)
    {
        FlowDocument flow;
        flow.envelope = CreateDocumentEnvelope(
            projectId,
            StoryFlowDocumentType,
            "Content/Flow/Main.renegade-flow",
            "Story Flow Gate 8E outcome parity tests");
        flow.envelope.documentId = flowId;
        flow.startNodeId = startId;

        FlowNode start = MakeNode(startId, FlowNodeKind::GameStart, "Game Start");
        FlowNode screen = MakeNode(screenNodeId, FlowNodeKind::Screen, "Title Screen");
        screen.screenDocumentId = screenDocumentId;
        screen.screenPathHint = "Content/Screens/Title.renegade-screen";
        FlowNode complete = MakeNode(
            completeId, FlowNodeKind::CompleteGame, "Complete Game");
        FlowNode quit = MakeNode(quitId, FlowNodeKind::Quit, "Quit");
        flow.nodes = {start, screen, complete, quit};
        flow.routes = {
            MakeRoute(
                "10000000-0000-4000-8000-000000000001",
                startId,
                GameStartOutcome,
                screenNodeId),
            MakeRoute(
                "10000000-0000-4000-8000-000000000002",
                screenNodeId,
                "new_game",
                completeId),
            MakeRoute(
                "10000000-0000-4000-8000-000000000003",
                screenNodeId,
                "options",
                quitId),
        };
        return flow;
    }

    RuntimeBootstrapResult Bootstrap(
        const fs::path& root,
        const StableId& projectId,
        const StableId& flowId,
        const fs::path& flowPath)
    {
        RuntimeBootstrapResult result;
        result.succeeded = true;
        result.code = RuntimeBootstrapCode::Success;
        result.project.projectId = projectId;
        result.project.rootPath = root.generic_u8string();
        result.project.startupFlowId = flowId;
        result.startupFlowPath = flowPath.generic_u8string();
        return result;
    }
}

int main()
{
    const StableId projectId = "20000000-0000-4000-8000-000000000001";
    const StableId flowId = "20000000-0000-4000-8000-000000000002";
    const StableId startId = "20000000-0000-4000-8000-000000000003";
    const StableId screenNodeId = "20000000-0000-4000-8000-000000000004";
    const StableId screenDocumentId = "20000000-0000-4000-8000-000000000005";
    const StableId completeId = "20000000-0000-4000-8000-000000000006";
    const StableId quitId = "20000000-0000-4000-8000-000000000007";

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade gate8e outcome parity " + std::to_string(unique))
    };
    const fs::path root = temporary.path / "Project With Spaces";
    const fs::path screenPath = root / "Content/Screens/Title.renegade-screen";
    const fs::path flowPath = root / "Content/Flow/Main.renegade-flow";
    fs::create_directories(screenPath.parent_path());
    fs::create_directories(flowPath.parent_path());

    ScreenDocument screen = MakeScreen(projectId, screenDocumentId);
    FlowDocument flow = MakeFlow(
        projectId,
        flowId,
        startId,
        screenNodeId,
        screenDocumentId,
        completeId,
        quitId);

    // UI-024 destination picker plans are entirely side-effect free.
    // A second valid Screen node is enough to exercise route planning without
    // altering the filesystem fixture or Runtime's existing Title Screen flow.
    const StableId optionsNodeId = "20000000-0000-4000-8000-000000000008";
    FlowDocument planFlow = flow;
    FlowNode optionsNode = MakeNode(
        optionsNodeId, FlowNodeKind::Screen, "Options Screen");
    optionsNode.screenDocumentId = "20000000-0000-4000-8000-000000000009";
    optionsNode.screenPathHint = "Content/Screens/Options.renegade-screen";
    planFlow.nodes.push_back(optionsNode);
    const StableId selectedButtonId = screen.widgets[1].id;
    const auto routePlan = PlanButtonOpenScreen(
        screen, planFlow, screenNodeId, selectedButtonId, optionsNodeId);
    Check(routePlan.succeeded && routePlan.updatingExisting &&
            !routePlan.noChanges &&
            routePlan.candidateFlow.routes[2].destinationNodeId == optionsNodeId &&
            planFlow.routes[2].destinationNodeId == quitId,
        "open Screen route planner mutated source or failed to target real Screen");
    const auto noOpPlan = PlanButtonOpenScreen(
        screen, routePlan.candidateFlow, screenNodeId,
        selectedButtonId, optionsNodeId);
    Check(noOpPlan.succeeded && noOpPlan.noChanges,
        "routing an already-linked Button should be a no-op");

    FlowDocument emptyRouteFlow = planFlow;
    emptyRouteFlow.routes.pop_back();
    const auto newRoutePlan = PlanButtonOpenScreen(
        screen, emptyRouteFlow, screenNodeId,
        selectedButtonId, optionsNodeId);
    Check(newRoutePlan.succeeded && !newRoutePlan.updatingExisting &&
            newRoutePlan.candidateFlow.routes.size() ==
                emptyRouteFlow.routes.size() + 1 &&
            newRoutePlan.candidateFlow.routes.back().outcome == "options" &&
            emptyRouteFlow.routes.size() == 2,
        "unrouted Button did not plan exactly one governed Flow route");
    const auto terminalPlan = PlanButtonOpenScreen(screen, planFlow, screenNodeId,
        selectedButtonId, quitId);
    Check(!PlanButtonOpenScreen(screen, planFlow, screenNodeId,
            selectedButtonId, startId).succeeded && terminalPlan.succeeded &&
          terminalPlan.updatingExisting &&
          terminalPlan.candidateFlow.routes.size() == planFlow.routes.size() &&
          std::any_of(terminalPlan.candidateFlow.routes.begin(), terminalPlan.candidateFlow.routes.end(),
              [&](const auto& route) { return route.id == terminalPlan.routeId &&
                  route.destinationNodeId == quitId && route.destinationEntry.empty(); }),
        "destination picker must reject Game Start and preserve governed terminal routing");

    ScreenDocument sharedButtonScreen = screen;
    sharedButtonScreen.widgets[0].actionId = "options";
    Check(!PlanButtonOpenScreen(sharedButtonScreen, planFlow, screenNodeId,
            selectedButtonId, optionsNodeId).succeeded,
        "shared action would silently repoint multiple Buttons");

    FlowDocument conditionalFlow = planFlow;
    conditionalFlow.routes[2].conditions.push_back(
        {"profile", FlowConditionOperator::Exists, ""});
    Check(!PlanButtonOpenScreen(screen, conditionalFlow, screenNodeId,
            selectedButtonId, optionsNodeId).succeeded,
        "conditional destination was silently overwritten");

    FlowDocument branchingFlow = planFlow;
    auto otherRoute = branchingFlow.routes[2];
    otherRoute.id = "10000000-0000-4000-8000-000000000099";
    otherRoute.priority = 1;
    otherRoute.conditions.push_back(
        {"profile", FlowConditionOperator::Exists, ""});
    branchingFlow.routes.push_back(otherRoute);
    Check(!PlanButtonOpenScreen(screen, branchingFlow, screenNodeId,
            selectedButtonId, optionsNodeId).succeeded,
        "multi-route action should require Graph authoring");
    Check(!PlanButtonOpenScreen(screen, planFlow, startId,
            selectedButtonId, optionsNodeId).succeeded,
        "the Screen action planner accepted a mismatched source identity");

    // UI-024: the plan is committed through the real Flow history, not
    // a parallel Graph or Screen route cache. Exercise save/reopen as well.
    const fs::path routingFlowPath =
        root / "Content/Flow/Routing.renegade-flow";
    std::string transactionError;
    Check(WriteFlowDocument(
            routingFlowPath.generic_u8string(), planFlow, transactionError),
        "could not write UI-024 routing fixture");
    StoryFlowAuthoringSession routingSession;
    Check(routingSession.Open(
            routingFlowPath.generic_u8string(), projectId, transactionError),
        "routing authoring session could not open fixture");
    const auto plannedRoute = std::find_if(
        routePlan.candidateFlow.routes.begin(),
        routePlan.candidateFlow.routes.end(),
        [&](const FlowRoute& route) { return route.id == routePlan.routeId; });
    if (plannedRoute == routePlan.candidateFlow.routes.end())
        return 1;
    const auto routingUndoBefore = routingSession.UndoCount();
    Check(routingSession.UpdateRoute(
            routePlan.routeId, *plannedRoute, transactionError) &&
            routingSession.UndoCount() == routingUndoBefore + 1 &&
            routingSession.Document().routes[2].destinationNodeId == optionsNodeId,
        "route picker did not create one real Flow authoring history entry");
    Check(routingSession.Undo(transactionError) &&
            routingSession.Document().routes[2].destinationNodeId == quitId &&
            routingSession.Redo(transactionError) &&
            routingSession.Document().routes[2].destinationNodeId == optionsNodeId,
        "destination route did not round-trip through Flow Undo/Redo");
    Check(routingSession.Save(transactionError),
        "destination route save failed");
    StoryFlowAuthoringSession reopenedRouting;
    Check(reopenedRouting.Open(
            routingFlowPath.generic_u8string(), projectId, transactionError) &&
            reopenedRouting.Document().routes[2].destinationNodeId == optionsNodeId &&
            !reopenedRouting.IsDirty(),
        "saved destination route did not persist across Flow reopen");
    const auto savedScreenAction = screen.widgets[1].actionId;
    Check(savedScreenAction == "options" &&
            planFlow.routes[2].destinationNodeId == quitId,
        "routing transaction unexpectedly changed Screen or source Flow");

    std::string error;
    Check(WriteScreenDocument(screenPath.generic_u8string(), screen, error),
        "could not write valid Gate 8E Screen");
    Check(WriteFlowDocument(flowPath.generic_u8string(), flow, error),
        "could not write valid Gate 8E Flow");

    StoryFlowScreenReferenceService references;
    auto audit = references.AuditScreenOutcomes(
        root.generic_u8string(), projectId, flow, screenNodeId);
    Check(audit.succeeded,
        "valid Screen/Button actions and Flow routes did not pass parity audit");
    Check(audit.buttonActionIds.size() == 2 &&
            Contains(audit.buttonActionIds, "new_game") &&
            Contains(audit.buttonActionIds, "options"),
        "audit did not identify Button-used actions");
    Check(audit.actionIds.size() == 3 &&
            Contains(audit.actionIds, "unused_declared"),
        "audit did not preserve unused declared actions");
    Check(audit.unroutedButtonActionIds.empty() &&
            audit.invalidRouteOutcomes.empty(),
        "valid parity audit reported false route defects");

    // Built-in player accessibility actions are handled by Runtime, not
    // StoryFlow routes. The Screen may declare them without adding a fake
    // destination; the destination picker refuses to route their identity.
    auto accessibleScreen=screen;
    accessibleScreen.actions.push_back({RuntimeUiReducedMotionToggleAction});
    accessibleScreen.widgets.push_back(MakeButton(
        "REDUCED MOTION",RuntimeUiReducedMotionToggleAction,445.0f));
    accessibleScreen.focusOrder.push_back(accessibleScreen.widgets.back().id);
    Check(WriteScreenDocument(screenPath.generic_u8string(),accessibleScreen,error),
        "could not write a Screen with a built-in accessibility action");
    auto accessibilityAudit=references.AuditScreenOutcomes(
        root.generic_u8string(),projectId,flow,screenNodeId);
    Check(accessibilityAudit.succeeded&&
        !Contains(accessibilityAudit.buttonActionIds,RuntimeUiReducedMotionToggleAction)&&
        !PlanButtonOpenScreen(accessibleScreen,planFlow,screenNodeId,
            accessibleScreen.widgets.back().id,optionsNodeId).succeeded,
        "player accessibility action unexpectedly required or received a StoryFlow route");
    RuntimeFlowController accessibilityRuntime;
    Check(accessibilityRuntime.Initialize(
        Bootstrap(root,projectId,flowId,flowPath),error),
        "Runtime rejected a Screen containing an independent accessibility action");
    Check(WriteScreenDocument(screenPath.generic_u8string(),screen,error),
        "could not restore ordinary Screen outcome fixture");

    FlowDocument reopenedFlow;
    ScreenDocument reopenedScreen;
    Check(ReadFlowDocument(
            flowPath.generic_u8string(), projectId, reopenedFlow, error),
        "Gate 8E Flow did not reopen");
    Check(ReadScreenDocument(
            screenPath.generic_u8string(), projectId, reopenedScreen, error),
        "Gate 8E Screen did not reopen");
    audit = references.AuditScreenOutcomes(
        root.generic_u8string(), projectId, reopenedFlow, screenNodeId);
    Check(audit.succeeded,
        "save/reopen changed valid Screen/Flow outcome parity");

    RuntimeFlowController runtime;
    Check(runtime.Initialize(
            Bootstrap(root, projectId, flowId, flowPath), error),
        "Runtime rejected a parity-valid Screen/Flow pair");

    // Rename one Screen action transactionally inside the Screen document but
    // deliberately leave Story Flow untouched. Gate 8E must diagnose both the
    // stale route and the now-unrouted Button action; it must not silently
    // rewrite Story Flow behind the creator's back.
    screen.actions[1].id = "settings";
    screen.widgets[1].actionId = "settings";
    Check(WriteScreenDocument(screenPath.generic_u8string(), screen, error),
        "could not persist renamed Screen action");

    audit = references.AuditScreenOutcomes(
        root.generic_u8string(), projectId, flow, screenNodeId);
    Check(!audit.succeeded &&
            Contains(audit.invalidRouteOutcomes, "options") &&
            Contains(audit.unroutedButtonActionIds, "settings"),
        "renamed Screen action did not expose stale/unrouted Story Flow diagnostics");

    RuntimeFlowController invalidRuntime;
    error.clear();
    Check(!invalidRuntime.Initialize(
            Bootstrap(root, projectId, flowId, flowPath), error) &&
            error.find("outcome parity") != std::string::npos,
        "Runtime did not fail closed on Screen/Flow outcome mismatch");

    // Repair Story Flow explicitly. Destination routing remains a Story Flow
    // mutation; the Screen document only owns the symbolic action identity.
    flow.routes[2].outcome = "settings";
    Check(WriteFlowDocument(flowPath.generic_u8string(), flow, error),
        "could not persist explicit Story Flow outcome repair");
    audit = references.AuditScreenOutcomes(
        root.generic_u8string(), projectId, flow, screenNodeId);
    Check(audit.succeeded,
        "explicit Story Flow route repair did not restore parity");

    RuntimeFlowController repairedRuntime;
    Check(repairedRuntime.Initialize(
            Bootstrap(root, projectId, flowId, flowPath), error),
        "Runtime did not accept repaired Screen/Flow parity");

    // Removing a destination for a Button-used action is also invalid, while
    // the unused declared Screen action remains legal without a route.
    flow.routes.erase(flow.routes.begin() + 2);
    Check(WriteFlowDocument(flowPath.generic_u8string(), flow, error),
        "could not persist unrouted Button fixture");
    audit = references.AuditScreenOutcomes(
        root.generic_u8string(), projectId, flow, screenNodeId);
    Check(!audit.succeeded &&
            audit.invalidRouteOutcomes.empty() &&
            Contains(audit.unroutedButtonActionIds, "settings") &&
            !Contains(audit.unroutedButtonActionIds, "unused_declared"),
        "audit did not distinguish Button-used actions from unused declarations");

    // A local-only Button deliberately executes UI actions without requesting
    // Flow navigation. It must not be reported as an unrouted Flow outcome;
    // switching back restores strict route parity without changing Flow data.
    screen.widgets[1].localOnly=true;
    Check(WriteScreenDocument(screenPath.generic_u8string(),screen,error),
        "could not persist local-only Button mode");
    ScreenDocument localReopened;
    Check(ReadScreenDocument(screenPath.generic_u8string(),projectId,localReopened,error) &&
        localReopened.widgets[1].localOnly && !localReopened.widgets[0].localOnly,
        "local-only Button mode failed save/reopen");
    audit=references.AuditScreenOutcomes(
        root.generic_u8string(),projectId,flow,screenNodeId);
    Check(audit.succeeded &&
        !Contains(audit.buttonActionIds,"settings") &&
        audit.unroutedButtonActionIds.empty(),
        "local-only Button falsely required a StoryFlow destination");
    const auto localDestination=PlanButtonOpenScreen(
        screen,planFlow,screenNodeId,screen.widgets[1].id,optionsNodeId);
    Check(!localDestination.succeeded &&
        localDestination.message.find("Local Only")!=std::string::npos,
        "destination picker silently routed an explicit local-only Button");
    screen.widgets[1].localOnly=false;
    Check(WriteScreenDocument(screenPath.generic_u8string(),screen,error),
        "could not restore Flow-dispatching Button");
    audit=references.AuditScreenOutcomes(
        root.generic_u8string(),projectId,flow,screenNodeId);
    Check(!audit.succeeded && Contains(audit.unroutedButtonActionIds,"settings"),
        "Flow parity did not resume when the Button exited Local Only mode");

    // Multiple routes for one valid Screen action remain legal. Conditions and
    // priority are still interpreted by the existing FlowInterpreter contract.
    FlowRoute settingsA = MakeRoute(
        "10000000-0000-4000-8000-000000000004",
        screenNodeId,
        "settings",
        quitId);
    settingsA.conditions.push_back({"profile", FlowConditionOperator::Equals, "a"});
    FlowRoute settingsB = MakeRoute(
        "10000000-0000-4000-8000-000000000005",
        screenNodeId,
        "settings",
        completeId);
    settingsB.conditions.push_back({"profile", FlowConditionOperator::Equals, "b"});
    flow.routes.push_back(std::move(settingsA));
    flow.routes.push_back(std::move(settingsB));
    audit = references.AuditScreenOutcomes(
        root.generic_u8string(), projectId, flow, screenNodeId);
    Check(audit.succeeded,
        "audit incorrectly rejected multiple conditional routes for one Screen action");

    // UI-05: preserve authoring history while journaling Screen and Flow
    // together. Exercise full success, rolled-back second-write failure,
    // external edit guard, and on-disk round trip on isolated files.
    {
        const fs::path jointScreen =
            root / "Content/Screens/UI05.renegade-screen";
        const fs::path jointFlow =
            root / "Content/Flow/UI05.renegade-flow";
        std::string jointError;
        Check(WriteScreenDocument(
                jointScreen.generic_u8string(), reopenedScreen, jointError) &&
            WriteFlowDocument(
                jointFlow.generic_u8string(), reopenedFlow, jointError),
            "UI-05 could not create isolated starting Screen/Flow fixtures");

        ScreenAuthoringSession screenSession;
        StoryFlowAuthoringSession flowSession;
        Check(screenSession.Open(
                jointScreen.generic_u8string(), projectId, jointError) &&
            flowSession.Open(
                jointFlow.generic_u8string(), projectId, jointError),
            "UI-05 isolated sessions could not load");

        auto title = screenSession.Document().widgets.front();
        ScreenWidgetAuthoringEdit titleEdit;
        titleEdit.name = "UI-05 Saved Title";
        titleEdit.text = "ATOMIC UI";
        titleEdit.resolvedRect = title.rect;
        titleEdit.visible = title.visible;
        titleEdit.enabled = title.enabled;
        Check(screenSession.UpdateWidget(
                title.id, titleEdit, jointError) &&
            flowSession.RenameNode(
                screenNodeId, "UI-05 Screen", jointError),
            "UI-05 could not create two dirty session revisions");

        // Inject failure at the second replacement, after one document
        // has already been replaced. The transaction must roll it back.
        ProjectDocumentTransactionOptions rollbackOptions;
        rollbackOptions.operationHook = [](
            ProjectDocumentTransactionStage stage,
            std::size_t index,
            const std::string&, std::string& failure)
        {
            if (stage == ProjectDocumentTransactionStage::Replace && index == 1)
            {
                failure = "Injected second-document replacement failure";
                return ProjectDocumentTransactionHookAction::Fail;
            }
            return ProjectDocumentTransactionHookAction::Continue;
        };
        auto rejected = ScreenFlowAtomicSaveService::Save(
            screenSession, flowSession, root.generic_u8string(),
            rollbackOptions);
        Check(!rejected.succeeded && rejected.transaction.rolledBack &&
            screenSession.IsDirty() && flowSession.IsDirty(),
            "UI-05 failure did not roll back both documents/dirty state");
        ScreenDocument afterFailureScreen;
        FlowDocument afterFailureFlow;
        Check(ReadScreenDocument(
                jointScreen.generic_u8string(), projectId,
                afterFailureScreen, jointError) &&
            ReadFlowDocument(
                jointFlow.generic_u8string(), projectId,
                afterFailureFlow, jointError) &&
            afterFailureScreen.widgets.front().name ==
                reopenedScreen.widgets.front().name &&
            afterFailureFlow.nodes[1].name == reopenedFlow.nodes[1].name,
            "UI-05 rollback did not restore original Screen/Flow bytes");

        const auto screenUndoBefore = screenSession.UndoCount();
        const auto flowUndoBefore = flowSession.UndoCount();
        const auto accepted = ScreenFlowAtomicSaveService::Save(
            screenSession, flowSession, root.generic_u8string());
        Check(accepted.succeeded && !screenSession.IsDirty() &&
            !flowSession.IsDirty() &&
            screenSession.UndoCount() == screenUndoBefore &&
            flowSession.UndoCount() == flowUndoBefore,
            "UI-05 atomic save lost existing authoring histories");
        ScreenDocument persistedScreen;
        FlowDocument persistedFlow;
        Check(ReadScreenDocument(
                jointScreen.generic_u8string(), projectId,
                persistedScreen, jointError) &&
            ReadFlowDocument(
                jointFlow.generic_u8string(), projectId,
                persistedFlow, jointError) &&
            persistedScreen.widgets.front().name == "UI-05 Saved Title" &&
            persistedFlow.nodes[1].name == "UI-05 Screen",
            "UI-05 saved pair did not survive disk reopen");
        Check(screenSession.Undo(jointError) &&
            flowSession.Undo(jointError) &&
            screenSession.IsDirty() && flowSession.IsDirty() &&
            screenSession.Redo(jointError) &&
            flowSession.Redo(jointError) &&
            !screenSession.IsDirty() && !flowSession.IsDirty(),
            "UI-05 saved history lost ability to Undo/Redo");

        const auto duplicate = ScreenFlowAtomicSaveService::Save(
            screenSession, flowSession, root.generic_u8string());
        Check(duplicate.succeeded && !screenSession.IsDirty() &&
            !flowSession.IsDirty(),
            "UI-05 idempotent joint Save failed");

        // Undo the saved revision then author an alternative branch. The
        // previously saved revision disappears from Undo history; autosave
        // must still use the separate persisted baseline safely.
        Check(screenSession.Undo(jointError) &&
              flowSession.Undo(jointError),
            "UI-07 could not undo saved histories before branching");
        auto branchWidget = screenSession.Document().widgets.front();
        ScreenWidgetAuthoringEdit branchEdit;
        branchEdit.name = "UI-07 Undo Branch";
        branchEdit.text = branchWidget.text;
        branchEdit.resolvedRect = branchWidget.rect;
        branchEdit.visible = branchWidget.visible;
        branchEdit.enabled = branchWidget.enabled;
        Check(screenSession.UpdateWidget(
                branchWidget.id, branchEdit, jointError) &&
              flowSession.RenameNode(
                screenNodeId, "UI-07 Flow Branch", jointError) &&
              screenSession.IsDirty() && flowSession.IsDirty(),
            "UI-07 could not create a new edit after Undo");
        const auto branchSave = ScreenFlowAtomicSaveService::Save(
            screenSession, flowSession, root.generic_u8string());
        ScreenDocument branchScreenDisk;
        FlowDocument branchFlowDisk;
        Check(branchSave.succeeded &&
              !screenSession.IsDirty() && !flowSession.IsDirty() &&
              ReadScreenDocument(jointScreen.generic_u8string(), projectId,
                branchScreenDisk, jointError) &&
              ReadFlowDocument(jointFlow.generic_u8string(), projectId,
                branchFlowDisk, jointError) &&
              branchScreenDisk.widgets.front().name == "UI-07 Undo Branch" &&
              branchFlowDisk.nodes[1].name == "UI-07 Flow Branch" &&
              screenSession.CanUndo() && flowSession.CanUndo(),
            "UI-07 autosave failed after Undo and a divergent new edit");

        // Byte baselines must capture what was actually opened. Formatting
        // differences in a valid legacy document are not external edits;
        // formatting changes after Open are external edits and fail closed.
        {
            { std::ofstream append(jointScreen, std::ios::app); append << "\n\n"; }
            { std::ofstream append(jointFlow, std::ios::app); append << "\n\n"; }
            ScreenAuthoringSession formattedScreen;
            StoryFlowAuthoringSession formattedFlow;
            Check(formattedScreen.Open(jointScreen.generic_u8string(), projectId, jointError) &&
                  formattedFlow.Open(jointFlow.generic_u8string(), projectId, jointError),
                "Formatted document fixture did not open");
            const auto normalized = ScreenFlowAtomicSaveService::Save(
                formattedScreen, formattedFlow, root.generic_u8string());
            Check(normalized.succeeded,
                "Exact loaded bytes were replaced by a canonical reserialization baseline");
            { std::ofstream append(jointFlow, std::ios::app); append << "\n"; }
            Check(!formattedFlow.Save(jointError),
                "Direct authoring Save overwrote an external formatting edit");
            const auto changedFormatting = ScreenFlowAtomicSaveService::Save(
                formattedScreen, formattedFlow, root.generic_u8string());
            Check(!changedFormatting.succeeded,
                "Coordinated Save overwrote an external formatting edit");
        }

        // Another editor changes Flow on disk without touching our loaded
        // session. Our next attempt must refuse to overwrite that edit.
        FlowDocument externallyChanged = persistedFlow;
        externallyChanged.nodes[1].name = "EXTERNAL FLOW EDIT";
        Check(WriteFlowDocument(
                jointFlow.generic_u8string(), externallyChanged, jointError),
            "UI-05 external edit fixture write failed");
        Check(flowSession.RenameNode(
                screenNodeId, "STALE LOCAL FLOW EDIT", jointError),
            "UI-05 stale authoring mutation failed");
        const auto stale = ScreenFlowAtomicSaveService::Save(
            screenSession, flowSession, root.generic_u8string());
        FlowDocument diskAfterStale;
        Check(!stale.succeeded &&
            ReadFlowDocument(
                jointFlow.generic_u8string(), projectId,
                diskAfterStale, jointError) &&
            diskAfterStale.nodes[1].name == "EXTERNAL FLOW EDIT" &&
            flowSession.IsDirty(),
            "UI-05 overwrote an external Flow edit");
    }

    if (failures != 0)
    {
        std::cerr << "RenegadeStoryFlowGate8EOutcomeParityTests: "
                  << failures << " failure(s)\n";
        return 1;
    }

    std::cout
        << "PASS: Story Flow Gate 8E Screen outcome routing parity\n";
    return 0;
}
