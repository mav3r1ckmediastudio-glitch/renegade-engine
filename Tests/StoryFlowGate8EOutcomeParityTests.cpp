#include "RuntimeFlow.h"
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/StoryFlowScreenReferenceService.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
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
