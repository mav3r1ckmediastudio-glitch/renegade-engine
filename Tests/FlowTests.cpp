#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/DependencyService.h"

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
        std::cerr << "RenegadeFlowTests: " << message << '\n';
        return 1;
    }

    FlowDocument MakeHappyFlow(const StableId& projectId)
    {
        FlowDocument document;
        document.envelope = CreateDocumentEnvelope(
            projectId,
            StoryFlowDocumentType,
            "Content/Flow/Main.renegade-flow",
            "Renegade LP02 tests");

        const StableId gameStart = GenerateStableId();
        const StableId levelOne = GenerateStableId();
        const StableId levelTwo = GenerateStableId();
        const StableId completeGame = GenerateStableId();
        document.startNodeId = gameStart;
        document.nodes = {
            {gameStart, FlowNodeKind::GameStart, "Game Start", {}, {}},
            {levelOne, FlowNodeKind::Level, "Level One",
                GenerateStableId(), "Content/Scenes/LevelOne.wiscene"},
            {levelTwo, FlowNodeKind::Level, "Level Two",
                GenerateStableId(), "Content/Scenes/LevelTwo.wiscene"},
            {completeGame, FlowNodeKind::CompleteGame,
                "Complete Game", {}, {}},
        };
        document.routes = {
            {GenerateStableId(), gameStart, GameStartOutcome, levelOne,
                "default", 0, {}},
            {GenerateStableId(), levelOne, "level.complete", levelTwo,
                "from-level-one", 0, {}},
            {GenerateStableId(), levelTwo, "level.complete", completeGame,
                {}, 0, {}},
        };
        return document;
    }
}

