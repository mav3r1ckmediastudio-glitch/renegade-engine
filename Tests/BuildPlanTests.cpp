#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "renegade/bridge/BuildService.h"

namespace
{
    using namespace renegade::bridge;

    constexpr const char* ProjectId =
        "11111111-1111-4111-8111-111111111111";
    constexpr const char* SaveDataId =
        "22222222-2222-4222-8222-222222222222";

    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    DependencyNode Node(
        std::string id,
        std::string path,
        const DependencyClass dependencyClass,
        const DependencyRequirement requirement,
        std::string hash,
        const bool runtimeSupport = false)
    {
        DependencyNode node;
        node.id = std::move(id);
        node.projectRelativePath = std::move(path);
        node.dependencyClass = dependencyClass;
        node.requirement = requirement;
        node.provider = runtimeSupport ? "lp06-runtime-support" : "lp06-fixture";
        node.providerVersion = 1;
        node.contentHash = std::move(hash);
        node.runtimeSupport = runtimeSupport;
        return node;
    }

    DependencyGraph RepresentativeGraph()
    {
        DependencyGraph graph;
        graph.rootIds = {"project", "source", "runtime"};
        graph.nodes = {
            Node("project", "ProofGame.renegade",
                DependencyClass::ProjectDocument,
                DependencyRequirement::Required,
                "fnv1a64:1000000000000001"),
            Node("screen", "Content/UI/Main.renegade-screen",
                DependencyClass::RuntimeScreenDocument,
                DependencyRequirement::Required,
                "fnv1a64:2000000000000002"),
            Node("level-one", "Content/Scenes/LevelOne.wiscene",
                DependencyClass::Scene,
                DependencyRequirement::Required,
                "fnv1a64:3000000000000003"),
            Node("level-two", "Content/Scenes/LevelTwo.wiscene",
                DependencyClass::Scene,
                DependencyRequirement::Required,
                "fnv1a64:4000000000000004"),
            Node("product", "Content/Imported/model.wiscene",
                DependencyClass::ImportedContent,
                DependencyRequirement::Required,
                "fnv1a64:5000000000000005"),
            Node("source", "Content/Source/model.glb",
                DependencyClass::ImportedContent,
                DependencyRequirement::EditorOnly,
                "fnv1a64:6000000000000006"),
            Node("unused", "Content/Unused/unused.png",
                DependencyClass::Texture,
                DependencyRequirement::Required,
                "fnv1a64:7000000000000007"),
            Node("runtime", "Runtime/RenegadeRuntime.exe",
                DependencyClass::RuntimeSupport,
                DependencyRequirement::Required,
                "fnv1a64:8000000000000008",
                true),
        };
        graph.edges = {
            {"project", "screen", "project.startup_screen"},
            {"screen", "level-one", "screen.play.level_one"},
            {"screen", "level-two", "screen.play.level_two"},
            {"level-one", "product", "wiscene.imported_product"},
        };
        return graph;
    }

    std::vector<StableId> StableIds()
    {
        return {
            "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
            "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb",
            "cccccccc-cccc-4ccc-8ccc-cccccccccccc",
            "dddddddd-dddd-4ddd-8ddd-dddddddddddd",
            "eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee",
            "ffffffff-ffff-4fff-8fff-ffffffffffff",
            "12345678-1234-4234-8234-123456789abc",
            "87654321-4321-4321-8321-cba987654321",
            "13572468-2468-4246-8246-135724681357",
            "24681357-1357-4135-8135-246813572468",
        };
    }

    bool MakeRegistry(
        const DependencyGraph& graph,
        AssetRegistry& registry,
        std::string& error)
    {
        const auto ids = StableIds();
        std::size_t index = 0;
        AssetRegistryRefresh refresh;
        if (!RefreshAssetRegistry(
                ProjectId,
                graph,
                nullptr,
                refresh,
                error,
                [&ids, &index]
                {
                    return ids.at(index++);
                }))
        {
            return false;
        }
        registry = std::move(refresh.registry);
        return true;
    }

    AssetRecord* FindRecord(AssetRegistry& registry, const std::string& path)
    {
        const auto found = std::find_if(
            registry.records.begin(),
            registry.records.end(),
            [&path](const AssetRecord& record)
            {
                return record.projectRelativePath == path;
            });
        return found == registry.records.end() ? nullptr : &*found;
    }

    const WindowsGameBuildFile* FindPlanFile(
        const WindowsGameBuildPlan& plan,
        const std::string& destination)
    {
        const auto found = std::find_if(
            plan.files.begin(),
            plan.files.end(),
            [&destination](const WindowsGameBuildFile& file)
            {
                return file.destinationPath == destination;
            });
        return found == plan.files.end() ? nullptr : &*found;
    }

    ProjectMetadata Project()
    {
        ProjectMetadata project;
        project.projectId = ProjectId;
        project.name = "Proof Game";
        project.descriptorPath = "C:/proof/ProofGame.renegade";
        project.rootPath = "C:/proof";
        project.startupScene = "Content/Scenes/LevelOne.wiscene";
        project.startupScreenId =
            "99999999-9999-4999-8999-999999999999";
        project.startupScreen = "Content/UI/Main.renegade-screen";
        return project;
    }

