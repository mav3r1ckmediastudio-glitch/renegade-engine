#include "renegade/bridge/DependencyService.h"

#include <WickedEngine.h>

#include <algorithm>
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
    std::ofstream(root / "Content/Scripts/main.lua") << "return true\n";
    std::ofstream(root / "Content/Scripts/shared.lua") << "fixture";
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

    // Gate 5 Terrain: default terrain materials are ordinary
    // MaterialComponents (see TerrainService::ConfigureDefaultGrassMaterial),
    // already walked by WalkWisceneDependencies above -- no new provider
    // code exists or is needed for terrain. The one real edge case is that
    // their texture paths are built at runtime as
    // wi::helper::GetCurrentPath() + "/Content/terrain/...", an absolute,
    // install-anchored path that was never an authored project-relative
    // declaration. This proves ProjectRelativeWisceneCandidate and
    // ResolveDependencyPath already classify that correctly as
    // OutsideProject rather than silently mishandling it, and that a pure
    // in-memory generated terrain (no material at all) produces zero
    // dependency edges, since there is nothing on disk to discover.
    {
        const fs::path terrainScene = root / "Content/Scenes/Gate5Terrain.wiscene";
        wi::scene::Scene terrainWorld;
        auto& terrainMaterial =
            terrainWorld.materials.Create(wi::ecs::CreateEntity());
        const std::string runtimeAbsoluteGrassPath =
            wi::helper::GetCurrentPath() +
            "/Content/terrain/default_grass/default_grass_basecolor.tga";
        terrainMaterial.textures[wi::scene::MaterialComponent::BASECOLORMAP]
            .name = runtimeAbsoluteGrassPath;
        terrainWorld.terrains.Create(wi::ecs::CreateEntity());

        wi::Archive terrainArchive(terrainScene.generic_u8string(), false, false);
        if (!terrainArchive.IsOpen())
            return Fail("Gate 5 terrain fixture archive could not be opened");
        terrainWorld.Serialize(terrainArchive);
        if (!terrainArchive.SaveFile(terrainScene.generic_u8string()))
            return Fail("Gate 5 terrain fixture could not be serialized");
        terrainArchive = wi::Archive();

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
        {
            std::cerr << "DEBUG runtimeAbsoluteGrassPath=" << runtimeAbsoluteGrassPath << '\n';
            std::cerr << "DEBUG diagnostics.size()=" << terrainGraph.diagnostics.size() << '\n';
            for (const auto& diagnostic : terrainGraph.diagnostics)
                std::cerr << "DEBUG diag code=" << static_cast<int>(diagnostic.code)
                    << " path=" << diagnostic.path
                    << " message=" << diagnostic.message << '\n';
            std::cerr << "DEBUG nodes.size()=" << terrainGraph.nodes.size() << '\n';
            for (const auto& node : terrainGraph.nodes)
                std::cerr << "DEBUG node path=" << node.projectRelativePath << '\n';
            return Fail("runtime-absolute terrain material path was not "
                "diagnosed as outside the project");
        }
        // Only the scene root node itself; the rejected absolute texture
        // path must never have become a graph node.
        if (terrainGraph.nodes.size() != 1)
            return Fail("an outside-project terrain path incorrectly "
                "became a dependency node");
    }

    fs::remove_all(root, ignored);
    fs::remove_all(outside, ignored);
    std::cout << "RenegadeDependencyTests passed\n";
    return 0;
}
