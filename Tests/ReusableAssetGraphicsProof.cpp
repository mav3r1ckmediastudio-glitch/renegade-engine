#include "renegade/bridge/ReusableAssetService.h"

#include <WickedEngine.h>
#include <wiJobSystem.h>

#include <Windows.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr wchar_t WindowClassName[] = L"RenegadeLP07ReusableAssetProofWindow";
    constexpr const char* ProjectId = "55555555-5555-4555-8555-555555555555";

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
        std::cerr << "LP07 GATE 3 PROOF FAIL // " << message << '\n';
        return false;
    }

    bool ReadBytes(const fs::path& path, std::vector<std::uint8_t>& bytes)
    {
        bytes.clear();
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return false;
        input.seekg(0, std::ios::end);
        const auto size = input.tellg();
        if (size < 0)
            return false;
        input.seekg(0, std::ios::beg);
        bytes.resize(static_cast<std::size_t>(size));
        if (!bytes.empty())
            input.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        return input.good() || input.eof();
    }

    bool WriteBytes(const fs::path& path, const std::vector<std::uint8_t>& bytes)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            return false;
        if (!bytes.empty())
            output.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        return static_cast<bool>(output);
    }

    bool WriteText(const fs::path& path, const std::string& text)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            return false;
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(output);
    }

    std::vector<std::uint8_t> ThumbnailFixture()
    {
        return {0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au,
            0x52u, 0x45u, 0x4eu, 0x45u, 0x47u, 0x41u, 0x44u, 0x45u};
    }

    std::vector<std::string> CollectAnimationNames(const wi::scene::Scene& scene)
    {
        std::vector<std::string> names;
        for (std::size_t index = 0; index < scene.animations.GetCount(); ++index)
        {
            const auto entity = scene.animations.GetEntity(index);
            const auto* name = scene.names.GetComponent(entity);
            if (name != nullptr && !name->name.empty())
                names.push_back(name->name);
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    bool LoadPayloadScene(
        const fs::path& path,
        const std::vector<std::uint8_t>& payload,
        wi::scene::Scene& scene,
        std::string& error)
    {
        if (!WriteBytes(path, payload))
        {
            error = "could not stage internal WISCENE payload for reopen proof";
            return false;
        }
        wi::Archive archive(path.generic_u8string(), true, false);
        if (!archive.IsOpen())
        {
            error = "could not open internal WISCENE payload";
            return false;
        }
        scene.Serialize(archive);
        if (archive.GetPos() != archive.GetSize())
        {
            error = "internal WISCENE payload had trailing or incomplete data";
            return false;
        }
        error.clear();
        return true;
    }

    const renegade::bridge::AssetRecord* FindRecord(
        const renegade::bridge::AssetRegistry& registry,
        const renegade::bridge::StableId& id)
    {
        const auto found = std::find_if(registry.records.begin(), registry.records.end(),
            [&id](const auto& record) { return record.assetId == id; });
        return found == registry.records.end() ? nullptr : &*found;
    }

    const renegade::bridge::AssetCatalogueEntry* FindCatalogueEntry(
        const renegade::bridge::AssetCatalogue& catalogue,
        const renegade::bridge::StableId& id)
    {
        const auto found = std::find_if(catalogue.entries.begin(), catalogue.entries.end(),
            [&id](const auto& entry) { return entry.registered && entry.assetId == id; });
        return found == catalogue.entries.end() ? nullptr : &*found;
    }

    bool RunReusableImportCase(
        const char* label,
        const fs::path& projectRoot,
        const std::string& sourceRelative,
        const std::string& assetRelative,
        const renegade::bridge::ModelSourceFormat expectedFormat,
        const char* expectedFormatToken,
        const bool requireSkin,
        const bool requireAnimation)
    {
        using namespace renegade::bridge;
        const fs::path sourcePath = projectRoot / fs::u8path(sourceRelative);
        const fs::path assetPath = projectRoot / fs::u8path(assetRelative);
        std::vector<std::uint8_t> sourceBefore;
        if (!Require(ReadBytes(sourcePath, sourceBefore),
                std::string(label) + ": could not read project-owned source before import"))
            return false;

        ReusableModelImportRequest request;
        request.projectRoot = projectRoot.generic_u8string();
        request.projectId = ProjectId;
        request.sourceProjectRelativePath = sourceRelative;
        request.assetProjectRelativePath = assetRelative;
        request.expectedFormat = expectedFormat;
        request.thumbnailPngBytes = ThumbnailFixture();

        ReusableAssetService service;
        const auto result = service.ImportModelAsset(request);
        if (!Require(result.succeeded,
                std::string(label) + ": reusable import failed: " + result.error) ||
            !Require(result.transaction.committed,
                std::string(label) + ": transaction did not commit") ||
            !Require(IsValidStableId(result.sourceAssetId) && IsValidStableId(result.assetId) &&
                    result.sourceAssetId != result.assetId,
                std::string(label) + ": stable source/product identity was not established") ||
            !Require(fs::is_regular_file(assetPath),
                std::string(label) + ": .rasset product was not created") ||
            !Require(result.import.imported == result.import.reloaded,
                std::string(label) + ": converter WISCENE structure changed before container commit") ||
            !Require(result.import.importedEvidence == result.import.reloadedEvidence,
                std::string(label) + ": converter rig/animation evidence changed before container commit"))
        {
            return false;
        }

        const fs::path projectionPath = projectRoot / fs::u8path(
            ResolveReusableModelManagedProjectionPath(assetRelative));
        const fs::path thumbnailPath = projectRoot / fs::u8path(
            ResolveReusableModelThumbnailPath(assetRelative));
        std::vector<std::uint8_t> projectionBytes;
        std::vector<std::uint8_t> thumbnailBytes;
        if (!Require(fs::is_regular_file(projectionPath) &&
                fs::is_regular_file(thumbnailPath),
                std::string(label) + ": physical creator package is incomplete") ||
            !Require(ReadBytes(projectionPath, projectionBytes),
                std::string(label) + ": managed projection could not be read") ||
            !Require(ReadBytes(thumbnailPath, thumbnailBytes) &&
                    thumbnailBytes == request.thumbnailPngBytes,
                std::string(label) + ": thumbnail bytes did not commit atomically"))
        {
            return false;
        }
        const std::string projectionText(
            projectionBytes.begin(), projectionBytes.end());
        if (!Require(projectionText.find(ReusableModelManagedProjectionFormat) !=
                    std::string::npos &&
                projectionText.find(result.assetId) != std::string::npos &&
                projectionText.find(result.sourceAssetId) != std::string::npos &&
                projectionText.find(sourceRelative) != std::string::npos &&
                projectionText.find(assetRelative) != std::string::npos &&
                projectionText.find(
                    ResolveReusableModelThumbnailPath(assetRelative)) !=
                    std::string::npos,
                std::string(label) + ": managed projection is missing package provenance"))
        {
            return false;
        }

        ReusableModelAssetDocument asset;
        std::string error;
        if (!Require(ReadReusableModelAssetDocument(
                assetPath.generic_u8string(), asset, error),
                std::string(label) + ": .rasset reopen failed: " + error) ||
            !Require(asset.manifest.projectId == ProjectId &&
                    asset.manifest.assetId == result.assetId &&
                    asset.manifest.sourceAssetId == result.sourceAssetId,
                std::string(label) + ": .rasset identity changed after reopen") ||
            !Require(asset.manifest.sourceFormat == expectedFormatToken,
                std::string(label) + ": .rasset source format recipe is wrong") ||
            !Require(asset.manifest.importer == result.import.importerBackend,
                std::string(label) + ": .rasset importer recipe is wrong"))
        {
            return false;
        }

        wi::scene::Scene reopenedPayload;
        const fs::path payloadPath =
            projectRoot / "Intermediate" / fs::u8path(std::string(label) + "-payload.wiscene");
        if (!Require(LoadPayloadScene(payloadPath, asset.payload, reopenedPayload, error),
                std::string(label) + ": " + error))
            return false;
        const auto reopenedSummary = ImportService::Summarize(reopenedPayload);
        const auto reopenedEvidence = ImportService::SummarizeModelEvidence(reopenedPayload);
        if (!Require(reopenedSummary == result.import.imported,
                std::string(label) + ": .rasset internal payload lost imported structure") ||
            !Require(reopenedEvidence == result.import.importedEvidence,
                std::string(label) + ": .rasset internal payload lost rig/animation evidence"))
        {
            return false;
        }

        if (requireSkin &&
            (!Require(result.modelMetadata.skinned && result.modelMetadata.armatureCount > 0 &&
                    result.modelMetadata.boneCount > 0,
                std::string(label) + ": skinned derived metadata is incomplete")))
            return false;
        if (requireAnimation)
        {
            const auto names = CollectAnimationNames(reopenedPayload);
            if (!Require(result.modelMetadata.animated &&
                    result.modelMetadata.animationClipCount > 0 &&
                    result.modelMetadata.animationChannelCount > 0,
                    std::string(label) + ": animation derived metadata is incomplete") ||
                !Require(!names.empty(),
                    std::string(label) + ": .rasset payload retained no named animation take/clip"))
            {
                return false;
            }
        }

        AssetRegistry registry;
        AssetCatalogueMetadataDocument metadata;
        if (!Require(ReadAssetRegistry(projectRoot.generic_u8string(), ProjectId,
                registry, error),
                std::string(label) + ": registry reopen failed: " + error) ||
            !Require(ReadAssetCatalogueMetadata(projectRoot.generic_u8string(), ProjectId,
                metadata, error),
                std::string(label) + ": metadata reopen failed: " + error))
        {
            return false;
        }
        const auto* sourceRecord = FindRecord(registry, result.sourceAssetId);
        const auto* assetRecord = FindRecord(registry, result.assetId);
        if (!Require(sourceRecord != nullptr &&
                    sourceRecord->projectRelativePath == sourceRelative &&
                    sourceRecord->requirement == DependencyRequirement::EditorOnly,
                std::string(label) + ": project-owned source record was not retained correctly") ||
            !Require(assetRecord != nullptr &&
                    assetRecord->projectRelativePath == assetRelative &&
                    assetRecord->provider == "lp07.rasset",
                std::string(label) + ": reusable product registry record is wrong"))
        {
            return false;
        }

        const auto provenance = std::find_if(registry.importedProducts.begin(),
            registry.importedProducts.end(), [&result](const auto& item)
            { return item.productAssetId == result.assetId; });
        if (!Require(provenance != registry.importedProducts.end() &&
                    provenance->sourceAssetId == result.sourceAssetId &&
                    provenance->importer == result.import.importerBackend &&
                    provenance->settingsJson.find(expectedFormatToken) != std::string::npos,
                std::string(label) + ": LC01 provenance recipe is missing or wrong"))
        {
            return false;
        }

        AssetCatalogue catalogue;
        if (!Require(BuildAssetCatalogue(projectRoot.generic_u8string(), ProjectId,
                registry, metadata, catalogue, error),
                std::string(label) + ": Gate 2 catalogue projection failed: " + error))
            return false;
        const auto* entry = FindCatalogueEntry(catalogue, result.assetId);
        if (!Require(entry != nullptr && entry->state == AssetCatalogueState::Current &&
                    entry->importedProduct && entry->sourceAssetId == result.sourceAssetId &&
                    entry->sourceFormat == expectedFormatToken &&
                    entry->model == result.modelMetadata,
                std::string(label) + ": catalogue did not expose the accepted reusable asset"))
        {
            return false;
        }

        std::vector<std::uint8_t> sourceAfter;
        if (!Require(ReadBytes(sourcePath, sourceAfter),
                std::string(label) + ": could not read source after import") ||
            !Require(sourceAfter == sourceBefore,
                std::string(label) + ": project-owned creator source bytes changed during import"))
        {
            return false;
        }

        std::cout << "LP07 GATE 3 IMPORT PASS // " << label
            << " // asset_id=" << result.assetId
            << " // source_id=" << result.sourceAssetId
            << " // format=" << expectedFormatToken
            << " // meshes=" << result.modelMetadata.meshCount
            << " // skinned=" << result.modelMetadata.skinned
            << " // animated=" << result.modelMetadata.animated
            << '\n';
        return true;
    }

    bool RunFailurePreservationProof(const fs::path& projectRoot)
    {
        using namespace renegade::bridge;
        std::vector<std::uint8_t> registryBefore;
        std::vector<std::uint8_t> metadataBefore;
        if (!Require(ReadBytes(projectRoot / AssetRegistryDocumentName, registryBefore),
                "rollback: could not capture registry before forced failure") ||
            !Require(ReadBytes(projectRoot / AssetCatalogueMetadataDocumentName, metadataBefore),
                "rollback: could not capture metadata before forced failure"))
        {
            return false;
        }

        ReusableModelImportRequest request;
        request.projectRoot = projectRoot.generic_u8string();
        request.projectId = ProjectId;
        request.sourceProjectRelativePath = "SourceAssets/Models/static.fbx";
        request.assetProjectRelativePath = "Content/Models/forced-failure.rasset";
        request.expectedFormat = ModelSourceFormat::Fbx;
        request.thumbnailPngBytes = ThumbnailFixture();

        ReusableModelImportOptions options;
        options.transactionId = "lp07-gate3-forced-rollback";
        options.operationHook = [](const ProjectDocumentTransactionStage stage,
            std::size_t,
            const std::string& path,
            std::string& error)
        {
            if (stage == ProjectDocumentTransactionStage::AfterReplace &&
                path.find("forced-failure.rasset") != std::string::npos)
            {
                error = "intentional Gate 3 rollback proof";
                return ProjectDocumentTransactionHookAction::Fail;
            }
            return ProjectDocumentTransactionHookAction::Continue;
        };

        ReusableAssetService service;
        const auto failed = service.ImportModelAsset(request, std::move(options));
        if (!Require(!failed.succeeded && failed.transaction.rolledBack,
            "forced commit failure did not roll back") ||
        !Require(!fs::exists(projectRoot / "Content/Models/forced-failure.rasset"),
            "forced commit failure left a half-imported RAsset") ||
        !Require(!fs::exists(projectRoot / "Content/Models/forced-failure.rasset.json"),
            "forced commit failure left a managed projection") ||
        !Require(!fs::exists(projectRoot / "Content/Models/forced-failure.thumbnail.png"),
            "forced commit failure left a creator thumbnail"))
{
            return false;
        }

        std::vector<std::uint8_t> registryAfter;
        std::vector<std::uint8_t> metadataAfter;
        if (!Require(ReadBytes(projectRoot / AssetRegistryDocumentName, registryAfter),
                "rollback: could not read registry after failure") ||
            !Require(ReadBytes(projectRoot / AssetCatalogueMetadataDocumentName, metadataAfter),
                "rollback: could not read metadata after failure") ||
            !Require(registryAfter == registryBefore,
                "forced commit failure changed authoritative registry bytes") ||
            !Require(metadataAfter == metadataBefore,
                "forced commit failure changed authoritative metadata bytes"))
        {
            return false;
        }
        std::cout << "LP07 GATE 3 ROLLBACK PASS\n";
        return true;
    }

    // Public FBX proof covers ordinary weight normalization AND a valid
    // floating-point two-cycle that the former 32-pass fixed-point repair rejected.
    bool RunSkinWeightNormalizationProof(const fs::path& projectRoot)
    {
        using namespace renegade::bridge;
        ImportService imports;
        const fs::path source = projectRoot / "SourceAssets/Models/animated.fbx";
        const auto makePrepared = [&](const char* leaf)
        {
            ModelImportRequest request;
            request.sourcePath = source.generic_u8string();
            request.assetPath = (projectRoot / "Intermediate" / leaf).generic_u8string();
            request.expectedFormat = ModelSourceFormat::Fbx;
            return imports.PrepareModelAsset(request);
        };
        const auto setWeights = [&](PreparedModelImport& prepared, std::size_t& meshIndex)
        {
            auto* scene = prepared.PeekMutableScene();
            if (scene == nullptr)
                return false;
            for (meshIndex = 0; meshIndex < scene->meshes.GetCount(); ++meshIndex)
            {
                auto& mesh = scene->meshes[meshIndex];
                if (mesh.vertex_boneindices.empty() || mesh.vertex_boneweights.size() <= 5)
                    continue;
                mesh.vertex_boneweights[0] = XMFLOAT4(0.2f, 0.2f, 0.2f, 0.2f);
                // Exact IEEE-754 input bits; normalizing once/twice alternates
                // between two different weight vectors on pinned Wicked/MSVC.
                const std::uint32_t cycleBits[4] = {
                    0x3eec3653u, 0x3f12861du, 0x3eea5aa6u, 0x3f4e81e1u
                };
                float cycle[4] = {};
                for (int index = 0; index < 4; ++index)
                    std::memcpy(&cycle[index], &cycleBits[index], sizeof(float));
                mesh.vertex_boneweights[5] =
                    XMFLOAT4(cycle[0], cycle[1], cycle[2], cycle[3]);
                if (!mesh.vertex_boneweights2.empty())
                {
                    mesh.vertex_boneweights2[0] = XMFLOAT4(0, 0, 0, 0);
                    mesh.vertex_boneweights2[5] = XMFLOAT4(0, 0, 0, 0);
                }
                return true;
            }
            return false;
        };
        const auto readScene = [&](const fs::path& path,
            wi::scene::Scene& scene, std::string& error)
        {
            wi::Archive archive(path.generic_u8string(), true, false);
            if (!archive.IsOpen())
            {
                error = "could not open normalization proof WISCENE";
                return false;
            }
            scene.Serialize(archive);
            if (archive.GetPos() != archive.GetSize())
            {
                error = "normalization proof WISCENE had trailing/incomplete data";
                return false;
            }
            return true;
        };
        // Confirm the regression is genuinely nonconvergent: the original
        // 32-pass repair would reject it even though all weights are finite.
        const std::uint32_t cycleBits[4] = {
            0x3eec3653u, 0x3f12861du, 0x3eea5aa6u, 0x3f4e81e1u
        };
        float cycle[4] = {};
        for (int index = 0; index < 4; ++index)
            std::memcpy(&cycle[index], &cycleBits[index], sizeof(float));
        const auto normalizeOnce = [](const XMFLOAT4& original)
        {
            float weights[8] = {
                original.x, original.y, original.z, original.w, 0, 0, 0, 0
            };
            float sum = 0.0f;
            for (const float weight : weights)
                sum += weight;
            if (sum > 0.0f)
            {
                const float factor = 1.0f / sum;
                for (float& weight : weights)
                    weight *= factor;
            }
            return XMFLOAT4(weights[0], weights[1], weights[2], weights[3]);
        };
        const XMFLOAT4 cycleStart(cycle[0], cycle[1], cycle[2], cycle[3]);
        const auto cycleFirst = normalizeOnce(cycleStart);
        const auto cycleSecond = normalizeOnce(cycleFirst);
        const auto cycleThird = normalizeOnce(cycleSecond);
        if (!Require(std::memcmp(&cycleFirst, &cycleSecond, sizeof(XMFLOAT4)) != 0 &&
                std::memcmp(&cycleFirst, &cycleThird, sizeof(XMFLOAT4)) == 0,
                "public cycle fixture must reproduce nonconvergent Wicked weights"))
            return false;
        std::string error;
        auto raw = makePrepared("a2-normalization-raw.wiscene");
        std::size_t meshIndex = 0;
        if (!Require(raw.IsReady(), "weight proof could not prepare public FBX: " +
                raw.Result().error) ||
            !Require(setWeights(raw, meshIndex), "public FBX had no skinned mesh") ||
            !Require(imports.RefreshPreparedModelEvidence(raw, error),
                "weight proof could not refresh raw evidence: " + error))
            return false;
        const auto original = raw.Result().importedEvidence;
        const auto rawSave = imports.SavePreparedGltfAsset(raw);
        if (!Require(rawSave.succeeded,
                "original structural WISCENE proof failed: " + rawSave.error) ||
            !Require(original == ImportService::SummarizeModelEvidence(*raw.PeekScene()),
                "raw prepared skin weights changed during WISCENE write"))
            return false;
        auto rawReload = wi::allocator::make_shared_single<wi::scene::Scene>();
        if (!Require(readScene(fs::u8path(rawSave.assetPath), *rawReload, error),
                "raw WISCENE reload failed: " + error) ||
            !Require(meshIndex < rawReload->meshes.GetCount(),
                "raw WISCENE lost its skinned mesh"))
            return false;
        const auto& reloadedMesh = rawReload->meshes[meshIndex];
        if (!Require(!reloadedMesh.vertex_boneweights.empty() &&
                reloadedMesh.vertex_boneweights[0].x == 0.25f &&
                reloadedMesh.vertex_boneweights[0].y == 0.25f &&
                reloadedMesh.vertex_boneweights[0].z == 0.25f &&
                reloadedMesh.vertex_boneweights[0].w == 0.25f,
                "pinned Wicked reload did not normalize the controlled 0.8 sum to unity"))
            return false;
        const auto normalized = ImportService::SummarizeModelEvidence(*rawReload);
        if (!Require(original.skinWeightFingerprint != normalized.skinWeightFingerprint &&
                original.meshFingerprint != normalized.meshFingerprint &&
                original.skinIndexFingerprint == normalized.skinIndexFingerprint &&
                original.armatureFingerprint == normalized.armatureFingerprint &&
                original.animationFingerprint == normalized.animationFingerprint &&
                original.animationDataFingerprint == normalized.animationDataFingerprint,
                "weight proof changed unrelated rig/animation evidence"))
            return false;
        std::uint32_t originalBits = 0;
        std::uint32_t normalizedBits = 0;
        const float rawValue = 0.2f;
        const float fixedValue = reloadedMesh.vertex_boneweights[0].x;
        std::memcpy(&originalBits, &rawValue, sizeof(originalBits));
        std::memcpy(&normalizedBits, &fixedValue, sizeof(normalizedBits));
        std::cout << "A2 PINNED WICKED SKIN NORMALIZATION REPRO PASS"
            << " // first_vertex_weight_x_bits=0x" << std::hex << originalBits
            << " -> 0x" << normalizedBits << std::dec << '\n';
        rawReload.reset();

        auto repaired = makePrepared("a2-normalization-fixed.wiscene");
        std::size_t repairedMeshIndex = 0;
        if (!Require(repaired.IsReady(), "repair proof could not prepare public FBX: " +
                repaired.Result().error) ||
            !Require(setWeights(repaired, repairedMeshIndex),
                "repair proof had no skinned mesh") ||
            !Require(imports.RefreshPreparedModelEvidence(repaired, error),
                "repair proof could not refresh raw evidence: " + error))
            return false;
        const auto originalPreparedEvidence = repaired.Result().importedEvidence;
        const auto saved = imports.SavePreparedModelAsset(repaired);
        if (!Require(saved.succeeded,
                "single-pass WISCENE save failed: " + saved.error) ||
            !Require(originalPreparedEvidence ==
                ImportService::SummarizeModelEvidence(*repaired.PeekScene()),
                "reload prediction mutated original prepared weights") ||
            !Require(originalPreparedEvidence.skinWeightFingerprint !=
                saved.importedEvidence.skinWeightFingerprint &&
                originalPreparedEvidence.skinIndexFingerprint ==
                saved.importedEvidence.skinIndexFingerprint &&
                originalPreparedEvidence.armatureFingerprint ==
                saved.importedEvidence.armatureFingerprint &&
                originalPreparedEvidence.animationFingerprint ==
                saved.importedEvidence.animationFingerprint,
                "single-pass prediction changed unexpected rig or animation groups") ||
            !Require(saved.importedEvidence == saved.reloadedEvidence,
                "single-pass rig/animation evidence did not match exactly"))
            return false;
        for (int pass = 0; pass < 2; ++pass)
        {
            auto reopened = wi::allocator::make_shared_single<wi::scene::Scene>();
            if (!Require(readScene(fs::u8path(saved.assetPath), *reopened, error),
                    "single-pass WISCENE reopen failed: " + error) ||
                !Require(ImportService::SummarizeModelEvidence(*reopened) ==
                    saved.importedEvidence,
                    "single-pass rig/animation evidence differed across independent reloads") ||
                !Require(repairedMeshIndex < reopened->meshes.GetCount() &&
                    !reopened->meshes[repairedMeshIndex].vertex_boneweights.empty() &&
                    reopened->meshes[repairedMeshIndex].vertex_boneweights[0].x == 0.25f,
                    "normalized first-vertex skin weight was not preserved"))
                return false;
        }
        std::cout << "A2 SINGLE-PASS SKIN ROUND-TRIP AND NONCONVERGENT CYCLE PASS\n";
        return true;
    }

    bool RunExistingProductPreservationProof(const fs::path& projectRoot)
    {
        using namespace renegade::bridge;
        const fs::path product = projectRoot / "Content/Models/static.rasset";
        std::vector<std::uint8_t> before;
        if (!Require(ReadBytes(product, before),
                "existing-product: could not capture accepted RAsset"))
            return false;

        ReusableModelImportRequest request;
        request.projectRoot = projectRoot.generic_u8string();
        request.projectId = ProjectId;
        request.sourceProjectRelativePath = "SourceAssets/Models/static.fbx";
        request.assetProjectRelativePath = "Content/Models/static.rasset";
        request.expectedFormat = ModelSourceFormat::Fbx;
        ReusableAssetService service;
        const auto rejected = service.ImportModelAsset(request);
        std::vector<std::uint8_t> after;
        if (!Require(!rejected.succeeded,
                "existing product import was not rejected for Gate 4 ownership") ||
            !Require(ReadBytes(product, after) && after == before,
                "rejected replacement changed last-good RAsset bytes"))
        {
            return false;
        }
        std::cout << "LP07 GATE 3 LAST-GOOD PASS\n";
        return true;
    }
}

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "Usage: RenegadeReusableAssetGraphicsProof "
            << "<static.fbx> <skinned-animated.fbx> <output-directory>\n";
        return 2;
    }

    const fs::path staticFixture = fs::u8path(argv[1]);
    const fs::path animatedFixture = fs::u8path(argv[2]);
    const fs::path outputRoot = fs::u8path(argv[3]);
    if (!Require(fs::is_regular_file(staticFixture), "static FBX fixture is missing") ||
        !Require(fs::is_regular_file(animatedFixture), "animated FBX fixture is missing"))
        return 3;

    std::error_code ec;
    fs::remove_all(outputRoot, ec);
    ec.clear();
    const fs::path projectRoot = outputRoot / "Project";
    fs::create_directories(projectRoot / "Content" / "Models", ec);
    fs::create_directories(projectRoot / "SourceAssets" / "Models", ec);
    fs::create_directories(projectRoot / "Intermediate" / "Transactions", ec);
    if (!Require(!ec, "could not create Gate 3 project fixture"))
        return 4;

    fs::copy_file(staticFixture, projectRoot / "SourceAssets/Models/static.fbx",
        fs::copy_options::overwrite_existing, ec);
    fs::copy_file(animatedFixture, projectRoot / "SourceAssets/Models/animated.fbx",
        fs::copy_options::overwrite_existing, ec);
    if (!Require(!ec, "could not stage FBX sources into project SourceAssets"))
        return 5;

    const std::string gltf =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA\",\"byteLength\":42}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36,\"target\":34962},{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6,\"target\":34963}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}],"
        "\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],\"scene\":0}";
    if (!Require(WriteText(projectRoot / "SourceAssets/Models/triangle.gltf", gltf),
            "could not create self-contained GLTF regression source"))
        return 6;

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = WindowClassName;
    RegisterClassExW(&windowClass);
    const HWND window = CreateWindowExW(0, WindowClassName,
        L"Renegade LP07 Gate 3 RAsset Proof", WS_OVERLAPPEDWINDOW,
        0, 0, 64, 64, nullptr, nullptr, instance, nullptr);
    if (!Require(window != nullptr, "could not create Gate 3 graphics proof window"))
        return 7;

    wi::jobsystem::Initialize();
    int exitCode = 0;
    {
        wi::Application application;
        application.allow_hdr = false;
        application.SetWindow(window);
        if (!Require(wi::graphics::GetDevice() != nullptr,
                "Wicked graphics device was not initialized"))
        {
            exitCode = 8;
        }
        else
        {
            const bool staticPassed = RunReusableImportCase(
                "static", projectRoot,
                "SourceAssets/Models/static.fbx",
                "Content/Models/static.rasset",
                renegade::bridge::ModelSourceFormat::Fbx,
                "fbx", false, false);
            const bool animatedPassed = RunReusableImportCase(
                "animated", projectRoot,
                "SourceAssets/Models/animated.fbx",
                "Content/Models/animated.rasset",
                renegade::bridge::ModelSourceFormat::Fbx,
                "fbx", true, true);
            const bool gltfPassed = RunReusableImportCase(
                "gltf", projectRoot,
                "SourceAssets/Models/triangle.gltf",
                "Content/Models/triangle.rasset",
                renegade::bridge::ModelSourceFormat::Gltf,
                "gltf", false, false);
            const bool weightProofPassed = animatedPassed &&
                RunSkinWeightNormalizationProof(projectRoot);
            const bool rollbackPassed = staticPassed && animatedPassed && gltfPassed &&
                weightProofPassed && RunFailurePreservationProof(projectRoot);
            const bool lastGoodPassed = rollbackPassed &&
                RunExistingProductPreservationProof(projectRoot);
            if (!staticPassed || !animatedPassed || !gltfPassed ||
                !weightProofPassed || !rollbackPassed || !lastGoodPassed)
                exitCode = 9;
        }
    }

    wi::jobsystem::ShutDown();
    DestroyWindow(window);
    fs::remove_all(outputRoot, ec);
    if (exitCode == 0)
        std::cout << "LP07 GATE 3 RASSET GRAPHICS PROOF PASS\n";
    return exitCode;
}
