#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "renegade/bridge/AssetRegistryService.h"

namespace
{
    using namespace renegade::bridge;

    constexpr const char* ProjectId =
        "11111111-1111-4111-8111-111111111111";
    constexpr const char* ProjectAssetId =
        "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
    constexpr const char* SceneAssetId =
        "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb";
    constexpr const char* TextureAssetId =
        "cccccccc-cccc-4ccc-8ccc-cccccccccccc";
    constexpr const char* AudioAssetId =
        "dddddddd-dddd-4ddd-8ddd-dddddddddddd";

    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    DependencyNode Node(
        std::string id,
        std::string path,
        const DependencyClass dependencyClass,
        std::string hash,
        const bool runtimeSupport = false)
    {
        DependencyNode node;
        node.id = std::move(id);
        node.projectRelativePath = std::move(path);
        node.dependencyClass = dependencyClass;
        node.provider = runtimeSupport ? "runtime-support" : "lc01-fixture";
        node.contentHash = std::move(hash);
        node.runtimeSupport = runtimeSupport;
        return node;
    }

    const AssetRecord* FindRecord(
        const AssetRegistry& registry,
        const std::string& path)
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

    bool ContainsId(
        const std::vector<StableId>& values,
        const StableId& expected)
    {
        return std::find(values.begin(), values.end(), expected) != values.end();
    }
}

