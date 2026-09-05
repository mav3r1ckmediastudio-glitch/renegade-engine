#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "renegade/bridge/AssetBrowserService.h"
#include "renegade/bridge/AssetCatalogueService.h"

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    int failures = 0;

    constexpr const char* ProjectId =
        "90000000-0000-4000-8000-000000000001";
    constexpr const char* OtherProjectId =
        "90000000-0000-4000-8000-000000000002";
    constexpr const char* SoldierSourceId =
        "10000000-0000-4000-8000-000000000001";
    constexpr const char* SoldierProductId =
        "20000000-0000-4000-8000-000000000001";
    constexpr const char* KnightSourceId =
        "10000000-0000-4000-8000-000000000002";
    constexpr const char* KnightProductId =
        "20000000-0000-4000-8000-000000000002";
    constexpr const char* RockId =
        "10000000-0000-4000-8000-000000000003";
    constexpr const char* InvalidSourceId =
        "10000000-0000-4000-8000-000000000004";
    constexpr const char* InvalidProductId =
        "20000000-0000-4000-8000-000000000004";
    constexpr const char* MissingId =
        "10000000-0000-4000-8000-000000000005";
    constexpr const char* TextureId =
        "30000000-0000-4000-8000-000000000001";
    constexpr const char* SceneId =
        "40000000-0000-4000-8000-000000000001";

    void Check(const bool condition, const char* message)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    void WriteText(const fs::path& path, const std::string& text)
    {
        fs::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    std::string ReadText(const fs::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return std::string(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
    }

    AssetRecord MakeRecord(
        const StableId& assetId,
        const std::string& nodeId,
        const std::string& path,
        const DependencyClass dependencyClass,
        const std::string& hash)
    {
        AssetRecord record;
        record.assetId = assetId;
        record.dependencyNodeId = nodeId;
        record.projectRelativePath = path;
        record.dependencyClass = dependencyClass;
        record.provider = "lp07-gate2-fixture";
        record.providerVersion = 1;
        record.contentHash = hash;
        record.sourceAvailable = true;
        return record;
    }

    ImportedProductRecord MakeImportedProduct(
        const StableId& sourceId,
        const StableId& productId,
        const std::string& sourceHash,
        const std::string& productHash)
    {
        ImportedProductRecord imported;
        imported.sourceAssetId = sourceId;
        imported.productAssetId = productId;
        imported.importer = "wicked.ufbx";
        imported.importerVersion = 1;
        imported.settingsSchema = "renegade-model-import";
        imported.settingsVersion = 1;
        imported.settingsJson = "{}";
        imported.sourceContentHashAtImport = sourceHash;
        imported.productContentHashAtImport = productHash;
        return imported;
    }

    const AssetCatalogueEntry* FindEntry(
        const AssetCatalogue& catalogue,
        const StableId& assetId)
    {
        const auto found = std::find_if(catalogue.entries.begin(),
            catalogue.entries.end(), [&assetId](const AssetCatalogueEntry& entry)
            { return entry.assetId == assetId; });
        return found == catalogue.entries.end() ? nullptr : &*found;
    }

    const AssetCatalogueEntry* FindPath(
        const AssetCatalogue& catalogue,
        const std::string& path)
    {
        const auto found = std::find_if(catalogue.entries.begin(),
            catalogue.entries.end(), [&path](const AssetCatalogueEntry& entry)
            { return entry.projectRelativePath == path; });
        return found == catalogue.entries.end() ? nullptr : &*found;
    }

    std::string CatalogueSignature(const AssetCatalogue& catalogue)
    {
        std::ostringstream stream;
        stream << catalogue.projectId << '\n';
        for (const auto& entry : catalogue.entries)
        {
            stream << entry.projectRelativePath << '|'
                << entry.assetId << '|'
                << AssetCatalogueStateLabel(entry.state) << '|'
                << static_cast<int>(entry.type) << '|'
                << entry.sourceFormat << '|'
                << entry.importer << '|'
                << entry.model.known << '|'
                << entry.model.meshCount << '|'
                << entry.model.skinned << '|'
                << entry.model.animated << '|';
            for (const auto& tag : entry.creatorTags)
                stream << tag << ',';
            stream << '|';
            for (const auto& id : entry.dependencyAssetIds)
                stream << id << ',';
            stream << '|';
            for (const auto& id : entry.referencedByAssetIds)
                stream << id << ',';
            stream << '\n';
        }
        return stream.str();
    }

    bool IsSingleResult(
        const std::vector<AssetCatalogueEntry>& result,
        const StableId& assetId)
    {
        return result.size() == 1 && result.front().assetId == assetId;
    }
}

int main()
{
    using namespace renegade::bridge;

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        fs::u8path("renegade-lp07-gate2-" + std::to_string(unique));

    WriteText(root / "Content/Models/Soldier/Soldier.fbx", "soldier-source");
    WriteText(root / "Content/Models/Soldier/Soldier.wiscene", "soldier-product");
    WriteText(root / "Content/Models/Knight/Knight.fbx", "knight-source-v2");
    WriteText(root / "Content/Models/Knight/Knight.wiscene", "knight-product");
    WriteText(root / "Content/Models/Rock/Rock.fbx", "moved-rock");
    WriteText(root / "Content/Models/Invalid/Invalid.fbx", "invalid-source");
    WriteText(root / "Content/Models/Invalid/Invalid.wiscene", "tampered-product");
    WriteText(root / "Content/Models/Crate/Crate.fbx", "unregistered-crate");
    WriteText(root / "Content/Textures/Soldier.png", "texture");
    WriteText(root / "Content/Scenes/Main.wiscene", "scene");
    WriteText(root / "Content/Scenes/Main.wiscene.rmeta", "engine-owned-sidecar");
    WriteText(root / "Saved/Outside.txt", "outside-content");

    AssetRegistry registry;
    registry.projectId = ProjectId;

    auto soldierSource = MakeRecord(SoldierSourceId, "soldier-source",
        "Content/Models/Soldier/Soldier.fbx", DependencyClass::ImportedContent,
        "fnv1a64:1111111111111111");
    auto soldierProduct = MakeRecord(SoldierProductId, "soldier-product",
        "Content/Models/Soldier/Soldier.wiscene", DependencyClass::ImportedContent,
        "fnv1a64:2222222222222222");
    soldierProduct.dependencyAssetIds = {TextureId};

    auto knightSource = MakeRecord(KnightSourceId, "knight-source",
        "Content/Models/Knight/Knight.fbx", DependencyClass::ImportedContent,
        "fnv1a64:3333333333333333");
    auto knightProduct = MakeRecord(KnightProductId, "knight-product",
        "Content/Models/Knight/Knight.wiscene", DependencyClass::ImportedContent,
        "fnv1a64:4444444444444444");

    auto rock = MakeRecord(RockId, "rock",
        "Content/Models/Rock/Rock.fbx", DependencyClass::ImportedContent,
        "fnv1a64:8888888888888888");

    auto invalidSource = MakeRecord(InvalidSourceId, "invalid-source",
        "Content/Models/Invalid/Invalid.fbx", DependencyClass::ImportedContent,
        "fnv1a64:5555555555555555");
    auto invalidProduct = MakeRecord(InvalidProductId, "invalid-product",
        "Content/Models/Invalid/Invalid.wiscene", DependencyClass::ImportedContent,
        "fnv1a64:6666666666666666");

    auto texture = MakeRecord(TextureId, "soldier-texture",
        "Content/Textures/Soldier.png", DependencyClass::Texture,
        "fnv1a64:9999999999999999");
    auto scene = MakeRecord(SceneId, "main-scene",
        "Content/Scenes/Main.wiscene", DependencyClass::Scene,
        "fnv1a64:abababababababab");
    scene.dependencyAssetIds = {SoldierProductId};

    registry.records = {
        soldierSource,
        soldierProduct,
        knightSource,
        knightProduct,
        rock,
        invalidSource,
        invalidProduct,
        texture,
        scene,
    };
    registry.importedProducts = {
        MakeImportedProduct(SoldierSourceId, SoldierProductId,
            "fnv1a64:1111111111111111", "fnv1a64:2222222222222222"),
        MakeImportedProduct(KnightSourceId, KnightProductId,
            "fnv1a64:aaaaaaaaaaaaaaaa", "fnv1a64:4444444444444444"),
        MakeImportedProduct(InvalidSourceId, InvalidProductId,
            "fnv1a64:5555555555555555", "fnv1a64:7777777777777777"),
    };
    MissingAssetRecord missing;
    missing.assetId = MissingId;
    missing.lastKnownPath = "Content/Models/Missing/Missing.fbx";
    missing.dependencyClass = DependencyClass::ImportedContent;
    missing.provider = "lp07-gate2-fixture";
    missing.providerVersion = 1;
    missing.contentHash = "fnv1a64:cdcdcdcdcdcdcdcd";
    registry.missingAssets = {missing};

    std::string error;
    Check(ValidateAssetRegistry(registry, error),
        "catalogue fixture registry was invalid");

    AssetCatalogueMetadataDocument metadata;
    Check(ReadAssetCatalogueMetadata(root.generic_u8string(), ProjectId,
            metadata, error) && metadata.projectId == ProjectId &&
            metadata.records.empty(),
        "missing metadata document did not open as an empty project-owned document");

    ModelDerivedMetadata animatedModel;
    animatedModel.known = true;
    animatedModel.meshCount = 2;
    animatedModel.materialCount = 3;
    animatedModel.armatureCount = 1;
    animatedModel.boneCount = 32;
    animatedModel.animationClipCount = 2;
    animatedModel.animationChannelCount = 8;
    animatedModel.morphTargetCount = 1;
    animatedModel.skinned = true;
    animatedModel.animated = true;

    ModelDerivedMetadata staticModel;
    staticModel.known = true;
    staticModel.meshCount = 1;
    staticModel.materialCount = 1;

    Check(SetAssetCreatorTags(metadata, SoldierProductId,
            {" Medieval ", "enemy", "MEDIEVAL"}, error),
        "creator-tag canonicalisation failed");
    Check(SetAssetModelDerivedMetadata(metadata, SoldierProductId,
            animatedModel, error),
        "soldier model metadata assignment failed");
    Check(SetAssetCreatorTags(metadata, KnightProductId,
            {"medieval", "knight"}, error) &&
        SetAssetModelDerivedMetadata(metadata, KnightProductId,
            animatedModel, error),
        "knight metadata assignment failed");
    Check(SetAssetCreatorTags(metadata, RockId,
            {"Architecture"}, error) &&
        SetAssetModelDerivedMetadata(metadata, RockId, staticModel, error),
        "rock metadata assignment failed");
    Check(SetAssetCreatorTags(metadata, MissingId, {"lost"}, error),
        "missing-asset tag assignment failed");

    const auto soldierMetadata = std::find_if(metadata.records.begin(),
        metadata.records.end(), [](const auto& record)
        { return record.assetId == SoldierProductId; });
    Check(soldierMetadata != metadata.records.end() &&
            soldierMetadata->creatorTags ==
                std::vector<std::string>({"enemy", "medieval"}),
        "creator tags were not canonical, sorted, and deduplicated");

    std::string metadataJsonA;
    std::string metadataJsonB;
    Check(SerializeAssetCatalogueMetadata(metadata, metadataJsonA, error) &&
            SerializeAssetCatalogueMetadata(metadata, metadataJsonB, error) &&
            metadataJsonA == metadataJsonB,
        "asset metadata serialization was not deterministic");

    AssetCatalogueMetadataPersistenceOptions metadataOptions;
    metadataOptions.transactionId = "lp07-gate2-metadata";
    const auto written = WriteAssetCatalogueMetadata(root.generic_u8string(),
        metadata, std::move(metadataOptions));
    Check(written.success && written.committed,
        "asset metadata transaction did not commit");

    std::string metadataPath;
    Check(ResolveAssetCatalogueMetadataDocumentPath(root.generic_u8string(),
            metadataPath, error) && ReadText(fs::u8path(metadataPath)) == metadataJsonA,
        "asset metadata authoritative bytes were not canonical");

    AssetCatalogueMetadataDocument reopenedMetadata;
    Check(ReadAssetCatalogueMetadata(root.generic_u8string(), ProjectId,
            reopenedMetadata, error),
        "asset metadata did not reopen");
    std::string reopenedJson;
    Check(SerializeAssetCatalogueMetadata(reopenedMetadata, reopenedJson, error) &&
            reopenedJson == metadataJsonA,
        "reopened asset metadata was not byte-identical");

    AssetCatalogueMetadataPersistenceOptions noOpOptions;
    noOpOptions.transactionId = "lp07-gate2-metadata-noop";
    const auto noOp = WriteAssetCatalogueMetadata(root.generic_u8string(),
        reopenedMetadata, std::move(noOpOptions));
    Check(noOp.success && noOp.committed && noOp.noChanges &&
            ReadText(fs::u8path(metadataPath)) == metadataJsonA,
        "unchanged asset metadata was not a byte-preserving no-op");

    AssetCatalogueMetadataDocument wrongProjectMetadata;
    Check(!ReadAssetCatalogueMetadata(root.generic_u8string(), OtherProjectId,
            wrongProjectMetadata, error) && !error.empty(),
        "cross-project asset metadata was accepted");

    AssetCatalogueBuildOptions buildOptions;
    buildOptions.movedAssetIds = {RockId};
    AssetCatalogue catalogue;
    Check(BuildAssetCatalogue(root.generic_u8string(), ProjectId, registry,
            reopenedMetadata, catalogue, error, buildOptions),
        "asset catalogue build failed");

    const auto* soldier = FindEntry(catalogue, SoldierProductId);
    Check(soldier != nullptr && soldier->registered &&
            soldier->state == AssetCatalogueState::Current &&
            soldier->sourceFormat == "fbx" && soldier->importer == "wicked.ufbx" &&
            soldier->model.known && soldier->model.skinned && soldier->model.animated &&
            soldier->creatorTags == std::vector<std::string>({"enemy", "medieval"}) &&
            soldier->dependencyAssetIds == std::vector<StableId>({TextureId}) &&
            soldier->referencedByAssetIds == std::vector<StableId>({SceneId}),
        "current imported product catalogue projection was incomplete");

    const auto* knight = FindEntry(catalogue, KnightProductId);
    Check(knight != nullptr && knight->state == AssetCatalogueState::Stale,
        "changed FBX source did not mark the imported product stale");

    const auto* moved = FindEntry(catalogue, RockId);
    Check(moved != nullptr && moved->state == AssetCatalogueState::Moved &&
            moved->creatorTags == std::vector<std::string>({"architecture"}),
        "recovered stable asset did not surface moved state and metadata");

    const auto* invalid = FindEntry(catalogue, InvalidProductId);
    Check(invalid != nullptr && invalid->state == AssetCatalogueState::Invalid,
        "product drift did not surface invalid state");

    const auto* missingEntry = FindEntry(catalogue, MissingId);
    Check(missingEntry != nullptr &&
            missingEntry->state == AssetCatalogueState::Missing &&
            missingEntry->creatorTags == std::vector<std::string>({"lost"}),
        "missing tombstone did not remain a stable tagged catalogue entry");

    const auto* crate = FindPath(catalogue,
        "Content/Models/Crate/Crate.fbx");
    Check(crate != nullptr && !crate->registered && crate->assetId.empty() &&
            crate->state == AssetCatalogueState::Unregistered &&
            crate->sourceFormat == "fbx",
        "filesystem-only source was assigned a fake identity or wrong state");
    Check(FindPath(catalogue, "Content/Scenes/Main.wiscene.rmeta") == nullptr,
        "engine-owned .rmeta sidecar leaked into the creator catalogue");

    AssetCatalogueQuery currentAnimatedMedieval;
    currentAnimatedMedieval.type = AssetType::Model;
    currentAnimatedMedieval.state = AssetCatalogueState::Current;
    currentAnimatedMedieval.sourceFormat = "FBX";
    currentAnimatedMedieval.skinned = true;
    currentAnimatedMedieval.animated = true;
    currentAnimatedMedieval.tags = {"MEDIEVAL", " enemy "};
    Check(IsSingleResult(QueryAssetCatalogue(catalogue,
            currentAnimatedMedieval), SoldierProductId),
        "combined model/source/rig/state/tag query was not deterministic");

    AssetCatalogueQuery staleQuery;
    staleQuery.state = AssetCatalogueState::Stale;
    staleQuery.text = "knight";
    Check(IsSingleResult(QueryAssetCatalogue(catalogue, staleQuery),
            KnightProductId),
        "stale text query did not return the expected asset");

    AssetCatalogueQuery staticQuery;
    staticQuery.staticModelsOnly = true;
    staticQuery.tags = {"architecture"};
    Check(IsSingleResult(QueryAssetCatalogue(catalogue, staticQuery), RockId),
        "static-model/tag query did not return the moved rock");

    AssetCatalogueQuery unregisteredQuery;
    unregisteredQuery.state = AssetCatalogueState::Unregistered;
    unregisteredQuery.text = "crate";
    const auto unregistered = QueryAssetCatalogue(catalogue, unregisteredQuery);
    Check(unregistered.size() == 1 &&
            unregistered.front().projectRelativePath ==
                "Content/Models/Crate/Crate.fbx",
        "unregistered query did not distinguish filesystem-only content");

    AssetCatalogue secondBuild;
    Check(BuildAssetCatalogue(root.generic_u8string(), ProjectId, registry,
            reopenedMetadata, secondBuild, error, buildOptions) &&
            CatalogueSignature(secondBuild) == CatalogueSignature(catalogue),
        "repeated catalogue build was not deterministic");

    AssetCatalogue reopenedCatalogue;
    Check(BuildAssetCatalogue(root.generic_u8string(), ProjectId, registry,
            reopenedMetadata, reopenedCatalogue, error),
        "reopened catalogue build failed");
    const auto* reopenedRock = FindEntry(reopenedCatalogue, RockId);
    Check(reopenedRock != nullptr && reopenedRock->assetId == RockId &&
            reopenedRock->state == AssetCatalogueState::Current &&
            reopenedRock->creatorTags == std::vector<std::string>({"architecture"}),
        "moved-source recovery did not preserve stable ID and metadata on reopen");

    AssetCatalogueMetadataDocument unknownMetadata = reopenedMetadata;
    AssetCatalogueMetadataRecord unknown;
    unknown.assetId = "50000000-0000-4000-8000-000000000001";
    unknown.creatorTags = {"orphan"};
    unknownMetadata.records.push_back(std::move(unknown));
    std::sort(unknownMetadata.records.begin(), unknownMetadata.records.end(),
        [](const auto& left, const auto& right)
        { return left.assetId < right.assetId; });
    AssetCatalogue rejectedCatalogue;
    Check(!BuildAssetCatalogue(root.generic_u8string(), ProjectId, registry,
            unknownMetadata, rejectedCatalogue, error) && !error.empty(),
        "metadata for an unknown asset ID did not fail closed");

    AssetRegistry wrongRegistry = registry;
    wrongRegistry.projectId = OtherProjectId;
    Check(!BuildAssetCatalogue(root.generic_u8string(), ProjectId, wrongRegistry,
            reopenedMetadata, rejectedCatalogue, error) && !error.empty(),
        "cross-project registry did not fail closed");

    AssetCatalogueMetadataDocument corruptDocument;
    Check(!DeserializeAssetCatalogueMetadata("{not-json", corruptDocument, error) &&
            !error.empty(),
        "corrupt asset metadata JSON was accepted");

    AssetBrowserService browser;
    const auto unsafe = browser.Scan(root.generic_u8string(), "../Saved");
    Check(!unsafe.succeeded && !unsafe.error.empty(),
        "Gate 2 regressed AssetBrowserService Content containment");

    std::error_code ignored;
    fs::remove_all(root, ignored);
    if (failures != 0)
    {
        std::cerr << failures << " LP07 Gate 2 check(s) failed\n";
        return 1;
    }

    std::cout
        << "PASS: LP07 Gate 2 stable catalogue, metadata, health, queries, and recovery\n";
    return 0;
}
