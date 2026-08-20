#include "RuntimeFlow.h"
#include "RuntimeScreen.h"
#include "renegade/bridge/DependencyService.h"
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/ScreenService.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;
    using namespace renegade::runtime;

    struct TemporaryDirectory
    {
        fs::path path;
        ~TemporaryDirectory()
        {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    };

    int Fail(const fs::path& root, const char* message)
    {
        std::error_code ignored;
        fs::remove_all(root, ignored);
        std::cerr << "RenegadeStoryFlowScreenSemanticsTests: "
                  << message << '\n';
        return 1;
    }

    ScreenDocument MakeActionScreen(
        const StableId& projectId,
        const StableId& documentId,
        const std::string& pathHint,
        const std::string& actionId,
        const std::string& title)
    {
        ScreenDocument screen;
        screen.envelope = CreateDocumentEnvelope(
            projectId,
            RuntimeScreenDocumentType,
            pathHint,
            "Story Flow Gate 2 runtime-screen proof");
        screen.envelope.documentId = documentId;
        screen.designWidth = 1280.0f;
        screen.designHeight = 720.0f;
        screen.actions = {{actionId}};

        ScreenWidget label;
        label.id = GenerateStableId();
        label.kind = ScreenWidgetKind::Text;
        label.name = title + " Label";
        label.rect = {240.0f, 140.0f, 800.0f, 100.0f};
        label.visible = true;
        label.enabled = false;
        label.text = title;

        ScreenWidget button;
        button.id = GenerateStableId();
        button.kind = ScreenWidgetKind::Button;
        button.name = title + " Action";
        button.rect = {440.0f, 360.0f, 400.0f, 80.0f};
        button.visible = true;
        button.enabled = true;
        button.text = actionId;
        button.actionId = actionId;

        screen.widgets = {label, button};
        screen.focusOrder = {button.id};
        return screen;
    }

    FlowNode MakeNode(
        const StableId& id,
        const FlowNodeKind kind,
        std::string name)
    {
        FlowNode node;
        node.id = id;
        node.kind = kind;
        node.name = std::move(name);
        return node;
    }

    FlowRoute MakeRoute(
        const StableId& id,
        const StableId& source,
        std::string outcome,
        const StableId& destination,
        std::string destinationEntry = {})
    {
        FlowRoute route;
        route.id = id;
        route.sourceNodeId = source;
        route.outcome = std::move(outcome);
        route.destinationNodeId = destination;
        route.destinationEntry = std::move(destinationEntry);
        route.priority = 0;
        return route;
    }
}

