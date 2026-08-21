#include "renegade/bridge/StoryFlowLevelLifecycleService.h"

#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/SceneDocumentService.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    int failures = 0;

    void Check(const bool condition, const char* message)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    void DumpFailure(
        const char* label,
        const StoryFlowNewLevelResult& result)
    {
        std::cerr
            << label
            << ": succeeded=" << (result.succeeded ? "true" : "false")
            << " code=" << StoryFlowLevelCreateCodeName(result.code)
            << " message=\"" << result.message << "\""
            << " level_node_id=\"" << result.levelNodeId << "\""
            << " scene_document_id=\"" << result.sceneDocumentId << "\""
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
            << '\n';
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

    std::vector<std::uint8_t> ReadBytes(const fs::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return std::vector<std::uint8_t>(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
    }

    FlowDocument MakeBaseFlow(
        const StableId& projectId,
        const std::string& pathHint)
    {
        FlowDocument document;
        document.envelope = CreateDocumentEnvelope(
            projectId,
            StoryFlowDocumentType,
            pathHint,
            "Renegade Story Flow Gate 4A tests");

        const StableId start = GenerateStableId();
        const StableId complete = GenerateStableId();
        document.startNodeId = start;
        document.nodes = {
            {start, FlowNodeKind::GameStart, "Game Start", {}, {}, {}, {}},
            {complete, FlowNodeKind::CompleteGame, "Complete Game", {}, {}, {}, {}},
        };
        document.routes = {
            {GenerateStableId(), start, GameStartOutcome, complete, {}, 0, {}},
        };
        return document;
    }

    bool WriteBaseFlow(
        const fs::path& root,
        const StableId& projectId,
        FlowDocument& flow,
        fs::path& flowPath)
    {
        fs::create_directories(root);
        flowPath = root / "Content/Flow/Main.renegade-flow";
        flow = MakeBaseFlow(projectId, "Content/Flow/Main.renegade-flow");
        std::string error;
        return WriteFlowDocument(
            flowPath.generic_u8string(),
            flow,
            error);
    }

    const FlowNode* FindNode(
        const FlowDocument& document,
        const StableId& nodeId)
    {
        for (const auto& node : document.nodes)
        {
            if (node.id == nodeId)
                return &node;
        }
        return nullptr;
    }

    void TestAtomicCreate(const fs::path& root)
    {
        const StableId projectId = GenerateStableId();
        FlowDocument flow;
        fs::path flowPath;
        Check(WriteBaseFlow(root, projectId, flow, flowPath),
            "could not write Gate 4A base Flow");
        if (!fs::is_regular_file(flowPath))
            return;

        const std::vector<std::uint8_t> originalFlow = ReadBytes(flowPath);

        StoryFlowNewLevelRequest request;
        request.projectRoot = root.generic_u8string();
        request.projectId = projectId;
        request.flowPath = flowPath.generic_u8string();
        request.flow = flow;
        request.levelName = "Level One";
        request.scenePathHint = "Content/Scenes/LevelOne.wiscene";
        request.transactionId = "gate4a-success";

        StoryFlowLevelLifecycleService service;
        const StoryFlowNewLevelResult result = service.CreateNewLevel(request);
        if (!result.succeeded ||
            result.code != StoryFlowLevelCreateCode::Success)
        {
            DumpFailure("GATE4A_CREATE_RESULT", result);
        }

        Check(result.succeeded &&
                result.code == StoryFlowLevelCreateCode::Success,
            "Add New Level did not report success");
        Check(result.transaction.success && result.transaction.committed,
            "Add New Level did not commit through LF02 transaction");
        Check(IsValidStableId(result.levelNodeId) &&
                IsValidStableId(result.sceneDocumentId),
            "Add New Level did not create stable node and Scene IDs");

        const fs::path scenePath =
            root / "Content/Scenes/LevelOne.wiscene";
        fs::path sidecarPath = scenePath;
        sidecarPath += ".rmeta";
        const fs::path backupPath =
            fs::u8path(flowPath.generic_u8string() + ".bak");

        Check(fs::is_regular_file(scenePath),
            "Add New Level did not create the WISCENE");
        Check(fs::is_regular_file(sidecarPath),
            "Add New Level did not create the WISCENE identity sidecar");
        Check(fs::is_regular_file(backupPath),
            "Add New Level did not preserve the previous Flow backup");
        Check(ReadBytes(backupPath) == originalFlow,
            "Add New Level Flow backup does not match the pre-transaction Flow");

        const PreparedSceneOpen prepared =
            PrepareWickedSceneOpen(scenePath.generic_u8string());
        Check(prepared.IsReady(),
            "created Level is not a valid WISCENE archive");

        DocumentEnvelope sceneEnvelope;
        std::string error;
        Check(ReadDocumentEnvelope(
                sidecarPath.generic_u8string(),
                sceneEnvelope,
                error),
            "created Level identity sidecar could not be read");
        Check(sceneEnvelope.documentId == result.sceneDocumentId &&
                sceneEnvelope.projectId == projectId &&
                sceneEnvelope.documentType == SceneDocumentType &&
                sceneEnvelope.pathHint == "Content/Scenes/LevelOne.wiscene",
            "created Level identity sidecar has incorrect authority fields");

        FlowDocument committed;
        Check(ReadFlowDocument(
                flowPath.generic_u8string(),
                projectId,
                committed,
                error),
            "committed Story Flow could not be read");
        const FlowNode* level = FindNode(committed, result.levelNodeId);
        Check(level != nullptr &&
                level->kind == FlowNodeKind::Level &&
                level->name == "Level One" &&
                level->sceneAssetId == result.sceneDocumentId &&
                level->scenePathHint == "Content/Scenes/LevelOne.wiscene",
            "committed Story Flow did not retain the new stable Level reference");

        std::string resolvedScene;
        Check(ResolveSceneDocumentPath(
                root.generic_u8string(),
                projectId,
                result.sceneDocumentId,
                "Content/Scenes/LevelOne.wiscene",
                resolvedScene,
                error),
            "new Level could not be resolved through stable Scene identity");
    }

    void TestRollbackAfterPartialReplacement(const fs::path& root)
    {
        const StableId projectId = GenerateStableId();
        FlowDocument flow;
        fs::path flowPath;
        Check(WriteBaseFlow(root, projectId, flow, flowPath),
            "could not write rollback base Flow");
        if (!fs::is_regular_file(flowPath))
            return;

        const std::vector<std::uint8_t> originalFlow = ReadBytes(flowPath);

        StoryFlowNewLevelRequest request;
        request.projectRoot = root.generic_u8string();
        request.projectId = projectId;
        request.flowPath = flowPath.generic_u8string();
        request.flow = flow;
        request.levelName = "Rollback Level";
        request.scenePathHint = "Content/Scenes/RollbackLevel.wiscene";
        request.transactionId = "gate4a-rollback";
        request.transactionHook = [](
            const ProjectDocumentTransactionStage stage,
            const std::size_t index,
            const std::string&,
            std::string& error)
        {
            if (stage == ProjectDocumentTransactionStage::Replace && index == 2)
            {
                error = "forced Gate 4A later replacement failure";
                return ProjectDocumentTransactionHookAction::Fail;
            }
            return ProjectDocumentTransactionHookAction::Continue;
        };

        StoryFlowLevelLifecycleService service;
        const StoryFlowNewLevelResult result = service.CreateNewLevel(request);
        if (result.succeeded ||
            result.code != StoryFlowLevelCreateCode::TransactionFailed ||
            !result.transaction.rolledBack ||
            result.transaction.recoveryRequired)
        {
            DumpFailure("GATE4A_ROLLBACK_RESULT", result);
        }

        Check(!result.succeeded &&
                result.code == StoryFlowLevelCreateCode::TransactionFailed,
            "forced Gate 4A transaction failure was not surfaced");
        Check(result.transaction.rolledBack &&
                !result.transaction.recoveryRequired,
            "forced Gate 4A failure did not complete rollback");
        Check(ReadBytes(flowPath) == originalFlow,
            "Gate 4A rollback did not restore original Story Flow bytes");

        const fs::path scenePath =
            root / "Content/Scenes/RollbackLevel.wiscene";
        fs::path sidecarPath = scenePath;
        sidecarPath += ".rmeta";
        Check(!fs::exists(scenePath) && !fs::exists(sidecarPath),
            "Gate 4A rollback left half-created Level content");

        FlowDocument restored;
        std::string error;
        Check(ReadFlowDocument(
                flowPath.generic_u8string(),
                projectId,
                restored,
                error) && restored.nodes.size() == flow.nodes.size(),
            "Gate 4A rollback left Story Flow semantically changed");
    }

    void TestUnsafeAndConflictingDestinations(const fs::path& root)
    {
        const StableId projectId = GenerateStableId();
        FlowDocument flow;
        fs::path flowPath;
        Check(WriteBaseFlow(root, projectId, flow, flowPath),
            "could not write path-validation base Flow");
        if (!fs::is_regular_file(flowPath))
            return;

        StoryFlowLevelLifecycleService service;
        StoryFlowNewLevelRequest request;
        request.projectRoot = root.generic_u8string();
        request.projectId = projectId;
        request.flowPath = flowPath.generic_u8string();
        request.flow = flow;
        request.levelName = "Unsafe";
        request.scenePathHint = "../Outside.wiscene";

        auto unsafe = service.CreateNewLevel(request);
        Check(!unsafe.succeeded &&
                unsafe.code == StoryFlowLevelCreateCode::InvalidRequest,
            "Add New Level accepted a project-escaping Scene path");

        const fs::path existing =
            root / "Content/Scenes/Existing.wiscene";
        fs::create_directories(existing.parent_path());
        {
            std::ofstream file(existing, std::ios::binary | std::ios::trunc);
            file << "occupied";
        }
        request.levelName = "Existing";
        request.scenePathHint = "Content/Scenes/Existing.wiscene";
        auto conflict = service.CreateNewLevel(request);
        Check(!conflict.succeeded &&
                conflict.code == StoryFlowLevelCreateCode::Conflict,
            "Add New Level accepted an occupied Scene destination");
    }
}

int main()
{
    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade story flow gate4a " + std::to_string(unique))
    };

    fs::create_directories(temporary.path);
    TestAtomicCreate(temporary.path / "success project");
    TestRollbackAfterPartialReplacement(temporary.path / "rollback project");
    TestUnsafeAndConflictingDestinations(temporary.path / "validation project");

    if (failures != 0)
    {
        std::cerr << "RenegadeStoryFlowGate4LevelLifecycleTests: "
                  << failures << " failure(s)\n";
        return 1;
    }

    std::cout
        << "PASS: Story Flow Gate 4A governed Level creation and rollback\n";
    return 0;
}
