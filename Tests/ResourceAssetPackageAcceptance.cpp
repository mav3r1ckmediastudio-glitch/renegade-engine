#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/PackageIntegrityService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/ResourceAssetService.h"
#include "renegade/bridge/WindowsGameBuildProjectService.h"
#include "renegade/bridge/WindowsGameBuildWorkflow.h"

// Reuse the already-accepted LP07 process, package-document, icon, workflow
// request and owner-build smoke helpers instead of cloning a second LP06 test
// harness. Only the lifecycle below is new LP08 evidence.
#define main RenegadeLP07PackageAcceptanceUnusedMain
#include "ReusableAssetPackageAcceptance.cpp"
#undef main

namespace
{
    using namespace renegade::bridge;

    const std::vector<std::uint8_t>& InitialPng()
    {
        static const std::vector<std::uint8_t> bytes = {
            137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,
            0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,
            0,0,0,13,73,68,65,84,120,156,99,248,207,192,240,
            31,0,5,0,1,255,137,153,61,29,0,0,0,0,73,69,78,
            68,174,66,96,130};
        return bytes;
    }

    const std::vector<std::uint8_t>& UpdatedPng()
    {
        static const std::vector<std::uint8_t> bytes = {
            137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,
            0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,
            0,0,0,13,73,68,65,84,120,156,99,96,248,207,240,
            31,0,4,1,1,255,113,235,71,229,0,0,0,0,73,69,78,
            68,174,66,96,130};
        return bytes;
    }

