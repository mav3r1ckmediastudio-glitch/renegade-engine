#include "renegade/bridge/StoryFlowScreenLifecycleService.h"
#include "renegade/bridge/StoryFlowScreenReferenceService.h"

#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/IdentityService.h"

#include <chrono>
#include <filesystem>
#include <iostream>
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

    FlowDocument MakeBaseFlow(
        const StableId& projectId,
        const std::string& pathHint)
    {
        FlowDocument document;
        document.envelope = CreateDocumentEnvelope(
            projectId,
            StoryFlowDocumentType,
            pathHint,
            "Renegade Story Flow Gate 5B tests");
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

    StoryFlowNewScreenResult CreateTitleScreen(
        const fs::path& root,
        const StableId& projectId,
        const fs::path& flowPath,
        const FlowDocument& flow)
    {
        StoryFlowNewScreenRequest request;
        request.projectRoot = root.generic_u8string();
        request.projectId = projectId;
        request.flowPath = flowPath.generic_u8string();
        request.flow = flow;
        request.screenName = "Title Screen";
        request.screenPathHint = "Content/Screens/Title.renegade-screen";
        request.screenTemplate = StoryFlowScreenTemplate::Title;
        request.transactionId = "gate5b-create";
        StoryFlowScreenLifecycleService service;
        return service.CreateNewScreen(request);
    }

    void TestMoveAndOutcomeExposure(const fs::path& root)
    {
        const StableId projectId = GenerateStableId();
        FlowDocument flow;
        fs::path flowPath;
        Check(WriteBaseFlow(root, projectId, flow, flowPath),
            "could not write Gate 5B base Flow");
        if (!fs::is_regular_file(flowPath)) return;

        const StoryFlowNewScreenResult created =
            CreateTitleScreen(root, projectId, flowPath, flow);
        Check(created.succeeded, "Gate 5B fixture Screen creation failed");
        if (!created.succeeded) return;

        const fs::path original = root / "Content/Screens/Title.renegade-screen";
        const fs::path moved = root / "Content/Screens/Renamed/TitleMoved.renegade-screen";
        fs::create_directories(moved.parent_path());
        std::error_code moveError;
        fs::rename(original, moved, moveError);
        Check(!moveError, "could not move Gate 5B Screen fixture");
        if (moveError) return;

        StoryFlowScreenReferenceService service;
        const StoryFlowScreenResolution resolved = service.ResolveScreen(
            root.generic_u8string(),
            projectId,
            created.committedFlow,
            created.screenNodeId);

        Check(resolved.succeeded &&
                resolved.code == StoryFlowScreenReferenceCode::Success,
            "moved Screen did not resolve by stable identity");
        Check(resolved.pathHintMoved &&
                resolved.requestedPathHint == "Content/Screens/Title.renegade-screen" &&
                resolved.resolvedPathHint ==
                    "Content/Screens/Renamed/TitleMoved.renegade-screen",
            "moved Screen did not report a stale path hint");
        Check(resolved.screenDocumentId == created.screenDocumentId,
            "moved Screen changed stable document identity");
        Check(resolved.actionIds == std::vector<std::string>({
                "new_game", "load_game", "options", "credits", "quit"}),
            "Screen resolution did not expose authored named outcomes");

        const StableId completeNodeId = created.committedFlow.nodes[1].id;
        const StoryFlowScreenResolution wrongKind = service.ResolveScreen(
            root.generic_u8string(), projectId,
            created.committedFlow, completeNodeId);
        Check(!wrongKind.succeeded &&
                wrongKind.code == StoryFlowScreenReferenceCode::InvalidRequest,
            "Screen resolution accepted a non-Screen Flow node");
    }

    void TestDuplicateAndMissingDiagnostics(const fs::path& root)
    {
        const StableId projectId = GenerateStableId();
        FlowDocument flow;
        fs::path flowPath;
        Check(WriteBaseFlow(root, projectId, flow, flowPath),
            "could not write Gate 5B diagnostics Flow");
        if (!fs::is_regular_file(flowPath)) return;

        const StoryFlowNewScreenResult created =
            CreateTitleScreen(root, projectId, flowPath, flow);
        Check(created.succeeded, "Gate 5B diagnostics Screen creation failed");
        if (!created.succeeded) return;

        const fs::path original = root / "Content/Screens/Title.renegade-screen";
        const fs::path duplicate = root / "Content/Screens/Duplicate.renegade-screen";
        std::error_code copyError;
        fs::copy_file(original, duplicate, fs::copy_options::overwrite_existing, copyError);
        Check(!copyError, "could not create duplicate Screen identity fixture");

        StoryFlowScreenReferenceService service;
        StoryFlowScreenResolution resolved = service.ResolveScreen(
            root.generic_u8string(), projectId,
            created.committedFlow, created.screenNodeId);
        Check(!resolved.succeeded &&
                resolved.code == StoryFlowScreenReferenceCode::Conflict &&
                resolved.message.find("ambiguous") != std::string::npos,
            "duplicate Screen identity was not diagnosed as a conflict");

        std::error_code removeError;
        fs::remove(original, removeError);
        removeError.clear();
        fs::remove(duplicate, removeError);
        resolved = service.ResolveScreen(
            root.generic_u8string(), projectId,
            created.committedFlow, created.screenNodeId);
        Check(!resolved.succeeded &&
                resolved.code == StoryFlowScreenReferenceCode::Missing,
            "missing Screen identity was not diagnosed as missing");
    }
}

int main()
{
    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade story flow gate5b " + std::to_string(unique))
    };

    TestMoveAndOutcomeExposure(temporary.path / "move project");
    TestDuplicateAndMissingDiagnostics(temporary.path / "diagnostics project");

    if (failures != 0)
    {
        std::cerr << "RenegadeStoryFlowGate5ScreenReferenceTests: "
                  << failures << " failure(s)\n";
        return 1;
    }

    std::cout << "PASS: Story Flow Gate 5B Screen resolution and outcomes\n";
    return 0;
}
