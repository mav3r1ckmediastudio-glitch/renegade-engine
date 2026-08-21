#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SelectionService.h"
#include "renegade/bridge/StoryFlowAuthoringSession.h"
#include "renegade/bridge/StoryFlowLevelLifecycleService.h"
#include "renegade/bridge/StoryFlowLevelReferenceService.h"

#include <chrono>
#include <filesystem>
#include <iostream>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    int failures = 0;
    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    FlowDocument MakeFlow(const StableId& projectId)
    {
        FlowDocument flow;
        flow.envelope = CreateDocumentEnvelope(
            projectId,
            StoryFlowDocumentType,
            "Content/Flow/Main.renegade-flow",
            "Gate 4D acceptance");
        const StableId start = GenerateStableId();
        const StableId complete = GenerateStableId();
        flow.startNodeId = start;
        flow.nodes = {
            {start, FlowNodeKind::GameStart, "Game Start", {}, {}, {}, {}},
            {complete, FlowNodeKind::CompleteGame, "Complete Game", {}, {}, {}, {}},
        };
        flow.routes = {
            {GenerateStableId(), start, GameStartOutcome, complete, {}, 0, {}},
        };
        return flow;
    }
}

int main()
{
    using namespace renegade::bridge;

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        fs::u8path("renegade-story-flow-gate4d-" + std::to_string(unique));
    const fs::path flowPath = root / "Content/Flow/Main.renegade-flow";
    const StableId projectId = GenerateStableId();
    std::string error;

    FlowDocument initial = MakeFlow(projectId);
    Check(WriteFlowDocument(flowPath.generic_u8string(), initial, error),
        "could not create initial Story Flow");

    StoryFlowNewLevelRequest create;
    create.projectRoot = root.generic_u8string();
    create.projectId = projectId;
    create.flowPath = flowPath.generic_u8string();
    create.flow = initial;
    create.levelName = "Owner Test Level";
    create.scenePathHint = "Content/Scenes/Owner-Test-Level.wiscene";

    StoryFlowLevelLifecycleService lifecycle;
    const auto created = lifecycle.CreateNewLevel(create);
    Check(created.succeeded, "Story Flow could not create governed Level");
    Check(fs::is_regular_file(fs::u8path(created.scenePath)),
        "created Level WISCENE is missing");
    Check(fs::is_regular_file(fs::u8path(created.scenePath + ".rmeta")),
        "created Level stable identity sidecar is missing");

    StoryFlowAuthoringSession authoring;
    Check(authoring.Open(flowPath.generic_u8string(), projectId, error),
        "could not reopen Story Flow after Level creation");

    StoryFlowLevelReferenceService references;
    auto resolved = references.ResolveLevel(root.generic_u8string(), projectId,
        authoring.Document(), created.levelNodeId);
    Check(resolved.succeeded &&
            resolved.sceneDocumentId == created.sceneDocumentId,
        "created Level did not resolve to its original stable Scene identity");

    SceneService scenes;
    SelectionService selection;
    CommandService commands;
    ProjectService projects;
    SceneDocumentService documents(scenes, selection, commands, projects);
    Check(documents.Open(resolved.resolvedPath),
        "created Level did not open in governed Scene document service");

    scenes.GetScene().Entity_CreateTransform("Gate 4 Owner Edit");
    Check(documents.Save(resolved.resolvedPath),
        "edited Level did not save through existing Level document service");
    const std::size_t editedEntityCount = scenes.EntityCount();
    Check(editedEntityCount > 0,
        "owner edit was not present before reopen");

    documents.NewScene();
    Check(documents.Open(resolved.resolvedPath),
        "saved Level did not reopen");
    Check(scenes.EntityCount() == editedEntityCount,
        "saved Level edit did not survive reopen");

    FlowDocument reopenedFlow;
    Check(ReadFlowDocument(flowPath.generic_u8string(), projectId,
            reopenedFlow, error),
        "Story Flow did not reopen after Level edit/save");
    resolved = references.ResolveLevel(root.generic_u8string(), projectId,
        reopenedFlow, created.levelNodeId);
    Check(resolved.succeeded,
        "reopened Story Flow no longer resolves the Level");
    Check(resolved.sceneDocumentId == created.sceneDocumentId,
        "Level stable Scene identity changed across edit/save/reopen");

    std::error_code ignored;
    fs::remove_all(root, ignored);
    if (failures != 0)
    {
        std::cerr << "RenegadeStoryFlowGate4DAcceptanceTests: "
                  << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "PASS: Story Flow Gate 4 Add Level -> edit/save -> reopen stable identity acceptance\n";
    return 0;
}