    bool WriteBinary(
        const fs::path& path,
        const std::vector<std::uint8_t>& bytes,
        std::string& error)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
        {
            error = "could not create resource fixture folder: " + ec.message();
            return false;
        }
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "could not create resource fixture file";
            return false;
        }
        output.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        if (!output)
        {
            error = "could not write complete resource fixture";
            return false;
        }
        error.clear();
        return true;
    }

    bool WriteEmptyAssetRegistry(const fs::path& projectRoot, std::string& error)
    {
        AssetRegistry registry;
        registry.projectId = ProjectId;
        registry.schemaVersion = AssetRegistry::CurrentSchemaVersion;
        std::string json;
        if (!SerializeAssetRegistry(registry, json, error))
            return false;
        return WriteText(projectRoot / AssetRegistryDocumentName, json);
    }

    bool SaveStableTextureScene(
        const fs::path& scenePath,
        const StableId& textureAssetId,
        std::string& error)
    {
        wi::scene::Scene scene;
        const wi::ecs::Entity materialEntity = wi::ecs::CreateEntity();
        scene.materials.Create(materialEntity);
        auto& metadata = scene.metadatas.Create(materialEntity);
        metadata.int_values.set(
            MaterialTextureAssetBindingVersionMetadataKey,
            MaterialTextureAssetBindingVersion);
        metadata.string_values.set(
            MaterialBaseColorTextureAssetIdMetadataKey,
            textureAssetId);

        try
        {
            wi::Archive archive(scenePath.generic_u8string(), false, false);
            if (!archive.IsOpen())
            {
                error = "could not create Gate 5 LevelOne WISCENE";
                return false;
            }
            archive.SetCompressionEnabled(true);
            scene.Serialize(archive);
            if (!archive.SaveFile(scenePath.generic_u8string()))
            {
                error = "could not save Gate 5 LevelOne WISCENE";
                return false;
            }
            archive = wi::Archive();
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
        error.clear();
        return true;
    }

    bool ValidateResourceRuntimeEvidence(
        const fs::path& evidencePath,
        const StableId& assetId,
        const std::string& currentPayloadHash,
        const std::string& previousPayloadHash,
        std::string& error)
    {
        std::string text;
        if (!ReadText(evidencePath, text))
        {
            error = "could not read packaged Runtime Gate 5 evidence";
            return false;
        }
        if (text.find("status=PASS") == std::string::npos ||
            text.find("package_relative_launch=true") == std::string::npos ||
            text.find("resource_asset_id=" + assetId) == std::string::npos ||
            text.find("payload_hash=" + currentPayloadHash) ==
                std::string::npos ||
            (previousPayloadHash != currentPayloadHash &&
                text.find("resource_asset_id=" + assetId +
                    " payload_hash=" + previousPayloadHash) !=
                    std::string::npos))
        {
            error =
                "packaged Runtime evidence did not prove the current governed resource payload";
            return false;
        }
        error.clear();
        return true;
    }

    bool RunResourceLifecycle(
        const fs::path& root,
        const fs::path& runtimePath,
        const fs::path& dxcPath,
        const fs::path& lp03FixtureRoot,
        const fs::path& packageDocRoot,
        const std::string& renegadeRevision,
        const std::string& wickedRevision)
    {
        std::error_code ec;
        fs::remove_all(root, ec);
        ec.clear();
        const fs::path projectRoot = root / "Source Project";
        const fs::path outputParent = root / "Build Output";
        const fs::path localAppData = root / "LocalAppData";
        const fs::path detachedCwd = root / "Detached CWD";
        const fs::path iconPath = root / "Build Inputs" / "Gate5Resource.ico";
        const fs::path externalTexture = root / "External" / "gate5.png";
        std::string error;

        if (!CopyTreeContents(lp03FixtureRoot, projectRoot, error))
            return Require(false, error);
        fs::create_directories(projectRoot / "Intermediate" / "Transactions", ec);
        fs::create_directories(detachedCwd, ec);
        fs::create_directories(localAppData, ec);
        if (!Require(!ec,
                "could not create Gate 5 project/runtime working directories") ||
            !Require(WriteIcon(iconPath, error), error) ||
            !Require(WriteEmptyAssetRegistry(projectRoot, error),
                "could not create Gate 5 LC01 registry: " + error) ||
            !Require(WriteBinary(externalTexture, InitialPng(), error), error))
            return false;

        const std::string descriptor =
            "format = renegade-project\n"
            "version = 1\n\n"
            "[project]\n"
            "project_id = 61111111-1111-4111-8111-111111111111\n"
            "name = Gate6Proof\n"
            "startup_scene = Content/Scenes/LevelOne.wiscene\n"
            "startup_flow_id = 62222222-2222-4222-8222-222222222222\n"
            "startup_flow = Content/Flow/Main.renegade-flow\n"
            "startup_screen_id = 63333333-3333-4333-8333-333333333333\n"
            "startup_screen = Content/UI/Main.renegade-screen\n";
        const fs::path descriptorPath = projectRoot / "ScreenProject.renegade";
        if (!Require(WriteText(descriptorPath, descriptor),
                "could not write Gate 5 project descriptor"))
            return false;

        CreatorTextureWorkflowService textureWorkflow;
        const CreatorTextureImportResult imported = textureWorkflow.ImportTexture(
            projectRoot.generic_u8string(), ProjectId,
            externalTexture.generic_u8string());
        if (!Require(imported.succeeded && imported.committed &&
                imported.asset.succeeded,
                "creator texture import failed: " + imported.error))
            return false;

        const StableId productId = imported.assetId;
        const StableId sourceId = imported.sourceAssetId;
        const fs::path retainedSource =
            projectRoot / fs::u8path(imported.sourceProjectRelativePath);
        const fs::path productPath =
            projectRoot / fs::u8path(imported.assetProjectRelativePath);
        if (!Require(IsValidStableId(productId) && IsValidStableId(sourceId),
                "texture import did not create stable identities") ||
            !Require(fs::is_regular_file(retainedSource) &&
                    fs::is_regular_file(productPath),
                "texture import did not retain source/governed product"))
            return false;

        ResourceAssetDocument initialDocument;
        if (!Require(ReadResourceAssetDocument(
                productPath.generic_u8string(), initialDocument, error),
                "could not read initial governed texture: " + error))
            return false;
        const std::string initialPayloadHash = initialDocument.manifest.payloadHash;

        // The scene persists only the governed stable ID. It is deliberately
        // never resaved after reimport, so the packaged Runtime must follow that
        // identity to the newly accepted product.
        const fs::path levelOne =
            projectRoot / "Content" / "Scenes" / "LevelOne.wiscene";
        if (!Require(SaveStableTextureScene(levelOne, productId, error), error))
            return false;

        if (!Require(WriteBinary(retainedSource, UpdatedPng(), error), error))
            return false;

        CreatorAssetWorkflowService catalogueWorkflow;
        AssetCatalogue catalogue;
        if (!Require(catalogueWorkflow.BuildCatalogue(
                projectRoot.generic_u8string(), ProjectId, catalogue, error),
                "could not refresh LC01 after texture edit: " + error))
            return false;

        ProjectService inspector;
        ProjectMetadata project;
        if (!Require(inspector.InspectProject(
                descriptorPath.generic_u8string(), project, error),
                "could not inspect Gate 5 build project: " + error))
            return false;

        WindowsGameBuildProjectState staleState;
        if (!Require(PrepareWindowsGameBuildProjectState(
                project, staleState, error),
                "could not prepare stale Gate 5 build state: " + error) ||
            !Require(GraphContainsAssetPath(
                    staleState.dependencyGraph,
                    imported.assetProjectRelativePath,
                    DependencyRequirement::Required),
                "saved material stable ID did not make governed texture reachable") ||
            !Require(GraphContainsAssetPath(
                    staleState.dependencyGraph,
                    imported.sourceProjectRelativePath,
                    DependencyRequirement::EditorOnly),
                "build freshness did not retain editor-only texture source"))
            return false;

        WindowsGameBuildWorkflowRequest staleRequest;
        if (!Require(MakeWorkflowRequest(
                project, staleState, runtimePath, dxcPath, packageDocRoot,
                outputParent, iconPath, renegadeRevision, wickedRevision,
                "lp08-gate5-stale", staleRequest, error),
                "could not compose stale Gate 5 owner build: " + error))
            return false;

        const fs::path previousFinal =
            outputParent / (std::string(GameName) + " Windows Build");
        const fs::path sentinel = previousFinal / "last-good-sentinel.txt";
        if (!Require(WriteText(sentinel, "last-good-build\n"),
                "could not create Gate 5 last-good output sentinel"))
            return false;
        bool staleSmokeCalled = false;
        WindowsGameBuildWorkflowResult staleBuild;
        const bool staleBuilt = BuildWindowsGame(
            staleRequest,
            [&staleSmokeCalled](
                const WindowsGameBuildPlan&,
                const WindowsGameBuildStageResult&,
                std::string&,
                std::string& smokeError)
            {
                staleSmokeCalled = true;
                smokeError = "stale resource build reached smoke unexpectedly";
                return false;
            },
            staleBuild,
            error);
        std::string sentinelText;
        if (!Require(!staleBuilt && !staleSmokeCalled &&
                error.find("stale imported product") != std::string::npos,
                "stale governed texture did not fail owner build freshness: " +
                    error) ||
            !Require(ReadText(sentinel, sentinelText) &&
                    sentinelText == "last-good-build\n",
                "failed stale resource build changed prior owner output"))
            return false;
        fs::remove_all(previousFinal, ec);
        ec.clear();

        ResourceAssetReimportRequest reimportRequest;
        reimportRequest.projectRoot = projectRoot.generic_u8string();
        reimportRequest.projectId = ProjectId;
        reimportRequest.assetId = productId;
        const ResourceAssetReimportResult reimported =
            ResourceAssetService().ReimportResourceAsset(reimportRequest);
        if (!Require(reimported.succeeded &&
                reimported.assetId == productId &&
                reimported.sourceAssetId == sourceId,
                "explicit texture reimport failed or changed stable identity: " +
                    reimported.error))
            return false;

        ResourceAssetDocument currentDocument;
        if (!Require(ReadResourceAssetDocument(
                productPath.generic_u8string(), currentDocument, error),
                "could not read current governed texture: " + error))
            return false;
        const std::string currentPayloadHash = currentDocument.manifest.payloadHash;
        if (!Require(!currentPayloadHash.empty() &&
                currentPayloadHash != initialPayloadHash,
                "texture reimport did not change governed payload hash"))
            return false;

        // Deliberately do not resave LevelOne here.
        WindowsGameBuildProjectState currentState;
        if (!Require(PrepareWindowsGameBuildProjectState(
                project, currentState, error),
                "could not prepare current Gate 5 build state: " + error))
            return false;

        WindowsGameBuildWorkflowRequest currentRequest;
        if (!Require(MakeWorkflowRequest(
                project, currentState, runtimePath, dxcPath, packageDocRoot,
                outputParent, iconPath, renegadeRevision, wickedRevision,
                "lp08-gate5-current", currentRequest, error),
                "could not compose current Gate 5 owner build: " + error))
            return false;

        const std::wstring localAppDataWide = localAppData.wstring();
        if (!Require(SetEnvironmentVariableW(
                L"LOCALAPPDATA", localAppDataWide.c_str()) != FALSE,
                "could not isolate Gate 5 Runtime LOCALAPPDATA"))
            return false;
        const fs::path evidencePath =
            localAppData / "RenegadeEngine" / ProjectId /
            "Logs" / "RuntimeBootstrap.log";
        const std::size_t completionCount = currentState.levelCompletionCount;
        if (!Require(completionCount > 0,
                "Gate 5 owner build produced no Story Flow completion count"))
            return false;

        WindowsGameBuildWorkflowResult built;
        const WindowsGameBuildSmokeRunner smoke =
            [&](const WindowsGameBuildPlan& plan,
                const WindowsGameBuildStageResult& stage,
                std::string& runtimeEvidencePath,
                std::string& smokeError)
            {
                if (!PlanContainsAsset(
                        plan, productId, imported.assetProjectRelativePath))
                {
                    smokeError =
                        "LP06 plan omitted reachable governed texture product";
                    return false;
                }
                if (PlanContainsAsset(
                        plan, sourceId, imported.sourceProjectRelativePath) ||
                    fs::exists(fs::u8path(stage.stagingPath) /
                        "GameData" /
                        fs::u8path(imported.sourceProjectRelativePath)))
                {
                    smokeError =
                        "editor-only texture source leaked into Runtime package";
                    return false;
                }

                std::error_code removeError;
                fs::remove(evidencePath, removeError);
                DWORD exitCode = 1;
                const fs::path executable =
                    fs::u8path(stage.stagingPath) / plan.executableFileName;
                if (!RunProcess(
                        executable,
                        SmokeArguments(completionCount),
                        detachedCwd,
                        exitCode,
                        smokeError))
                    return false;
                if (exitCode != 0)
                {
                    smokeError =
                        "staged named Runtime returned " +
                        std::to_string(exitCode);
                    return false;
                }
                if (!ValidateResourceRuntimeEvidence(
                        evidencePath, productId,
                        currentPayloadHash, initialPayloadHash,
                        smokeError))
                    return false;
                runtimeEvidencePath = evidencePath.generic_u8string();
                return true;
            };

        if (!Require(BuildWindowsGame(
                currentRequest, smoke, built, error),
                "current Gate 5 owner Windows build failed: " + error) ||
            !Require(built.promotion.succeeded &&
                    !built.finalOutputPath.empty(),
                "current Gate 5 build was not safely promoted"))
            return false;

        const fs::path finalPackage = fs::u8path(built.finalOutputPath);
        WindowsGamePackageIntegrityResult integrity;
        if (!Require(ValidateWindowsGamePackage(
                finalPackage.generic_u8string(), integrity, error),
                "promoted Gate 5 package failed SHA-256 integrity: " + error))
            return false;

        std::string contentManifest;
        if (!Require(ReadText(
                finalPackage / "GameData" / "content-manifest.json",
                contentManifest),
                "could not read promoted Gate 5 content manifest") ||
            !Require(contentManifest.find(productId) != std::string::npos &&
                    contentManifest.find(imported.assetProjectRelativePath) !=
                        std::string::npos,
                "promoted manifest omitted governed texture stable identity/path") ||
            !Require(contentManifest.find(imported.sourceProjectRelativePath) ==
                    std::string::npos &&
                    !fs::exists(finalPackage / "GameData" /
                        fs::u8path(imported.sourceProjectRelativePath)),
                "promoted package contains retained texture source"))
            return false;

        // Final proof: remove the entire source project and launch the promoted
        // named executable directly from an unrelated working directory.
        fs::remove(evidencePath, ec);
        ec.clear();
        fs::remove_all(projectRoot, ec);
        if (!Require(!ec && !fs::exists(projectRoot),
                "could not remove Gate 5 source project before final launch"))
            return false;
        DWORD finalExitCode = 1;
        const fs::path finalExecutable =
            finalPackage / (std::string(GameName) + ".exe");
        if (!Require(RunProcess(
                finalExecutable,
                SmokeArguments(completionCount),
                detachedCwd,
                finalExitCode,
                error),
                "final Gate 5 named Runtime process failed: " + error) ||
            !Require(finalExitCode == 0,
                "final Gate 5 named Runtime returned " +
                    std::to_string(finalExitCode)) ||
            !Require(ValidateResourceRuntimeEvidence(
                evidencePath, productId,
                currentPayloadHash, initialPayloadHash,
                error),
                "final Gate 5 Runtime resource evidence failed: " + error))
            return false;

        fs::remove_all(root, ec);
        std::cout
            << "LP08 GATE 5 PACKAGE ACCEPTANCE PASS // asset_id="
            << productId
            << " old_payload=" << initialPayloadHash
            << " current_payload=" << currentPayloadHash
            << " source_excluded=true direct_promoted_launch=true\n";
        return true;
    }
}

