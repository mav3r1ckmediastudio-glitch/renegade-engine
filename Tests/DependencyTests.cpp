#include "renegade/bridge/DependencyService.h"

#include <WickedEngine.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <utility>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "RenegadeDependencyTests: " << message << '\n';
        return 1;
    }

    std::string ReadBytes(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
    }

    void PopulateGate4Scene(
        wi::scene::Scene& scene,
        const std::filesystem::path& path,
        const bool includeEnvironmentProbe)
    {
        const auto projectRoot =
            path.parent_path().parent_path().parent_path();
        const auto resource = [&projectRoot](const char* relative)
        {
            return (projectRoot / relative).generic_u8string();
        };

        auto& firstMaterial = scene.materials.Create(wi::ecs::CreateEntity());
        firstMaterial.textures[
            wi::scene::MaterialComponent::BASECOLORMAP].name =
                resource("Content/Textures/albedo.png");
        firstMaterial.textures[
            wi::scene::MaterialComponent::NORMALMAP].name =
                resource("Content/Textures/missing-normal.png");
        auto& secondMaterial = scene.materials.Create(wi::ecs::CreateEntity());
        secondMaterial.textures[
            wi::scene::MaterialComponent::BASECOLORMAP].name =
                resource("Content/Textures/albedo.png");

        auto& light = scene.lights.Create(wi::ecs::CreateEntity());
        light.lensFlareNames = {
            resource("Content/Textures/flare.png"),
            resource("Content/Textures/albedo.png"),
        };

        if (includeEnvironmentProbe)
        {
            scene.probes.Create(wi::ecs::CreateEntity()).textureName =
                resource("Content/Textures/probe.dds");
        }

        auto& weather = scene.weathers.Create(wi::ecs::CreateEntity());
        weather.skyMapName = resource("Content/Textures/sky.dds");
        weather.colorGradingMapName = resource("Content/Textures/grade.png");
        weather.volumetricCloudsWeatherMapFirstName =
            resource("Content/Textures/cloud-first.png");
        weather.volumetricCloudsWeatherMapSecondName =
            resource("Content/Textures/cloud-second.png");

        scene.sounds.Create(wi::ecs::CreateEntity()).filename =
            resource("Content/Audio/ambient.ogg");
        scene.videos.Create(wi::ecs::CreateEntity()).filename =
            resource("Content/Video/intro.mp4");
        scene.scripts.Create(wi::ecs::CreateEntity()).filename =
            resource("Content/Scripts/main.lua");

        // This deliberately looks like a resource path. Gate 4 must not scan
        // untyped metadata strings.
        scene.metadatas.Create(wi::ecs::CreateEntity()).string_values.set(
            "description", "Content/Textures/not-a-dependency.png");
    }

    bool WriteGate4Scene(const std::filesystem::path& path)
    {
        try
        {
            wi::scene::Scene scene;
            // EnvironmentProbeComponent deserialization creates its render
            // cubemap immediately. Keep the archive proof GPU-free and cover
            // that public field through the direct const-scene walker below.
            PopulateGate4Scene(scene, path, false);

            wi::Archive archive(path.generic_u8string(), false, false);
            if (!archive.IsOpen())
                return false;
            scene.Serialize(archive);
            const bool saved = archive.SaveFile(path.generic_u8string());
            archive = wi::Archive();
            return saved;
        }
        catch (...)
        {
            return false;
        }
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
            const renegade::bridge::DependencyDiagnosticSink&,
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
            const renegade::bridge::DependencyDiagnosticSink& diagnose,
            std::string& error) const override
        {
            emit({"Content/Data/must-not-leak.json",
                renegade::bridge::DependencyClass::Data,
                renegade::bridge::DependencyRequirement::Required,
                "failing.partial", false});
            diagnose({
                renegade::bridge::DependencyDiagnosticCode::
                    UndeclaredComputedReference,
                "computed-before-failure",
                "must not leak",
            });
            error = "intentional failure";
            return false;
        }
    };

    class PairProvider final : public renegade::bridge::IDependencyProvider
    {
    public:
        PairProvider(std::string name, std::string first, std::string second)
            : name_(std::move(name)), first_(std::move(first)),
              second_(std::move(second)) {}

        const char* Name() const noexcept override { return name_.c_str(); }
        std::uint32_t Version() const noexcept override { return 1; }
        bool Supports(const renegade::bridge::DependencyClass value)
            const noexcept override
        {
            return value == renegade::bridge::DependencyClass::ProjectDocument;
        }
        bool Discover(
            const renegade::bridge::DependencyProviderContext&,
            const renegade::bridge::DependencyCandidateSink& emit,
            const renegade::bridge::DependencyDiagnosticSink&,
            std::string& error) const override
        {
            emit({first_, renegade::bridge::DependencyClass::Data,
                renegade::bridge::DependencyRequirement::Required,
                "fixture.first", false});
            emit({second_, renegade::bridge::DependencyClass::Data,
                renegade::bridge::DependencyRequirement::Required,
                "fixture.second", false});
            error.clear();
            return true;
        }

    private:
        std::string name_;
        std::string first_;
        std::string second_;
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
    std::ofstream(root / "Content/Scripts/main.lua")
        << "local metadata = 'Content/Textures/not-a-dependency.png'\n"
           "local next_script = choose_script_at_runtime()\n"
           "dofile(script_root .. next_script)\n"
           "return true\n";
    std::ofstream(root / "Content/Scripts/shared.lua") << "fixture";
    std::ofstream(root / "Content/Scripts/Alpha.lua") << "fixture";
    fs::create_directories(root / "Content/Models");
    const std::string unicodeModelUpper =
        "Content/Models/\xC3\x89" "p\xC3\xA9" "e.glb";
    const std::string unicodeModelLower =
        "Content/Models/\xC3\xA9" "p\xC3\xA9" "e.glb";
    std::ofstream(root / fs::u8path(unicodeModelUpper)) << "fixture";

    const auto existing = renegade::bridge::ResolveDependencyPath(
        root.string(), "Content/Textures/../Textures/Stone.png");
    if (!existing.accepted || !existing.exists ||
        existing.canonicalRelativePath != "Content/Textures/Stone.png")
        return Fail("existing dependency did not canonicalize deterministically");

    const auto existingCaseVariant = renegade::bridge::ResolveDependencyPath(
        root.generic_u8string(), "Content/Textures/stone.png");
    if (!existingCaseVariant.accepted ||
        existingCaseVariant.canonicalRelativePath !=
            "Content/Textures/stone.png")
        return Fail("existing dependency declaration casing was not preserved");

    const auto missing = renegade::bridge::ResolveDependencyPath(
        root.string(), "Content/Audio/missing.ogg");
    if (!missing.accepted || missing.exists ||
        missing.canonicalRelativePath != "Content/Audio/missing.ogg")
        return Fail("missing dependency was not accepted for graph diagnosis");

    const auto missingCaseVariant = renegade::bridge::ResolveDependencyPath(
        root.generic_u8string(), "Content/Audio/MISSING.ogg");
    if (!missingCaseVariant.accepted || missingCaseVariant.exists ||
        missingCaseVariant.canonicalRelativePath != "Content/Audio/MISSING.ogg")
        return Fail("missing dependency declaration casing was not preserved");

    const auto emptyDeclaredPath = renegade::bridge::ResolveDependencyPath(
        root.generic_u8string(), "");
    if (emptyDeclaredPath.accepted ||
        emptyDeclaredPath.error !=
            "Project root and dependency path are required." ||
        emptyDeclaredPath.error.find("UTF-8") != std::string::npos)
        return Fail("empty dependency path did not retain its required-path error");

    const auto unicodeCaseVariant = renegade::bridge::ResolveDependencyPath(
        root.generic_u8string(), unicodeModelLower);
    if (!unicodeCaseVariant.accepted ||
        unicodeCaseVariant.canonicalRelativePath != unicodeModelLower)
        return Fail("Unicode dependency declaration casing was not preserved");

    const auto escaped = renegade::bridge::ResolveDependencyPath(
        root.string(), "../outside-project.txt");
    if (escaped.accepted)
        return Fail("parent traversal escaped the project boundary");

    const auto absolute = renegade::bridge::ResolveDependencyPath(
        root.string(), fs::absolute(root / "Content/Textures/Stone.png").string());
    if (absolute.accepted)
        return Fail("absolute dependency path was accepted");

#if defined(_WIN32)
    const auto invalidUtf8 = renegade::bridge::ResolveDependencyPath(
        root.generic_u8string(), std::string("Content/Models/\xC3\x28.glb", 21));
    if (invalidUtf8.accepted || invalidUtf8.error.find("valid UTF-8") ==
            std::string::npos)
        return Fail("invalid UTF-8 dependency path was not rejected");
#endif

    renegade::bridge::DependencyPathRegistry registry;
    const auto first = registry.Register(
        "scene:startup", "Content/Textures/Stone.png");
    if (!first.inserted || !first.diagnostics.empty())
        return Fail("first canonical dependency path was not registered");

    const auto duplicate = registry.Register(
        "scene:secondary", "Content/Textures/Stone.png");
    if (duplicate.inserted || duplicate.diagnostics.size() != 1 ||
        duplicate.existingCanonicalRelativePath !=
            "Content/Textures/Stone.png" ||
        duplicate.diagnostics.front().code !=
            renegade::bridge::DependencyDiagnosticCode::Duplicate)
        return Fail("exact duplicate dependency was not diagnosed");

    const auto caseCollision = registry.Register(
        "scene:secondary", "Content/Textures/stone.png");
    if (caseCollision.inserted || caseCollision.diagnostics.size() != 1 ||
        caseCollision.existingCanonicalRelativePath !=
            "Content/Textures/Stone.png" ||
        caseCollision.diagnostics.front().code !=
            renegade::bridge::DependencyDiagnosticCode::CaseCollision)
        return Fail("case-only dependency collision was not diagnosed");

#if defined(_WIN32)
    renegade::bridge::DependencyPathRegistry unicodeRegistry;
    const auto unicodeFirst = unicodeRegistry.Register(
        "scene:startup", unicodeModelUpper);
    const auto unicodeCollision = unicodeRegistry.Register(
        "scene:secondary", unicodeModelLower);
    if (!unicodeFirst.inserted || unicodeCollision.inserted ||
        unicodeCollision.existingCanonicalRelativePath != unicodeModelUpper ||
        unicodeCollision.diagnostics.size() != 1 ||
        unicodeCollision.diagnostics.front().code !=
            renegade::bridge::DependencyDiagnosticCode::CaseCollision)
        return Fail("Unicode case-only dependency collision was not diagnosed");
#endif

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

    PairProvider duplicateProvider("fixture-duplicate",
        "Content/Scenes/Startup.wiscene",
        "Content/Scenes/Startup.wiscene");
    renegade::bridge::DependencyCollector duplicateCollector(root.string());
    if (!duplicateCollector.RegisterProvider(duplicateProvider, collectorError) ||
        !duplicateCollector.AddRoot({"representative.renegade",
            renegade::bridge::DependencyClass::ProjectDocument,
            renegade::bridge::DependencyRequirement::Required,
            "fixture.project"}, collectorError) ||
        !duplicateCollector.DiscoverRootDependencies(collectorError))
        return Fail("duplicate collector fixture failed");
    const auto& duplicateGraph = duplicateCollector.Graph();
    if (duplicateGraph.nodes.size() != 2 || duplicateGraph.edges.size() != 2 ||
        duplicateGraph.diagnostics.size() != 1 ||
        duplicateGraph.diagnostics.front().code !=
            renegade::bridge::DependencyDiagnosticCode::Duplicate ||
        duplicateGraph.edges[1].targetId != duplicateGraph.nodes[1].id)
        return Fail("exact duplicate did not reuse the existing graph node");

    PairProvider missingCaseProvider("fixture-missing-case",
        "Content/Audio/missing.ogg", "Content/Audio/MISSING.ogg");
    renegade::bridge::DependencyCollector missingCaseCollector(root.string());
    if (!missingCaseCollector.RegisterProvider(
            missingCaseProvider, collectorError) ||
        !missingCaseCollector.AddRoot({"representative.renegade",
            renegade::bridge::DependencyClass::ProjectDocument,
            renegade::bridge::DependencyRequirement::Required,
            "fixture.project"}, collectorError) ||
        !missingCaseCollector.DiscoverRootDependencies(collectorError))
        return Fail("missing case-collision collector fixture failed");
    const auto& missingCaseGraph = missingCaseCollector.Graph();
    if (missingCaseGraph.nodes.size() != 2 ||
        missingCaseGraph.edges.size() != 2 ||
        missingCaseGraph.diagnostics.size() != 2 ||
        missingCaseGraph.diagnostics[0].code !=
            renegade::bridge::DependencyDiagnosticCode::Missing ||
        missingCaseGraph.diagnostics[1].code !=
            renegade::bridge::DependencyDiagnosticCode::CaseCollision ||
        missingCaseGraph.edges[1].targetId != missingCaseGraph.nodes[1].id)
        return Fail("missing case collision did not reuse its graph node");

#if defined(_WIN32)
    PairProvider unicodeProvider(
        "fixture-unicode-case", unicodeModelUpper, unicodeModelLower);
    renegade::bridge::DependencyCollector unicodeCollector(
        root.generic_u8string());
    if (!unicodeCollector.RegisterProvider(unicodeProvider, collectorError) ||
        !unicodeCollector.AddRoot({"representative.renegade",
            renegade::bridge::DependencyClass::ProjectDocument,
            renegade::bridge::DependencyRequirement::Required,
            "fixture.project"}, collectorError) ||
        !unicodeCollector.DiscoverRootDependencies(collectorError))
        return Fail("Unicode case-collision collector fixture failed");
    const auto& unicodeGraph = unicodeCollector.Graph();
    if (unicodeGraph.nodes.size() != 2 || unicodeGraph.edges.size() != 2 ||
        unicodeGraph.diagnostics.size() != 1 ||
        unicodeGraph.diagnostics.front().code !=
            renegade::bridge::DependencyDiagnosticCode::CaseCollision ||
        unicodeGraph.edges[1].targetId != unicodeGraph.nodes[1].id)
        return Fail("Unicode case collision did not reuse its graph node");
#endif

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
        !failingCollector.Graph().edges.empty() ||
        !failingCollector.Graph().diagnostics.empty())
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

    const std::string luaBytesBefore =
        ReadBytes(root / "Content/Scripts/main.lua");
    LuaDependencyPolicyProvider luaProvider({
        {"Content/Scripts/main.lua",
            {"../outside.lua", DependencyClass::Script,
                DependencyRequirement::Required,
                "lua.declared_dependency.outside", false}},
        {"Content/Scripts/main.lua",
            {"Content/Scripts/Alpha.lua", DependencyClass::Script,
                DependencyRequirement::Required,
                "lua.declared_dependency.alpha", false}},
        {"Content/Scripts/main.lua",
            {"Content/Scripts/alpha.lua", DependencyClass::Script,
                DependencyRequirement::Required,
                "lua.declared_dependency.case_collision", false}},
        {"Content/Scripts/main.lua",
            {"Content/Scripts/missing.lua", DependencyClass::Script,
                DependencyRequirement::Required,
                "lua.declared_dependency.missing", false}},
        {"Content/Scripts/main.lua",
            {"Content/Scripts/shared.lua", DependencyClass::Script,
                DependencyRequirement::Required,
                "lua.declared_dependency.shared[0]", false}},
        {"Content/Scripts/main.lua",
            {"Content/Scripts/shared.lua", DependencyClass::Script,
                DependencyRequirement::Required,
                "lua.declared_dependency.shared[1]", false}},
    }, {
        {"Content/Scripts/main.lua", "script_root .. next_script",
            "lua.dofile[0]"},
    });
    DependencyCollector luaCollector(root.string());
    if (!luaCollector.RegisterProvider(luaProvider, collectorError) ||
        !luaCollector.AddRoot({"Content/Scripts/main.lua",
            DependencyClass::Script, DependencyRequirement::Required,
            "fixture.lua"}, collectorError) ||
        !luaCollector.DiscoverRootDependencies(collectorError))
        return Fail("Lua policy provider failed");

    const auto& luaGraph = luaCollector.Graph();
    const std::vector<DependencyDiagnosticCode> expectedLuaDiagnostics = {
        DependencyDiagnosticCode::OutsideProject,
        DependencyDiagnosticCode::CaseCollision,
        DependencyDiagnosticCode::Missing,
        DependencyDiagnosticCode::Duplicate,
        DependencyDiagnosticCode::UndeclaredComputedReference,
    };
    std::vector<DependencyDiagnosticCode> actualLuaDiagnostics;
    for (const auto& diagnostic : luaGraph.diagnostics)
        actualLuaDiagnostics.push_back(diagnostic.code);
    if (luaGraph.nodes.size() != 4 || luaGraph.edges.size() != 5 ||
        actualLuaDiagnostics != expectedLuaDiagnostics ||
        luaGraph.diagnostics.back().path != "script_root .. next_script" ||
        luaGraph.diagnostics.back().sourceId != luaGraph.rootIds.front() ||
        luaGraph.edges[0].targetId != luaGraph.edges[1].targetId ||
        luaGraph.edges[3].targetId != luaGraph.edges[4].targetId)
    {
        return Fail("Lua policy did not produce deterministic complete diagnostics");
    }
    if (std::any_of(luaGraph.nodes.begin(), luaGraph.nodes.end(),
            [](const DependencyNode& node)
            {
                return node.projectRelativePath ==
                    "Content/Textures/not-a-dependency.png";
            }) ||
        ReadBytes(root / "Content/Scripts/main.lua") != luaBytesBefore)
    {
        return Fail("Lua policy scanned or modified source text");
    }

    const fs::path gate4Scene = root / "Content/Scenes/Gate4.wiscene";
    wi::scene::Scene directGate4Scene;
    PopulateGate4Scene(directGate4Scene, gate4Scene, true);
    WisceneDependencyDocument directWalk;
    InspectWisceneDependencies(directGate4Scene, directWalk);
    if (directWalk.references.size() != 13 ||
        directWalk.references[5].provenance !=
            "wiscene.environment_probe[0].texture")
        return Fail("const WISCENE walker omitted a native component field");

    if (!WriteGate4Scene(gate4Scene))
        return Fail("Gate 4 WISCENE fixture could not be serialized");
    const std::string gate4BytesBefore = ReadBytes(gate4Scene);
    if (gate4BytesBefore.empty())
        return Fail("Gate 4 WISCENE fixture was empty");

    const auto gate4Reader = MakeWisceneDependencyReader();
    WisceneDependencyDocument firstRead;
    WisceneDependencyDocument secondRead;
    std::string gate4Error;
    if (!gate4Reader(gate4Scene.generic_u8string(), firstRead, gate4Error) ||
        !gate4Reader(gate4Scene.generic_u8string(), secondRead, gate4Error))
        return Fail("production WISCENE dependency reader rejected its fixture");
    if (firstRead.references.size() != 12 ||
        secondRead.references.size() != firstRead.references.size())
        return Fail("WISCENE walker did not emit the expected typed fields");
    for (std::size_t index = 0; index < firstRead.references.size(); ++index)
    {
        const auto& firstReference = firstRead.references[index];
        const auto& secondReference = secondRead.references[index];
        if (firstReference.declaredPath != secondReference.declaredPath ||
            firstReference.dependencyClass != secondReference.dependencyClass ||
            firstReference.requirement != secondReference.requirement ||
            firstReference.provenance != secondReference.provenance)
            return Fail("repeated WISCENE walks changed logical output order");
    }
    if (firstRead.references.front().provenance !=
            "wiscene.material[0].texture.base_color" ||
        firstRead.references[9].dependencyClass != DependencyClass::Audio ||
        firstRead.references[10].dependencyClass != DependencyClass::Video ||
        firstRead.references[11].dependencyClass != DependencyClass::Script)
        return Fail("WISCENE walker lost component field provenance or type");
    for (const auto& reference : firstRead.references)
        if (reference.declaredPath.find("not-a-dependency.png") !=
                std::string::npos)
            return Fail("path-looking metadata became a dependency edge");

    WisceneDependencyProvider wisceneProvider(gate4Reader);
    DependencyCollector wisceneCollector(root.generic_u8string());
    if (!wisceneCollector.RegisterProvider(wisceneProvider, gate4Error) ||
        !wisceneCollector.AddRoot({"Content/Scenes/Gate4.wiscene",
            DependencyClass::Scene, DependencyRequirement::Required,
            "fixture.wiscene"}, gate4Error) ||
        !wisceneCollector.DiscoverRootDependencies(gate4Error))
        return Fail("Gate 4 WISCENE provider failed");
    const auto& wisceneGraph = wisceneCollector.Graph();
    const auto missingCount = std::count_if(
        wisceneGraph.diagnostics.begin(), wisceneGraph.diagnostics.end(),
        [](const DependencyDiagnostic& diagnostic)
        {
            return diagnostic.code == DependencyDiagnosticCode::Missing;
        });
    const auto duplicateCount = std::count_if(
        wisceneGraph.diagnostics.begin(), wisceneGraph.diagnostics.end(),
        [](const DependencyDiagnostic& diagnostic)
        {
            return diagnostic.code == DependencyDiagnosticCode::Duplicate;
        });
    if (wisceneGraph.nodes.size() != 11 ||
        wisceneGraph.edges.size() != 12 ||
        wisceneGraph.diagnostics.size() != 11 ||
        missingCount != 9 || duplicateCount != 2 ||
        wisceneGraph.nodes[1].provider != "wiscene" ||
        wisceneGraph.nodes[1].providerVersion != 1)
        return Fail("WISCENE provider graph, diagnostics or ownership was incomplete");
    if (wisceneGraph.edges[2].targetId != wisceneGraph.nodes[1].id ||
        wisceneGraph.edges[4].targetId != wisceneGraph.nodes[1].id)
        return Fail("duplicate native component references did not reuse one node");
    if (ReadBytes(gate4Scene) != gate4BytesBefore)
        return Fail("dependency extraction modified the authoritative WISCENE");

    // Gate 5 Terrain/generated data: terrain materials are ordinary scene
    // MaterialComponents and therefore use the Gate 4 typed walker. Sculpted
    // height samples and authored blend maps are different: Wicked embeds
    // them in the Terrain payload inside WISCENE, so they must be recorded as
    // embedded generated-data evidence rather than fabricated file nodes.
    // The fixture also retains the real bundled-default edge case: an
    // install-anchored material path outside the project must be diagnosed
    // and excluded, while project-owned terrain material files are found.
    {
        const fs::path terrainScene = root / "Content/Scenes/Gate5Terrain.wiscene";
        wi::scene::Scene terrainWorld;
        const auto terrainMaterialEntity = wi::ecs::CreateEntity();
        auto& terrainMaterial =
            terrainWorld.materials.Create(terrainMaterialEntity);
        const fs::path baseColor =
            root / "Content/terrain/default_grass/default_grass_basecolor.tga";
        const fs::path surface =
            root / "Content/terrain/default_grass/default_grass_surface.tga";
        fs::create_directories(baseColor.parent_path());
        std::ofstream(baseColor) << "base";
        std::ofstream(surface) << "surface";
        terrainMaterial.textures[wi::scene::MaterialComponent::BASECOLORMAP]
            .name = baseColor.generic_u8string();
        terrainMaterial.textures[wi::scene::MaterialComponent::SURFACEMAP]
            .name = surface.generic_u8string();
        const std::string runtimeAbsoluteGrassPath =
            (outside / "Content/terrain/default_grass/default_grass_normal.tga")
                .generic_u8string();
        terrainMaterial.textures[wi::scene::MaterialComponent::NORMALMAP]
            .name = runtimeAbsoluteGrassPath;
        auto& terrain = terrainWorld.terrains.Create(wi::ecs::CreateEntity());
        terrain.materialEntities.push_back(terrainMaterialEntity);
        wi::terrain::Chunk generatedChunk;
        generatedChunk.x = 2;
        generatedChunk.z = -1;
        auto& generated = terrain.chunks[generatedChunk];
        generated.heightmap_data = {0, 8192, 32768, 65535};
        generated.blendmap_layers.emplace_back().pixels = {0, 64, 128, 255};
        wi::terrain::Chunk earlierChunk;
        earlierChunk.x = -3;
        earlierChunk.z = 4;
        terrain.chunks[earlierChunk].heightmap_data = {100, 200};
        auto heightmapModifier =
            wi::allocator::make_shared_single<wi::terrain::HeightmapModifier>();
        heightmapModifier->data = {10, 20, 30, 40};
        heightmapModifier->width = 2;
        heightmapModifier->height = 2;
        terrain.modifiers.push_back(heightmapModifier);

        WisceneDependencyDocument directTerrainWalk;
        InspectWisceneDependencies(terrainWorld, directTerrainWalk);
        if (directTerrainWalk.embeddedGeneratedData.size() != 4 ||
            directTerrainWalk.embeddedGeneratedData[0].provenance !=
                "wiscene.terrain[0].chunk[-3,4].heightmap" ||
            directTerrainWalk.embeddedGeneratedData[0].byteCount != 4 ||
            directTerrainWalk.embeddedGeneratedData[1].byteCount != 8 ||
            directTerrainWalk.embeddedGeneratedData[2].byteCount != 4 ||
            directTerrainWalk.embeddedGeneratedData[3].provenance !=
                "wiscene.terrain[0].modifier[0].heightmap" ||
            directTerrainWalk.embeddedGeneratedData[3].byteCount != 4)
            return Fail("terrain walker did not record deterministic embedded "
                "height/blend generated data");

        wi::Archive terrainArchive(terrainScene.generic_u8string(), false, false);
        if (!terrainArchive.IsOpen())
            return Fail("Gate 5 terrain fixture archive could not be opened");
        terrainWorld.Serialize(terrainArchive);
        if (!terrainArchive.SaveFile(terrainScene.generic_u8string()))
            return Fail("Gate 5 terrain fixture could not be serialized");
        terrainArchive = wi::Archive();

        const std::string terrainBytesBefore = ReadBytes(terrainScene);
        WisceneDependencyDocument serializedTerrainRead;
        WisceneDependencyDocument serializedTerrainReadAgain;
        std::string terrainReadError;
        if (!gate4Reader(terrainScene.generic_u8string(),
                serializedTerrainRead, terrainReadError) ||
            !gate4Reader(terrainScene.generic_u8string(),
                serializedTerrainReadAgain, terrainReadError) ||
            serializedTerrainRead.embeddedGeneratedData.size() != 4 ||
            serializedTerrainReadAgain.embeddedGeneratedData.size() != 4)
            return Fail(("serialized terrain generated data was not observed: " +
                terrainReadError).c_str());
        for (std::size_t index = 0;
            index < serializedTerrainRead.embeddedGeneratedData.size(); ++index)
        {
            const auto& first = serializedTerrainRead.embeddedGeneratedData[index];
            const auto& second =
                serializedTerrainReadAgain.embeddedGeneratedData[index];
            if (first.provenance != second.provenance ||
                first.byteCount != second.byteCount)
                return Fail("serialized terrain generated-data order changed "
                    "between reads");
        }
        if (ReadBytes(terrainScene) != terrainBytesBefore)
            return Fail("terrain dependency inspection modified the WISCENE");

        std::string terrainError;
        DependencyCollector terrainCollector(root.generic_u8string());
        if (!terrainCollector.RegisterProvider(wisceneProvider, terrainError) ||
            !terrainCollector.AddRoot({"Content/Scenes/Gate5Terrain.wiscene",
                DependencyClass::Scene, DependencyRequirement::Required,
                "fixture.terrain"}, terrainError) ||
            !terrainCollector.DiscoverRootDependencies(terrainError))
            return Fail(("Gate 5 terrain provider failed: " + terrainError).c_str());

        const auto& terrainGraph = terrainCollector.Graph();
        const auto outsideProjectCount = std::count_if(
            terrainGraph.diagnostics.begin(), terrainGraph.diagnostics.end(),
            [](const DependencyDiagnostic& diagnostic)
            {
                return diagnostic.code == DependencyDiagnosticCode::OutsideProject;
            });
        if (outsideProjectCount == 0)
            return Fail("runtime-absolute terrain material path was not "
                "diagnosed as outside the project");
        // Scene root plus the two project-owned terrain textures. Embedded
        // generated data stays owned by the scene; the rejected external
        // runtime texture must not become a graph node.
        if (terrainGraph.nodes.size() != 3 || terrainGraph.edges.size() != 2)
            return Fail("terrain material closure or embedded-data boundary "
                "was incorrect");
    }

    // Gate 5 Imported content: a raw, un-imported .gltf/.glb source file
    // declaring its own external buffer/image dependencies. This is
    // distinct from ImportService, which bakes GLTF into an embedded scene
    // and never persists the source's own external paths -- see
    // GltfDependencyDocument's declaration for the full reasoning.
    {
        const auto gltfReader = MakeGltfDependencyReader();

        // -- ASCII (.gltf): representative imported content with an external
        // BIN used by mesh/animation accessors, multiple material texture
        // slots, an external image with a percent-encoded space, embedded
        // bufferView/data-URI images, and a missing external buffer. The
        // dependency closure is still exactly buffers/images; the material
        // and animation structure proves those external resources are used by
        // the content required by LP05 rather than being bare URI examples.
        const fs::path gltfPath = root / "Content/Models/Prop.gltf";
        {
            std::ofstream gltfFile(gltfPath);
            gltfFile << R"({
  "asset": { "version": "2.0" },
  "buffers": [
    { "uri": "Prop.bin", "byteLength": 132 },
    { "uri": "Missing.bin", "byteLength": 16 }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
    { "buffer": 0, "byteOffset": 36, "byteLength": 12 },
    { "buffer": 0, "byteOffset": 48, "byteLength": 48 },
    { "buffer": 0, "byteOffset": 96, "byteLength": 36 }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3" },
    { "bufferView": 1, "componentType": 5126, "count": 3, "type": "SCALAR" },
    { "bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC4" }
  ],
  "images": [
    { "uri": "albedo%20base.png" },
    { "bufferView": 3, "mimeType": "image/png" },
    { "uri": "DATA:image/png;base64,AAAA" }
  ],
  "textures": [
    { "source": 0 }, { "source": 1 }, { "source": 2 }
  ],
  "materials": [{
    "pbrMetallicRoughness": {
      "baseColorTexture": { "index": 0 },
      "metallicRoughnessTexture": { "index": 1 }
    },
    "normalTexture": { "index": 2 }
  }],
  "nodes": [{ "mesh": 0 }],
  "meshes": [{ "primitives": [{
    "attributes": { "POSITION": 0 }, "material": 0
  }] }],
  "animations": [{
    "samplers": [{ "input": 1, "output": 2, "interpolation": "LINEAR" }],
    "channels": [{
      "sampler": 0, "target": { "node": 0, "path": "rotation" }
    }]
  }],
  "scenes": [{ "nodes": [0] }],
  "scene": 0
})";
        }
        std::ofstream(root / "Content/Models/Prop.bin", std::ios::binary)
            << std::string(132, 'x');
        std::ofstream(root / "Content/Models/albedo base.png") << "fixture";
        // "Missing.bin" is intentionally never created.

        GltfDependencyDocument directRead;
        std::string gltfReadError;
        if (!gltfReader(gltfPath.generic_u8string(), directRead, gltfReadError))
            return Fail(("glTF reader rejected a well-formed fixture: " +
                gltfReadError).c_str());
        if (directRead.references.size() != 3)
            return Fail("glTF reader did not emit exactly the three "
                "external references (one missing buffer, one present "
                "buffer, one present image)");
        if (directRead.materialTextureSlotCount != 3 ||
            directRead.animationCount != 1)
            return Fail("glTF reader did not retain representative material "
                "texture-slot and animation evidence");
        const bool sawDecodedImagePath = std::any_of(
            directRead.references.begin(), directRead.references.end(),
            [](const DependencyCandidate& candidate)
            {
                return candidate.declaredPath.find("albedo base.png") !=
                    std::string::npos;
            });
        if (!sawDecodedImagePath)
            return Fail("glTF reader did not percent-decode an image URI");
        for (const auto& candidate : directRead.references)
            if (candidate.declaredPath.find("data:") != std::string::npos)
                return Fail("glTF reader emitted a data-URI as a file dependency");

        std::string gltfError;
        GltfDependencyProvider gltfProvider(gltfReader);
        DependencyCollector gltfCollector(root.generic_u8string());
        if (!gltfCollector.RegisterProvider(gltfProvider, gltfError) ||
            !gltfCollector.AddRoot({"Content/Models/Prop.gltf",
                DependencyClass::ImportedContent, DependencyRequirement::Required,
                "fixture.gltf"}, gltfError) ||
            !gltfCollector.DiscoverRootDependencies(gltfError))
            return Fail(("Gate 5 glTF provider failed: " + gltfError).c_str());

        const auto& gltfGraph = gltfCollector.Graph();
        // Root + present buffer + present image = 3 nodes. The missing
        // buffer still becomes a node (declared dependencies that don't
        // exist are still real graph nodes, per every other provider's
        // established behaviour), so 4 nodes total.
        if (gltfGraph.nodes.size() != 4)
            return Fail("glTF provider graph did not contain the expected "
                "node count");
        const auto missingBufferCount = std::count_if(
            gltfGraph.diagnostics.begin(), gltfGraph.diagnostics.end(),
            [](const DependencyDiagnostic& diagnostic)
            {
                return diagnostic.code == DependencyDiagnosticCode::Missing &&
                    diagnostic.path.find("Missing.bin") != std::string::npos;
            });
        if (missingBufferCount != 1)
            return Fail("glTF provider did not diagnose the missing "
                "external buffer as Missing rather than failing outright");
        const auto importedContentCount = std::count_if(
            gltfGraph.nodes.begin(), gltfGraph.nodes.end(),
            [](const DependencyNode& node)
            {
                return node.dependencyClass == DependencyClass::ImportedContent;
            });
        // The root itself plus the two declared buffers are all
        // ImportedContent; the image is Texture.
        if (importedContentCount != 3)
            return Fail("glTF buffer references were not classified as "
                "ImportedContent");

        // The class also covers other Phase 4 import formats. A registered
        // glTF provider must ignore those roots instead of trying to parse
        // an FBX/OBJ/PLY file as JSON and failing the whole collection.
        std::ofstream(root / "Content/Models/Other.fbx", std::ios::binary)
            << "not-gltf";
        DependencyCollector otherFormatCollector(root.generic_u8string());
        if (!otherFormatCollector.RegisterProvider(gltfProvider, gltfError) ||
            !otherFormatCollector.AddRoot({"Content/Models/Other.fbx",
                DependencyClass::ImportedContent, DependencyRequirement::Required,
                "fixture.fbx"}, gltfError) ||
            !otherFormatCollector.DiscoverRootDependencies(gltfError) ||
            otherFormatCollector.Graph().nodes.size() != 1 ||
            !otherFormatCollector.Graph().edges.empty())
            return Fail("glTF provider claimed a non-glTF ImportedContent root");

        // -- GLB (binary container): the same JSON content, wrapped in a
        // minimal valid GLB chunk container, to prove the header/chunk
        // unwrapping path also works, not just the ASCII path.
        const std::string glbJsonText =
            R"({"asset":{"version":"2.0"},)"
            R"("buffers":[{"uri":"Prop.bin","byteLength":4}],)"
            R"("images":[{"uri":"albedo%20base.png"}]})";
        std::string paddedJson = glbJsonText;
        while (paddedJson.size() % 4 != 0)
            paddedJson.push_back(' ');

        const fs::path glbPath = root / "Content/Models/Prop.glb";
        {
            std::ofstream glbFile(glbPath, std::ios::binary);
            const auto writeUint32 = [&glbFile](const std::uint32_t value)
            {
                const unsigned char bytes[4] = {
                    static_cast<unsigned char>(value & 0xFF),
                    static_cast<unsigned char>((value >> 8) & 0xFF),
                    static_cast<unsigned char>((value >> 16) & 0xFF),
                    static_cast<unsigned char>((value >> 24) & 0xFF),
                };
                glbFile.write(reinterpret_cast<const char*>(bytes), 4);
            };
            const std::uint32_t totalLength = static_cast<std::uint32_t>(
                12 + 8 + paddedJson.size());
            glbFile.write("glTF", 4);
            writeUint32(2);
            writeUint32(totalLength);
            writeUint32(static_cast<std::uint32_t>(paddedJson.size()));
            glbFile.write("JSON", 4);
            glbFile.write(paddedJson.data(),
                static_cast<std::streamsize>(paddedJson.size()));
        }

        GltfDependencyDocument glbRead;
        std::string glbReadError;
        if (!gltfReader(glbPath.generic_u8string(), glbRead, glbReadError))
            return Fail(("glTF reader rejected a well-formed GLB fixture: " +
                glbReadError).c_str());
        if (glbRead.references.size() != 2)
            return Fail("GLB reader did not emit the expected two "
                "references from the unwrapped JSON chunk");

        const fs::path badGlbPath = root / "Content/Models/BadLength.glb";
        {
            std::ofstream badGlb(badGlbPath, std::ios::binary);
            const auto writeUint32 = [&badGlb](const std::uint32_t value)
            {
                const unsigned char bytes[4] = {
                    static_cast<unsigned char>(value & 0xFF),
                    static_cast<unsigned char>((value >> 8) & 0xFF),
                    static_cast<unsigned char>((value >> 16) & 0xFF),
                    static_cast<unsigned char>((value >> 24) & 0xFF),
                };
                badGlb.write(reinterpret_cast<const char*>(bytes), 4);
            };
            const std::uint32_t actualLength = static_cast<std::uint32_t>(
                12 + 8 + paddedJson.size());
            badGlb.write("glTF", 4);
            writeUint32(2);
            writeUint32(actualLength + 4); // Deliberately inconsistent.
            writeUint32(static_cast<std::uint32_t>(paddedJson.size()));
            badGlb.write("JSON", 4);
            badGlb.write(paddedJson.data(),
                static_cast<std::streamsize>(paddedJson.size()));
        }
        GltfDependencyDocument rejectedGlb;
        if (gltfReader(badGlbPath.generic_u8string(),
                rejectedGlb, glbReadError))
            return Fail("GLB reader accepted an inconsistent declared length");
    }

    fs::remove_all(root, ignored);
    fs::remove_all(outside, ignored);
    std::cout << "RenegadeDependencyTests passed\n";
    return 0;
}
