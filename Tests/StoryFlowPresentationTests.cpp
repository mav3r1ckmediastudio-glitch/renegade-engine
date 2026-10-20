#include "renegade/bridge/StoryFlowAuthoringModel.h"
#include "../Studio/src/StoryFlowOverviewPolicy.h"
#include "../Studio/src/StoryFlowTitleDraft.h"
#include <cmath>
#include "renegade/bridge/StoryFlowInteractionPolicy.h"
#include "renegade/bridge/StoryFlowJourneyModel.h"
#include "renegade/bridge/StoryFlowLayoutService.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    constexpr const char* ProjectId =
        "11111111-1111-4111-8111-111111111111";
    constexpr const char* FlowDocumentId =
        "22222222-2222-4222-8222-222222222222";
    constexpr const char* GameStartId =
        "33333333-3333-4333-8333-333333333333";
    constexpr const char* LevelOneId =
        "44444444-4444-4444-8444-444444444444";
    constexpr const char* LevelTwoId =
        "55555555-5555-4555-8555-555555555555";
    constexpr const char* CompleteId =
        "66666666-6666-4666-8666-666666666666";
    constexpr const char* SceneOneId =
        "77777777-7777-4777-8777-777777777777";
    constexpr const char* SceneTwoId =
        "88888888-8888-4888-8888-888888888888";
    constexpr const char* RouteStartId =
        "99999999-9999-4999-8999-999999999999";
    constexpr const char* RouteOneId =
        "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
    constexpr const char* RouteTwoId =
        "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb";
    constexpr const char* StaleNodeId =
        "cccccccc-cccc-4ccc-8ccc-cccccccccccc";
    constexpr const char* ScreenNodeId =
        "dddddddd-dddd-4ddd-8ddd-dddddddddddd";
    constexpr const char* ScreenDocumentId =
        "eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee";
    constexpr const char* RouteBranchId =
        "ffffffff-ffff-4fff-8fff-ffffffffffff";
    constexpr const char* RouteScreenId =
        "12121212-1212-4212-8212-121212121212";
    constexpr const char* RouteCycleId =
        "13131313-1313-4313-8313-131313131313";

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
        std::cerr << "RenegadeStoryFlowPresentationTests: " << message << '\n';
        return 1;
    }

    FlowDocument MakeFlow()
    {
        FlowDocument document;
        document.envelope = CreateDocumentEnvelope(
            ProjectId,
            StoryFlowDocumentType,
            "Content/Flow/Main.renegade-flow",
            "Renegade Story Flow Gate 1 tests");
        document.envelope.documentId = FlowDocumentId;
        document.startNodeId = GameStartId;
        document.nodes = {
            {GameStartId, FlowNodeKind::GameStart, "Game Start", {}, {}},
            {LevelOneId, FlowNodeKind::Level, "Level One",
                SceneOneId, "Content/Scenes/LevelOne.wiscene"},
            {LevelTwoId, FlowNodeKind::Level, "Level Two",
                SceneTwoId, "Content/Scenes/LevelTwo.wiscene"},
            {CompleteId, FlowNodeKind::CompleteGame, "Complete Game", {}, {}},
        };
        document.routes = {
            {RouteStartId, GameStartId, GameStartOutcome, LevelOneId,
                "default", 0, {}},
            {RouteOneId, LevelOneId, "level.complete", LevelTwoId,
                "from-level-one", 0, {}},
            {RouteTwoId, LevelTwoId, "level.complete", CompleteId,
                {}, 0, {}},
        };
        return document;
    }

    FlowDocument MakeJourneyFlow()
    {
        FlowDocument document = MakeFlow();
        document.nodes.push_back({
            ScreenNodeId, FlowNodeKind::Screen, "Options Screen", {}, {},
            ScreenDocumentId, "Content/Screens/Options.renegade-screen"});
        document.routes = {
            {RouteStartId, GameStartId, GameStartOutcome, LevelOneId,
                "default", 0, {}},
            {RouteOneId, LevelOneId, "continue", LevelTwoId,
                "from-level-one", 0, {}},
            {RouteBranchId, LevelOneId, "options", ScreenNodeId,
                {}, 1, {}},
            {RouteCycleId, LevelTwoId, "retry", LevelOneId,
                "from-level-two", 0, {}},
            {RouteScreenId, ScreenNodeId, "back", CompleteId,
                {}, 0, {}},
        };
        return document;
    }

    std::vector<std::string> JourneySignature(
        const StoryFlowJourneyModel& journey)
    {
        std::vector<std::string> signature;
        for (const auto& card : journey.Cards())
        {
            signature.push_back(
                card.nodeId + ":" + std::to_string(card.trackIndex) + ":" +
                std::to_string(card.sequenceIndex) + ":" +
                std::to_string(card.columnIndex));
        }
        return signature;
    }

    std::unordered_map<StableId, std::pair<float, float>> LayoutPositions(
        const StoryFlowLayoutDocument& layout)
    {
        std::unordered_map<StableId, std::pair<float, float>> positions;
        for (const auto& node : layout.nodes)
            positions.emplace(node.nodeId, std::make_pair(node.x, node.y));
        return positions;
    }
}