int main()
{
    using namespace renegade::bridge;
    using namespace renegade::runtime;

    const StableId projectId = "11111111-1111-4111-8111-111111111111";
    const StableId flowId = "22222222-2222-4222-8222-222222222222";
    const StableId gameStartId = "33333333-3333-4333-8333-333333333333";
    const StableId titleNodeId = "44444444-4444-4444-8444-444444444444";
    const StableId titleScreenId = "55555555-5555-4555-8555-555555555555";
    const StableId levelNodeId = "66666666-6666-4666-8666-666666666666";
    const StableId sceneId = "77777777-7777-4777-8777-777777777777";
    const StableId victoryNodeId = "88888888-8888-4888-8888-888888888888";
    const StableId victoryScreenId = "99999999-9999-4999-8999-999999999999";
    const StableId completeNodeId = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade story flow gate2 " + std::to_string(unique))
    };
    const fs::path root = temporary.path / "Project With Spaces";
    const fs::path flowPath = root / "Content/Flow/Main.renegade-flow";
    const fs::path titlePath =
        root / "Content/UI/Renamed/Title.renegade-screen";
    const fs::path victoryPath =
        root / "Content/UI/Victory.renegade-screen";
    fs::create_directories(flowPath.parent_path());
    fs::create_directories(titlePath.parent_path());
    fs::create_directories(victoryPath.parent_path());

    ScreenDocument title = MakeActionScreen(
        projectId,
        titleScreenId,
        "Content/UI/Renamed/Title.renegade-screen",
        "new_game",
        "TITLE SCREEN");
    ScreenDocument victory = MakeActionScreen(
        projectId,
        victoryScreenId,
        "Content/UI/Victory.renegade-screen",
        "continue",
        "VICTORY SCREEN");

    std::string error;
    if (!ValidateScreenDocument(title, projectId, error) ||
        !ValidateScreenDocument(victory, projectId, error))
    {
        return Fail(temporary.path,
            "arbitrary authored Screen actions were rejected");
    }
    if (!WriteScreenDocument(titlePath.generic_u8string(), title, error) ||
        !WriteScreenDocument(victoryPath.generic_u8string(), victory, error))
    {
        return Fail(temporary.path,
            "Gate 2 Screen documents did not serialize");
    }

    FlowDocument flow;
    flow.envelope = CreateDocumentEnvelope(
        projectId,
        StoryFlowDocumentType,
        "Content/Flow/Main.renegade-flow",
        "Story Flow Gate 2 Screen semantics proof");
    flow.envelope.documentId = flowId;
    flow.startNodeId = gameStartId;

    FlowNode gameStart = MakeNode(
        gameStartId,
        FlowNodeKind::GameStart,
        "Game Start");

    FlowNode titleNode = MakeNode(
        titleNodeId,
        FlowNodeKind::Screen,
        "Title Screen");
    titleNode.screenDocumentId = titleScreenId;
    // Intentionally stale. Stable document identity must repair this after a move.
    titleNode.screenPathHint = "Content/UI/Old/Title.renegade-screen";

    FlowNode level = MakeNode(
        levelNodeId,
        FlowNodeKind::Level,
        "Level One");
    level.sceneAssetId = sceneId;
    level.scenePathHint = "Content/Scenes/LevelOne.wiscene";

    FlowNode victoryNode = MakeNode(
        victoryNodeId,
        FlowNodeKind::Screen,
        "Victory Screen");
    victoryNode.screenDocumentId = victoryScreenId;
    victoryNode.screenPathHint = "Content/UI/Victory.renegade-screen";

    FlowNode complete = MakeNode(
        completeNodeId,
        FlowNodeKind::CompleteGame,
        "Complete Game");

    flow.nodes = {gameStart, titleNode, level, victoryNode, complete};
    flow.routes = {
        MakeRoute(
            "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbb1",
            gameStartId,
            GameStartOutcome,
            titleNodeId),
        MakeRoute(
            "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbb2",
            titleNodeId,
            "new_game",
            levelNodeId,
            "default"),
        MakeRoute(
            "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbb3",
            levelNodeId,
            "level.complete",
            victoryNodeId),
        MakeRoute(
            "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbb4",
            victoryNodeId,
            "continue",
            completeNodeId),
    };

    if (!ValidateFlowDocument(flow, projectId, error) ||
        !WriteFlowDocument(flowPath.generic_u8string(), flow, error))
    {
        return Fail(temporary.path,
            "Screen-containing Story Flow did not validate and serialize");
    }

    FlowDocument reopened;
    if (!ReadFlowDocument(
            flowPath.generic_u8string(),
            projectId,
            reopened,
            error) ||
        reopened.nodes.size() != 5 ||
        reopened.nodes[1].kind != FlowNodeKind::Screen ||
        reopened.nodes[1].screenDocumentId != titleScreenId ||
        reopened.nodes[3].screenDocumentId != victoryScreenId)
    {
        return Fail(temporary.path,
            "Screen references did not round-trip through Story Flow");
    }

    StoryFlowDependencyDocument dependencies;
    const auto dependencyReader = MakeStoryFlowDependencyReader(projectId);
    if (!dependencyReader(
            flowPath.generic_u8string(),
            dependencies,
            error) ||
        dependencies.scenePathHints.size() != 1 ||
        dependencies.screenPathHints.size() != 2 ||
        dependencies.screenPathHints[0] !=
            "Content/UI/Old/Title.renegade-screen" ||
        dependencies.screenPathHints[1] !=
            "Content/UI/Victory.renegade-screen")
    {
        return Fail(temporary.path,
            "Story Flow dependency reader did not expose Screen documents");
    }

    FlowInterpreter interpreter;
    if (!interpreter.Initialize(reopened, error))
    {
        return Fail(temporary.path, "Flow interpreter rejected Gate 2 document");
    }
    auto step = interpreter.Start();
    if (!step.succeeded || step.currentNodeKind != FlowNodeKind::GameStart)
    {
        return Fail(temporary.path, "Gate 2 Flow did not start at Game Start");
    }
    step = interpreter.EmitOutcome(GameStartOutcome);
    if (!step.succeeded || step.currentNodeKind != FlowNodeKind::Screen ||
        step.screenDocumentId != titleScreenId)
    {
        return Fail(temporary.path,
            "Game Start did not enter the Title Screen destination");
    }

    RuntimeScreenController titleController;
    if (!titleController.Initialize(title, error))
    {
        return Fail(temporary.path,
            "Runtime Screen controller rejected Title Screen");
    }
    RuntimeActionRequest request;
    if (!titleController.MakeFocusedActionRequest(
            RuntimeInputSource::Test,
            1,
            request,
            error) ||
        request.actionId != "new_game")
    {
        return Fail(temporary.path,
            "Title Screen did not emit its authored new_game action");
    }

    step = interpreter.EmitOutcome(request.actionId);
    if (!step.succeeded || step.currentNodeKind != FlowNodeKind::Level ||
        step.destinationEntry != "default")
    {
        return Fail(temporary.path,
            "Title Screen action did not advance into Level One");
    }
    step = interpreter.EmitOutcome("level.complete");
    if (!step.succeeded || step.currentNodeKind != FlowNodeKind::Screen ||
        step.screenDocumentId != victoryScreenId)
    {
        return Fail(temporary.path,
            "Level One did not advance into Victory Screen");
    }

    RuntimeScreenController victoryController;
    if (!victoryController.Initialize(victory, error) ||
        !victoryController.MakeFocusedActionRequest(
            RuntimeInputSource::Test,
            2,
            request,
            error) ||
        request.actionId != "continue")
    {
        return Fail(temporary.path,
            "Victory Screen did not emit its authored continue action");
    }
    step = interpreter.EmitOutcome(request.actionId);
    if (!step.succeeded ||
        step.currentNodeKind != FlowNodeKind::CompleteGame ||
        step.terminalAction != FlowTerminalAction::CompleteGame)
    {
        return Fail(temporary.path,
            "Victory Screen action did not reach Complete Game");
    }

    RuntimeBootstrapResult bootstrap;
    bootstrap.succeeded = true;
    bootstrap.code = RuntimeBootstrapCode::Success;
    bootstrap.project.projectId = projectId;
    bootstrap.project.rootPath = root.generic_u8string();
    bootstrap.project.startupFlowId = flowId;
    bootstrap.startupFlowPath = flowPath.generic_u8string();

    RuntimeFlowController runtimeFlow;
    if (!runtimeFlow.Initialize(bootstrap, error))
    {
        return Fail(temporary.path,
            "Runtime Flow controller rejected Gate 2 document");
    }
    step = runtimeFlow.Start();
    if (!step.succeeded)
    {
        return Fail(temporary.path,
            "Runtime Flow controller did not start");
    }
    step = runtimeFlow.EmitOutcome(GameStartOutcome);
    if (!step.succeeded || step.currentNodeKind != FlowNodeKind::Screen)
    {
        return Fail(temporary.path,
            "Runtime Flow controller did not enter Title Screen");
    }

    SceneService scenes;
    if (!runtimeFlow.ApplyStep(scenes, bootstrap, step, error) ||
        bootstrap.screenDocumentId != titleScreenId ||
        fs::u8path(bootstrap.startupScreenPath) !=
            fs::weakly_canonical(titlePath))
    {
        return Fail(temporary.path,
            "Runtime did not resolve the moved Title Screen by stable ID");
    }

    FlowDocument invalid = flow;
    invalid.nodes[1].screenDocumentId.clear();
    if (ValidateFlowDocument(invalid, projectId, error))
    {
        return Fail(temporary.path,
            "Screen node without stable document identity was accepted");
    }

    invalid = flow;
    invalid.nodes[2].screenDocumentId = titleScreenId;
    invalid.nodes[2].screenPathHint = "Content/UI/Renamed/Title.renegade-screen";
    if (ValidateFlowDocument(invalid, projectId, error))
    {
        return Fail(temporary.path,
            "Level node carrying a Screen reference was accepted");
    }

    std::cout
        << "PASS: Game Start -> Title Screen -> Level -> Victory Screen -> Complete Game\n";
    return 0;
}
