#include "renegade/bridge/StoryFlowAuthoringModel.h"
#include "renegade/bridge/StoryFlowLayoutService.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    constexpr const char* ProjectId =
        "11111111-1111-4111-8111-111111111111";
    constexpr const char* FlowDocumentId =
        "22222222-2222-4222-8222-222222222222";
    constexpr const char* GameStartId =
        "33333333-3333-4333-8333-333333333333";
    constexpr const char* LevelOneId =
        "44444444-4444-4444-8444-444444444444";
    constexpr const char* LevelTwoId =
        "55555555-5555-4555-8555-555555555555";
    constexpr const char* CompleteId =
        "66666666-6666-4666-8666-666666666666";
    constexpr const char* SceneOneId =
        "77777777-7777-4777-8777-777777777777";
    constexpr const char* SceneTwoId =
        "88888888-8888-4888-8888-888888888888";
    constexpr const char* RouteStartId =
        "99999999-9999-4999-8999-999999999999";
    constexpr const char* RouteOneId =
        "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
    constexpr const char* RouteTwoId =
        "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb";
    constexpr const char* StaleNodeId =
        "cccccccc-cccc-4ccc-8ccc-cccccccccccc";

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
        std::cerr << "RenegadeStoryFlowPresentationTests: " << message << '\n';
        return 1;
    }

    FlowDocument MakeFlow()
    {
        FlowDocument document;
        document.envelope = CreateDocumentEnvelope(
            ProjectId,
            StoryFlowDocumentType,
            "Content/Flow/Main.renegade-flow",
            "Renegade Story Flow Gate 1 tests");
        document.envelope.documentId = FlowDocumentId;
        document.startNodeId = GameStartId;
        document.nodes = {
            {GameStartId, FlowNodeKind::GameStart, "Game Start", {}, {}},
            {LevelOneId, FlowNodeKind::Level, "Level One",
                SceneOneId, "Content/Scenes/LevelOne.wiscene"},
            {LevelTwoId, FlowNodeKind::Level, "Level Two",
                SceneTwoId, "Content/Scenes/LevelTwo.wiscene"},
            {CompleteId, FlowNodeKind::CompleteGame, "Complete Game", {}, {}},
        };
        document.routes = {
            {RouteStartId, GameStartId, GameStartOutcome, LevelOneId,
                "default", 0, {}},
            {RouteOneId, LevelOneId, "level.complete", LevelTwoId,
                "from-level-one", 0, {}},
            {RouteTwoId, LevelTwoId, "level.complete", CompleteId,
                {}, 0, {}},
        };
        return document;
    }

    std::unordered_map<StableId, std::pair<float, float>> LayoutPositions(
        const StoryFlowLayoutDocument& layout)
    {
        std::unordered_map<StableId, std::pair<float, float>> positions;
        for (const auto& node : layout.nodes)
            positions.emplace(node.nodeId, std::make_pair(node.x, node.y));
        return positions;
    }
}

