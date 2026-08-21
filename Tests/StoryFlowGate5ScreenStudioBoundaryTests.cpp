#include "StoryFlowScreenEditorHandoff.h"

#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/StoryFlowScreenLifecycleService.h"
#include "renegade/bridge/StoryFlowScreenReferenceService.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;
    using namespace renegade::studio;

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
            "Renegade Story Flow Gate 5C tests");
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

    void TestStableScreenEditorHandoff(const fs::path& root)
    {
        const StableId projectId = GenerateStableId();
        const fs::path flowPath = root / "Content/Flow/Main.renegade-flow";
        FlowDocument flow = MakeBaseFlow(
            projectId,
            "Content/Flow/Main.renegade-flow");
        std::string error;
        Check(WriteFlowDocument(flowPath.generic_u8string(), flow, error),
            "could not write Gate 5C base Flow");
        if (!fs::is_regular_file(flowPath)) return;

        StoryFlowNewScreenRequest request;
        request.projectRoot = root.generic_u8string();
        request.projectId = projectId;
        request.flowPath = flowPath.generic_u8string();
        request.flow = flow;
        request.screenName = "Title Screen";
        request.screenPathHint = "Content/Screens/Title.renegade-screen";
        request.screenTemplate = StoryFlowScreenTemplate::Title;
        request.transactionId = "gate5c-create";

        StoryFlowScreenLifecycleService lifecycle;
        const StoryFlowNewScreenResult created = lifecycle.CreateNewScreen(request);
        Check(created.succeeded, "Gate 5C fixture Screen creation failed");
        if (!created.succeeded) return;

        const fs::path original = root / "Content/Screens/Title.renegade-screen";
        const fs::path moved = root / "Content/Screens/Menu/TitleMoved.renegade-screen";
        fs::create_directories(moved.parent_path());
        std::error_code moveError;
        fs::rename(original, moved, moveError);
        Check(!moveError, "could not move Gate 5C Screen fixture");
        if (moveError) return;

        StoryFlowScreenReferenceService references;
        const StoryFlowScreenResolution resolved = references.ResolveScreen(
            root.generic_u8string(),
            projectId,
            created.committedFlow,
            created.screenNodeId);
        Check(resolved.succeeded, "Gate 5C Screen did not resolve for editor handoff");
        if (!resolved.succeeded) return;

        StoryFlowScreenEditorHandoff handoff;
        error.clear();
        Check(BuildStoryFlowScreenEditorHandoff(resolved, handoff, error),
            "Gate 5C failed to construct Screen Editor handoff");
        Check(handoff.ready,
            "Gate 5C Screen Editor handoff was not marked ready");
        Check(handoff.screenNodeId == created.screenNodeId &&
                handoff.screenDocumentId == created.screenDocumentId,
            "Gate 5C Screen Editor handoff lost stable identity");
        std::error_code pathIdentityError;
        Check(fs::equivalent(fs::u8path(handoff.resolvedPath), moved, pathIdentityError) &&
                !pathIdentityError,
            "Gate 5C Screen Editor handoff did not carry resolved moved path");
        Check(handoff.resolvedPathHint ==
                "Content/Screens/Menu/TitleMoved.renegade-screen" &&
                handoff.pathHintMoved,
            "Gate 5C Screen Editor handoff did not preserve moved-hint diagnostics");
        Check(handoff.actionIds == std::vector<std::string>({
                "new_game", "load_game", "options", "credits", "quit"}),
            "Gate 5C Screen Editor handoff did not carry authored outcomes");

        StoryFlowScreenResolution rejected;
        rejected.succeeded = false;
        rejected.message = "missing Screen";
        StoryFlowScreenEditorHandoff invalidHandoff;
        error.clear();
        Check(!BuildStoryFlowScreenEditorHandoff(rejected, invalidHandoff, error) &&
                !invalidHandoff.ready && !error.empty(),
            "Gate 5C handoff accepted an unresolved Screen");
    }
}

int main()
{
    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade story flow gate5c " + std::to_string(unique))
    };

    TestStableScreenEditorHandoff(temporary.path / "screen boundary project");

    if (failures != 0)
    {
        std::cerr << "RenegadeStoryFlowGate5ScreenStudioBoundaryTests: "
                  << failures << " failure(s)\n";
        return 1;
    }

    std::cout << "PASS: Story Flow Gate 5C Screen Editor stable handoff boundary\n";
    return 0;
}