int main()
{
    using namespace renegade::studio;
    bool cameraFailed = false;
    const auto Check = [&cameraFailed](bool ok, const char* message) { if (!ok) { cameraFailed = true; std::cerr << message << "\n"; } };
    StoryFlowTitleDraft title;
    Check(title.UseTemplate("Main Menu") && title.UseTemplate("Options") && title.text == "Options",
        "Options template replaces untouched default instead of appending");
    title.Input(""); // real control emits the empty draft immediately on entry
    Check(title.text.empty() && !title.UseTemplate("Pause"), "title click clears immediately and stops template ownership");
    title.Input("Options");
    Check(title.text == "Options" && !title.UseTemplate("Credits") && title.text == "Options",
        "custom unfocused draft survives template changes and is ready for Create without Enter");
    bool suppressed = false;
    Check(ResolveOverviewIntent(true, true, true, false, suppressed) == OverviewIntent::Navigate,
        "overview single click navigates");
    Check(ResolveOverviewIntent(true, true, true, true, suppressed) == OverviewIntent::Fit,
        "overview double click fits");
    Check(ResolveOverviewIntent(true, false, true, false, suppressed) == OverviewIntent::None,
        "double click suppresses held drag");
    ResolveOverviewIntent(false, false, false, false, suppressed);
    Check(ResolveOverviewIntent(true, true, true, false, suppressed) == OverviewIntent::Navigate,
        "release restores overview navigation");
    const auto camera = FitOverview(-100000, -200000, 100000, 200000, 1200, 800, 30, 2);
    Check(std::isfinite(camera.zoom) && std::isfinite(camera.panX) && std::isfinite(camera.panY) &&
        camera.zoom >= 0.05f && camera.panX == 600 && camera.panY == 400,
        "extreme bounds recover finite centered camera at persisted zoom limit");
    const auto whole = FitOverview(-1000, -200, 3000, 1000, 1200, 800, 30, 2);
    Check(-1000 * whole.zoom + whole.panX >= 29 && 3000 * whole.zoom + whole.panX <= 1171,
        "fit frames whole graph instead of retaining old pan/zoom");
    if (cameraFailed) return 1;
    using namespace renegade::bridge;

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade story flow presentation " +
            std::to_string(unique))
    };
    fs::create_directories(temporary.path);

    FlowDocument flow = MakeFlow();
    std::string error;
    if (!ValidateFlowDocument(flow, ProjectId, error))
        return Fail(temporary.path, "valid LP02 fixture was rejected");

    StoryFlowAuthoringModel model;
    if (!model.Load(flow, ProjectId, error))
        return Fail(temporary.path, "read-only authoring model did not load");
    if (!model.IsLoaded() || model.Nodes().size() != 4 ||
        model.Routes().size() != 3 || model.GameStartNodeId() != GameStartId)
    {
        return Fail(temporary.path, "authoring model did not expose the Flow");
    }
    if (model.Nodes()[0].id != GameStartId ||
        model.Nodes()[0].presentationColumn != 0 ||
        model.FindNode(LevelOneId) == nullptr ||
        model.FindNode(LevelOneId)->presentationColumn != 1 ||
        model.FindNode(LevelTwoId) == nullptr ||
        model.FindNode(LevelTwoId)->presentationColumn != 2 ||
        model.FindNode(CompleteId) == nullptr ||
        model.FindNode(CompleteId)->presentationColumn != 3)
    {
        return Fail(temporary.path, "deterministic presentation order was wrong");
    }

    const StoryFlowLayoutDocument firstLayout =
        BuildDeterministicStoryFlowLayout(model, ProjectId, FlowDocumentId);
    if (firstLayout.nodes.size() != model.Nodes().size())
        return Fail(temporary.path, "default layout omitted Flow nodes");
    if (firstLayout.schemaVersion != StoryFlowLayoutDocument::CurrentSchemaVersion ||
        firstLayout.activeView != StoryFlowViewMode::Journey ||
        firstLayout.journeyCards.size() != model.Nodes().size())
    {
        return Fail(temporary.path,
            "default layout did not establish Journey as the primary view");
    }
    StoryFlowLayoutDocument splitPresentation = firstLayout;
    splitPresentation.journeyCards.pop_back();
    if (ValidateStoryFlowLayout(
            splitPresentation, ProjectId, FlowDocumentId, error))
    {
        return Fail(temporary.path,
            "layout accepted divergent Graph/Journey semantic node references");
    }

    FlowDocument reordered = MakeFlow();
    std::reverse(reordered.nodes.begin(), reordered.nodes.end());
    std::reverse(reordered.routes.begin(), reordered.routes.end());
    StoryFlowAuthoringModel reorderedModel;
    if (!reorderedModel.Load(reordered, ProjectId, error))
        return Fail(temporary.path, "reordered semantic Flow was rejected");
    const StoryFlowLayoutDocument reorderedLayout =
        BuildDeterministicStoryFlowLayout(
            reorderedModel, ProjectId, FlowDocumentId);
    if (LayoutPositions(firstLayout) != LayoutPositions(reorderedLayout))
    {
        return Fail(temporary.path,
            "default layout depended on serialized node/route order");
    }

    const std::string layoutPath = ResolveStoryFlowLayoutPath(
        temporary.path.generic_u8string(), FlowDocumentId);
    if (layoutPath.empty() ||
        fs::u8path(layoutPath).filename() !=
            fs::u8path(std::string(FlowDocumentId) + StoryFlowLayoutExtension))
    {
        return Fail(temporary.path, "layout path was not project/editor scoped");
    }
    if (!WriteStoryFlowLayout(layoutPath, firstLayout, error))
        return Fail(temporary.path, "layout could not be written");

    StoryFlowLayoutDocument reopened;
    if (!ReadStoryFlowLayout(
            layoutPath, ProjectId, FlowDocumentId, reopened, error) ||
        reopened.nodes.size() != firstLayout.nodes.size() ||
        LayoutPositions(reopened) != LayoutPositions(firstLayout))
    {
        return Fail(temporary.path, "layout did not round-trip");
    }

    reopened.activeView = StoryFlowViewMode::Graph;
    reopened.journeyCanvas.panX = 41.0f;
    reopened.journeyCanvas.panY = -19.0f;
    reopened.journeyCanvas.zoom = 1.4f;
    reopened.journeyCards.front().offsetX = 23.0f;
    reopened.journeyCards.front().offsetY = -7.0f;
    if (!WriteStoryFlowLayout(layoutPath, reopened, error))
        return Fail(temporary.path, "Journey layout state could not be written");
    StoryFlowLayoutDocument reopenedJourney;
    if (!ReadStoryFlowLayout(
            layoutPath, ProjectId, FlowDocumentId, reopenedJourney, error) ||
        reopenedJourney.activeView != StoryFlowViewMode::Graph ||
        reopenedJourney.journeyCanvas.panX != 41.0f ||
        reopenedJourney.journeyCanvas.panY != -19.0f ||
        reopenedJourney.journeyCanvas.zoom != 1.4f ||
        reopenedJourney.journeyCards.front().offsetX != 23.0f ||
        reopenedJourney.journeyCards.front().offsetY != -7.0f)
    {
        return Fail(temporary.path,
            "Journey view/canvas/card presentation state did not round-trip");
    }

    const fs::path schemaOnePath = temporary.path / "schema-one.renegade-flow-layout";
    {
        std::ofstream legacy(schemaOnePath, std::ios::binary | std::ios::trunc);
        legacy << "{\"format\":\"renegade-story-flow-layout\","
            "\"schema_version\":1,\"project_id\":\"" << ProjectId <<
            "\",\"flow_document_id\":\"" << FlowDocumentId <<
            "\",\"canvas\":{\"pan_x\":9,\"pan_y\":-4,\"zoom\":1.2},"
            "\"nodes\":[{\"node_id\":\"" << GameStartId <<
            "\",\"x\":17,\"y\":29}]}";
    }
    StoryFlowLayoutDocument migrated;
    if (!ReadStoryFlowLayout(
            schemaOnePath.generic_u8string(), ProjectId, FlowDocumentId,
            migrated, error) ||
        migrated.schemaVersion != StoryFlowLayoutDocument::CurrentSchemaVersion ||
        migrated.activeView != StoryFlowViewMode::Journey ||
        migrated.canvas.panX != 9.0f || migrated.canvas.panY != -4.0f ||
        migrated.nodes.size() != 1 || migrated.journeyCards.size() != 1 ||
        migrated.journeyCards.front().nodeId != GameStartId)
    {
        return Fail(temporary.path,
            "schema-v1 Graph layout did not migrate losslessly to Journey layout state");
    }

    reopened.nodes.push_back({StaleNodeId, 1200.0f, 900.0f});
    reopened.journeyCards.push_back({StaleNodeId, 15.0f, -12.0f});
    reopened.canvas.panX = 17.0f;
    reopened.canvas.panY = -31.0f;
    reopened.canvas.zoom = 1.25f;
    if (!ReconcileStoryFlowLayout(
            model, ProjectId, FlowDocumentId, reopened, error))
    {
        return Fail(temporary.path, "stale layout could not be reconciled");
    }
    if (reopened.nodes.size() != model.Nodes().size() ||
        std::any_of(
            reopened.nodes.begin(), reopened.nodes.end(),
            [](const StoryFlowNodeLayout& node)
            {
                return node.nodeId == StaleNodeId;
            }) ||
        reopened.canvas.panX != 17.0f || reopened.canvas.panY != -31.0f ||
        reopened.canvas.zoom != 1.25f)
    {
        return Fail(temporary.path,
            "layout reconciliation retained stale semantics or lost canvas state");
    }

    // Presentation operations must not alter the authoritative Flow document.
    if (flow.startNodeId != GameStartId || flow.nodes.size() != 4 ||
        flow.routes.size() != 3 || flow.nodes[1].id != LevelOneId ||
        flow.routes[1].id != RouteOneId ||
        flow.routes[1].destinationNodeId != LevelTwoId)
    {
        return Fail(temporary.path, "presentation work mutated Flow semantics");
    }

    FlowInterpreter interpreter;
    if (!interpreter.Initialize(flow, error))
        return Fail(temporary.path, "Flow interpreter did not initialize");
    auto step = interpreter.Start();
    if (!step.succeeded || step.currentNodeId != GameStartId)
        return Fail(temporary.path, "Flow no longer started at Game Start");
    step = interpreter.EmitOutcome(GameStartOutcome);
    if (!step.succeeded || step.currentNodeId != LevelOneId ||
        step.destinationEntry != "default")
    {
        return Fail(temporary.path, "layout changed Game Start traversal");
    }
    step = interpreter.EmitOutcome("level.complete");
    if (!step.succeeded || step.currentNodeId != LevelTwoId ||
        step.destinationEntry != "from-level-one")
    {
        return Fail(temporary.path, "layout changed Level One traversal");
    }
    step = interpreter.EmitOutcome("level.complete");
    if (!step.succeeded || step.currentNodeId != CompleteId ||
        step.terminalAction != FlowTerminalAction::CompleteGame)
    {
        return Fail(temporary.path, "layout changed terminal traversal");
    }

    FlowDocument unreachable = MakeFlow();
    unreachable.routes.erase(unreachable.routes.begin() + 1);
    unreachable.routes.erase(unreachable.routes.begin() + 1);
    StoryFlowAuthoringModel diagnosticModel;
    if (!diagnosticModel.Load(unreachable, ProjectId, error))
        return Fail(temporary.path, "valid unreachable Flow was rejected");
    if (diagnosticModel.Diagnostics().empty() ||
        diagnosticModel.FindNode(LevelTwoId) == nullptr ||
        diagnosticModel.FindNode(LevelTwoId)->reachableFromStart)
    {
        return Fail(temporary.path, "unreachable node diagnostic was not exposed");
    }

    FlowDocument journeyFlow = MakeJourneyFlow();
    if (!ValidateFlowDocument(journeyFlow, ProjectId, error))
        return Fail(temporary.path, "valid branching/cyclic Journey fixture was rejected");
    StoryFlowAuthoringModel journeyAuthoringModel;
    if (!journeyAuthoringModel.Load(journeyFlow, ProjectId, error))
        return Fail(temporary.path, "Journey authoring model did not load");
    StoryFlowJourneyModel journey;
    if (!journey.Build(journeyAuthoringModel, error) ||
        journey.Cards().size() != journeyFlow.nodes.size() ||
        journey.Exits().size() != journeyFlow.routes.size() ||
        journey.Tracks().size() != 2 ||
        !journey.Tracks().front().mainTrack ||
        journey.FindCard(GameStartId) == nullptr ||
        journey.FindCard(GameStartId)->trackIndex != 0 ||
        journey.FindCard(LevelOneId) == nullptr ||
        journey.FindCard(LevelOneId)->trackIndex != 0 ||
        journey.FindCard(LevelTwoId) == nullptr ||
        journey.FindCard(LevelTwoId)->trackIndex != 0 ||
        journey.FindCard(ScreenNodeId) == nullptr ||
        journey.FindCard(ScreenNodeId)->trackIndex != 1 ||
        journey.FindCard(ScreenNodeId)->columnIndex != 2 ||
        journey.FindCard(CompleteId) == nullptr ||
        journey.FindCard(CompleteId)->trackIndex != 1 ||
        journey.FindCard(CompleteId)->columnIndex != 3)
    {
        return Fail(temporary.path,
            "Journey projection did not derive deterministic main/alternate tracks");
    }

    std::reverse(journeyFlow.nodes.begin(), journeyFlow.nodes.end());
    std::reverse(journeyFlow.routes.begin(), journeyFlow.routes.end());
    StoryFlowAuthoringModel reorderedJourneyAuthoringModel;
    StoryFlowJourneyModel reorderedJourney;
    if (!reorderedJourneyAuthoringModel.Load(journeyFlow, ProjectId, error) ||
        !reorderedJourney.Build(reorderedJourneyAuthoringModel, error) ||
        JourneySignature(reorderedJourney) != JourneySignature(journey))
    {
        return Fail(temporary.path,
            "Journey projection depended on serialized node/route order");
    }

    if (!ShouldActivateStoryFlowNodeClick(
            FlowNodeKind::Level, true, 0.20f, 2.0f) ||
        !ShouldActivateStoryFlowNodeClick(
            FlowNodeKind::Screen, true, 0.35f, 8.0f) ||
        ShouldActivateStoryFlowNodeClick(
            FlowNodeKind::CompleteGame, true, 0.10f, 1.0f) ||
        ShouldActivateStoryFlowNodeClick(
            FlowNodeKind::Level, false, 0.10f, 1.0f) ||
        ShouldActivateStoryFlowNodeClick(
            FlowNodeKind::Level, true, 0.65f, 1.0f))
    {
        return Fail(temporary.path,
            "double-click activation policy did not preserve Level/Screen boundaries");
    }
    if (StoryFlowActivationTargetForKind(FlowNodeKind::Level) !=
            StoryFlowActivationTarget::LevelEditor ||
        StoryFlowActivationTargetForKind(FlowNodeKind::Screen) !=
            StoryFlowActivationTarget::ScreenEditor ||
        StoryFlowActivationTargetForKind(FlowNodeKind::GameStart) !=
            StoryFlowActivationTarget::None ||
        StoryFlowActivationTargetForKind(FlowNodeKind::Quit) !=
            StoryFlowActivationTarget::None)
    {
        return Fail(temporary.path,
            "node activation did not dispatch to the accepted Level/Screen boundary");
    }

    // Far/nondefault cameras: measured Graph dimensions and Journey base+offset
    // must centre the new bounds without changing either camera or other cards.
    for (const float zoom : {0.17f, 0.63f, 1.71f})
    {
        auto placement = BuildDeterministicStoryFlowLayout(journeyAuthoringModel, ProjectId, FlowDocumentId);
        placement.canvas = {-18432.0f, 27123.0f, zoom};
        placement.journeyCanvas = {22341.0f, -14782.0f, zoom};
        const auto oldGraph = placement.canvas;
        const auto oldJourney = placement.journeyCanvas;
        const auto untouched = placement.nodes.front();
        for (bool journeyView : {false, true})
        {
            auto& camera = journeyView ? placement.journeyCanvas : placement.canvas;
            const float width = journeyView ? 284.0f : 231.5f;
            const float height = journeyView ? 148.0f : 197.25f;
            const float baseX = journeyView ? 948.0f : 0;
            const float baseY = journeyView ? 392.0f : 0;
            const auto point = CentreNewCard(613, 347, camera.panX, camera.panY,
                camera.zoom, width, height, baseX, baseY);
            const auto centred = [&](float x, float y) {
                return std::abs(camera.panX + (baseX + x + width * 0.5f) * camera.zoom - 613) < 0.01f &&
                    std::abs(camera.panY + (baseY + y + height * 0.5f) * camera.zoom - 347) < 0.01f;
            };
            if (!centred(point.x, point.y)) return Fail(temporary.path, "new bounds not centred at far camera");
            auto card = std::find_if(placement.journeyCards.begin(), placement.journeyCards.end(), [](const auto& item) { return item.nodeId == ScreenNodeId; });
            auto node = std::find_if(placement.nodes.begin(), placement.nodes.end(), [](const auto& item) { return item.nodeId == ScreenNodeId; });
            if (journeyView) { card->offsetX = point.x; card->offsetY = point.y; }
            else { node->x = point.x; node->y = point.y; }
            placement.activeView = journeyView ? StoryFlowViewMode::Journey : StoryFlowViewMode::Graph;
            StoryFlowLayoutDocument reopened;
            if (!WriteStoryFlowLayout(layoutPath, placement, error) ||
                !ReadStoryFlowLayout(layoutPath, ProjectId, FlowDocumentId, reopened, error))
                return Fail(temporary.path, "placement save/reopen failed");
            const auto savedCard = std::find_if(reopened.journeyCards.begin(), reopened.journeyCards.end(), [](const auto& item) { return item.nodeId == ScreenNodeId; });
            const auto savedNode = std::find_if(reopened.nodes.begin(), reopened.nodes.end(), [](const auto& item) { return item.nodeId == ScreenNodeId; });
            const float x = journeyView ? savedCard->offsetX : savedNode->x;
            const float y = journeyView ? savedCard->offsetY : savedNode->y;
            if (!centred(x,y) || reopened.activeView != placement.activeView ||
                reopened.canvas.panX != oldGraph.panX || reopened.canvas.panY != oldGraph.panY || reopened.canvas.zoom != oldGraph.zoom ||
                reopened.journeyCanvas.panX != oldJourney.panX || reopened.journeyCanvas.panY != oldJourney.panY || reopened.journeyCanvas.zoom != oldJourney.zoom ||
                reopened.nodes.front().x != untouched.x || reopened.nodes.front().y != untouched.y)
                return Fail(temporary.path, "placement reopen changed camera or existing card");
        }
    }

    std::cout << "RenegadeStoryFlowPresentationTests: PASS\n";
    return 0;
}
