#include "renegade/bridge/StoryFlowScreenLifecycleService.h"

#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScreenService.h"

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
            "Renegade Story Flow Gate 5A tests");
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
        return WriteFlowDocument(flowPath.generic_u8string(), flow, error);
    }

    const FlowNode* FindNode(const FlowDocument& document, const StableId& nodeId)
    {
        for (const auto& node : document.nodes)
        {
            if (node.id == nodeId) return &node;
        }
        return nullptr;
    }

    std::vector<std::string> ActionIds(const ScreenDocument& document)
    {
        std::vector<std::string> result;
        for (const auto& action : document.actions) result.push_back(action.id);
        return result;
    }

    void TestAtomicTitleCreate(const fs::path& root)
    {
        const StableId projectId = GenerateStableId();
        FlowDocument flow;
        fs::path flowPath;
        Check(WriteBaseFlow(root, projectId, flow, flowPath),
            "could not write Gate 5A base Flow");
        if (!fs::is_regular_file(flowPath)) return;

        const std::vector<std::uint8_t> originalFlow = ReadBytes(flowPath);

        StoryFlowNewScreenRequest request;
        request.projectRoot = root.generic_u8string();
        request.projectId = projectId;
        request.flowPath = flowPath.generic_u8string();
        request.flow = flow;
        request.screenName = "Title Screen";
        request.screenPathHint = "Content/Screens/Title.renegade-screen";
        request.screenTemplate = StoryFlowScreenTemplate::Title;
        request.transactionId = "gate5a-title-success";

        StoryFlowScreenLifecycleService service;
        const StoryFlowNewScreenResult result = service.CreateNewScreen(request);

        Check(result.succeeded && result.code == StoryFlowScreenCreateCode::Success,
            "Add Screen did not report success");
        Check(result.transaction.success && result.transaction.committed,
            "Add Screen did not commit through the project transaction");
        Check(IsValidStableId(result.screenNodeId) &&
                IsValidStableId(result.screenDocumentId),
            "Add Screen did not create stable node and Screen IDs");
        Check(result.actionIds == std::vector<std::string>({
                "new_game", "load_game", "options", "credits", "quit"}),
            "Title template did not expose the expected named outcomes");

        const fs::path screenPath = root / "Content/Screens/Title.renegade-screen";
        const fs::path backupPath = fs::u8path(flowPath.generic_u8string() + ".bak");
        Check(fs::is_regular_file(screenPath),
            "Add Screen did not create the Runtime Screen document");
        Check(fs::is_regular_file(backupPath) && ReadBytes(backupPath) == originalFlow,
            "Add Screen did not preserve the previous Flow backup");

        std::string error;
        ScreenDocument screen;
        Check(ReadScreenDocument(screenPath.generic_u8string(), projectId, screen, error),
            "created Runtime Screen could not be read");
        Check(screen.envelope.documentId == result.screenDocumentId &&
                screen.envelope.projectId == projectId &&
                screen.envelope.documentType == RuntimeScreenDocumentType &&
                screen.envelope.pathHint == "Content/Screens/Title.renegade-screen",
            "created Runtime Screen has incorrect authority fields");
        Check(ActionIds(screen) == result.actionIds,
            "created Runtime Screen did not persist template outcomes");

        FlowDocument committed;
        Check(ReadFlowDocument(flowPath.generic_u8string(), projectId, committed, error),
            "committed Story Flow could not be read");
        const FlowNode* node = FindNode(committed, result.screenNodeId);
        Check(node && node->kind == FlowNodeKind::Screen &&
                node->name == "Title Screen" &&
                node->screenDocumentId == result.screenDocumentId &&
                node->screenPathHint == "Content/Screens/Title.renegade-screen",
            "committed Story Flow did not retain the stable Screen reference");

        std::string resolved;
        Check(ResolveRuntimeScreenDocumentPath(
                root.generic_u8string(), projectId, result.screenDocumentId,
                "Content/Screens/Title.renegade-screen", resolved, error),
            "created Screen did not resolve through stable Screen identity");
    }

    void TestRollback(const fs::path& root)
    {
        const StableId projectId = GenerateStableId();
        FlowDocument flow;
        fs::path flowPath;
        Check(WriteBaseFlow(root, projectId, flow, flowPath),
            "could not write Gate 5A rollback Flow");
        if (!fs::is_regular_file(flowPath)) return;
        const std::vector<std::uint8_t> originalFlow = ReadBytes(flowPath);

        StoryFlowNewScreenRequest request;
        request.projectRoot = root.generic_u8string();
        request.projectId = projectId;
        request.flowPath = flowPath.generic_u8string();
        request.flow = flow;
        request.screenName = "Rollback Screen";
        request.screenPathHint = "Content/Screens/RollbackScreen.renegade-screen";
        request.screenTemplate = StoryFlowScreenTemplate::Victory;
        request.transactionId = "gate5a-rollback";
        request.transactionHook = [](
            const ProjectDocumentTransactionStage stage,
            const std::size_t,
            const std::string& path,
            std::string& error)
        {
            if (stage == ProjectDocumentTransactionStage::Replace &&
                path.find("RollbackScreen.renegade-screen") != std::string::npos)
            {
                error = "forced Gate 5A Screen replacement failure";
                return ProjectDocumentTransactionHookAction::Fail;
            }
            return ProjectDocumentTransactionHookAction::Continue;
        };

        StoryFlowScreenLifecycleService service;
        const StoryFlowNewScreenResult result = service.CreateNewScreen(request);
        Check(!result.succeeded &&
                result.code == StoryFlowScreenCreateCode::TransactionFailed,
            "forced Gate 5A transaction failure was not surfaced");
        Check(result.transaction.rolledBack && !result.transaction.recoveryRequired,
            "forced Gate 5A failure did not complete rollback");
        Check(ReadBytes(flowPath) == originalFlow,
            "Gate 5A rollback did not restore original Story Flow bytes");
        Check(!fs::exists(root / "Content/Screens/RollbackScreen.renegade-screen"),
            "Gate 5A rollback left a half-created Runtime Screen");
    }

    void TestUnsafeAndConflictingDestinations(const fs::path& root)
    {
        const StableId projectId = GenerateStableId();
        FlowDocument flow;
        fs::path flowPath;
        Check(WriteBaseFlow(root, projectId, flow, flowPath),
            "could not write Gate 5A conflict Flow");
        if (!fs::is_regular_file(flowPath)) return;

        StoryFlowNewScreenRequest request;
        request.projectRoot = root.generic_u8string();
        request.projectId = projectId;
        request.flowPath = flowPath.generic_u8string();
        request.flow = flow;
        request.screenName = "Unsafe";
        request.screenPathHint = "../Unsafe.renegade-screen";
        request.screenTemplate = StoryFlowScreenTemplate::Custom;

        StoryFlowScreenLifecycleService service;
        auto result = service.CreateNewScreen(request);
        Check(!result.succeeded && result.code == StoryFlowScreenCreateCode::InvalidRequest,
            "Add Screen accepted an escaping Screen path");

        request.screenPathHint = "Content/UI/WrongFolder.renegade-screen";
        result = service.CreateNewScreen(request);
        Check(!result.succeeded && result.code == StoryFlowScreenCreateCode::InvalidRequest,
            "Add Screen accepted a destination outside Content/Screens");

        const fs::path existing = root / "Content/Screens/Existing.renegade-screen";
        fs::create_directories(existing.parent_path());
        {
            std::ofstream stream(existing, std::ios::binary);
            stream << "existing fixture";
        }
        request.screenPathHint = "Content/Screens/Existing.renegade-screen";
        result = service.CreateNewScreen(request);
        Check(!result.succeeded && result.code == StoryFlowScreenCreateCode::Conflict,
            "Add Screen attempted to overwrite existing content");
    }

    void TestTemplateNames()
    {
        Check(std::string(StoryFlowScreenTemplateName(StoryFlowScreenTemplate::Title)) == "title",
            "Title template name is unstable");
        Check(std::string(StoryFlowScreenTemplateName(StoryFlowScreenTemplate::Loading)) == "loading",
            "Loading template name is unstable");
        Check(std::string(StoryFlowScreenTemplateName(StoryFlowScreenTemplate::SaveLoad)) == "save-load",
            "Save/Load template name is unstable");
    }
}

int main()
{
    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade story flow gate5a " + std::to_string(unique))
    };

    TestAtomicTitleCreate(temporary.path / "success project");
    TestRollback(temporary.path / "rollback project");
    TestUnsafeAndConflictingDestinations(temporary.path / "conflict project");
    TestTemplateNames();

    if (failures != 0)
    {
        std::cerr << "RenegadeStoryFlowGate5ScreenLifecycleTests: "
                  << failures << " failure(s)\n";
        return 1;
    }

    std::cout << "PASS: Story Flow Gate 5A governed Screen lifecycle\n";
    return 0;
}
