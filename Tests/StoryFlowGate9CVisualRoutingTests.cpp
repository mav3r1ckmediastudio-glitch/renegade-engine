#include "renegade/bridge/StoryFlowAuthoringModel.h"
#include "renegade/bridge/StoryFlowAuthoringSession.h"
#include "renegade/bridge/StoryFlowJourneyModel.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

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
        std::cerr << "RenegadeStoryFlowGate9CVisualRoutingTests: "
                  << message << '\n';
        return 1;
    }

    const FlowRoute* FindRoute(const FlowDocument& document, const StableId& id)
    {
        const auto found = std::find_if(
            document.routes.begin(), document.routes.end(),
            [&](const FlowRoute& route) { return route.id == id; });
        return found == document.routes.end() ? nullptr : &*found;
    }

    bool LoadProjection(
        StoryFlowAuthoringSession& session,
        const StableId& projectId,
        StoryFlowAuthoringModel& model,
        StoryFlowJourneyModel& journey,
        std::string& error)
    {
        if (!model.Load(session.Document(), projectId, error))
            return false;
        return journey.Build(model, error);
    }

    const StoryFlowJourneyExit* FindExit(
        const StoryFlowJourneyModel& journey,
        const StableId& routeId)
    {
        const auto found = std::find_if(
            journey.Exits().begin(), journey.Exits().end(),
            [&](const StoryFlowJourneyExit& exit) { return exit.routeId == routeId; });
        return found == journey.Exits().end() ? nullptr : &*found;
    }
}

