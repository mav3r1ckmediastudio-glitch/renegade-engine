#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/StoryFlowAuthoringModel.h"
#include "renegade/bridge/StoryFlowAuthoringSession.h"
#include "renegade/bridge/StoryFlowJourneyModel.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_set>

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
        std::cerr << "RenegadeStoryFlowGate9ENonlinearAuthoringTests: "
                  << message << '\n';
        return 1;
    }

    const FlowRoute* FindRoute(
        const FlowDocument& document,
        const StableId& routeId)
    {
        const auto found = std::find_if(
            document.routes.begin(), document.routes.end(),
            [&](const FlowRoute& route) { return route.id == routeId; });
        return found == document.routes.end() ? nullptr : &*found;
    }

    bool BuildJourney(
        const StoryFlowAuthoringSession& session,
        StoryFlowAuthoringModel& authoring,
        StoryFlowJourneyModel& journey,
        std::string& error)
    {
        return authoring.Load(session.Document(), session.ProjectId(), error) &&
            journey.Build(authoring, error);
    }

    bool HasSingleCardPerNode(
        const FlowDocument& document,
        const StoryFlowJourneyModel& journey)
    {
        std::unordered_set<StableId> cardNodeIds;
        for (const auto& card : journey.Cards())
            cardNodeIds.insert(card.nodeId);
        return journey.Cards().size() == document.nodes.size() &&
            cardNodeIds.size() == document.nodes.size();
    }

    bool JourneyAndAuthoringShareRoutes(
        const StoryFlowAuthoringModel& authoring,
        const StoryFlowJourneyModel& journey)
    {
        std::unordered_set<StableId> authoringRouteIds;
        std::unordered_set<StableId> journeyRouteIds;
        for (const auto& route : authoring.Routes())
            authoringRouteIds.insert(route.id);
        for (const auto& route : journey.Exits())
            journeyRouteIds.insert(route.routeId);
        return authoringRouteIds == journeyRouteIds;
    }
}