int main()
{
    using namespace renegade::bridge;

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade story flow presentation " +
            std::to_string(unique))
    };
    fs::create_directories(temporary.path);

    FlowDocument flow = MakeFlow();
    std::string error;
    if (!ValidateFlowDocument(flow, ProjectId, error))
        return Fail(temporary.path, "valid LP02 fixture was rejected");

    StoryFlowAuthoringModel model;
    if (!model.Load(flow, ProjectId, error))
        return Fail(temporary.path, "read-only authoring model did not load");
    if (!model.IsLoaded() || model.Nodes().size() != 4 ||
        model.Routes().size() != 3 || model.GameStartNodeId() != GameStartId)
    {
        return Fail(temporary.path, "authoring model did not expose the Flow");
    }
    if (model.Nodes()[0].id != GameStartId ||
        model.Nodes()[0].presentationColumn != 0 ||
        model.FindNode(LevelOneId) == nullptr ||
        model.FindNode(LevelOneId)->presentationColumn != 1 ||
        model.FindNode(LevelTwoId) == nullptr ||
        model.FindNode(LevelTwoId)->presentationColumn != 2 ||
        model.FindNode(CompleteId) == nullptr ||
        model.FindNode(CompleteId)->presentationColumn != 3)
    {
        return Fail(temporary.path, "deterministic presentation order was wrong");
    }

    const StoryFlowLayoutDocument firstLayout =
        BuildDeterministicStoryFlowLayout(model, ProjectId, FlowDocumentId);
    if (firstLayout.nodes.size() != model.Nodes().size())
        return Fail(temporary.path, "default layout omitted Flow nodes");

    FlowDocument reordered = MakeFlow();
    std::reverse(reordered.nodes.begin(), reordered.nodes.end());
    std::reverse(reordered.routes.begin(), reordered.routes.end());
    StoryFlowAuthoringModel reorderedModel;
    if (!reorderedModel.Load(reordered, ProjectId, error))
        return Fail(temporary.path, "reordered semantic Flow was rejected");
    const StoryFlowLayoutDocument reorderedLayout =
        BuildDeterministicStoryFlowLayout(
            reorderedModel, ProjectId, FlowDocumentId);
    if (LayoutPositions(firstLayout) != LayoutPositions(reorderedLayout))
    {
        return Fail(temporary.path,
            "default layout depended on serialized node/route order");
    }

    const std::string layoutPath = ResolveStoryFlowLayoutPath(
        temporary.path.generic_u8string(), FlowDocumentId);
    if (layoutPath.empty() ||
        fs::u8path(layoutPath).filename() !=
            fs::u8path(std::string(FlowDocumentId) + StoryFlowLayoutExtension))
    {
        return Fail(temporary.path, "layout path was not project/editor scoped");
    }
    if (!WriteStoryFlowLayout(layoutPath, firstLayout, error))
        return Fail(temporary.path, "layout could not be written");

    StoryFlowLayoutDocument reopened;
    if (!ReadStoryFlowLayout(
            layoutPath, ProjectId, FlowDocumentId, reopened, error) ||
        reopened.nodes.size() != firstLayout.nodes.size() ||
        LayoutPositions(reopened) != LayoutPositions(firstLayout))
    {
        return Fail(temporary.path, "layout did not round-trip");
    }

    reopened.nodes.push_back({StaleNodeId, 1200.0f, 900.0f});
    reopened.canvas.panX = 17.0f;
    reopened.canvas.panY = -31.0f;
    reopened.canvas.zoom = 1.25f;
    if (!ReconcileStoryFlowLayout(
            model, ProjectId, FlowDocumentId, reopened, error))
    {
        return Fail(temporary.path, "stale layout could not be reconciled");
    }
    if (reopened.nodes.size() != model.Nodes().size() ||
        std::any_of(
            reopened.nodes.begin(), reopened.nodes.end(),
            [](const StoryFlowNodeLayout& node)
            {
                return node.nodeId == StaleNodeId;
            }) ||
        reopened.canvas.panX != 17.0f || reopened.canvas.panY != -31.0f ||
        reopened.canvas.zoom != 1.25f)
    {
        return Fail(temporary.path,
            "layout reconciliation retained stale semantics or lost canvas state");
    }

    // Presentation operations must not alter the authoritative Flow document.
    if (flow.startNodeId != GameStartId || flow.nodes.size() != 4 ||
        flow.routes.size() != 3 || flow.nodes[1].id != LevelOneId ||
        flow.routes[1].id != RouteOneId ||
        flow.routes[1].destinationNodeId != LevelTwoId)
    {
        return Fail(temporary.path, "presentation work mutated Flow semantics");
    }

    FlowInterpreter interpreter;
    if (!interpreter.Initialize(flow, error))
        return Fail(temporary.path, "Flow interpreter did not initialize");
    auto step = interpreter.Start();
    if (!step.succeeded || step.currentNodeId != GameStartId)
        return Fail(temporary.path, "Flow no longer started at Game Start");
    step = interpreter.EmitOutcome(GameStartOutcome);
    if (!step.succeeded || step.currentNodeId != LevelOneId ||
        step.destinationEntry != "default")
    {
        return Fail(temporary.path, "layout changed Game Start traversal");
    }
    step = interpreter.EmitOutcome("level.complete");
    if (!step.succeeded || step.currentNodeId != LevelTwoId ||
        step.destinationEntry != "from-level-one")
    {
        return Fail(temporary.path, "layout changed Level One traversal");
    }
    step = interpreter.EmitOutcome("level.complete");
    if (!step.succeeded || step.currentNodeId != CompleteId ||
        step.terminalAction != FlowTerminalAction::CompleteGame)
    {
        return Fail(temporary.path, "layout changed terminal traversal");
    }

    FlowDocument unreachable = MakeFlow();
    unreachable.routes.erase(unreachable.routes.begin() + 1);
    unreachable.routes.erase(unreachable.routes.begin() + 1);
    StoryFlowAuthoringModel diagnosticModel;
    if (!diagnosticModel.Load(unreachable, ProjectId, error))
        return Fail(temporary.path, "valid unreachable Flow was rejected");
    if (diagnosticModel.Diagnostics().empty() ||
        diagnosticModel.FindNode(LevelTwoId) == nullptr ||
        diagnosticModel.FindNode(LevelTwoId)->reachableFromStart)
    {
        return Fail(temporary.path, "unreachable node diagnostic was not exposed");
    }

    std::cout << "RenegadeStoryFlowPresentationTests: PASS\n";
    return 0;
}
