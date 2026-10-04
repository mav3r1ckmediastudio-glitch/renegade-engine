
#pragma once
#include "renegade/bridge/PlayerViewGripService.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "../WickedEngine/Editor/json.hpp"

inline void TestPlayerViewGripAuthoring()
{
    using namespace renegade::bridge;
    namespace fs = std::filesystem;
    std::string error;
    wi::scene::Scene scene;
    renegade::tests::CreatePlayerViewSocketAsset(scene);
    PlayerViewGripSettings initial;
    if (!CapturePlayerViewGrips(scene, initial, error) ||
        initial[0].bonePath.empty() || initial[1].bonePath.empty())
        Fail("capture original bone grips: " + error);
    auto desired = initial;
    desired[0].position = XMFLOAT3(0.03f, -0.02f, 0.09f);
    desired[0].rotationDegrees = XMFLOAT3(18, 32, -14);
    desired[1].position = XMFLOAT3(-0.04f, 0.01f, 0.08f);
    desired[2].bonePath = initial[0].bonePath;
    desired[2].position = XMFLOAT3(0, 0.05f, 0.12f);
    if (!ApplyPlayerViewGrips(scene, desired, error))
        Fail("apply grip offsets: " + error);
    PlayerViewGripSettings captured;
    if (!CapturePlayerViewGrips(scene, captured, error) ||
        !Near3(captured[0].position, desired[0].position) ||
        !Near3(captured[0].rotationDegrees, desired[0].rotationDegrees, 0.01f))
        Fail("grip position/rotation roundtrip: " + error + " position=" + std::to_string(captured[0].position.x) + "," + std::to_string(captured[0].position.y) + "," + std::to_string(captured[0].position.z) + " rotation=" + std::to_string(captured[0].rotationDegrees.x) + "," + std::to_string(captured[0].rotationDegrees.y) + "," + std::to_string(captured[0].rotationDegrees.z));

    auto invalid = desired;
    invalid[0].bonePath = "[\"Missing bone\"]";
    const auto entityCount = scene.transforms.GetCount();
    if (ApplyPlayerViewGrips(scene, invalid, error) ||
        scene.transforms.GetCount() != entityCount)
        Fail("missing bone authoring was not rejected atomically");
    auto* firstMeta = scene.metadatas.GetComponent(scene.armatures[0].boneCollection[0]);
    firstMeta->bool_values.set("renegade.player.view_grip.owned", true);
    if (ApplyPlayerViewGrips(scene, desired, error) ||
        scene.transforms.GetCount() != entityCount)
        Fail("owned marker on bone was not rejected atomically");
    firstMeta->bool_values.erase("renegade.player.view_grip.owned");

    const auto root = fs::temp_directory_path() / "renegade-p1-grip-authoring";
    fs::remove_all(root);
    fs::create_directories(root / "Content/Models");
    const auto assetPath = root / "Content/Models/grips.rasset";
    ReusableModelAssetDocument document;
    document.manifest.projectId = P1ProjectId;
    document.manifest.assetId = P1ArmsAssetId;
    document.manifest.sourceAssetId = P1SourceAssetId;
    document.manifest.sourceFormat = "fbx";
    document.manifest.importer = "wicked.ufbx";
    document.manifest.settingsJson = "{\"options\":{},\"source_format\":\"fbx\"}";
    wi::Archive archive;
    scene.Serialize(archive);
    archive.WriteData(document.payload);
    document.manifest.payloadHash = HashBytes(document.payload);
    std::vector<std::uint8_t> bytes;
    if (!SerializeReusableModelAssetDocument(document, bytes, error) ||
        !WriteBytes(assetPath, bytes))
        Fail("write governed grip fixture: " + error);
    ReusableModelManagedProjection projection;
    projection.projectId = P1ProjectId; projection.assetId = P1ArmsAssetId;
    projection.sourceAssetId = P1SourceAssetId;
    projection.sourceProjectRelativePath = "SourceAssets/Models/generated.fbx";
    projection.assetProjectRelativePath = "Content/Models/grips.rasset";
    projection.sourceFormat = "fbx"; projection.importer = "wicked.ufbx";
    projection.settingsJson = document.manifest.settingsJson;
    projection.payloadHash = document.manifest.payloadHash;
    projection.modelMetadata.known = true;
    projection.modelMetadata.armatureCount = 1;
    projection.modelMetadata.boneCount = 2;
    std::string projectionJson;
    const auto projectionPath = root / ResolveReusableModelManagedProjectionPath(projection.assetProjectRelativePath);
    if (!SerializeReusableModelManagedProjection(projection, projectionJson, error) ||
        !WriteText(projectionPath, projectionJson))
        Fail("write governed grip projection: " + error);
    AssetRegistry registry;
    registry.projectId = P1ProjectId;
    AssetRecord source;
    source.assetId = P1SourceAssetId;
    source.dependencyNodeId = "grip-source";
    source.projectRelativePath = projection.sourceProjectRelativePath;
    source.provider = "renegade.test";
    source.contentHash = "fnv1a64:0000000000000000";
    registry.records.push_back(source);
    AssetRecord product;
    product.assetId = P1ArmsAssetId;
    product.dependencyNodeId = "grip-product";
    product.projectRelativePath = projection.assetProjectRelativePath;
    product.provider = "wicked.ufbx";
    product.contentHash = HashBytes(bytes);
    registry.records.push_back(product);
    ImportedProductRecord provenance;
    provenance.sourceAssetId = P1SourceAssetId;
    provenance.productAssetId = P1ArmsAssetId;
    provenance.importer = "wicked.ufbx";
    provenance.settingsSchema = ReusableModelImportSettingsSchema;
    provenance.settingsVersion = 1;
    provenance.settingsJson = document.manifest.settingsJson;
    provenance.sourceContentHashAtImport = source.contentHash;
    provenance.productContentHashAtImport = product.contentHash;
    registry.importedProducts.push_back(provenance);
    if (!WriteAssetRegistry(root.generic_u8string(), registry).success)
        Fail("write governed grip registry");
    PlayerViewGripSession editor;
    if (!editor.Open(root.generic_u8string(), P1ProjectId, P1ArmsAssetId, error))
        Fail("open native grip editor: " + error);
    auto edited = editor.Settings()[0];
    edited.position.z = 0.24f;
    edited.rotationDegrees.y = 45;
    if (!editor.SetBinding(0, edited, error) || !editor.IsDirty() ||
        !editor.Undo() || editor.IsDirty() || !editor.Redo() || !editor.IsDirty())
        Fail("grip Undo/Redo/save boundary: " + error);
    const auto readBytes = [](const fs::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input), {});
    };
    std::string registryPath, registryError;
    if (!ResolveAssetRegistryDocumentPath(root.generic_u8string(), registryPath, registryError))
        Fail("resolve fixture registry: " + registryError);
    const auto beforeAsset = readBytes(assetPath);
    const auto beforeProjection = readBytes(projectionPath);
    const auto beforeRegistry = readBytes(fs::u8path(registryPath));
    if (editor.Save(error, [](ProjectDocumentTransactionStage stage, std::size_t index,
            const std::string&, std::string& message)
        {
            if (stage == ProjectDocumentTransactionStage::AfterReplace && index == 1)
            { message = "Injected grip save failure"; return ProjectDocumentTransactionHookAction::Fail; }
            return ProjectDocumentTransactionHookAction::Continue;
        }) || !editor.IsDirty() || readBytes(assetPath) != beforeAsset ||
        readBytes(projectionPath) != beforeProjection ||
        readBytes(fs::u8path(registryPath)) != beforeRegistry)
        Fail("grip save failed to roll back all three documents: " + error);
    if (!editor.Save(error) || editor.IsDirty())
        Fail("retry grip save: " + error);
    PlayerViewGripSession reopened;
    if (!reopened.Open(root.generic_u8string(), P1ProjectId, P1ArmsAssetId, error) ||
        !Near3(reopened.Settings()[0].position, edited.position) ||
        !Near3(reopened.Settings()[0].rotationDegrees, edited.rotationDegrees, 0.01f))
        Fail("saved grip asset reopen: " + error);
    if (!ReadReusableModelAssetDocument(assetPath.generic_u8string(), document, error))
        Fail("read authored grip product: " + error);
    CreatorModelImportRecipe recipe;
    if (!ParseCreatorModelImportOptions(nlohmann::json::parse(document.manifest.settingsJson).at("options").dump(), recipe, error) ||
        !recipe.hasHandGrips || recipe.handGrips[0].bonePath != edited.bonePath)
        Fail("saved reimport recipe lost hand grips: " + error);
    wi::scene::Scene reimported;
    renegade::tests::CreatePlayerViewSocketAsset(reimported);
    if (!ApplyCreatorModelImportRecipe(reimported, root.generic_u8string(), P1ProjectId, recipe, error) ||
        !CapturePlayerViewGrips(reimported, captured, error) ||
        !Near3(captured[0].position, edited.position))
        Fail("reimport recipe failed to restore authored grips: " + error);
    if (const char* output = std::getenv("RENEGADE_GRIP_FIXTURE_OUTPUT"))
    {
        fs::create_directories(fs::u8path(output));
        fs::copy(root, fs::u8path(output),
            fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    }
    auto stale = reopened.Settings()[1]; stale.position.x += 0.01f;
    if (!reopened.SetBinding(1, stale, error)) Fail("stale edit setup: " + error);
    std::ofstream external(assetPath, std::ios::binary | std::ios::app); external.put('x'); external.close();
    const auto externalBytes = readBytes(assetPath);
    if (reopened.Save(error) || readBytes(assetPath) != externalBytes)
        Fail("stale editor overwrote externally changed product");
    fs::remove_all(root);
    std::cout << "PASS: hand grip authoring Undo/Redo, native offset roundtrip, three-document rollback, save/reopen, reimport and stale-save rejection\n";
}