int main(int argc, char** argv)
{
    if (argc != 7)
    {
        std::cerr
            << "Usage: RenegadeResourceAssetPackageAcceptance "
            << "<runtime.exe> <dxcompiler.dll> <LP03 fixture root> "
            << "<package docs root> <renegade sha> <wicked sha>\n";
        return 2;
    }

    const fs::path runtimePath = fs::weakly_canonical(fs::u8path(argv[1]));
    const fs::path dxcPath = fs::weakly_canonical(fs::u8path(argv[2]));
    const fs::path lp03FixtureRoot = fs::weakly_canonical(fs::u8path(argv[3]));
    const fs::path packageDocRoot = fs::weakly_canonical(fs::u8path(argv[4]));
    const std::string renegadeRevision = argv[5];
    const std::string wickedRevision = argv[6];

    if (!Require(fs::is_regular_file(runtimePath),
            "Release Runtime executable missing") ||
        !Require(fs::is_regular_file(dxcPath), "dxcompiler.dll missing") ||
        !Require(fs::is_directory(lp03FixtureRoot), "LP03 fixture root missing") ||
        !Require(fs::is_directory(packageDocRoot), "package-doc fixture root missing") ||
        !Require(renegadeRevision.size() == 40 && wickedRevision.size() == 40,
            "exact source revisions were not supplied"))
        return 3;

    const auto nonce = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        fs::u8path(u8"Renegade LP08 Gate5 Resource Ω " +
            std::to_string(nonce));
    return RunResourceLifecycle(
        root, runtimePath, dxcPath, lp03FixtureRoot, packageDocRoot,
        renegadeRevision, wickedRevision) ? 0 : 4;
}