int main()
{
    using namespace renegade::bridge;

    const StableId projectId = "9e000000-0000-4000-8000-000000000001";
    const StableId flowId = "9e000000-0000-4000-8000-000000000002";
    const StableId startId = "9e000000-0000-4000-8000-000000000003";
    const StableId hubId = "9e000000-0000-4000-8000-000000000004";
    const StableId mainId = "9e000000-0000-4000-8000-000000000005";
    const StableId branchId = "9e000000-0000-4000-8000-000000000006";
    const StableId mergeId = "9e000000-0000-4000-8000-000000000007";
    const StableId completeId = "9e000000-0000-4000-8000-000000000008";
    const StableId startRouteId = "9e100000-0000-4000-8000-000000000001";
    const StableId mainRouteId = "9e100000-0000-4000-8000-000000000002";
    const StableId mergeRouteId = "9e100000-0000-4000-8000-000000000003";
    const StableId finishRouteId = "9e100000-0000-4000-8000-000000000004";

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade story flow gate9e " + std::to_string(unique))};
    const fs::path flowPath =
        temporary.path / "Content/Flow/Main.renegade-flow";
    fs::create_directories(flowPath.parent_path());

    FlowDocument document;
    document.envelope = CreateDocumentEnvelope(
        projectId,
        StoryFlowDocumentType,
        "Content/Flow/Main.renegade-flow",
        "Story Flow Gate 9E nonlinear Journey proof");
    document.envelope.documentId = flowId;
    document.startNodeId = startId;

    FlowNode start{startId, FlowNodeKind::GameStart, "Game Start"};
    FlowNode hub;
    hub.id = hubId;
    hub.kind = FlowNodeKind::Screen;
    hub.name = "Route Hub";
    hub.screenDocumentId = "9e200000-0000-4000-8000-000000000001";
    hub.screenPathHint = "Content/Screens/Hub.renegade-screen";
    FlowNode mainLevel;
    mainLevel.id = mainId;
    mainLevel.kind = FlowNodeKind::Level;
    mainLevel.name = "Main Route";
    mainLevel.sceneAssetId = "9e300000-0000-4000-8000-000000000001";
    mainLevel.scenePathHint = "Content/Scenes/Main.wiscene";
    FlowNode branchLevel;
    branchLevel.id = branchId;
    branchLevel.kind = FlowNodeKind::Level;
    branchLevel.name = "Alternate Route";
    branchLevel.sceneAssetId = "9e300000-0000-4000-8000-000000000002";
    branchLevel.scenePathHint = "Content/Scenes/Alternate.wiscene";
    FlowNode merge;
    merge.id = mergeId;
    merge.kind = FlowNodeKind::Level;
    merge.name = "Shared Destination";
    merge.sceneAssetId = "9e300000-0000-4000-8000-000000000003";
    merge.scenePathHint = "Content/Scenes/Shared.wiscene";
    FlowNode complete;
    complete.id = completeId;
    complete.kind = FlowNodeKind::CompleteGame;
    complete.name = "Victory";

    document.nodes = {
        start, hub, mainLevel, branchLevel, merge, complete};
    document.routes = {
        {startRouteId, startId, GameStartOutcome, hubId, {}, 0, {}},
        {mainRouteId, hubId, "new_game", mainId, "player_entry", 0, {}},
        {mergeRouteId, mainId, "next", mergeId, "player_entry", 0, {}},
        {finishRouteId, mergeId, "finish", completeId, {}, 0, {}},
    };

    std::string error;
    if (!WriteFlowDocument(flowPath.generic_u8string(), document, error))
        return Fail(temporary.path, "fixture Flow did not serialize");

    StoryFlowAuthoringSession session;
    if (!session.Open(flowPath.generic_u8string(), projectId, error))
        return Fail(temporary.path, "authoring session did not open");

    // Add an alternate route and merge it back into the existing main card.
    FlowRoute branchRoute;
    branchRoute.sourceNodeId = hubId;
    branchRoute.outcome = "options";
    branchRoute.destinationNodeId = branchId;
    branchRoute.destinationEntry = "player_entry";
    branchRoute.priority = 1;
    StableId branchRouteId;
    if (!session.AddRoute(branchRoute, branchRouteId, error) ||
        !IsValidStableId(branchRouteId))
    {
        return Fail(temporary.path, "alternate Journey action was not authored");
    }

    FlowRoute returnToMerge;
    returnToMerge.sourceNodeId = branchId;
    returnToMerge.outcome = "return";
    returnToMerge.destinationNodeId = mergeId;
    returnToMerge.destinationEntry = "player_entry";
    StableId returnRouteId;
    if (!session.AddRoute(returnToMerge, returnRouteId, error) ||
        !IsValidStableId(returnRouteId))
    {
        return Fail(temporary.path, "alternate Journey merge was not authored");
    }

    // Rewire the existing finish route into a loop. UpdateRoute must preserve
    // its identity so Journey and Graph continue to describe one topology.
    const FlowRoute* finish = FindRoute(session.Document(), finishRouteId);
    if (!finish)
        return Fail(temporary.path, "finish route was missing before rewire");
    FlowRoute loop = *finish;
    loop.destinationNodeId = hubId;
    loop.destinationEntry.clear();
    if (!session.UpdateRoute(finishRouteId, loop, error))
        return Fail(temporary.path, "stable-ID Journey loop rewire failed");

    const FlowRoute* rewired = FindRoute(session.Document(), finishRouteId);
    if (!rewired || rewired->id != finishRouteId ||
        rewired->destinationNodeId != hubId)
    {
        return Fail(temporary.path, "Journey rewire replaced route identity");
    }

    StoryFlowAuthoringModel authoring;
    StoryFlowJourneyModel journey;
    if (!BuildJourney(session, authoring, journey, error) ||
        !HasSingleCardPerNode(session.Document(), journey) ||
        !JourneyAndAuthoringShareRoutes(authoring, journey) ||
        journey.Tracks().size() < 2)
    {
        return Fail(temporary.path,
            "nonlinear Journey projection duplicated or omitted authoritative topology");
    }
    const std::size_t incomingMergeCount = std::count_if(
        journey.Exits().begin(), journey.Exits().end(),
        [&](const StoryFlowJourneyExit& route)
        {
            return route.destinationNodeId == mergeId;
        });
    const bool hasLoop = std::any_of(
        journey.Exits().begin(), journey.Exits().end(),
        [&](const StoryFlowJourneyExit& route)
        {
            return route.routeId == finishRouteId &&
                route.destinationNodeId == hubId;
        });
    if (incomingMergeCount != 2 || !hasLoop)
    {
        return Fail(temporary.path,
            "Journey did not represent the authored merge and loop");
    }

    // Shared history must undo and redo the same route rewire.
    if (!session.Undo(error))
        return Fail(temporary.path, "Journey rewire Undo failed");
    const FlowRoute* undone = FindRoute(session.Document(), finishRouteId);
    if (!undone || undone->destinationNodeId != completeId)
        return Fail(temporary.path, "Journey rewire Undo restored wrong topology");
    if (!session.Redo(error))
        return Fail(temporary.path, "Journey rewire Redo failed");
    const FlowRoute* redone = FindRoute(session.Document(), finishRouteId);
    if (!redone || redone->destinationNodeId != hubId)
        return Fail(temporary.path, "Journey rewire Redo restored wrong topology");

    // Persist and reopen through the governed session. The exact stable route
    // set and one-card-per-node projection must survive process boundaries.
    if (!session.Save(error) || session.IsDirty())
        return Fail(temporary.path, "nonlinear Journey Flow did not save cleanly");
    StoryFlowAuthoringSession reopened;
    if (!reopened.Open(flowPath.generic_u8string(), projectId, error))
        return Fail(temporary.path, "saved nonlinear Journey Flow did not reopen");
    StoryFlowAuthoringModel reopenedAuthoring;
    StoryFlowJourneyModel reopenedJourney;
    if (!BuildJourney(reopened, reopenedAuthoring, reopenedJourney, error) ||
        !HasSingleCardPerNode(reopened.Document(), reopenedJourney) ||
        !JourneyAndAuthoringShareRoutes(reopenedAuthoring, reopenedJourney) ||
        reopenedJourney.Exits().size() != journey.Exits().size())
    {
        return Fail(temporary.path,
            "saved nonlinear topology diverged between Journey and Graph authority");
    }
    const FlowRoute* reopenedLoop =
        FindRoute(reopened.Document(), finishRouteId);
    if (!reopenedLoop || reopenedLoop->destinationNodeId != hubId ||
        FindRoute(reopened.Document(), branchRouteId) == nullptr ||
        FindRoute(reopened.Document(), returnRouteId) == nullptr)
    {
        return Fail(temporary.path,
            "saved branch/merge/loop stable identities did not round-trip");
    }

    std::cout << "PASS: Story Flow Gate 9E nonlinear Journey authoring\n";
    return 0;
}
