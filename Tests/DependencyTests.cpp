#include "renegade/bridge/DependencyService.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "RenegadeDependencyTests: " << message << '\n';
        return 1;
    }

    class FixtureProvider final : public renegade::bridge::IDependencyProvider
    {
    public:
        const char* Name() const noexcept override { return "fixture-project"; }
        std::uint32_t Version() const noexcept override { return 2; }
        bool Supports(const renegade::bridge::DependencyClass dependencyClass)
            const noexcept override
        {
            return dependencyClass ==
                renegade::bridge::DependencyClass::ProjectDocument;
        }
        bool Discover(
            const renegade::bridge::DependencyProviderContext& context,
            const renegade::bridge::DependencyCandidateSink& emit,
            std::string& error) const override
        {
            if (context.source == nullptr)
            {
                error = "missing source";
                return false;
            }
            emit({"Content/Scenes/Startup.wiscene",
                renegade::bridge::DependencyClass::Scene,
                renegade::bridge::DependencyRequirement::Required,
                "project.startup_scene", false});
            emit({"Content/Audio/missing.ogg",
                renegade::bridge::DependencyClass::Audio,
                renegade::bridge::DependencyRequirement::Required,
                "fixture.missing", false});
            emit({"Content/Scenes/startup.wiscene",
                renegade::bridge::DependencyClass::Scene,
                renegade::bridge::DependencyRequirement::Required,
                "fixture.case_collision", false});
            error.clear();
            return true;
        }
    };

    class FailingProvider final : public renegade::bridge::IDependencyProvider
    {
    public:
        const char* Name() const noexcept override { return "failing"; }
        std::uint32_t Version() const noexcept override { return 1; }
        bool Supports(renegade::bridge::DependencyClass) const noexcept override
        {
            return true;
        }
        bool Discover(
            const renegade::bridge::DependencyProviderContext&,
            const renegade::bridge::DependencyCandidateSink& emit,
            std::string& error) const override
        {
            emit({"Content/Data/must-not-leak.json",
                renegade::bridge::DependencyClass::Data,
                renegade::bridge::DependencyRequirement::Required,
                "failing.partial", false});
            error = "intentional failure";
            return false;
        }
    };
}
int main()
{
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "renegade-lp05-path-tests";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::create_directories(root / "Content/Textures");
    std::ofstream(root / "Content/Textures/Stone.png") << "fixture";
    fs::create_directories(root / "Content/Scenes");
    std::ofstream(root / "representative.renegade") << "fixture";
    std::ofstream(root / "Content/Scenes/Startup.wiscene") << "fixture";
    std::ofstream(root / "Content/Scenes/Secondary.wiscene") << "fixture";
    fs::create_directories(root / "Content/Flow");
    std::ofstream(root / "Content/Flow/Main.renegade-flow") << "fixture";
    fs::create_directories(root / "Content/UI");
    std::ofstream(root / "Content/UI/Main.renegade-screen") << "fixture";
    std::ofstream(root / "Content/UI/background.png") << "fixture";
    fs::create_directories(root / "Content/Scripts");
    std::ofstream(root / "Content/Scripts/main.lua") << "fixture";
    std::ofstream(root / "Content/Scripts/shared.lua") << "fixture";

    const auto existing = renegade::bridge::ResolveDependencyPath(
        root.string(), "Content/Textures/../Textures/Stone.png");
    if (!existing.accepted || !existing.exists ||
        existing.canonicalRelativePath != "Content/Textures/Stone.png")
        return Fail("existing dependency did not canonicalize deterministically");

    const auto missing = renegade::bridge::ResolveDependencyPath(
        root.string(), "Content/Audio/missing.ogg");
    if (!missing.accepted || missing.exists ||
        missing.canonicalRelativePath != "Content/Audio/missing.ogg")
        return Fail("missing dependency was not accepted for graph diagnosis");

    const auto escaped = renegade::bridge::ResolveDependencyPath(
        root.string(), "../outside-project.txt");
    if (escaped.accepted)
        return Fail("parent traversal escaped the project boundary");

    const auto absolute = renegade::bridge::ResolveDependencyPath(
        root.string(), fs::absolute(root / "Content/Textures/Stone.png").string());
    if (absolute.accepted)
        return Fail("absolute dependency path was accepted");

    renegade::bridge::DependencyPathRegistry registry;
    const auto first = registry.Register(
        "scene:startup", "Content/Textures/Stone.png");
    if (!first.inserted || !first.diagnostics.empty())
        return Fail("first canonical dependency path was not registered");

    const auto duplicate = registry.Register(
        "scene:secondary", "Content/Textures/Stone.png");
    if (duplicate.inserted || duplicate.diagnostics.size() != 1 ||
        duplicate.diagnostics.front().code !=
            renegade::bridge::DependencyDiagnosticCode::Duplicate)
        return Fail("exact duplicate dependency was not diagnosed");

    const auto caseCollision = registry.Register(
        "scene:secondary", "Content/Textures/stone.png");
    if (caseCollision.inserted || caseCollision.diagnostics.size() != 1 ||
        caseCollision.diagnostics.front().code !=
            renegade::bridge::DependencyDiagnosticCode::CaseCollision)
        return Fail("case-only dependency collision was not diagnosed");

    const auto outside = fs::temp_directory_path() / "renegade-lp05-outside";
    fs::create_directories(outside);
    std::ofstream(outside / "escaped.txt") << "outside";
    fs::create_directory_symlink(outside, root / "Content/Linked", ignored);
    if (!ignored)
    {
        const auto symlinkEscape = renegade::bridge::ResolveDependencyPath(
            root.string(), "Content/Linked/escaped.txt");
        if (symlinkEscape.accepted)
            return Fail("symlink escaped the project boundary");
    }

    FixtureProvider provider;
    renegade::bridge::DependencyCollector collector(root.string());
    std::string collectorError;
    if (!collector.RegisterProvider(provider, collectorError))
        return Fail("fixture provider registration failed");
    if (collector.RegisterProvider(provider, collectorError))
        return Fail("duplicate provider name was accepted");
    if (!collector.AddRoot({"representative.renegade",
            renegade::bridge::DependencyClass::ProjectDocument,
            renegade::bridge::DependencyRequirement::Required,
            "fixture.project"}, collectorError))
        return Fail("project graph root was rejected");
    if (!collector.DiscoverRootDependencies(collectorError))
        return Fail("root provider dispatch failed");

    const auto& graph = collector.Graph();
    if (graph.rootIds.size() != 1 || graph.nodes.size() != 3 ||
        graph.edges.size() != 3)
        return Fail("collector did not build the expected one-hop graph");
    if (graph.nodes[1].provider != "fixture-project" ||
        graph.nodes[1].providerVersion != 2 ||
        graph.edges[0].provenance != "project.startup_scene")
        return Fail("provider provenance was not preserved");
    if (graph.diagnostics.size() != 2 ||
        graph.diagnostics[0].code !=
            renegade::bridge::DependencyDiagnosticCode::Missing ||
        graph.diagnostics[1].code !=
            renegade::bridge::DependencyDiagnosticCode::CaseCollision)
        return Fail("emitted dependency diagnostics were incomplete");
    if (graph.edges[2].targetId != graph.nodes[1].id)
        return Fail("case-collision edge did not resolve to its existing node");
    if (graph.nodes.front().provider != "fixture.project")
        return Fail("graph root provenance was discarded");

    FailingProvider failingProvider;
    renegade::bridge::DependencyCollector failingCollector(root.string());
    if (!failingCollector.RegisterProvider(failingProvider, collectorError) ||
        !failingCollector.AddRoot({"representative.renegade",
            renegade::bridge::DependencyClass::ProjectDocument,
            renegade::bridge::DependencyRequirement::Required,
            "fixture.project"}, collectorError))
        return Fail("failing-provider fixture setup failed");
    if (failingCollector.DiscoverRootDependencies(collectorError))
        return Fail("provider failure was reported as success");
    if (failingCollector.Graph().nodes.size() != 1 ||
        !failingCollector.Graph().edges.empty())
        return Fail("failed provider leaked a partial graph mutation");

    renegade::bridge::DependencyCollector unsafeCollector(root.string());
    if (unsafeCollector.AddRoot({"../outside.renegade",
            renegade::bridge::DependencyClass::ProjectDocument,
            renegade::bridge::DependencyRequirement::Required,
            "fixture.unsafe"}, collectorError))
        return Fail("outside-project graph root was accepted");
    if (unsafeCollector.Graph().diagnostics.size() != 1 ||
        unsafeCollector.Graph().diagnostics.front().code !=
            renegade::bridge::DependencyDiagnosticCode::OutsideProject)
        return Fail("outside-project graph root was not diagnosed");

    using namespace renegade::bridge;
    ProjectDependencyProvider projectProvider(
        [](const std::string& path, ProjectDependencyDocument& document,
            std::string& error)
        {
            if (fs::path(path).filename() != "representative.renegade")
            {
                error = "unexpected project path";
                return false;
            }
            document.projectId = "fixture-project";
            document.startupScene = "Content/Scenes/Startup.wiscene";
            document.startupFlow = "Content/Flow/Main.renegade-flow";
            document.startupScreen = "Content/UI/Main.renegade-screen";
            error.clear();
            return true;
        });
    DependencyCollector projectCollector(root.string());
    if (!projectCollector.RegisterProvider(projectProvider, collectorError) ||
        !projectCollector.AddRoot({"representative.renegade",
            DependencyClass::ProjectDocument,
            DependencyRequirement::Required, "fixture.project"}, collectorError) ||
        !projectCollector.DiscoverRootDependencies(collectorError))
        return Fail("Gate 3 project provider failed");
    const auto& projectGraph = projectCollector.Graph();
    if (projectGraph.nodes.size() != 4 || projectGraph.edges.size() != 3 ||
        projectGraph.nodes[1].dependencyClass != DependencyClass::Scene ||
        projectGraph.nodes[2].dependencyClass != DependencyClass::StoryFlowDocument ||
        projectGraph.nodes[3].dependencyClass != DependencyClass::RuntimeScreenDocument)
        return Fail("project provider did not emit the typed startup documents");

    StoryFlowDependencyProvider flowProvider(
        [](const std::string&, StoryFlowDependencyDocument& document,
            std::string& error)
        {
            document.projectId = "fixture-project";
            document.scenePathHints = {
                "Content/Scenes/Startup.wiscene",
                "Content/Scenes/Secondary.wiscene"};
            error.clear();
            return true;
        });
    DependencyCollector flowCollector(root.string());
    if (!flowCollector.RegisterProvider(flowProvider, collectorError) ||
        !flowCollector.AddRoot({"Content/Flow/Main.renegade-flow",
            DependencyClass::StoryFlowDocument,
            DependencyRequirement::Required, "fixture.flow"}, collectorError) ||
        !flowCollector.DiscoverRootDependencies(collectorError) ||
        flowCollector.Graph().nodes.size() != 3 ||
        flowCollector.Graph().edges[1].provenance !=
            "story_flow.level[1].scene_path_hint")
        return Fail("Story Flow provider did not emit level scene hints");

    RuntimeScreenDependencyProvider screenProvider(
        [](const std::string&, RuntimeScreenDependencyDocument& document,
            std::string& error)
        {
            document.projectId = "fixture-project";
            document.imagePaths = {"Content/UI/background.png"};
            document.fontPaths = {"Content/UI/missing-font.ttf"};
            error.clear();
            return true;
        });
    DependencyCollector screenCollector(root.string());
    if (!screenCollector.RegisterProvider(screenProvider, collectorError) ||
        !screenCollector.AddRoot({"Content/UI/Main.renegade-screen",
            DependencyClass::RuntimeScreenDocument,
            DependencyRequirement::Required, "fixture.screen"}, collectorError) ||
        !screenCollector.DiscoverRootDependencies(collectorError))
        return Fail("Runtime Screen provider failed");
    const auto& screenGraph = screenCollector.Graph();
    if (screenGraph.nodes.size() != 3 || screenGraph.edges.size() != 2 ||
        screenGraph.nodes[1].dependencyClass != DependencyClass::Texture ||
        screenGraph.nodes[2].dependencyClass != DependencyClass::Font ||
        screenGraph.diagnostics.size() != 1 ||
        screenGraph.diagnostics.front().code != DependencyDiagnosticCode::Missing)
        return Fail("Runtime Screen provider did not preserve typed resources");

    DeclaredReferenceDependencyProvider declaredProvider({
        {"Content/Scripts/main.lua",
            {"Content/Scripts/shared.lua", DependencyClass::Script,
                DependencyRequirement::Required,
                "script.declared_dependency[0]", false}}
    });
    DependencyCollector declaredCollector(root.string());
    if (!declaredCollector.RegisterProvider(declaredProvider, collectorError) ||
        !declaredCollector.AddRoot({"Content/Scripts/main.lua",
            DependencyClass::Script, DependencyRequirement::Required,
            "fixture.script"}, collectorError) ||
        !declaredCollector.DiscoverRootDependencies(collectorError) ||
        declaredCollector.Graph().nodes.size() != 2 ||
        declaredCollector.Graph().edges.front().provenance !=
            "script.declared_dependency[0]")
        return Fail("declared-reference provider did not emit typed declaration");

    fs::remove_all(root, ignored);
    fs::remove_all(outside, ignored);
    std::cout << "RenegadeDependencyTests passed\n";
    return 0;
}