int main()
{
    using namespace renegade::bridge;

    DependencyGraph graph;
    graph.rootIds = {"project"};
    graph.nodes = {
        Node("project", "representative.renegade",
            DependencyClass::ProjectDocument, "fnv1a64:1000000000000001"),
        Node("scene", "Content/Scenes/Startup.wiscene",
            DependencyClass::Scene, "fnv1a64:2000000000000002"),
        Node("texture", "Content/Textures/missing.png",
            DependencyClass::Texture, "missing"),
        Node("runtime", "Runtime/RenegadeRuntime.exe",
            DependencyClass::RuntimeSupport, "fnv1a64:4000000000000004", true),
    };
    graph.edges = {
        {"project", "scene", "project.startup_scene"},
        {"project", "runtime", "project.runtime"},
        {"scene", "texture", "wiscene.material[0].texture.base_color"},
        {"scene", "scene", "fixture.self_cycle"},
    };

    std::vector<StableId> generated = {
        ProjectAssetId, SceneAssetId, TextureAssetId,
    };
    std::size_t generatedIndex = 0;
    AssetRegistryRefresh first;
    std::string error;
    if (!RefreshAssetRegistry(
            ProjectId,
            graph,
            nullptr,
            first,
            error,
            [&generated, &generatedIndex]
            {
                return generated.at(generatedIndex++);
            }))
        return Fail(error.c_str());

    if (first.registry.records.size() != 3 ||
        first.addedAssetIds.size() != 3 ||
        !first.changedAssetIds.empty() ||
        !first.removedAssetIds.empty())
        return Fail("first refresh did not create exactly the project assets");

    const AssetRecord* project = FindRecord(
        first.registry, "representative.renegade");
    const AssetRecord* scene = FindRecord(
        first.registry, "Content/Scenes/Startup.wiscene");
    const AssetRecord* texture = FindRecord(
        first.registry, "Content/Textures/missing.png");
    if (project == nullptr || scene == nullptr || texture == nullptr ||
        project->assetId != ProjectAssetId || scene->assetId != SceneAssetId ||
        texture->assetId != TextureAssetId || !project->root || scene->root ||
        texture->sourceAvailable ||
        project->dependencyAssetIds != std::vector<StableId>{SceneAssetId} ||
        scene->dependencyAssetIds !=
            std::vector<StableId>{SceneAssetId, TextureAssetId})
        return Fail("graph was not projected onto stable asset records");

    std::string firstJson;
    if (!SerializeAssetRegistry(first.registry, firstJson, error))
        return Fail(error.c_str());

    DependencyGraph reordered = graph;
    std::reverse(reordered.nodes.begin(), reordered.nodes.end());
    std::reverse(reordered.edges.begin(), reordered.edges.end());
    generatedIndex = 0;
    AssetRegistryRefresh deterministic;
    if (!RefreshAssetRegistry(
            ProjectId,
            reordered,
            nullptr,
            deterministic,
            error,
            [&generated, &generatedIndex]
            {
                return generated.at(generatedIndex++);
            }))
        return Fail(error.c_str());
    std::string deterministicJson;
    if (!SerializeAssetRegistry(
            deterministic.registry, deterministicJson, error) ||
        deterministicJson != firstJson)
        return Fail("logical graph order changed fresh registry bytes");

    AssetRegistry loaded;
    if (!DeserializeAssetRegistry(firstJson, loaded, error))
        return Fail(error.c_str());
    std::string loadedJson;
    if (!SerializeAssetRegistry(loaded, loadedJson, error) ||
        loadedJson != firstJson)
        return Fail("registry JSON did not round-trip byte-identically");

    AssetRegistryRefresh unchanged;
    if (!RefreshAssetRegistry(
            ProjectId,
            graph,
            &loaded,
            unchanged,
            error,
            [] { return StableId{}; }))
        return Fail(error.c_str());
    if (!unchanged.addedAssetIds.empty() ||
        !unchanged.changedAssetIds.empty() ||
        !unchanged.removedAssetIds.empty())
        return Fail("unchanged graph was reported as changed");
    std::string unchangedJson;
    if (!SerializeAssetRegistry(unchanged.registry, unchangedJson, error) ||
        unchangedJson != firstJson)
        return Fail("unchanged refresh did not preserve IDs and bytes");

    DependencyGraph updated = graph;
    updated.nodes[1].contentHash = "fnv1a64:2222222222222222";
    updated.nodes.erase(updated.nodes.begin() + 2);
    updated.edges.erase(updated.edges.begin() + 2);
    updated.nodes.push_back(Node(
        "audio",
        "Content/Audio/theme.ogg",
        DependencyClass::Audio,
        "fnv1a64:5000000000000005"));
    updated.edges.push_back({"scene", "audio", "wiscene.sound[0].filename"});

    AssetRegistryRefresh changed;
    if (!RefreshAssetRegistry(
            ProjectId,
            updated,
            &unchanged.registry,
            changed,
            error,
            [] { return StableId(AudioAssetId); }))
        return Fail(error.c_str());
    const AssetRecord* changedScene = FindRecord(
        changed.registry, "Content/Scenes/Startup.wiscene");
    if (changedScene == nullptr || changedScene->assetId != SceneAssetId ||
        !ContainsId(changed.addedAssetIds, AudioAssetId) ||
        !ContainsId(changed.changedAssetIds, SceneAssetId) ||
        !ContainsId(changed.removedAssetIds, TextureAssetId))
        return Fail("refresh did not preserve identity and report graph changes");

    AssetRegistry wrongProject = changed.registry;
    wrongProject.projectId =
        "22222222-2222-4222-8222-222222222222";
    AssetRegistryRefresh rejected;
    if (RefreshAssetRegistry(
            ProjectId, updated, &wrongProject, rejected, error) || error.empty())
        return Fail("registry from another project was accepted");

    AssetRegistry invalid = changed.registry;
    invalid.records[1].projectRelativePath =
        invalid.records[0].projectRelativePath;
    if (ValidateAssetRegistry(invalid, error) || error.empty())
        return Fail("duplicate registry paths were accepted");

    invalid = changed.registry;
    invalid.records[0].projectRelativePath = "Content/../outside.asset";
    if (ValidateAssetRegistry(invalid, error) || error.empty())
        return Fail("non-canonical registry path was accepted");

    invalid = changed.registry;
    invalid.records[0].contentHash = "sha256:not-the-lp05-contract";
    if (ValidateAssetRegistry(invalid, error) || error.empty())
        return Fail("invalid source hash was accepted");

    invalid = changed.registry;
    invalid.records[0].dependencyAssetIds.push_back(
        "99999999-9999-4999-8999-999999999999");
    if (ValidateAssetRegistry(invalid, error) || error.empty())
        return Fail("dangling asset dependency was accepted");

    std::cout
        << "PASS: LP05 graph consumption, stable IDs, change tracking, "
           "runtime exclusion, and deterministic registry JSON\n";
    return 0;
}
