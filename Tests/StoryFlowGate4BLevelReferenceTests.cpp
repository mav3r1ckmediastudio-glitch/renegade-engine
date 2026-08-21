#include "renegade/bridge/StoryFlowLevelReferenceService.h"
#include "renegade/bridge/IdentityService.h"

#include <chrono>
#include <cstdlib>
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

    void DumpFailure(const StoryFlowExistingLevelResult& result)
    {
        std::cerr
            << "GATE4B_ADOPT_RESULT: succeeded="
            << (result.succeeded ? "true" : "false")
            << " code=" << StoryFlowLevelReferenceCodeName(result.code)
            << " message=\"" << result.message << "\""
            << " level_node_id=\"" << result.levelNodeId << "\""
            << " scene_document_id=\"" << result.sceneDocumentId << "\""
            << " created_metadata="
            << (result.createdMetadata ? "true" : "false")
            << " transaction_success="
            << (result.transaction.success ? "true" : "false")
            << " transaction_committed="
            << (result.transaction.committed ? "true" : "false")
            << " transaction_stage="
            << static_cast<int>(result.transaction.stage)
            << " transaction_document_index="
            << result.transaction.documentIndex
            << " transaction_code=\"" << result.transaction.code << "\""
            << " transaction_message=\"" << result.transaction.message << "\""
            << " rolled_back="
            << (result.transaction.rolledBack ? "true" : "false")
            << " recovery_required="
            << (result.transaction.recoveryRequired ? "true" : "false")
            << " event_count=" << result.transaction.events.size()
            << '\n' << std::flush;
    }

    bool WriteScene(const fs::path& path)
    {
        fs::create_directories(path.parent_path());
        wi::scene::Scene scene;
        scene.Entity_CreateTransform("Existing Level Proof");
        wi::Archive archive(path.generic_u8string(), false, false);
        if (!archive.IsOpen()) return false;
        scene.Serialize(archive);
        const bool written = archive.SaveFile(path.generic_u8string());
        archive = wi::Archive();
        return written && fs::is_regular_file(path);
    }

    FlowDocument MakeFlow(
        const StableId& projectId,
        const fs::path& flowPath,
        const fs::path& root)
    {
        FlowDocument flow;
        flow.envelope = CreateDocumentEnvelope(
            projectId,
            StoryFlowDocumentType,
            flowPath.lexically_relative(root).generic_u8string(),
            "Gate 4B test");
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
        fs::u8path("renegade-story-flow-gate4b-" + std::to_string(unique));
    const fs::path flowPath = root / "Content/Flow/Main.renegade-flow";
    const fs::path scenePath = root / "Content/Scenes/Existing.wiscene";
    const StableId projectId = GenerateStableId();
    std::string error;

    FlowDocument flow = MakeFlow(projectId, flowPath, root);
    Check(WriteScene(scenePath), "could not create existing WISCENE fixture");
    Check(WriteFlowDocument(flowPath.generic_u8string(), flow, error),
        "could not create Flow fixture");

    StoryFlowExistingLevelRequest request;
    request.projectRoot = root.generic_u8string();
    request.projectId = projectId;
    request.flowPath = flowPath.generic_u8string();
    request.flow = flow;
    request.levelName = "Existing Level";
    request.scenePath = scenePath.generic_u8string();

    StoryFlowLevelReferenceService service;
    auto adopted = service.AddExistingLevel(request);
    if (!adopted.succeeded)
    {
        DumpFailure(adopted);
        std::error_code cleanupError;
        fs::remove_all(root, cleanupError);
        std::cerr << "RenegadeStoryFlowGate4BLevelReferenceTests: adoption failed\n"
                  << std::flush;
        // Wicked owns process-lifetime callbacks that can strand a completed
        // test process. The functional failure has already been surfaced and
        // the fixture tree is gone, so do not let unrelated teardown hide it
        // behind the CTest timeout.
        std::_Exit(1);
    }

    Check(adopted.createdMetadata,
        "ungoverned existing WISCENE did not receive Scene metadata");
    Check(IsValidStableId(adopted.sceneDocumentId),
        "adopted Scene did not receive stable identity");
    Check(fs::is_regular_file(fs::path(scenePath.generic_u8string() + ".rmeta")),
        "adopted Scene metadata sidecar was not committed");

    auto resolved = service.ResolveLevel(root.generic_u8string(), projectId,
        adopted.committedFlow, adopted.levelNodeId);
    Check(resolved.succeeded && !resolved.pathHintMoved,
        "freshly adopted Level did not resolve by stable identity");

    const fs::path movedScene = root / "Content/Scenes/Renamed/Existing.wiscene";
    const fs::path oldMeta = fs::path(scenePath.generic_u8string() + ".rmeta");
    const fs::path movedMeta = fs::path(movedScene.generic_u8string() + ".rmeta");
    fs::create_directories(movedScene.parent_path());
    fs::rename(scenePath, movedScene);
    fs::rename(oldMeta, movedMeta);

    DocumentEnvelope movedEnvelope;
    Check(ReadDocumentEnvelope(movedMeta.generic_u8string(), movedEnvelope, error),
        "moved Scene metadata could not be read");
    Check(RetargetDocumentEnvelope(
            movedEnvelope,
            movedScene.lexically_relative(root).generic_u8string(),
            error),
        "moved Scene metadata path hint could not be retargeted");
    Check(WriteDocumentEnvelope(movedMeta.generic_u8string(), movedEnvelope, error),
        "moved Scene metadata could not be persisted");

    resolved = service.ResolveLevel(root.generic_u8string(), projectId,
        adopted.committedFlow, adopted.levelNodeId);
    Check(resolved.succeeded && resolved.pathHintMoved,
        "moved Level was not recovered by stable Scene identity");
    Check(fs::weakly_canonical(fs::u8path(resolved.resolvedPath)) ==
            fs::weakly_canonical(movedScene),
        "moved Level resolved to the wrong WISCENE");

    StoryFlowExistingLevelRequest duplicate = request;
    duplicate.flow = adopted.committedFlow;
    duplicate.scenePath = movedScene.generic_u8string();
    auto duplicateResult = service.AddExistingLevel(duplicate);
    Check(!duplicateResult.succeeded &&
            duplicateResult.code == StoryFlowLevelReferenceCode::Conflict,
        "duplicate stable Scene identity was accepted as a second Level node");

    std::error_code ignored;
    fs::remove(movedScene, ignored);
    auto missing = service.ResolveLevel(root.generic_u8string(), projectId,
        adopted.committedFlow, adopted.levelNodeId);
    Check(!missing.succeeded &&
            missing.code == StoryFlowLevelReferenceCode::Missing,
        "missing governed Level did not fail closed");

    fs::remove_all(root, ignored);
    if (failures != 0)
    {
        std::cerr << "RenegadeStoryFlowGate4BLevelReferenceTests: "
                  << failures << " failure(s)\n" << std::flush;
        std::_Exit(1);
    }
    std::cout << "PASS: Story Flow Gate 4B existing Level adoption and identity resolution\n"
              << std::flush;
    // Match the known-good Gate4SceneFixture lifetime policy: all owned files
    // and assertions are complete, so bypass unrelated Wicked static teardown.
    std::_Exit(0);
}
