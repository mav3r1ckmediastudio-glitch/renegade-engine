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
        std::cerr << "RenegadeStoryFlowAuthoringSessionTests: "
                  << message << '\n';
        return 1;
    }

    const FlowNode* FindNode(
        const FlowDocument& document,
        const StableId& id)
    {
        const auto found = std::find_if(
            document.nodes.begin(),
            document.nodes.end(),
            [&id](const FlowNode& node)
            {
                return node.id == id;
            });
        return found == document.nodes.end() ? nullptr : &*found;
    }

    const FlowRoute* FindRoute(
        const FlowDocument& document,
        const StableId& id)
    {
        const auto found = std::find_if(
            document.routes.begin(),
            document.routes.end(),
            [&id](const FlowRoute& route)
            {
                return route.id == id;
            });
        return found == document.routes.end() ? nullptr : &*found;
    }
}

int main()
{
    using namespace renegade::bridge;

    const StableId projectId = "11111111-1111-4111-8111-111111111111";
    const StableId flowId = "22222222-2222-4222-8222-222222222222";
    const StableId startId = "33333333-3333-4333-8333-333333333333";
    const StableId levelId = "44444444-4444-4444-8444-444444444444";
    const StableId sceneId = "55555555-5555-4555-8555-555555555555";
    const StableId completeId = "66666666-6666-4666-8666-666666666666";

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade story flow gate3 " + std::to_string(unique))
    };
    const fs::path root = temporary.path / "Project With Spaces";
    const fs::path flowPath = root / "Content/Flow/Main.renegade-flow";
    fs::create_directories(flowPath.parent_path());

    FlowDocument document;
    document.envelope = CreateDocumentEnvelope(
        projectId,
        StoryFlowDocumentType,
        "Content/Flow/Main.renegade-flow",
        "Story Flow Gate 3 authoring session proof");
    document.envelope.documentId = flowId;
    document.startNodeId = startId;

    FlowNode start;
    start.id = startId;
    start.kind = FlowNodeKind::GameStart;
    start.name = "Game Start";

    FlowNode level;
    level.id = levelId;
    level.kind = FlowNodeKind::Level;
    level.name = "Level One";
    level.sceneAssetId = sceneId;
    level.scenePathHint = "Content/Scenes/LevelOne.wiscene";

    FlowNode complete;
    complete.id = completeId;
    complete.kind = FlowNodeKind::CompleteGame;
    complete.name = "Complete Game";

    FlowRoute startRoute;
    startRoute.id = "77777777-7777-4777-8777-777777777771";
    startRoute.sourceNodeId = startId;
    startRoute.outcome = GameStartOutcome;
    startRoute.destinationNodeId = levelId;
    startRoute.destinationEntry = "default";

    FlowRoute completeRoute;
    completeRoute.id = "77777777-7777-4777-8777-777777777772";
    completeRoute.sourceNodeId = levelId;
    completeRoute.outcome = "level.complete";
    completeRoute.destinationNodeId = completeId;

    document.nodes = {start, level, complete};
    document.routes = {startRoute, completeRoute};

    std::string error;
    if (!WriteFlowDocument(flowPath.generic_u8string(), document, error))
    {
        return Fail(temporary.path, "fixture Flow did not serialize");
    }

    StoryFlowAuthoringSession session;
    if (!session.Open(flowPath.generic_u8string(), projectId, error) ||
        !session.IsLoaded() || session.IsDirty() ||
        session.CanUndo() || session.CanRedo())
    {
        return Fail(temporary.path, "authoring session did not open cleanly");
    }

    if (!session.RenameNode(levelId, "Level Prime", error) ||
        !session.IsDirty() || !session.CanUndo() ||
        FindNode(session.Document(), levelId) == nullptr ||
        FindNode(session.Document(), levelId)->name != "Level Prime")
    {
        return Fail(temporary.path, "rename did not create dirty history");
    }

    if (!session.Undo(error) || session.IsDirty() ||
        FindNode(session.Document(), levelId)->name != "Level One")
    {
        return Fail(temporary.path, "undo did not restore the saved Flow state");
    }
    if (!session.Redo(error) || !session.IsDirty() ||
        FindNode(session.Document(), levelId)->name != "Level Prime")
    {
        return Fail(temporary.path, "redo did not restore the Flow edit");
    }
    if (!session.Save(error) || session.IsDirty())
    {
        return Fail(temporary.path, "transactional Flow save did not mark history saved");
    }

    FlowDocument saved;
    if (!ReadFlowDocument(
            flowPath.generic_u8string(),
            projectId,
            saved,
            error) ||
        FindNode(saved, levelId) == nullptr ||
        FindNode(saved, levelId)->name != "Level Prime")
    {
        return Fail(temporary.path, "saved Flow did not reopen with the edit");
    }

    FlowNode returnMenu;
    returnMenu.kind = FlowNodeKind::ReturnToMainMenu;
    returnMenu.name = "Return Main Menu";
    StableId returnMenuId;
    if (!session.AddNode(returnMenu, returnMenuId, error) ||
        !IsValidStableId(returnMenuId))
    {
        return Fail(temporary.path, "terminal node authoring failed");
    }

    FlowRoute menuRoute;
    menuRoute.sourceNodeId = levelId;
    menuRoute.outcome = "return.menu";
    menuRoute.destinationNodeId = returnMenuId;
    StableId menuRouteId;
    if (!session.AddRoute(menuRoute, menuRouteId, error) ||
        FindRoute(session.Document(), menuRouteId) == nullptr)
    {
        return Fail(temporary.path, "route authoring failed");
    }

    FlowRoute editedRoute = *FindRoute(session.Document(), menuRouteId);
    editedRoute.priority = 3;
    editedRoute.conditions.push_back({
        "story.flag",
        FlowConditionOperator::Exists,
        {},
    });
    if (!session.UpdateRoute(menuRouteId, editedRoute, error) ||
        FindRoute(session.Document(), menuRouteId)->priority != 3 ||
        FindRoute(session.Document(), menuRouteId)->conditions.size() != 1)
    {
        return Fail(temporary.path, "route field editing failed");
    }

    if (!session.DeleteNode(returnMenuId, error) ||
        FindNode(session.Document(), returnMenuId) != nullptr ||
        FindRoute(session.Document(), menuRouteId) != nullptr)
    {
        return Fail(temporary.path,
            "node deletion did not remove connected routes atomically");
    }

    const std::size_t beforeProtectedDelete = session.UndoCount();
    if (session.DeleteNode(startId, error) ||
        session.UndoCount() != beforeProtectedDelete ||
        FindNode(session.Document(), startId) == nullptr)
    {
        return Fail(temporary.path, "permanent Game Start was deletable");
    }

    FlowNode invalidScreen;
    invalidScreen.kind = FlowNodeKind::Screen;
    invalidScreen.name = "Invalid Screen";
    invalidScreen.screenPathHint = "Content/UI/Invalid.renegade-screen";
    StableId ignoredId;
    if (session.AddNode(invalidScreen, ignoredId, error) ||
        session.UndoCount() != beforeProtectedDelete)
    {
        return Fail(temporary.path,
            "invalid semantic mutation escaped Flow validation/history isolation");
    }

    FlowNode screen;
    screen.kind = FlowNodeKind::Screen;
    screen.name = "Victory Screen";
    screen.screenDocumentId =
        "88888888-8888-4888-8888-888888888888";
    screen.screenPathHint = "Content/UI/Victory.renegade-screen";
    StableId screenNodeId;
    if (!session.AddNode(screen, screenNodeId, error))
    {
        return Fail(temporary.path, "valid Screen node authoring failed");
    }

    FlowRoute screenRoute;
    screenRoute.sourceNodeId = levelId;
    screenRoute.outcome = "show.victory";
    screenRoute.destinationNodeId = screenNodeId;
    StableId screenRouteId;
    if (!session.AddRoute(screenRoute, screenRouteId, error))
    {
        return Fail(temporary.path, "Screen route authoring failed");
    }

    if (!session.Undo(error) ||
        FindRoute(session.Document(), screenRouteId) != nullptr ||
        !session.Undo(error) ||
        FindNode(session.Document(), screenNodeId) != nullptr ||
        !session.Redo(error) ||
        FindNode(session.Document(), screenNodeId) == nullptr ||
        !session.Redo(error) ||
        FindRoute(session.Document(), screenRouteId) == nullptr)
    {
        return Fail(temporary.path,
            "Flow-specific undo/redo did not preserve semantic history");
    }

    if (!session.Save(error) || !session.Reload(error) || session.IsDirty() ||
        session.CanUndo() || session.CanRedo())
    {
        return Fail(temporary.path,
            "reopen did not establish a new clean authoring history boundary");
    }

    std::cout << "PASS: Story Flow Gate 3 authoring session/history/save\n";
    return 0;
}
