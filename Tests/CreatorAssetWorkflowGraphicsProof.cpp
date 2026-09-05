#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/CreatorAssetActionPolicy.h"
#include "renegade/bridge/ImportService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SelectionService.h"

#include <WickedEngine.h>
#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr wchar_t WindowClassName[] = L"RenegadeLP07Gate5CreatorAssetProofWindow";
    constexpr const char* ProjectId = "88888888-8888-4888-8888-888888888888";

    LRESULT CALLBACK WindowProc(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        if (message == WM_CLOSE)
        {
            DestroyWindow(window);
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    bool Require(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "LP07 GATE 5 PROOF FAIL // " << message << '\n';
        return false;
    }

    const renegade::bridge::AssetCatalogueEntry* FindEntry(
        const renegade::bridge::AssetCatalogue& catalogue,
        const renegade::bridge::StableId& id)
    {
        const auto found = std::find_if(
            catalogue.entries.begin(), catalogue.entries.end(),
            [&id](const auto& entry)
            {
                return entry.registered && entry.assetId == id;
            });
        return found == catalogue.entries.end() ? nullptr : &*found;
    }

    bool PrepareProject(const fs::path& root)
    {
        std::error_code ec;
        fs::remove_all(root, ec);
        ec.clear();
        fs::create_directories(root / "Content" / "Models", ec);
        if (ec) return false;
        fs::create_directories(root / "SourceAssets" / "Models", ec);
        if (ec) return false;
        fs::create_directories(root / "Intermediate" / "Transactions", ec);
        return !ec;
    }

    bool RequireCatalogueQuery(
        const renegade::bridge::AssetCatalogue& catalogue,
        const renegade::bridge::StableId& assetId)
    {
        using namespace renegade::bridge;

        AssetCatalogueQuery byName;
        byName.text = "maya_transformed_skin";
        const auto nameMatches = QueryAssetCatalogue(catalogue, byName);
        if (!Require(std::any_of(nameMatches.begin(), nameMatches.end(),
                [&assetId](const auto& entry) { return entry.assetId == assetId; }),
                "catalogue name search did not find the imported FBX product"))
            return false;

        AssetCatalogueQuery byTag;
        byTag.tags = {"hero", "gate5"};
        const auto tagMatches = QueryAssetCatalogue(catalogue, byTag);
        if (!Require(std::any_of(tagMatches.begin(), tagMatches.end(),
                [&assetId](const auto& entry) { return entry.assetId == assetId; }),
                "catalogue creator-tag search did not find the imported product"))
            return false;

        AssetCatalogueQuery bySystemMetadata;
        bySystemMetadata.sourceFormat = "fbx";
        bySystemMetadata.skinned = true;
        bySystemMetadata.animated = true;
        const auto metadataMatches = QueryAssetCatalogue(catalogue, bySystemMetadata);
        return Require(std::any_of(metadataMatches.begin(), metadataMatches.end(),
                [&assetId](const auto& entry) { return entry.assetId == assetId; }),
            "catalogue FBX/skinned/animated filter did not find the imported product");
    }

    bool ExecutePlacement(
        renegade::bridge::CreatorAssetWorkflowService& workflow,
        const fs::path& projectRoot,
        const renegade::bridge::StableId& assetId,
        renegade::bridge::SceneService& scenes,
        renegade::bridge::CommandService& commands,
        const XMFLOAT3& position,
        wi::ecs::Entity& placedEntity)
    {
        using namespace renegade::bridge;
        auto prepared = workflow.PrepareModelPlacement(
            projectRoot.generic_u8string(), ProjectId, assetId);
        if (!Require(prepared.IsReady(),
                "registered RAsset could not be prepared for placement: " +
                    prepared.Result().error))
            return false;

        if (!Require(prepared.Result().sceneSummary.meshes > 0 &&
                prepared.Result().sceneSummary.objects > 0,
                "prepared RAsset did not expose placeable model content"))
            return false;

        auto command = std::make_unique<PlaceImportedModelCommand>(
            scenes.GetScene(), prepared.ReleaseScene(), position,
            ImportService::ResolveScaleFactor(
                ModelScaleMode::Automatic, *prepared.PeekScene()));
        // ResolveScaleFactor cannot inspect after ReleaseScene(), so use the
        // accepted automatic result before relinquishing the payload.
        return false;
    }

    bool RunLifecycle(
        const fs::path& projectRoot,
        const fs::path& staticFixture,
        const fs::path& animatedFixture)
    {
        using namespace renegade::bridge;
        if (!Require(PrepareProject(projectRoot), "project setup failed"))
            return false;

        CreatorAssetWorkflowService workflow;

        // The creator importer already paid the conversion cost to create its
        // visible preview. Prove that exact prepared scene can be retargeted to
        // byte-identical project-owned source bytes, while even a one-byte source
        // change is rejected before governed commit.
        ImportService importer;
        ModelImportRequest previewRequest;
        previewRequest.sourcePath = animatedFixture.generic_u8string();
        previewRequest.assetPath =
            (projectRoot / "Intermediate" / "Imports" / ".retained-preview.wiscene")
                .generic_u8string();
        previewRequest.expectedFormat = ModelSourceFormat::Fbx;
        auto retained = importer.PrepareModelAsset(previewRequest);
        if (!Require(retained.IsReady(),
                "representative FBX could not be prepared once for retained creator commit: " +
                    retained.Result().error))
            return false;

        std::error_code retainedEc;
        const fs::path identicalSource =
            projectRoot / "Intermediate" / "Imports" / "retained-identical.fbx";
        fs::create_directories(identicalSource.parent_path(), retainedEc);
        if (!Require(!retainedEc, "could not create retained-import proof folder"))
            return false;
        fs::copy_file(animatedFixture, identicalSource,
            fs::copy_options::overwrite_existing, retainedEc);
        if (!Require(!retainedEc, "could not copy retained-import proof source"))
            return false;

        ModelImportRequest retargetRequest;
        retargetRequest.sourcePath = identicalSource.generic_u8string();
        retargetRequest.assetPath =
            (projectRoot / "Intermediate" / "Imports" / ".retargeted-preview.wiscene")
                .generic_u8string();
        retargetRequest.expectedFormat = ModelSourceFormat::Fbx;
        std::string retargetError;
        if (!Require(importer.RetargetPreparedModelAsset(
                retained, retargetRequest, retargetError),
                "byte-identical retained source was rejected: " + retargetError))
            return false;

        {
            std::ofstream mutate(identicalSource, std::ios::binary | std::ios::app);
            mutate.put('X');
        }
        if (!Require(!importer.RetargetPreparedModelAsset(
                retained, retargetRequest, retargetError),
                "mutated retained source was not rejected fail-closed"))
            return false;

        retainedEc.clear();
        fs::copy_file(animatedFixture, identicalSource,
            fs::copy_options::overwrite_existing, retainedEc);
        if (!Require(!retainedEc, "could not restore retained-import proof source") ||
            !Require(importer.RetargetPreparedModelAsset(
                retained, retargetRequest, retargetError),
                "restored byte-identical retained source was rejected: " + retargetError))
            return false;

        PreparedReusableModelPlacement importedPlacement;
        auto imported = workflow.ImportModel(
            projectRoot.generic_u8string(), ProjectId,
            animatedFixture.generic_u8string(),
            "{}", {}, "Content/Models", std::move(retained), {},
            &importedPlacement);
        if (!Require(imported.succeeded && imported.asset.succeeded &&
                imported.asset.transaction.committed,
                "creator FBX import failed: " + imported.error) ||
            !Require(imported.asset.modelMetadata.known &&
                    imported.asset.modelMetadata.skinned &&
                    imported.asset.modelMetadata.animated,
                "representative FBX did not retain skinned/animated metadata"))
            return false;

        if (!Require(importedPlacement.IsReady(),
                "successful creator import did not return an in-memory placement handoff") ||
            !Require(importedPlacement.Result().assetId == imported.asset.assetId &&
                    importedPlacement.Result().sourceAssetId == imported.asset.sourceAssetId &&
                    importedPlacement.Result().assetProjectRelativePath ==
                        imported.assetProjectRelativePath,
                "in-memory placement handoff identity/path differs from committed RAsset") ||
            !Require(importedPlacement.PeekScene() != nullptr &&
                    importedPlacement.Result().sceneSummary.meshes > 0 &&
                    importedPlacement.Result().sceneSummary.objects > 0 &&
                    ImportService::MeasureModelBounds(*importedPlacement.PeekScene()).valid,
                "in-memory placement handoff is not immediately measurable/placeable"))
            return false;

        const StableId sourceId = imported.asset.sourceAssetId;
        const StableId productId = imported.asset.assetId;
        if (!Require(IsValidStableId(sourceId) && IsValidStableId(productId),
                "creator import did not create stable source/product IDs") ||
            !Require(fs::is_regular_file(
                    projectRoot / fs::u8path(imported.stagedSourceProjectRelativePath)),
                "creator import did not retain the project-owned FBX source") ||
            !Require(fs::is_regular_file(
                    projectRoot / fs::u8path(imported.assetProjectRelativePath)),
                "creator import did not create the governed RAsset product"))
            return false;

        std::string error;
        if (!Require(workflow.SetCreatorTags(
                projectRoot.generic_u8string(), ProjectId, productId,
                {"Hero", "gate5"}, error),
                "creator tags could not be persisted: " + error))
            return false;

        AssetCatalogue committedSnapshot;
        if (!Require(workflow.BuildCatalogueSnapshot(
                projectRoot.generic_u8string(), ProjectId, committedSnapshot, error),
                "committed creator catalogue snapshot failed: " + error))
            return false;
        const auto* committedEntry = FindEntry(committedSnapshot, productId);
        if (!Require(committedEntry != nullptr &&
                committedEntry->projectRelativePath ==
                    imported.assetProjectRelativePath &&
                committedEntry->state == AssetCatalogueState::Current &&
                CanPlaceCreatorModelAsset(*committedEntry),
                "committed browser snapshot did not expose the exact stable-ID/path as a placeable model"))
            return false;

        AssetCatalogue catalogue;
        if (!Require(workflow.BuildCatalogue(
                projectRoot.generic_u8string(), ProjectId, catalogue, error),
                "creator catalogue build failed: " + error))
            return false;
        const auto* entry = FindEntry(catalogue, productId);
        if (!Require(entry != nullptr &&
                entry->state == AssetCatalogueState::Current &&
                entry->sourceFormat == "fbx" &&
                entry->model.skinned && entry->model.animated,
                "creator catalogue did not expose current FBX/system metadata") ||
            !Require(CanPlaceCreatorModelAsset(*entry),
                "creator catalogue entry was not accepted as a placeable model") ||
            !Require(entry->creatorTags == std::vector<std::string>({"gate5", "hero"}),
                "creator tags were not canonicalised/persisted") ||
            !RequireCatalogueQuery(catalogue, productId))
            return false;

        const fs::path importedFolder = fs::u8path(
            imported.assetProjectRelativePath).parent_path();
        const AssetBrowserSnapshot browserSnapshot = AssetBrowserService().Scan(
            projectRoot.generic_u8string(), importedFolder.generic_u8string());
        if (!Require(browserSnapshot.succeeded,
                "Asset Browser could not scan the imported product folder: " +
                    browserSnapshot.error) ||
            !Require(std::any_of(
                    browserSnapshot.assets.begin(), browserSnapshot.assets.end(),
                    [&imported](const AssetEntry& asset)
                    {
                        return !asset.directory &&
                            asset.projectRelativePath ==
                                imported.assetProjectRelativePath &&
                            asset.type == AssetType::Model;
                    }),
                "Asset Browser did not expose the newly imported .rasset product"))
            return false;

        // Placement must consume only the registered RAsset. Prove this before
        // the scene lifecycle by temporarily removing the authoritative source.
        const fs::path retainedSource =
            projectRoot / fs::u8path(imported.stagedSourceProjectRelativePath);
        const fs::path temporarilyMissing = retainedSource.string() + ".gate5-missing";
        std::error_code ec;
        fs::rename(retainedSource, temporarilyMissing, ec);
        if (!Require(!ec, "could not temporarily remove retained source"))
            return false;
        auto sourceIndependentPlacement = workflow.PrepareModelPlacement(
            projectRoot.generic_u8string(), ProjectId, productId);
        const bool sourceIndependentReady = sourceIndependentPlacement.IsReady();
        fs::rename(temporarilyMissing, retainedSource, ec);
        if (!Require(!ec, "could not restore retained source") ||
            !Require(sourceIndependentReady,
                "RAsset placement consulted/reconverted the missing FBX source"))
            return false;

        SceneService scenes;
        SelectionService selection;
        CommandService commands;
        ProjectService projects;
        SceneDocumentService documents(scenes, selection, commands, projects);
        scenes.NewScene();

        auto place = [&](const XMFLOAT3& position, wi::ecs::Entity& entity)
        {
            auto prepared = workflow.PrepareModelPlacement(
                projectRoot.generic_u8string(), ProjectId, productId);
            if (!Require(prepared.IsReady(),
                    "repeat RAsset placement preparation failed: " +
                        prepared.Result().error))
                return false;
            const wi::scene::Scene* preparedScene = prepared.PeekScene();
            const float scale = ImportService::ResolveScaleFactor(
                ModelScaleMode::Automatic, *preparedScene);
            auto command = std::make_unique<PlaceImportedModelCommand>(
                scenes.GetScene(), prepared.ReleaseScene(), position, scale);
            auto* raw = command.get();
            if (!Require(commands.Execute(std::move(command)),
                    "PlaceImportedModelCommand failed"))
                return false;
            entity = raw->PlacedEntity();
            return Require(entity != wi::ecs::INVALID_ENTITY,
                "placement command produced no entity");
        };

        wi::ecs::Entity first = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity second = wi::ecs::INVALID_ENTITY;
        if (!place(XMFLOAT3(0.0f, 0.0f, 0.0f), first))
            return false;
        const std::size_t afterFirst = scenes.EntityCount();
        if (!place(XMFLOAT3(2.0f, 0.0f, 0.0f), second))
            return false;
        const std::size_t afterSecond = scenes.EntityCount();
        if (!Require(afterFirst > 0 && afterSecond > afterFirst &&
                commands.UndoCount() == 2 && commands.IsDirty(),
                "two RAsset placements did not enter the normal command stack") ||
            !Require(commands.Undo() && scenes.EntityCount() == afterFirst &&
                    commands.CanRedo(),
                "Undo did not remove the second RAsset placement") ||
            !Require(commands.Redo() && scenes.EntityCount() == afterSecond,
                "Redo did not restore the second RAsset placement"))
            return false;

        const fs::path scenePath = projectRoot / "Content" / "Gate5Placement.wiscene";
        if (!Require(documents.Save(scenePath.generic_u8string()),
                "WISCENE save failed: " + scenes.LastError()) ||
            !Require(!commands.IsDirty(),
                "successful WISCENE save did not mark placement history saved"))
            return false;
        documents.NewScene();
        if (!Require(scenes.EntityCount() == 0,
                "scene close/new did not clear placed entities") ||
            !Require(documents.Open(scenePath.generic_u8string()),
                "saved WISCENE reopen failed: " + scenes.LastError()) ||
            !Require(scenes.EntityCount() == afterSecond,
                "WISCENE reopen did not preserve both RAsset placements"))
            return false;

        // Real source change must be projected as Stale before explicit
        // stable-ID reimport, then return to Current without changing identity.
        fs::copy_file(staticFixture, retainedSource,
            fs::copy_options::overwrite_existing, ec);
        if (!Require(!ec, "could not update retained FBX source"))
            return false;
        if (!Require(workflow.BuildCatalogue(
                projectRoot.generic_u8string(), ProjectId, catalogue, error),
                "stale catalogue refresh failed: " + error))
            return false;
        entry = FindEntry(catalogue, productId);
        if (!Require(entry != nullptr && entry->state == AssetCatalogueState::Stale,
                "changed FBX source was not exposed as Stale"))
            return false;

        const auto reimported = workflow.ReimportModel(
            projectRoot.generic_u8string(), ProjectId, productId);
        if (!Require(reimported.succeeded &&
                reimported.assetId == productId &&
                reimported.sourceAssetId == sourceId,
                "stable-ID explicit reimport failed or changed identity: " +
                    reimported.error) ||
            !Require(workflow.BuildCatalogue(
                projectRoot.generic_u8string(), ProjectId, catalogue, error),
                "post-reimport catalogue refresh failed: " + error))
            return false;
        entry = FindEntry(catalogue, productId);
        if (!Require(entry != nullptr && entry->state == AssetCatalogueState::Current,
                "successful reimport did not return the product to Current") ||
            !Require(entry->creatorTags == std::vector<std::string>({"gate5", "hero"}),
                "creator tags were lost across explicit reimport"))
            return false;

        // Compatibility regression: accepted GLB/GLTF support remains enabled.
        return Require(
            ImportService::IsModelSourceFormatSupported(ModelSourceFormat::Gltf) &&
            ImportService::IsModelSourceFormatSupported(ModelSourceFormat::Glb),
            "GLB/GLTF compatibility contract regressed");
    }
}

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "Usage: RenegadeCreatorAssetWorkflowGraphicsProof "
            << "<static.fbx> <skinned-animated.fbx> <output-directory>\n";
        return 2;
    }

    const fs::path staticFixture = fs::weakly_canonical(fs::u8path(argv[1]));
    const fs::path animatedFixture = fs::weakly_canonical(fs::u8path(argv[2]));
    const fs::path outputRoot = fs::absolute(fs::u8path(argv[3]));
    if (!Require(fs::is_regular_file(staticFixture), "static FBX fixture missing") ||
        !Require(fs::is_regular_file(animatedFixture),
            "skinned/animated FBX fixture missing"))
        return 3;

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = WindowClassName;
    RegisterClassExW(&windowClass);
    const HWND window = CreateWindowExW(0, WindowClassName,
        L"Renegade LP07 Gate 5 Creator Asset Proof", WS_OVERLAPPEDWINDOW,
        0, 0, 64, 64, nullptr, nullptr, instance, nullptr);
    if (!Require(window != nullptr, "could not create Gate 5 graphics proof window"))
        return 4;

    int exitCode = 0;
    {
        wi::Application application;
        application.allow_hdr = false;
        application.SetWindow(window);
        if (!Require(wi::graphics::GetDevice() != nullptr,
                "Wicked graphics device was not initialized"))
        {
            exitCode = 5;
        }
        else if (!RunLifecycle(outputRoot / "creator-asset-project",
                staticFixture, animatedFixture))
        {
            exitCode = 6;
        }
    }

    if (IsWindow(window))
        DestroyWindow(window);
    UnregisterClassW(WindowClassName, instance);
    if (exitCode == 0)
        std::cout << "LP07 GATE 5 CREATOR ASSET WORKFLOW PROOF PASS\n";
    return exitCode;
}
