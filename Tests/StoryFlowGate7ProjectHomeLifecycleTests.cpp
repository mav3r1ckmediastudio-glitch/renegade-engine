#include "RuntimeBootstrap.h"
#include "RuntimeFlow.h"

#include "renegade/bridge/DependencyService.h"
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/StudioProjectService.h"
#include "renegade/bridge/StudioSession.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include <wiConfig.h>

namespace
{
    namespace fs = std::filesystem;

    struct TemporaryDirectory
    {
        fs::path path;

        ~TemporaryDirectory()
        {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    };

    int Fail(const char* message)
    {
        std::cerr << "RenegadeStoryFlowGate7ProjectHomeLifecycleTests: "
                  << message << '\n';
        return 1;
    }

    std::string ReadText(const fs::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return std::string(
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>());
    }
}

int main()
{
    using namespace renegade;

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade-story-flow-gate7-" + std::to_string(unique))};
    fs::create_directories(temporary.path);

    bridge::ProjectService factory;
    if (!factory.CreateStoryFlowProject(
            temporary.path.generic_u8string(), "NativeA"))
    {
        return Fail(factory.LastError().c_str());
    }

    const bridge::ProjectMetadata nativeA = factory.CurrentProject();
    const fs::path nativeARoot = fs::u8path(nativeA.rootPath);
    const fs::path nativeAFlow =
        nativeARoot / fs::u8path(nativeA.startupFlow);
    if (!nativeA.startupScene.empty() ||
        nativeA.startupFlow != "Content/StoryFlow/Main.renegade-flow" ||
        !bridge::IsValidStableId(nativeA.startupFlowId) ||
        !fs::is_regular_file(nativeAFlow) ||
        fs::exists(nativeARoot / "Content/Scenes/Main.wiscene"))
    {
        return Fail("new project was not born with a Flow-only project home");
    }

    bridge::FlowDocument flow;
    std::string error;
    if (!bridge::ReadFlowDocument(
            nativeAFlow.generic_u8string(), nativeA.projectId, flow, error) ||
        flow.envelope.documentId != nativeA.startupFlowId ||
        flow.nodes.size() != 1u ||
        flow.nodes.front().kind != bridge::FlowNodeKind::GameStart ||
        flow.nodes.front().id != flow.startNodeId ||
        !flow.routes.empty())
    {
        return Fail("canonical project-home Flow is not exactly one Game Start");
    }

    const std::string descriptorText = ReadText(nativeA.descriptorPath);
    if (descriptorText.find("startup_scene = \n") == std::string::npos ||
        descriptorText.find("startup_flow_id = " + nativeA.startupFlowId) ==
            std::string::npos ||
        descriptorText.find(
            "startup_flow = Content/StoryFlow/Main.renegade-flow") ==
            std::string::npos)
    {
        return Fail("native descriptor did not retain its Flow-only launch root");
    }

    bridge::ProjectMetadata inspected;
    if (!factory.InspectProject(nativeA.descriptorPath, inspected, error) ||
        !factory.StartupScenePath().empty())
    {
        return Fail("Flow-native project did not round-trip without a Scene");
    }

    bridge::ProjectDependencyDocument dependencies;
    auto dependencyReader = bridge::MakeProjectDependencyReader();
    if (!dependencyReader(nativeA.descriptorPath, dependencies, error) ||
        !dependencies.startupScene.empty() ||
        dependencies.startupFlow != nativeA.startupFlow)
    {
        return Fail("dependency extraction invented a placeholder startup Scene");
    }

    auto resolvedRuntime = runtime::ResolveRuntimeProject(
        runtime::ParseRuntimeLaunchArguments(
            {"--project", nativeA.descriptorPath}));
    if (!resolvedRuntime.succeeded)
    {
        return Fail((
            "Runtime rejected the Flow-only project home: " +
            resolvedRuntime.message).c_str());
    }
    if (!resolvedRuntime.startupScenePath.empty())
    {
        return Fail("Runtime invented a startup Scene for a Flow-only project");
    }
    std::error_code equivalentError;
    if (!fs::equivalent(
            fs::u8path(resolvedRuntime.startupFlowPath),
            nativeAFlow,
            equivalentError) ||
        equivalentError)
    {
        return Fail(
            "Runtime startup Flow did not resolve to the canonical project-home file");
    }

    bridge::SceneService scenes;
    runtime::RuntimeFlowController flowController;
    const auto enteredFlow = runtime::LoadRuntimeProjectFlow(
        scenes, flowController, resolvedRuntime);
    if (!enteredFlow.succeeded ||
        enteredFlow.flowNodeId != flow.startNodeId ||
        enteredFlow.entityCount != 0u ||
        !enteredFlow.startupScenePath.empty())
    {
        return Fail(
            "Runtime could not enter Game Start without a placeholder Scene");
    }

    const auto invalidDirectSceneLoad =
        runtime::LoadRuntimeProjectScene(scenes, resolvedRuntime);
    if (invalidDirectSceneLoad.succeeded ||
        invalidDirectSceneLoad.code !=
            runtime::RuntimeBootstrapCode::SceneLoadFailed)
    {
        return Fail("Runtime did not fail closed on direct Scene entry");
    }

    bridge::StudioSession studio;
    studio.Projects().Initialize(
        (temporary.path / "studio-state.ini").generic_u8string());
    studio.Scenes().GetScene().Entity_CreateTransform("Previous project content");
    if (!studio.Projects().OpenProject(nativeA.descriptorPath) ||
        studio.Projects().HasProject() ||
        !studio.Projects().HasPendingProject() ||
        !studio.CommitPendingProjectWithoutScene() ||
        studio.Projects().CurrentProject().projectId != nativeA.projectId ||
        studio.Scenes().EntityCount() != 0u ||
        !studio.Scenes().CurrentPath().empty())
    {
        return Fail(
            "native project adoption was not staged or retained the previous Scene");
    }

    if (!factory.CreateStoryFlowProject(
            temporary.path.generic_u8string(), "NativeB"))
    {
        return Fail("second native fixture could not be created");
    }
    const bridge::ProjectMetadata nativeB = factory.CurrentProject();
    const fs::path nativeBFlow =
        fs::u8path(nativeB.rootPath) / fs::u8path(nativeB.startupFlow);
    std::ofstream(nativeBFlow, std::ios::binary | std::ios::trunc)
        << "corrupt-flow";

    if (studio.Projects().OpenProject(nativeB.descriptorPath) ||
        studio.Projects().HasPendingProject() ||
        studio.Projects().CurrentProject().projectId != nativeA.projectId ||
        studio.Projects().RecentProjects().size() != 1u)
    {
        return Fail("invalid candidate Flow displaced the active project");
    }

    const fs::path invalidRoot = temporary.path / "NoLaunchRoot";
    fs::create_directories(invalidRoot);
    const fs::path invalidDescriptor = invalidRoot / "NoLaunchRoot.renegade";
    wi::config::File invalid;
    invalid.Open(invalidDescriptor.generic_u8string());
    invalid.Set("format", "renegade-project");
    invalid.Set("version", 1);
    auto& project = invalid.GetSection("project");
    project.Set("project_id", "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
    project.Set("name", "NoLaunchRoot");
    project.Set("startup_scene", "");
    project.Set("startup_flow_id", "");
    project.Set("startup_flow", "");
    invalid.Commit();
    if (factory.InspectProject(
            invalidDescriptor.generic_u8string(), inspected, error) ||
        error.find("does not declare") == std::string::npos)
    {
        return Fail("descriptor without a Scene or Flow launch root was accepted");
    }

    std::cout << "PASS: Gate 7 Story Flow-native project-home lifecycle\n";
    return 0;
}