    WindowsGameBuildRequest Request()
    {
        WindowsGameBuildRequest request;
        request.gameName = "Proof Game";
        request.executableBaseName = "ProofGame";
        request.publicVersion = "0.1.0-gate1";
        request.saveDataId = SaveDataId;
        return request;
    }

    std::vector<WindowsRuntimeSupportInput> Support()
    {
        return {
            {
                "renegade-runtime",
                "ProofGame.exe",
                123456,
                std::string(64, 'a'),
                "RenegadeRuntime@01d790b",
            },
            {
                "directx-shader-compiler",
                "dxcompiler.dll",
                654321,
                std::string(64, 'b'),
                "WickedEngine/dxcompiler.dll@3a800b7",
            },
        };
    }

    bool AddCurrentImportProvenance(AssetRegistry& registry, std::string& error)
    {
        AssetRecord* source = FindRecord(registry, "Content/Source/model.glb");
        AssetRecord* product = FindRecord(registry, "Content/Imported/model.wiscene");
        if (source == nullptr || product == nullptr)
        {
            error = "fixture registry did not contain source/product records";
            return false;
        }

        ImportedProductRecord imported;
        imported.sourceAssetId = source->assetId;
        imported.productAssetId = product->assetId;
        imported.importer = "renegade.gltf";
        imported.importerVersion = 1;
        imported.settingsSchema = "renegade.gltf.settings";
        imported.settingsVersion = 1;
        imported.settingsJson = "{\"scale_mode\":\"automatic\"}";
        imported.sourceContentHashAtImport = source->contentHash;
        imported.productContentHashAtImport = product->contentHash;
        return SetImportedProductRecords(registry, {imported}, error);
    }
}

