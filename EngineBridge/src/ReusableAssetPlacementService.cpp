#include "renegade/bridge/ReusableAssetService.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <vector>

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr std::uint64_t FnvOffset = 1469598103934665603ull;
        constexpr std::uint64_t FnvPrime = 1099511628211ull;

        bool IsWithin(const fs::path& candidate, const fs::path& root)
        {
            auto candidatePart = candidate.begin();
            for (auto rootPart = root.begin(); rootPart != root.end();
                ++rootPart, ++candidatePart)
            {
                if (candidatePart == candidate.end() || *candidatePart != *rootPart)
                    return false;
            }
            return true;
        }

        std::string LowerExtension(const std::string& path)
        {
            std::string extension = fs::u8path(path).extension().generic_u8string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](const unsigned char value)
                {
                    return static_cast<char>(std::tolower(value));
                });
            return extension;
        }

        const AssetRecord* FindRecordById(
            const AssetRegistry& registry,
            const StableId& assetId)
        {
            const auto found = std::find_if(
                registry.records.begin(), registry.records.end(),
                [&assetId](const AssetRecord& record)
                {
                    return record.assetId == assetId;
                });
            return found == registry.records.end() ? nullptr : &*found;
        }

        const ImportedProductRecord* FindImportedProduct(
            const AssetRegistry& registry,
            const StableId& assetId)
        {
            const auto found = std::find_if(
                registry.importedProducts.begin(), registry.importedProducts.end(),
                [&assetId](const ImportedProductRecord& record)
                {
                    return record.productAssetId == assetId;
                });
            return found == registry.importedProducts.end() ? nullptr : &*found;
        }

        bool ReadBytes(
            const fs::path& path,
            std::vector<std::uint8_t>& bytes,
            std::string& error)
        {
            bytes.clear();
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                error = "Could not read registered reusable asset: " +
                    path.generic_u8string();
                return false;
            }
            bytes.assign(
                std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>());
            if (!stream && !stream.eof())
            {
                bytes.clear();
                error = "Could not read complete registered reusable asset: " +
                    path.generic_u8string();
                return false;
            }
            error.clear();
            return true;
        }

        std::string HashBytes(const std::vector<std::uint8_t>& bytes)
        {
            std::uint64_t hash = FnvOffset;
            for (const std::uint8_t value : bytes)
            {
                hash ^= value;
                hash *= FnvPrime;
            }
            std::ostringstream stream;
            stream << "fnv1a64:" << std::hex << std::setfill('0')
                   << std::setw(16) << hash;
            return stream.str();
        }

        bool WritePayload(
            const fs::path& path,
            const std::vector<std::uint8_t>& payload,
            std::string& error)
        {
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            if (!stream)
            {
                error = "Could not stage reusable asset payload for placement: " +
                    path.generic_u8string();
                return false;
            }
            stream.write(
                reinterpret_cast<const char*>(payload.data()),
                static_cast<std::streamsize>(payload.size()));
            if (!stream)
            {
                error = "Could not write complete reusable asset payload for placement.";
                return false;
            }
            error.clear();
            return true;
        }
    }

    PreparedReusableModelPlacement ReusableAssetService::PrepareModelAssetPlacement(
        const ReusableModelPlacementRequest& request) const
    {
        PreparedReusableModelPlacement prepared;
        ReusableModelPlacementResult& result = prepared.result_;
        result.assetId = request.assetId;

        if (!IsValidStableId(request.projectId) || !IsValidStableId(request.assetId))
        {
            result.error = "Reusable model placement requires valid project and asset IDs.";
            return prepared;
        }
        if (request.projectRoot.empty())
        {
            result.error = "Reusable model placement requires a project root.";
            return prepared;
        }

        std::error_code ec;
        const fs::path root = fs::weakly_canonical(
            fs::absolute(fs::u8path(request.projectRoot), ec), ec);
        if (ec || root.empty() || !fs::is_directory(root, ec) || ec)
        {
            result.error = "Reusable model placement project root is unavailable.";
            return prepared;
        }

        AssetRegistry registry;
        if (!ReadAssetRegistry(
                root.generic_u8string(), request.projectId, registry, result.error))
        {
            return prepared;
        }

        const AssetRecord* productRecord = FindRecordById(registry, request.assetId);
        const ImportedProductRecord* provenance = FindImportedProduct(
            registry, request.assetId);
        if (productRecord == nullptr || provenance == nullptr)
        {
            result.error =
                "Reusable model placement requires a registered imported product.";
            return prepared;
        }
        if (!productRecord->sourceAvailable ||
            productRecord->projectRelativePath.empty() ||
            LowerExtension(productRecord->projectRelativePath) != ReusableAssetExtension)
        {
            result.error =
                "Registered reusable model product is missing or is not an .rasset.";
            return prepared;
        }

        // Placement deliberately does not require the source record to be
        // active or its source file to exist. LC01 provenance and the RAsset
        // manifest already bind the accepted product to its stable source ID.
        // A creator can therefore keep placing the last-good reusable product
        // while its original FBX/GLTF source is missing and repair/reimport it
        // later through the explicit stale/missing workflow.

        const fs::path contentRoot = fs::weakly_canonical(root / "Content", ec);
        if (ec || !fs::is_directory(contentRoot, ec) || ec)
        {
            result.error = "Project Content folder is unavailable for reusable placement.";
            return prepared;
        }
        const fs::path productPath = fs::weakly_canonical(
            root / fs::u8path(productRecord->projectRelativePath), ec);
        if (ec || !fs::is_regular_file(productPath, ec) || ec ||
            !IsWithin(productPath, contentRoot))
        {
            result.error =
                "Registered reusable model product resolves outside Content or is unavailable.";
            return prepared;
        }

        std::vector<std::uint8_t> productBytes;
        if (!ReadBytes(productPath, productBytes, result.error))
            return prepared;
        const std::string productHash = HashBytes(productBytes);
        if (productHash != productRecord->contentHash ||
            productHash != provenance->productContentHashAtImport)
        {
            result.error =
                "Registered reusable model product bytes no longer match LC01 provenance.";
            return prepared;
        }

        ReusableModelAssetDocument document;
        if (!DeserializeReusableModelAssetDocument(
                productBytes, document, result.error))
        {
            return prepared;
        }
        if (document.manifest.projectId != request.projectId ||
            document.manifest.assetId != request.assetId ||
            document.manifest.sourceAssetId != provenance->sourceAssetId ||
            document.manifest.importer != provenance->importer ||
            document.manifest.importerVersion != provenance->importerVersion ||
            document.manifest.settingsSchema != provenance->settingsSchema ||
            document.manifest.settingsVersion != provenance->settingsVersion ||
            document.manifest.settingsJson != provenance->settingsJson)
        {
            result.error =
                "Reusable model manifest contradicts the authoritative registry/provenance.";
            return prepared;
        }

        // WISCENE material resources can retain relative paths from the model
        // import. For retained glTF sources, sidecar images/buffers live beside
        // the retained source under SourceAssets. Rehydrate the payload from
        // that same directory whenever the retained source is available so
        // Wicked resolves those relative paths against the correct base. This
        // fixes the creator-facing geometry-without-textures failure. If the
        // source is unavailable, preserve the accepted source-independent
        // placement fallback used by the lifecycle tests.
        fs::path placementDirectory = root / "Intermediate" / "Imports";
        const AssetRecord* sourceRecord = FindRecordById(
            registry, provenance->sourceAssetId);
        if (sourceRecord != nullptr && sourceRecord->sourceAvailable &&
            !sourceRecord->projectRelativePath.empty())
        {
            const fs::path sourceRoot = fs::weakly_canonical(root / "SourceAssets", ec);
            if (!ec && fs::is_directory(sourceRoot, ec) && !ec)
            {
                const fs::path retainedSource = fs::weakly_canonical(
                    root / fs::u8path(sourceRecord->projectRelativePath), ec);
                if (!ec && fs::is_regular_file(retainedSource, ec) && !ec &&
                    IsWithin(retainedSource, sourceRoot))
                {
                    placementDirectory = retainedSource.parent_path();
                }
                ec.clear();
            }
            else
            {
                ec.clear();
            }
        }

        fs::create_directories(placementDirectory, ec);
        if (ec)
        {
            result.error =
                "Could not create reusable placement working directory: " + ec.message();
            return prepared;
        }
        const fs::path stagedPayload = placementDirectory /
            fs::u8path(".renegade-" + request.assetId + ".placement.wiscene");
        fs::remove(stagedPayload, ec);
        ec.clear();

        const auto cleanup = [&stagedPayload]()
        {
            std::error_code ignored;
            fs::remove(stagedPayload, ignored);
        };
        if (!WritePayload(stagedPayload, document.payload, result.error))
        {
            cleanup();
            return prepared;
        }

        prepared.scene_ = wi::allocator::make_shared_single<wi::scene::Scene>();
        wi::Archive archive(stagedPayload.generic_u8string(), true, false);
        if (!archive.IsOpen())
        {
            result.error = "Could not open reusable asset WISCENE payload for placement.";
            prepared.scene_.reset();
            cleanup();
            return prepared;
        }
        prepared.scene_->Serialize(archive);
        if (archive.GetPos() != archive.GetSize())
        {
            result.error =
                "Reusable asset WISCENE payload contains trailing or incomplete data.";
            prepared.scene_.reset();
            cleanup();
            return prepared;
        }
        archive = wi::Archive();
        cleanup();

        result.sceneSummary = ImportService::Summarize(*prepared.scene_);
        result.modelEvidence = ImportService::SummarizeModelEvidence(*prepared.scene_);
        if (result.sceneSummary.meshes == 0 || result.sceneSummary.objects == 0)
        {
            result.error = "Reusable asset payload contains no placeable model content.";
            prepared.scene_.reset();
            return prepared;
        }

        result.sourceAssetId = provenance->sourceAssetId;
        result.assetProjectRelativePath = productRecord->projectRelativePath;
        result.succeeded = true;
        result.error.clear();
        return prepared;
    }
}
