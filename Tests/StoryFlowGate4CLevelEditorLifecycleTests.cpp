#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SelectionService.h"
#include "renegade/bridge/StoryFlowAuthoringSession.h"
#include "renegade/bridge/StoryFlowLevelReferenceService.h"

#include <chrono>
#include <filesystem>
#include <iostream>

#include <wiArchive.h>
#include <wiScene.h>

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

    bool WriteScene(const fs::path& path)
    {
        fs::create_directories(path.parent_path());
        wi::scene::Scene scene;
        scene.Entity_CreateTransform("Gate 4C Level");
        wi::Archive archive(path.generic_u8string(), false, false);
        if (!archive.IsOpen()) return false;
        scene.Serialize(archive);
        const bool saved = archive.SaveFile(path.generic_u8string());
        archive = wi::Archive();
        return saved;
    }
}

int main()
{
    using namespace renegade::bridge;

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        fs::u8path("renegade-story-flow-gate4c-" + std::to_string(unique));
    const fs::path scenePath = root / "Content/Scenes/Playable.wiscene";
    const fs::path flowPath = root / "Content/Flow/Main.renegade-flow";
    const StableId projectId = GenerateStableId();
    const StableId sceneId = GenerateStableId();
    const StableId startId = GenerateStableId();
    const StableId levelId = GenerateStableId();
    const StableId completeId = GenerateStableId();
    std::string error;

    Check(WriteScene(scenePath), "could not create playable WISCENE");

    DocumentEnvelope sceneEnvelope = CreateDocumentEnvelope(
        projectId,
        SceneDocumentType,
        "Content/Scenes/Playable.wiscene",
        "Gate 4C test");
    sceneEnvelope.documentId = sceneId;
    Check(WriteDocumentEnvelope(
            (scenePath.generic_u8string() + ".rmeta"),
            sceneEnvelope,
            error),
        "could not create governed Scene identity");

    FlowDocument flow;
    flow.envelope = CreateDocumentEnvelope(
        projectId,
        StoryFlowDocumentType,
        "Content/Flow/Main.renegade-flow",
        "Gate 4C test");
    flow.startNodeId = startId;
    flow.nodes = {
        {startId, FlowNodeKind::GameStart, "Game Start", {}, {}, {}, {}},
        {levelId, FlowNodeKind::Level, "Playable", sceneId,
            "Content/Scenes/Playable.wiscene", {}, {}},
        {completeId, FlowNodeKind::CompleteGame, "Complete Game", {}, {}, {}, {}},
    };
    flow.routes = {
        {GenerateStableId(), startId, GameStartOutcome, levelId, "default", 0, {}},
        {GenerateStableId(), levelId, "level.complete", completeId, {}, 0, {}},
    };
    Check(WriteFlowDocument(flowPath.generic_u8string(), flow, error),
        "could not create Story Flow fixture");

    StoryFlowAuthoringSession authoring;
    Check(authoring.Open(flowPath.generic_u8string(), projectId, error),
        "could not open Flow authoring session");

    StoryFlowLevelReferenceService references;
    const auto resolved = references.ResolveLevel(
        root.generic_u8string(), projectId, authoring.Document(), levelId);
    if (!resolved.succeeded)
    {
        std::cerr
            << "GATE4C_RESOLVE_RESULT: code="
            << StoryFlowLevelReferenceCodeName(resolved.code)
            << " message=\"" << resolved.message << "\""
            << " scene_document_id=\"" << resolved.sceneDocumentId << "\""
            << " requested_path_hint=\"" << resolved.requestedPathHint << "\""
            << " resolved_path=\"" << resolved.resolvedPath << "\""
            << " resolved_path_hint=\"" << resolved.resolvedPathHint << "\""
            << '\n';
    }
    Check(resolved.succeeded, "selected Level did not resolve");

    SceneService scenes;
    SelectionService selection;
    CommandService commands;
    ProjectService projects;
    SceneDocumentService documents(scenes, selection, commands, projects);
    const bool opened = documents.Open(resolved.resolvedPath);
    if (!opened)
    {
        std::cerr
            << "GATE4C_OPEN_RESULT: resolved_path=\""
            << resolved.resolvedPath << "\" warning=\""
            << documents.LastWarning() << "\"\n";
    }
    Check(opened,
        "resolved Level did not open through governed Scene document service");
    Check(scenes.CurrentPath() == resolved.resolvedPath,
        "Level Editor document path did not adopt resolved WISCENE");
    Check(authoring.IsLoaded() && authoring.Document().envelope.documentId ==
            flow.envelope.documentId,
        "opening Level destroyed or replaced the Story Flow authoring session");
    Check(authoring.Document().nodes.size() == flow.nodes.size(),
        "Story Flow semantic state changed while Level Editor document opened");

    std::error_code ignored;
    fs::remove_all(root, ignored);
    if (failures != 0)
    {
        std::cerr << "RenegadeStoryFlowGate4CLevelEditorLifecycleTests: "
                  << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "PASS: Story Flow Gate 4C Level open preserves Flow authoring session\n";
    return 0;
}