int main()
{
    using namespace renegade::bridge;

    const ProjectMetadata project = Project();
    const WindowsGameBuildRequest request = Request();
    const auto support = Support();
    DependencyGraph graph = RepresentativeGraph();

    AssetRegistry registry;
    std::string error;
    if (!MakeRegistry(graph, registry, error) ||
        !AddCurrentImportProvenance(registry, error))
    {
        return Fail(error.c_str());
    }

    WindowsGameBuildPlan plan;
    if (!CreateWindowsGameBuildPlan(
            project, graph, registry, request, support, plan, error))
    {
        return Fail(error.c_str());
    }

    if (plan.projectId != ProjectId ||
        plan.gameName != "Proof Game" ||
        plan.executableFileName != "ProofGame.exe" ||
        plan.buildFolderName != "Proof Game Windows Build" ||
        plan.saveDataId != SaveDataId ||
        plan.platform != "windows-x64" || plan.configuration != "Release")
    {
        return Fail("Gate 1 plan did not retain the requested standalone identity");
    }

    if (plan.excludedEditorOnly != 1 ||
        plan.excludedUnreachable != 1 ||
        plan.excludedOptionalMissing != 0)
    {
        return Fail("Gate 1 plan exclusion counts were incorrect");
    }

    if (FindPlanFile(plan, "ProofGame.exe") == nullptr ||
        FindPlanFile(plan, "dxcompiler.dll") == nullptr ||
        FindPlanFile(plan, "GameData/ProofGame.renegade") == nullptr ||
        FindPlanFile(plan, "GameData/Content/UI/Main.renegade-screen") == nullptr ||
        FindPlanFile(plan, "GameData/Content/Scenes/LevelOne.wiscene") == nullptr ||
        FindPlanFile(plan, "GameData/Content/Scenes/LevelTwo.wiscene") == nullptr ||
        FindPlanFile(plan, "GameData/Content/Imported/model.wiscene") == nullptr)
    {
        return Fail("Gate 1 plan omitted required project or Runtime support files");
    }
    if (FindPlanFile(plan, "GameData/Content/Source/model.glb") != nullptr ||
        FindPlanFile(plan, "GameData/Content/Unused/unused.png") != nullptr ||
        FindPlanFile(plan, "GameData/Runtime/RenegadeRuntime.exe") != nullptr)
    {
        return Fail("Gate 1 plan included editor-only, unreachable or graph Runtime-support content");
    }

    std::string firstJson;
    if (!SerializeWindowsGameBuildPlan(plan, firstJson, error))
        return Fail(error.c_str());

    DependencyGraph reordered = graph;
    std::reverse(reordered.rootIds.begin(), reordered.rootIds.end());
    std::reverse(reordered.nodes.begin(), reordered.nodes.end());
    std::reverse(reordered.edges.begin(), reordered.edges.end());
    auto reorderedSupport = support;
    std::reverse(reorderedSupport.begin(), reorderedSupport.end());
    WindowsGameBuildPlan reorderedPlan;
    if (!CreateWindowsGameBuildPlan(
            project,
            reordered,
            registry,
            request,
            reorderedSupport,
            reorderedPlan,
            error))
    {
        return Fail(error.c_str());
    }
    std::string reorderedJson;
    if (!SerializeWindowsGameBuildPlan(reorderedPlan, reorderedJson, error) ||
        reorderedJson != firstJson)
    {
        return Fail("logical input order changed canonical Gate 1 plan bytes");
    }

    DependencyGraph requiredMissing = graph;
    auto requiredNode = std::find_if(
        requiredMissing.nodes.begin(),
        requiredMissing.nodes.end(),
        [](const DependencyNode& node) { return node.id == "level-two"; });
    requiredNode->contentHash = "missing";
    AssetRegistry requiredMissingRegistry;
    if (!MakeRegistry(requiredMissing, requiredMissingRegistry, error))
        return Fail(error.c_str());
    WindowsGameBuildPlan rejected;
    if (CreateWindowsGameBuildPlan(
            project,
            requiredMissing,
            requiredMissingRegistry,
            request,
            support,
            rejected,
            error))
    {
        return Fail("Gate 1 accepted missing required project content");
    }

    DependencyGraph optionalMissing = graph;
    optionalMissing.nodes.push_back(Node(
        "optional",
        "Content/Audio/optional.ogg",
        DependencyClass::Audio,
        DependencyRequirement::Optional,
        "missing"));
    optionalMissing.edges.push_back(
        {"level-two", "optional", "wiscene.sound[0].filename"});
    AssetRegistry optionalRegistry;
    if (!MakeRegistry(optionalMissing, optionalRegistry, error) ||
        !AddCurrentImportProvenance(optionalRegistry, error))
    {
        return Fail(error.c_str());
    }
    WindowsGameBuildPlan optionalPlan;
    if (!CreateWindowsGameBuildPlan(
            project,
            optionalMissing,
            optionalRegistry,
            request,
            support,
            optionalPlan,
            error) ||
        optionalPlan.excludedOptionalMissing != 1 ||
        FindPlanFile(optionalPlan, "GameData/Content/Audio/optional.ogg") != nullptr)
    {
        return Fail("Gate 1 did not exclude a declared optional missing dependency");
    }

    DependencyGraph outsideProject = graph;
    outsideProject.diagnostics.push_back({
        DependencyDiagnosticCode::OutsideProject,
        "level-one",
        "../external.png",
        "fixture external dependency",
    });
    if (CreateWindowsGameBuildPlan(
            project,
            outsideProject,
            registry,
            request,
            support,
            rejected,
            error))
    {
        return Fail("Gate 1 accepted an outside-project dependency diagnostic");
    }

    WindowsGameBuildRequest invalidName = request;
    invalidName.executableBaseName = "CON";
    if (CreateWindowsGameBuildPlan(
            project, graph, registry, invalidName, support, rejected, error))
    {
        return Fail("Gate 1 accepted a reserved Windows executable name");
    }

    auto collidingSupport = support;
    collidingSupport.push_back({
        "duplicate-dxc",
        "DXCOMPILER.DLL",
        7,
        std::string(64, 'c'),
        "fixture.case-collision",
    });
    if (CreateWindowsGameBuildPlan(
            project,
            graph,
            registry,
            request,
            collidingSupport,
            rejected,
            error))
    {
        return Fail("Gate 1 accepted a case-colliding Windows package destination");
    }

    DependencyGraph staleGraph = graph;
    auto staleSourceNode = std::find_if(
        staleGraph.nodes.begin(),
        staleGraph.nodes.end(),
        [](const DependencyNode& node) { return node.id == "source"; });
    staleSourceNode->contentHash = "fnv1a64:9999999999999999";
    AssetRegistry staleRegistry;
    if (!MakeRegistry(staleGraph, staleRegistry, error))
        return Fail(error.c_str());

    AssetRecord* staleSource = FindRecord(staleRegistry, "Content/Source/model.glb");
    AssetRecord* staleProduct = FindRecord(staleRegistry, "Content/Imported/model.wiscene");
    if (staleSource == nullptr || staleProduct == nullptr)
        return Fail("stale fixture did not contain source/product records");
    ImportedProductRecord staleImport;
    staleImport.sourceAssetId = staleSource->assetId;
    staleImport.productAssetId = staleProduct->assetId;
    staleImport.importer = "renegade.gltf";
    staleImport.importerVersion = 1;
    staleImport.settingsSchema = "renegade.gltf.settings";
    staleImport.settingsVersion = 1;
    staleImport.settingsJson = "{\"scale_mode\":\"automatic\"}";
    staleImport.sourceContentHashAtImport = "fnv1a64:6000000000000006";
    staleImport.productContentHashAtImport = staleProduct->contentHash;
    if (!SetImportedProductRecords(staleRegistry, {staleImport}, error))
        return Fail(error.c_str());
    if (CreateWindowsGameBuildPlan(
            project,
            staleGraph,
            staleRegistry,
            request,
            support,
            rejected,
            error))
    {
        return Fail("Gate 1 accepted an imported product with stale source provenance");
    }

    std::cout << "PASS: LP06 Gate 1 standalone build plan contract\n";
    return 0;
}