int main()
{
    using namespace renegade::bridge;

    const StableId projectId = "91000000-0000-4000-8000-000000000001";
    const StableId flowId = "91000000-0000-4000-8000-000000000002";
    const StableId startId = "91000000-0000-4000-8000-000000000003";
    const StableId screenId = "91000000-0000-4000-8000-000000000004";
    const StableId levelAId = "91000000-0000-4000-8000-000000000005";
    const StableId levelBId = "91000000-0000-4000-8000-000000000006";
    const StableId completeId = "91000000-0000-4000-8000-000000000007";
    const StableId startRouteId = "92000000-0000-4000-8000-000000000001";
    const StableId playRouteId = "92000000-0000-4000-8000-000000000002";
    const StableId branchRouteId = "92000000-0000-4000-8000-000000000003";
    const StableId finishRouteId = "92000000-0000-4000-8000-000000000004";

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade story flow gate9c " + std::to_string(unique))};
    const fs::path root = temporary.path / "Routing Project";
    const fs::path flowPath = root / "Content/Flow/Main.renegade-flow";
    fs::create_directories(flowPath.parent_path());

    FlowDocument document;
    document.envelope = CreateDocumentEnvelope(
        projectId,
        StoryFlowDocumentType,
        "Content/Flow/Main.renegade-flow",
        "Story Flow Gate 9C visual routing proof");
    document.envelope.documentId = flowId;
    document.startNodeId = startId;

    FlowNode start{startId, FlowNodeKind::GameStart, "Game Start"};

    FlowNode screen;
    screen.id = screenId;
    screen.kind = FlowNodeKind::Screen;
    screen.name = "Title Screen";
    screen.screenDocumentId = "93000000-0000-4000-8000-000000000001";
    screen.screenPathHint = "Content/Screens/Title.renegade-screen";

    FlowNode levelA;
    levelA.id = levelAId;
    levelA.kind = FlowNodeKind::Level;
    levelA.name = "Village";
    levelA.sceneAssetId = "94000000-0000-4000-8000-000000000001";
    levelA.scenePathHint = "Content/Scenes/Village.wiscene";

    FlowNode levelB;
    levelB.id = levelBId;
    levelB.kind = FlowNodeKind::Level;
    levelB.name = "Forest";
    levelB.sceneAssetId = "94000000-0000-4000-8000-000000000002";
    levelB.scenePathHint = "Content/Scenes/Forest.wiscene";

    FlowNode complete;
    complete.id = completeId;
    complete.kind = FlowNodeKind::CompleteGame;
    complete.name = "Victory";

    FlowRoute startRoute;
    startRoute.id = startRouteId;
    startRoute.sourceNodeId = startId;
    startRoute.outcome = GameStartOutcome;
    startRoute.destinationNodeId = screenId;

    FlowRoute playRoute;
    playRoute.id = playRouteId;
    playRoute.sourceNodeId = screenId;
    playRoute.outcome = "play";
    playRoute.destinationNodeId = levelAId;
    playRoute.destinationEntry = "player_entry";

    FlowRoute branchRoute;
    branchRoute.id = branchRouteId;
    branchRoute.sourceNodeId = screenId;
    branchRoute.outcome = "alternate";
    branchRoute.destinationNodeId = levelBId;
    branchRoute.destinationEntry = "player_entry";
    branchRoute.priority = 7;
    branchRoute.conditions.push_back({
        "story.alt",
        FlowConditionOperator::Equals,
        "true",
    });

    FlowRoute finishRoute;
    finishRoute.id = finishRouteId;
    finishRoute.sourceNodeId = levelAId;
    finishRoute.outcome = "next";
    finishRoute.destinationNodeId = completeId;

    document.nodes = {start, screen, levelA, levelB, complete};
    document.routes = {startRoute, playRoute, branchRoute, finishRoute};

    std::string error;
    if (!WriteFlowDocument(flowPath.generic_u8string(), document, error))
        return Fail(temporary.path, "fixture Flow did not serialize");

    StoryFlowAuthoringSession session;
    if (!session.Open(flowPath.generic_u8string(), projectId, error))
        return Fail(temporary.path, "authoring session did not open");

    StoryFlowAuthoringModel model;
    StoryFlowJourneyModel journey;
    if (!LoadProjection(session, projectId, model, journey, error))
        return Fail(temporary.path, "initial Journey projection failed");

    if (journey.Exits().size() != session.Document().routes.size() ||
        FindExit(journey, startRouteId) == nullptr ||
        FindExit(journey, playRouteId) == nullptr ||
        FindExit(journey, branchRouteId) == nullptr ||
        FindExit(journey, finishRouteId) == nullptr)
    {
        return Fail(temporary.path,
            "Journey did not project every authoritative Story Flow route");
    }

    const auto* branchExit = FindExit(journey, branchRouteId);
    if (!branchExit || branchExit->primaryContinuation)
    {
        return Fail(temporary.path,
            "secondary Screen outcome was not projected as a branch route");
    }

    const FlowRoute* beforeRewire = FindRoute(session.Document(), branchRouteId);
    if (!beforeRewire)
        return Fail(temporary.path, "branch route missing before rewire");
    FlowRoute rewired = *beforeRewire;
    rewired.destinationNodeId = completeId;
    rewired.destinationEntry.clear();
    if (!session.UpdateRoute(branchRouteId, rewired, error))
        return Fail(temporary.path, "route destination rewire failed");

    const FlowRoute* afterRewire = FindRoute(session.Document(), branchRouteId);
    if (!afterRewire ||
        afterRewire->id != branchRouteId ||
        afterRewire->sourceNodeId != screenId ||
        afterRewire->outcome != "alternate" ||
        afterRewire->destinationNodeId != completeId ||
        afterRewire->priority != 7 ||
        afterRewire->conditions.size() != 1)
    {
        return Fail(temporary.path,
            "rewire did not preserve route identity and metadata");
    }

    if (!LoadProjection(session, projectId, model, journey, error) ||
        FindExit(journey, branchRouteId) == nullptr ||
        FindExit(journey, branchRouteId)->destinationNodeId != completeId)
    {
        return Fail(temporary.path,
            "Journey did not mirror the rewired authoritative destination");
    }

    if (!session.DeleteRoute(branchRouteId, error) ||
        FindRoute(session.Document(), branchRouteId) != nullptr)
    {
        return Fail(temporary.path, "selected route delete failed");
    }
    if (!LoadProjection(session, projectId, model, journey, error) ||
        FindExit(journey, branchRouteId) != nullptr)
    {
        return Fail(temporary.path, "Journey retained a deleted route");
    }

    if (!session.Undo(error) ||
        FindRoute(session.Document(), branchRouteId) == nullptr ||
        FindRoute(session.Document(), branchRouteId)->destinationNodeId != completeId)
    {
        return Fail(temporary.path,
            "Undo did not restore the deleted route with rewired identity");
    }
    if (!LoadProjection(session, projectId, model, journey, error) ||
        FindExit(journey, branchRouteId) == nullptr)
    {
        return Fail(temporary.path, "Journey did not mirror route Undo");
    }

    if (!session.Redo(error) || FindRoute(session.Document(), branchRouteId) != nullptr)
        return Fail(temporary.path, "Redo did not re-delete the same route");
    if (!LoadProjection(session, projectId, model, journey, error) ||
        FindExit(journey, branchRouteId) != nullptr)
    {
        return Fail(temporary.path, "Journey did not mirror route Redo");
    }

    std::cout << "PASS: Story Flow Gate 9C authoritative visual-routing semantics\n";
    return 0;
}