int main()
{
    using namespace renegade::bridge;

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade lp02 flow tests " + std::to_string(unique))
    };
    fs::create_directories(temporary.path / "Content/Flow");

    const StableId projectId = GenerateStableId();
    FlowDocument document = MakeHappyFlow(projectId);
    std::string error;
    if (!ValidateFlowDocument(document, projectId, error))
    {
        return Fail(temporary.path, "valid flow document was rejected");
    }

    const fs::path flowPath =
        temporary.path / "Content/Flow/Main.renegade-flow";
    if (!WriteFlowDocument(flowPath.generic_u8string(), document, error))
    {
        return Fail(temporary.path, "flow document could not be written");
    }

    FlowDocument reopened;
    if (!ReadFlowDocument(
            flowPath.generic_u8string(),
            projectId,
            reopened,
            error) ||
        reopened.envelope.documentId != document.envelope.documentId ||
        reopened.nodes.size() != 4 || reopened.routes.size() != 3)
    {
        return Fail(temporary.path, "flow document did not round-trip");
    }

    StoryFlowDependencyDocument dependencyDocument;
    auto dependencyReader = MakeStoryFlowDependencyReader(projectId);
    if (!dependencyReader(flowPath.generic_u8string(), dependencyDocument, error) ||
        dependencyDocument.scenePathHints.size() != 2 ||
        dependencyDocument.scenePathHints[1] !=
            "Content/Scenes/LevelTwo.wiscene")
    {
        return Fail(temporary.path,
            "Story Flow dependency adapter did not retain Level references");
    }

    const std::string previousLevelName = document.nodes[1].name;
    document.nodes[1].name = "Level One Updated";
    if (!WriteFlowDocument(
            flowPath.generic_u8string(),
            document,
            error))
    {
        return Fail(
            temporary.path,
            "updated flow document did not commit transactionally");
    }

    const fs::path flowBackup =
        flowPath.parent_path() /
        fs::u8path(flowPath.filename().generic_u8string() + ".bak");
    FlowDocument previousFlow;
    if (!ReadFlowDocument(
            flowBackup.generic_u8string(),
            projectId,
            previousFlow,
            error) ||
        previousFlow.nodes.size() != document.nodes.size() ||
        previousFlow.nodes[1].name != previousLevelName)
    {
        return Fail(
            temporary.path,
            "Story Flow transaction did not retain its last-good backup");
    }

    if (!ReadFlowDocument(
            flowPath.generic_u8string(),
            projectId,
            reopened,
            error) ||
        reopened.nodes[1].name != "Level One Updated")
    {
        return Fail(
            temporary.path,
            "Story Flow transactional update was not authoritative");
    }

    // The transactional-update assertions above intentionally renamed
    // nodes[1] to prove the write/backup/reopen path. Revert it in memory
    // *and* commit that revert to disk, since everything below this point
    // re-reads the file rather than reusing the in-memory `document` — so
    // the remaining interpreter assertions exercise the documented
    // "Level One" fixture rather than this test's own leftover mutation.
    document.nodes[1].name = previousLevelName;
    if (!WriteFlowDocument(flowPath.generic_u8string(), document, error))
    {
        return Fail(
            temporary.path,
            "reverted flow document did not commit transactionally");
    }

    const fs::path movedPath =
        temporary.path / "Content/Flow/Renamed.renegade-flow";
    fs::rename(flowPath, movedPath);
    FlowDocument moved;
    if (!ReadFlowDocument(
            movedPath.generic_u8string(),
            projectId,
            moved,
            error) ||
        moved.envelope.documentId != document.envelope.documentId)
    {
        return Fail(
            temporary.path,
            "flow document identity did not survive a file move");
    }

    std::string resolvedFlowPath;
    if (!ResolveStoryFlowDocumentPath(
            temporary.path.generic_u8string(),
            projectId,
            document.envelope.documentId,
            "Content/Flow/Main.renegade-flow",
            resolvedFlowPath,
            error) ||
        fs::u8path(resolvedFlowPath) !=
            fs::weakly_canonical(movedPath))
    {
        return Fail(
            temporary.path,
            "stable flow ID did not repair a stale path hint");
    }

    const fs::path duplicatePath =
        temporary.path / "Content/Flow/Duplicate.renegade-flow";
    fs::copy_file(movedPath, duplicatePath);
    if (ResolveStoryFlowDocumentPath(
            temporary.path.generic_u8string(),
            projectId,
            document.envelope.documentId,
            "Content/Flow/Main.renegade-flow",
            resolvedFlowPath,
            error) ||
        error.find("ambiguous") == std::string::npos)
    {
        return Fail(
            temporary.path,
            "duplicate flow document IDs were not rejected");
    }
    fs::remove(duplicatePath);

    FlowInterpreter interpreter;
    if (!interpreter.Initialize(std::move(moved), error))
    {
        return Fail(temporary.path, "flow interpreter did not initialize");
    }

    auto step = interpreter.Start();
    if (!step.succeeded || step.currentNodeName != "Game Start")
    {
        return Fail(temporary.path, "flow did not start at Game Start");
    }
    step = interpreter.EmitOutcome(GameStartOutcome);
    if (!step.succeeded || step.currentNodeName != "Level One" ||
        step.destinationEntry != "default")
    {
        return Fail(temporary.path, "game.start did not enter Level One");
    }
    step = interpreter.EmitOutcome("level.complete");
    if (!step.succeeded || step.currentNodeName != "Level Two" ||
        step.destinationEntry != "from-level-one")
    {
        return Fail(temporary.path, "Level One did not enter Level Two");
    }
    step = interpreter.EmitOutcome("level.complete");
    if (!step.succeeded || step.currentNodeName != "Complete Game" ||
        step.terminalAction != FlowTerminalAction::CompleteGame)
    {
        return Fail(temporary.path, "Level Two did not complete the game");
    }

    FlowDocument missingDocument = MakeHappyFlow(projectId);
    FlowInterpreter missing;
    if (!missing.Initialize(std::move(missingDocument), error) ||
        !missing.Start().succeeded ||
        !missing.EmitOutcome(GameStartOutcome).succeeded)
    {
        return Fail(temporary.path, "missing-route fixture did not initialize");
    }
    step = missing.EmitOutcome("secret.exit");
    if (step.succeeded || step.code != FlowStepCode::MissingRoute ||
        step.message.find("No Story Flow route") == std::string::npos)
    {
        return Fail(temporary.path, "missing route was not diagnosed");
    }

    FlowDocument invalidUnconditional = MakeHappyFlow(projectId);
    invalidUnconditional.routes.push_back({
        GenerateStableId(),
        invalidUnconditional.nodes[1].id,
        "level.complete",
        invalidUnconditional.nodes[2].id,
        "alternate-entry",
        0,
        {},
    });
    if (ValidateFlowDocument(invalidUnconditional, projectId, error) ||
        error.find("ambiguous equal-priority unconditional") ==
            std::string::npos)
    {
        return Fail(
            temporary.path,
            "equal-priority unconditional routes were not rejected");
    }

    FlowDocument ambiguousDocument = MakeHappyFlow(projectId);
    const StableId levelOneId = ambiguousDocument.nodes[1].id;
    const StableId levelTwoId = ambiguousDocument.nodes[2].id;
    ambiguousDocument.routes[1].priority = 10;
    ambiguousDocument.routes[1].conditions = {
        {"alternate.two", FlowConditionOperator::Missing, {}}
    };
    ambiguousDocument.routes.push_back({
        GenerateStableId(),
        levelOneId,
        "level.complete",
        levelTwoId,
        "alternate-entry",
        10,
        {{"alternate.one", FlowConditionOperator::Missing, {}}},
    });
    FlowInterpreter ambiguous;
    if (!ambiguous.Initialize(std::move(ambiguousDocument), error) ||
        !ambiguous.Start().succeeded ||
        !ambiguous.EmitOutcome(GameStartOutcome).succeeded)
    {
        return Fail(temporary.path, "ambiguous-route fixture did not initialize");
    }
    step = ambiguous.EmitOutcome("level.complete");
    if (step.succeeded || step.code != FlowStepCode::AmbiguousRoute ||
        step.message.find("matched 2 routes") == std::string::npos)
    {
        return Fail(temporary.path, "ambiguous routes were not diagnosed");
    }

    FlowDocument conditionalDocument = MakeHappyFlow(projectId);
    const StableId conditionalLevelOne = conditionalDocument.nodes[1].id;
    const StableId conditionalComplete = conditionalDocument.nodes[3].id;
    conditionalDocument.routes[1].priority = 100;
    conditionalDocument.routes.push_back({
        GenerateStableId(),
        conditionalLevelOne,
        "level.complete",
        conditionalComplete,
        {},
        0,
        {{"difficulty", FlowConditionOperator::Equals, "hard"}},
    });
    FlowInterpreter conditional;
    if (!conditional.Initialize(std::move(conditionalDocument), error) ||
        !conditional.SetState("difficulty", "hard", error) ||
        !conditional.Start().succeeded ||
        !conditional.EmitOutcome(GameStartOutcome).succeeded)
    {
        return Fail(temporary.path, "conditional-route fixture did not initialize");
    }
    step = conditional.EmitOutcome("level.complete");
    if (!step.succeeded || step.currentNodeName != "Complete Game")
    {
        return Fail(
            temporary.path,
            "first-priority matching condition was not deterministic");
    }

    FlowDocument hardCodedDestination = MakeHappyFlow(projectId);
    hardCodedDestination.routes[0].destinationNodeId =
        "Content/Scenes/LevelOne.wiscene";
    if (ValidateFlowDocument(hardCodedDestination, projectId, error) ||
        error.find("stable node IDs") == std::string::npos)
    {
        return Fail(
            temporary.path,
            "path-based route destination was not rejected");
    }

    FlowDocument wrongProject;
    if (ReadFlowDocument(
            movedPath.generic_u8string(),
            GenerateStableId(),
            wrongProject,
            error) ||
        error.find("different project") == std::string::npos)
    {
        return Fail(
            temporary.path,
            "flow document project ownership mismatch was not rejected");
    }

    std::cout
        << "PASS: LP02 serialized deterministic Story Flow interpreter\n";
    return 0;
}
