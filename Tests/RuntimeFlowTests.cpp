#include "RuntimeBootstrap.h"
#include "RuntimeFlow.h"

#include "renegade/bridge/FlowService.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include <wiArchive.h>
#include <wiConfig.h>
#include <wiScene.h>

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
        std::cerr << "RenegadeRuntimeFlowTests: " << message << '\n';
        return 1;
    }

    bool WriteMinimalScene(const fs::path& scenePath, const char* name)
    {
        try
        {
            fs::create_directories(scenePath.parent_path());
            wi::scene::Scene scene;
            scene.Entity_CreateTransform(name);

            wi::Archive archive(scenePath.generic_u8string(), false, false);
            if (!archive.IsOpen())
            {
                return false;
            }
            scene.Serialize(archive);
            const bool written = archive.SaveFile(scenePath.generic_u8string());
            archive = wi::Archive();
            return written && fs::is_regular_file(scenePath);
        }
        catch (...)
        {
            return false;
        }
    }

    bool WriteDescriptor(
        const fs::path& descriptor,
        const StableId& projectId,
        const StableId& flowId,
        const std::string& flowPathHint)
    {
        fs::create_directories(descriptor.parent_path());
        wi::config::File file;
        file.Open(descriptor.generic_u8string());
        file.Set("format", "renegade-project");
        file.Set("version", 1);
        auto& project = file.GetSection("project");
        project.Set("project_id", projectId);
        project.Set("name", "LP02 Runtime Flow Project");
        project.Set("startup_scene", "Content/Scenes/LevelOne.wiscene");
        project.Set("startup_flow_id", flowId);
        project.Set("startup_flow", flowPathHint);
        file.Commit();
        return fs::is_regular_file(descriptor);
    }

    bool WriteSceneMetadata(
        const fs::path& root,
        const fs::path& scenePath,
        const StableId& projectId,
        const StableId& sceneId,
        std::string& error)
    {
        DocumentEnvelope envelope = CreateDocumentEnvelope(
            projectId,
            SceneDocumentType,
            fs::relative(scenePath, root).generic_u8string(),
            "Renegade LP02 Runtime tests");
        envelope.documentId = sceneId;
        fs::path sidecar = scenePath;
        sidecar += ".rmeta";
        return WriteDocumentEnvelope(
            sidecar.generic_u8string(),
            envelope,
            error);
    }

    FlowDocument MakeRuntimeFlow(
        const StableId& projectId,
        const StableId& flowId,
        const StableId& levelOneSceneId,
        const StableId& levelTwoSceneId,
        const std::string& flowPathHint)
    {
        FlowDocument document;
        document.envelope = CreateDocumentEnvelope(
            projectId,
            StoryFlowDocumentType,
            flowPathHint,
            "Renegade LP02 Runtime tests");
        document.envelope.documentId = flowId;

        const StableId gameStart = GenerateStableId();
        const StableId levelOne = GenerateStableId();
        const StableId levelTwo = GenerateStableId();
        const StableId completeGame = GenerateStableId();
        document.startNodeId = gameStart;
        document.nodes = {
            {gameStart, FlowNodeKind::GameStart, "Game Start", {}, {}},
            {levelOne, FlowNodeKind::Level, "Level One",
                levelOneSceneId, "Content/Scenes/LevelOne.wiscene"},
            {levelTwo, FlowNodeKind::Level, "Level Two",
                levelTwoSceneId, "Content/Scenes/LevelTwo.wiscene"},
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
    using namespace renegade;

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade lp02 runtime tests " + std::to_string(unique))
    };

    const fs::path root = temporary.path / "Flow Project With Spaces";
    const fs::path descriptor = root / "Flow Project.renegade";
    const fs::path actualFlowPath =
        root / "Content/Flow/Renamed/Main.renegade-flow";
    const fs::path levelOne = root / "Content/Scenes/LevelOne.wiscene";
    const fs::path actualLevelTwo =
        root / "Content/Scenes/Renamed/LevelTwo.wiscene";

    const bridge::StableId projectId = bridge::GenerateStableId();
    const bridge::StableId flowId = bridge::GenerateStableId();
    const bridge::StableId levelOneSceneId = bridge::GenerateStableId();
    const bridge::StableId levelTwoSceneId = bridge::GenerateStableId();
    std::string error;

    if (!WriteMinimalScene(levelOne, "LP02 Level One Entity") ||
        !WriteMinimalScene(actualLevelTwo, "LP02 Level Two Entity") ||
        !WriteSceneMetadata(
            root,
            levelOne,
            projectId,
            levelOneSceneId,
            error) ||
        !WriteSceneMetadata(
            root,
            actualLevelTwo,
            projectId,
            levelTwoSceneId,
            error) ||
        !WriteDescriptor(
            descriptor,
            projectId,
            flowId,
            "Content/Flow/Main.renegade-flow"))
    {
        return Fail(temporary.path, "could not create Runtime flow fixture");
    }

    bridge::FlowDocument document = MakeRuntimeFlow(
        projectId,
        flowId,
        levelOneSceneId,
        levelTwoSceneId,
        "Content/Flow/Renamed/Main.renegade-flow");
    if (!bridge::WriteFlowDocument(
            actualFlowPath.generic_u8string(),
            document,
            error))
    {
        return Fail(temporary.path, "could not write Runtime flow document");
    }

    auto parsed = runtime::ParseRuntimeLaunchArguments({
        "--project",
        descriptor.generic_u8string(),
        "--flow-outcome",
        "level.complete",
        "--flow-outcome=level.complete",
    });
    if (!parsed.succeeded || parsed.flowOutcomes.size() != 2)
    {
        return Fail(temporary.path, "flow proof arguments were not parsed");
    }

    auto resolved = runtime::ResolveRuntimeProject(std::move(parsed));
    if (!resolved.succeeded ||
        fs::u8path(resolved.startupFlowPath) !=
            fs::weakly_canonical(actualFlowPath))
    {
        return Fail(
            temporary.path,
            "Runtime did not resolve startup flow by stable ID after a move");
    }

    bridge::SceneService scenes;
    runtime::RuntimeFlowController flow;
    auto executed = runtime::LoadRuntimeProjectFlow(
        scenes,
        flow,
        std::move(resolved));
    if (!executed.succeeded ||
        executed.flowNodeName != "Complete Game" ||
        executed.flowTerminalAction !=
            bridge::FlowTerminalAction::CompleteGame ||
        executed.flowTrace.size() != 4 ||
        fs::u8path(executed.startupScenePath) !=
            fs::weakly_canonical(actualLevelTwo) ||
        executed.entityCount == 0)
    {
        return Fail(
            temporary.path,
            "Runtime did not execute Game Start -> Level One -> Level Two -> Complete Game");
    }

    const fs::path logPath = temporary.path / "Logs/RuntimeFlow.log";
    if (!runtime::WriteRuntimeBootstrapLog(
            executed,
            logPath.generic_u8string(),
            error))
    {
        return Fail(temporary.path, "Runtime flow evidence log was not written");
    }
    std::ifstream log(logPath, std::ios::binary);
    const std::string logText(
        (std::istreambuf_iterator<char>(log)),
        std::istreambuf_iterator<char>());
    if (logText.find("startup_flow_id=" + flowId) == std::string::npos ||
        logText.find("flow_node_name=Complete Game") == std::string::npos ||
        logText.find("flow_terminal=complete_game") == std::string::npos ||
        logText.find("flow_trace_count=4") == std::string::npos)
    {
        return Fail(
            temporary.path,
            "Runtime flow log did not contain deterministic trace evidence");
    }

    auto missing = runtime::ParseRuntimeLaunchArguments({
        "--project=" + descriptor.generic_u8string(),
        "--flow-outcome=missing.route",
    });
    missing = runtime::ResolveRuntimeProject(std::move(missing));
    runtime::RuntimeFlowController missingFlow;
    bridge::SceneService missingScenes;
    missing = runtime::LoadRuntimeProjectFlow(
        missingScenes,
        missingFlow,
        std::move(missing));
    if (missing.succeeded ||
        missing.code != runtime::RuntimeBootstrapCode::FlowExecutionFailed ||
        missing.message.find("No Story Flow route") == std::string::npos)
    {
        return Fail(
            temporary.path,
            "Runtime missing route did not produce a structured diagnostic");
    }

    bridge::FlowDocument ambiguousDocument = document;
    ambiguousDocument.routes[1].priority = 10;
    ambiguousDocument.routes[1].conditions = {
        {"alternate.one", bridge::FlowConditionOperator::Missing, {}}
    };
    ambiguousDocument.routes.push_back({
        bridge::GenerateStableId(),
        ambiguousDocument.nodes[1].id,
        "level.complete",
        ambiguousDocument.nodes[2].id,
        "ambiguous-entry",
        10,
        {{"alternate.two", bridge::FlowConditionOperator::Missing, {}}},
    });
    if (!bridge::WriteFlowDocument(
            actualFlowPath.generic_u8string(),
            ambiguousDocument,
            error))
    {
        return Fail(temporary.path, "could not write ambiguous flow fixture");
    }
    auto ambiguous = runtime::ParseRuntimeLaunchArguments({
        "--project",
        descriptor.generic_u8string(),
        "--flow-outcome",
        "level.complete",
    });
    ambiguous = runtime::ResolveRuntimeProject(std::move(ambiguous));
    runtime::RuntimeFlowController ambiguousFlow;
    bridge::SceneService ambiguousScenes;
    ambiguous = runtime::LoadRuntimeProjectFlow(
        ambiguousScenes,
        ambiguousFlow,
        std::move(ambiguous));
    if (ambiguous.succeeded ||
        ambiguous.code != runtime::RuntimeBootstrapCode::FlowExecutionFailed ||
        ambiguous.message.find("matched 2 routes") == std::string::npos)
    {
        return Fail(
            temporary.path,
            "Runtime ambiguous routes did not produce a structured diagnostic");
    }

    const fs::path incompleteDescriptor =
        root / "Incomplete Flow Reference.renegade";
    {
        wi::config::File file;
        file.Open(incompleteDescriptor.generic_u8string());
        file.Set("format", "renegade-project");
        file.Set("version", 1);
        auto& project = file.GetSection("project");
        project.Set("project_id", projectId);
        project.Set("name", "Incomplete Flow Reference");
        project.Set("startup_scene", "Content/Scenes/LevelOne.wiscene");
        project.Set("startup_flow", "Content/Flow/Main.renegade-flow");
        file.Commit();
    }
    auto incomplete = runtime::ResolveRuntimeProject(
        runtime::ParseRuntimeLaunchArguments(
            {"--project", incompleteDescriptor.generic_u8string()}));
    if (incomplete.succeeded ||
        incomplete.code != runtime::RuntimeBootstrapCode::ProjectRejected)
    {
        return Fail(
            temporary.path,
            "Runtime accepted a path-only startup Story Flow reference");
    }

    bridge::FlowDocument wrongOwner = document;
    wrongOwner.envelope.projectId = bridge::GenerateStableId();
    if (!bridge::WriteFlowDocument(
            actualFlowPath.generic_u8string(),
            wrongOwner,
            error))
    {
        return Fail(temporary.path, "could not write wrong-owner flow fixture");
    }
    auto rejected = runtime::ResolveRuntimeProject(
        runtime::ParseRuntimeLaunchArguments(
            {"--project", descriptor.generic_u8string()}));
    if (rejected.succeeded ||
        rejected.code != runtime::RuntimeBootstrapCode::StartupFlowRejected ||
        rejected.message.find("different project") == std::string::npos)
    {
        return Fail(
            temporary.path,
            "Runtime accepted a flow document owned by another project");
    }

    std::cout
        << "PASS: LP02 Runtime Story Flow execution and diagnostics\n";
    return 0;
}
