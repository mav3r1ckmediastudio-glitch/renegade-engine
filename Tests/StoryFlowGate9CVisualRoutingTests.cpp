#include "renegade/bridge/StoryFlowAuthoringModel.h"
#include "renegade/bridge/StoryFlowAuthoringSession.h"

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

    bool ReloadModel(
        StoryFlowAuthoringSession& session,
        const StableId& projectId,
        StoryFlowAuthoringModel& model,
        std::string& error)
    {
        return model.Load(session.Document(), projectId, error);
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

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade story flow gate9c " + std::to_string(unique))};
    const fs::path root = temporary.path / "Graph Project";
    const fs::path flowPath = root / "Content/Flow/Main.renegade-flow";
    fs::create_directories(flowPath.parent_path());

    FlowDocument document;
    document.envelope = CreateDocumentEnvelope(
        projectId,
        StoryFlowDocumentType,
        "Content/Flow/Main.renegade-flow",
        "Story Flow Gate 9C Graph routing proof");
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

    document.nodes = {start, screen, levelA, levelB, complete};
    document.routes = {startRoute, playRoute};

    std::string error;
    if (!WriteFlowDocument(flowPath.generic_u8string(), document, error))
        return Fail(temporary.path, "fixture Flow did not serialize");

    StoryFlowAuthoringSession session;
    if (!session.Open(flowPath.generic_u8string(), projectId, error))
        return Fail(temporary.path, "authoring session did not open");

    StoryFlowAuthoringModel model;
    if (!ReloadModel(session, projectId, model, error) ||
        model.Routes().size() != 2)
    {
        return Fail(temporary.path, "initial Graph projection was not authoritative");
    }

    // Equivalent of dragging a Graph output socket onto a destination input:
    // creation must produce one real Story Flow route and one Undo step.
    const std::size_t beforeCreateUndoCount = session.UndoCount();
    FlowRoute branch;
    branch.sourceNodeId = screenId;
    branch.outcome = "alternate";
    branch.destinationNodeId = levelBId;
    branch.destinationEntry = "player_entry";
    branch.priority = 7;
    branch.conditions.push_back({
        "story.alt",
        FlowConditionOperator::Equals,
        "true",
    });
    StableId branchRouteId;
    if (!session.AddRoute(branch, branchRouteId, error) ||
        !IsValidStableId(branchRouteId) ||
        session.UndoCount() != beforeCreateUndoCount + 1)
    {
        return Fail(temporary.path,
            "Graph-style link creation did not create one authoritative history mutation");
    }
    if (!ReloadModel(session, projectId, model, error) ||
        model.FindRoute(branchRouteId) == nullptr)
    {
        return Fail(temporary.path, "created Graph route did not project into the model");
    }

    // Equivalent of detaching the existing destination end and dropping it on
    // another input. It MUST update the same route, not delete+create a new ID.
    const FlowRoute* beforeRewire = FindRoute(session.Document(), branchRouteId);
    if (!beforeRewire)
        return Fail(temporary.path, "branch route missing before rewire");
    const StableId originalRouteId = beforeRewire->id;
    const std::size_t beforeRewireRouteCount = session.Document().routes.size();
    const std::size_t beforeRewireUndoCount = session.UndoCount();

    FlowRoute rewired = *beforeRewire;
    rewired.destinationNodeId = completeId;
    rewired.destinationEntry.clear();
    if (!session.UpdateRoute(branchRouteId, rewired, error))
        return Fail(temporary.path, "Graph destination-end rewire failed");

    const FlowRoute* afterRewire = FindRoute(session.Document(), branchRouteId);
    if (!afterRewire ||
        afterRewire->id != originalRouteId ||
        afterRewire->sourceNodeId != screenId ||
        afterRewire->outcome != "alternate" ||
        afterRewire->destinationNodeId != completeId ||
        afterRewire->priority != 7 ||
        afterRewire->conditions.size() != 1 ||
        session.Document().routes.size() != beforeRewireRouteCount ||
        session.UndoCount() != beforeRewireUndoCount + 1)
    {
        return Fail(temporary.path,
            "rewire duplicated/replaced the route instead of preserving identity and metadata");
    }
    if (!ReloadModel(session, projectId, model, error) ||
        model.FindRoute(branchRouteId) == nullptr ||
        model.FindRoute(branchRouteId)->destinationNodeId != completeId)
    {
        return Fail(temporary.path,
            "authoring model did not mirror the rewired authoritative destination");
    }

    // A cancelled visual detach performs no session mutation at all. The UI
    // implements this by keeping the FlowRoute untouched until a valid drop.
    const FlowRoute cancelledSnapshot = *FindRoute(session.Document(), branchRouteId);
    const std::size_t cancelledUndoCount = session.UndoCount();
    const std::size_t cancelledRouteCount = session.Document().routes.size();
    if (FindRoute(session.Document(), branchRouteId)->destinationNodeId !=
            cancelledSnapshot.destinationNodeId ||
        session.UndoCount() != cancelledUndoCount ||
        session.Document().routes.size() != cancelledRouteCount)
    {
        return Fail(temporary.path,
            "cancelled destination detach altered authoritative Flow state");
    }

    // Selected-link Delete is a normal Story Flow mutation. Undo/Redo must
    // restore/remove the same stable route, including its rewired destination.
    if (!session.DeleteRoute(branchRouteId, error) ||
        FindRoute(session.Document(), branchRouteId) != nullptr)
    {
        return Fail(temporary.path, "selected Graph link delete failed");
    }
    if (!session.Undo(error))
        return Fail(temporary.path, "Undo failed after Graph link delete");
    const FlowRoute* restored = FindRoute(session.Document(), branchRouteId);
    if (!restored || restored->id != originalRouteId ||
        restored->destinationNodeId != completeId ||
        restored->priority != 7 || restored->conditions.size() != 1)
    {
        return Fail(temporary.path,
            "Undo did not restore the same rewired route identity and metadata");
    }
    if (!session.Redo(error) || FindRoute(session.Document(), branchRouteId) != nullptr)
        return Fail(temporary.path, "Redo did not re-delete the same Graph route");

    std::cout << "PASS: Story Flow Gate 9C Graph link identity/history semantics\n";
    return 0;
}
