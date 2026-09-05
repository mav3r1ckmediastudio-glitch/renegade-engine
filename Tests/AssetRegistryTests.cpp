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
    constexpr const char* AmbiguousAssetId1 =
        "eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee";
    constexpr const char* AmbiguousAssetId2 =
        "ffffffff-ffff-4fff-8fff-ffffffffffff";

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

    AssetRegistry legacy = first.registry;
    legacy.schemaVersion = AssetRegistry::LegacySchemaVersion;
    std::string legacyJson;
    if (!SerializeAssetRegistry(legacy, legacyJson, error))
        return Fail(error.c_str());
    AssetRegistry legacyReloaded;
    std::string legacyReloadedJson;
    if (!DeserializeAssetRegistry(legacyJson, legacyReloaded, error) ||
        legacyReloaded.schemaVersion != AssetRegistry::LegacySchemaVersion ||
        !SerializeAssetRegistry(legacyReloaded, legacyReloadedJson, error) ||
        legacyReloadedJson != legacyJson)
        return Fail("legacy registry was not preserved for explicit provenance migration");

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

    // Gate 3 provenance is an ID-based recipe, not an importer invocation.
    // The synthetic project document acts as the source and the WISCENE as
    // its product here; the contract deliberately permits other importer
    // types to use the same durable relationship later.
    ImportedProductRecord imported;
    imported.sourceAssetId = ProjectAssetId;
    imported.productAssetId = SceneAssetId;
    imported.importer = "renegade.gltf";
    imported.importerVersion = 1;
    imported.settingsSchema = "renegade.gltf.settings";
    imported.settingsVersion = 1;
    imported.settingsJson = "{\"scale_mode\":\"automatic\",\"units\":\"meters\"}";
    imported.sourceContentHashAtImport = "fnv1a64:1000000000000001";
    imported.productContentHashAtImport = "fnv1a64:2000000000000002";
    if (!SetImportedProductRecords(loaded, {imported}, error))
        return Fail(error.c_str());
    std::string provenanceJson;
    if (!SerializeAssetRegistry(loaded, provenanceJson, error))
        return Fail(error.c_str());
    AssetRegistry provenanceReloaded;
    if (!DeserializeAssetRegistry(provenanceJson, provenanceReloaded, error))
        return Fail(error.c_str());
    std::string provenanceReloadedJson;
    if (!SerializeAssetRegistry(
            provenanceReloaded, provenanceReloadedJson, error) ||
        provenanceReloadedJson != provenanceJson)
        return Fail("import provenance did not round-trip byte-identically");
    ImportedProductStatus importStatus;
    if (!GetImportedProductStatus(
            provenanceReloaded, imported, importStatus, error) ||
        !importStatus.sourceAvailable || !importStatus.productAvailable ||
        importStatus.sourceChanged || importStatus.productChanged)
        return Fail("fresh imported-product provenance was not current");

    AssetRegistry sourceChangedRegistry = provenanceReloaded;
    const auto changedSource = std::find_if(
        sourceChangedRegistry.records.begin(),
        sourceChangedRegistry.records.end(),
        [](const AssetRecord& record)
        {
            return record.projectRelativePath == "representative.renegade";
        });
    if (changedSource == sourceChangedRegistry.records.end())
        return Fail("source fixture record was missing");
    changedSource->contentHash = "fnv1a64:1111111111111111";
    if (!GetImportedProductStatus(
            sourceChangedRegistry, imported, importStatus, error) ||
        !importStatus.sourceChanged || importStatus.productChanged)
        return Fail("source change was not detected from import provenance");

    AssetRegistry missingProductRegistry = provenanceReloaded;
    const auto missingProduct = std::find_if(
        missingProductRegistry.records.begin(),
        missingProductRegistry.records.end(),
        [](const AssetRecord& record)
        {
            return record.projectRelativePath == "Content/Scenes/Startup.wiscene";
        });
    if (missingProduct == missingProductRegistry.records.end())
        return Fail("product fixture record was missing");
    missingProduct->sourceAvailable = false;
    missingProduct->contentHash = "missing";
    if (!GetImportedProductStatus(
            missingProductRegistry, imported, importStatus, error) ||
        !importStatus.sourceAvailable || importStatus.productAvailable ||
        importStatus.sourceChanged || !importStatus.productChanged)
        return Fail("missing imported product was not detected from provenance");

    AssetRegistry rejectedProvenance = provenanceReloaded;
    ImportedProductRecord invalidImported = imported;
    invalidImported.productAssetId = imported.sourceAssetId;
    if (SetImportedProductRecords(
            rejectedProvenance, {invalidImported}, error) || error.empty() ||
        rejectedProvenance.importedProducts !=
            provenanceReloaded.importedProducts)
        return Fail("invalid imported-product provenance changed the registry");

    invalidImported = imported;
    invalidImported.sourceContentHashAtImport = "fnv1a64:ffffffffffffffff";
    if (SetImportedProductRecords(
            rejectedProvenance, {invalidImported}, error) || error.empty() ||
        rejectedProvenance.importedProducts !=
            provenanceReloaded.importedProducts)
        return Fail("non-current import hash snapshot changed the registry");

    AssetRegistry upgradedLegacy = legacyReloaded;
    if (!SetImportedProductRecords(upgradedLegacy, {imported}, error) ||
        upgradedLegacy.schemaVersion != AssetRegistry::CurrentSchemaVersion)
        return Fail("legacy registry did not upgrade when provenance was registered");

    AssetRegistryRefresh unchanged;
    if (!RefreshAssetRegistry(
            ProjectId,
            graph,
            &provenanceReloaded,
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
        unchangedJson != provenanceJson ||
        unchanged.registry.importedProducts != provenanceReloaded.importedProducts)
        return Fail("unchanged refresh did not preserve IDs, provenance and bytes");

    // Gate 4: a uniquely evidenced move keeps the durable identity and its
    // provenance. Recovery must be unique on both sides of the match.
    DependencyGraph movedGraph = graph;
    movedGraph.nodes[1].id = "moved-scene";
    movedGraph.nodes[1].projectRelativePath =
        "Content/Scenes/Moved/Startup.wiscene";
    for (auto& edge : movedGraph.edges)
    {
        if (edge.sourceId == "scene") edge.sourceId = "moved-scene";
        if (edge.targetId == "scene") edge.targetId = "moved-scene";
    }
    AssetRegistryRefresh moved;
    if (!RefreshAssetRegistry(ProjectId, movedGraph, &provenanceReloaded,
            moved, error, [] { return StableId{}; }))
        return Fail(error.c_str());
    const AssetRecord* movedScene = FindRecord(
        moved.registry, "Content/Scenes/Moved/Startup.wiscene");
    if (movedScene == nullptr || movedScene->assetId != SceneAssetId ||
        moved.recoveredAssetIds != std::vector<StableId>{SceneAssetId} ||
        !moved.addedAssetIds.empty() || !moved.removedAssetIds.empty() ||
        moved.registry.importedProducts != provenanceReloaded.importedProducts)
        return Fail("unique move did not preserve identity and provenance");

    DependencyGraph missingGraph = movedGraph;
    missingGraph.nodes.erase(missingGraph.nodes.begin() + 1);
    missingGraph.edges.erase(std::remove_if(
        missingGraph.edges.begin(), missingGraph.edges.end(),
        [](const DependencyEdge& edge)
        {
            return edge.sourceId == "moved-scene" ||
                edge.targetId == "moved-scene";
        }), missingGraph.edges.end());
    AssetRegistryRefresh missing;
    if (!RefreshAssetRegistry(ProjectId, missingGraph, &moved.registry,
            missing, error, [] { return StableId{}; }))
        return Fail(error.c_str());
    if (!ContainsId(missing.removedAssetIds, SceneAssetId) ||
        missing.registry.missingAssets.size() != 1 ||
        missing.registry.missingAssets.front().assetId != SceneAssetId ||
        missing.registry.missingAssets.front().lastKnownPath !=
            "Content/Scenes/Moved/Startup.wiscene" ||
        missing.registry.importedProducts != moved.registry.importedProducts)
        return Fail("genuine loss did not retain a deterministic tombstone");
    if (!GetImportedProductStatus(
            missing.registry, imported, importStatus, error) ||
        importStatus.productAvailable || !importStatus.productChanged)
        return Fail("missing product provenance did not report unavailable");
    std::string missingJson;
    AssetRegistry missingReloaded;
    std::string missingReloadedJson;
    if (!SerializeAssetRegistry(missing.registry, missingJson, error) ||
        !DeserializeAssetRegistry(missingJson, missingReloaded, error) ||
        !SerializeAssetRegistry(missingReloaded, missingReloadedJson, error) ||
        missingReloadedJson != missingJson)
        return Fail("recovery tombstone did not round-trip byte-identically");

    DependencyGraph reappearedGraph = missingGraph;
    reappearedGraph.nodes.push_back(Node("returned-scene",
        "Content/Scenes/Returned/Startup.wiscene", DependencyClass::Scene,
        "fnv1a64:2000000000000002"));
    AssetRegistryRefresh reappeared;
    if (!RefreshAssetRegistry(ProjectId, reappearedGraph, &missingReloaded,
            reappeared, error, [] { return StableId{}; }))
        return Fail(error.c_str());
    const AssetRecord* returnedScene = FindRecord(
        reappeared.registry, "Content/Scenes/Returned/Startup.wiscene");
    if (returnedScene == nullptr || returnedScene->assetId != SceneAssetId ||
        !reappeared.registry.missingAssets.empty() ||
        reappeared.recoveredAssetIds != std::vector<StableId>{SceneAssetId})
        return Fail("unique reappearance did not reclaim tombstoned identity");

    DependencyGraph ambiguousGraph = missingGraph;
    ambiguousGraph.nodes.push_back(Node("candidate-a",
        "Content/Scenes/CandidateA.wiscene", DependencyClass::Scene,
        "fnv1a64:2000000000000002"));
    ambiguousGraph.nodes.push_back(Node("candidate-b",
        "Content/Scenes/CandidateB.wiscene", DependencyClass::Scene,
        "fnv1a64:2000000000000002"));
    std::vector<StableId> ambiguousGenerated = {
        AmbiguousAssetId1, AmbiguousAssetId2};
    std::size_t ambiguousIndex = 0;
    AssetRegistryRefresh ambiguous;
    if (!RefreshAssetRegistry(ProjectId, ambiguousGraph, &missingReloaded,
            ambiguous, error, [&ambiguousGenerated, &ambiguousIndex]
            { return ambiguousGenerated.at(ambiguousIndex++); }))
        return Fail(error.c_str());
    if (!ambiguous.recoveredAssetIds.empty() ||
        ambiguous.ambiguousRecoveryAssetIds.size() != 2 ||
        ambiguous.registry.missingAssets.size() != 1)
        return Fail("ambiguous candidates stole a missing asset identity");
    const AssetRecord* candidateA = FindRecord(
        ambiguous.registry, "Content/Scenes/CandidateA.wiscene");
    const AssetRecord* candidateB = FindRecord(
        ambiguous.registry, "Content/Scenes/CandidateB.wiscene");
    if (candidateA == nullptr || candidateB == nullptr ||
        candidateA->assetId == SceneAssetId ||
        candidateB->assetId == SceneAssetId)
        return Fail("ambiguous candidates reused the tombstoned identity");

    AssetRegistry twoTombstones = missingReloaded;
    MissingAssetRecord secondTombstone = twoTombstones.missingAssets.front();
    secondTombstone.assetId = AudioAssetId;
    secondTombstone.lastKnownPath = "Content/Scenes/Other.wiscene";
    twoTombstones.missingAssets.push_back(secondTombstone);
    DependencyGraph oneCandidateGraph = missingGraph;
    oneCandidateGraph.nodes.push_back(Node("candidate-only",
        "Content/Scenes/CandidateOnly.wiscene", DependencyClass::Scene,
        "fnv1a64:2000000000000002"));
    AssetRegistryRefresh twoWayAmbiguous;
    if (!RefreshAssetRegistry(ProjectId, oneCandidateGraph, &twoTombstones,
            twoWayAmbiguous, error,
            [] { return StableId(AmbiguousAssetId1); }))
        return Fail(error.c_str());
    if (!twoWayAmbiguous.recoveredAssetIds.empty() ||
        twoWayAmbiguous.ambiguousRecoveryAssetIds !=
            std::vector<StableId>{AmbiguousAssetId1} ||
        twoWayAmbiguous.registry.missingAssets.size() != 2)
        return Fail("candidate matching two tombstones was recovered speculatively");

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
