#include "renegade/bridge/CreatorTextureWorkflowService.h"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <utility>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    std::string LowerAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](const unsigned char c)
            {
                return c >= 'A' && c <= 'Z'
                    ? static_cast<char>(c + ('a' - 'A'))
                    : static_cast<char>(c);
            });
        return value;
    }

    CreatorTextureImportResult Failure(std::string error)
    {
        CreatorTextureImportResult result;
        result.error = std::move(error);
        return result;
    }

    bool EnsureAssetRegistryDocument(
        const fs::path& root,
        const StableId& projectId,
        std::string& error)
    {
        const fs::path registryPath = root / AssetRegistryDocumentName;
        std::error_code ec;
        const bool exists = fs::exists(registryPath, ec);
        if (ec)
        {
            error = "Could not inspect the project asset registry: " + ec.message();
            return false;
        }
        if (exists)
        {
            error.clear();
            return true;
        }

        // New and pre-LC01 projects can legitimately have no registry until
        // their first governed asset is committed. LP07 model import already
        // supports this create-on-first-write lifecycle; creator texture
        // governance must do the same instead of failing before the model
        // transaction can begin.
        AssetRegistry registry;
        registry.projectId = projectId;
        registry.schemaVersion = AssetRegistry::CurrentSchemaVersion;

        AssetRegistryPersistenceOptions options;
        options.transactionId =
            "lp08-creator-registry-bootstrap-" + GenerateStableId();
        const auto written = WriteAssetRegistry(
            root.generic_u8string(), registry, std::move(options));
        if (!written.success || !written.committed)
        {
            error = written.message.empty()
                ? "Could not initialise the project asset registry."
                : written.message;
            return false;
        }
        error.clear();
        return true;
    }

    bool IsGovernedTextureEntry(const AssetCatalogueEntry& entry)
    {
        return entry.registered && entry.importedProduct &&
            entry.dependencyClass == DependencyClass::Texture &&
            IsValidStableId(entry.assetId);
    }
}

namespace renegade::bridge
{
    CreatorTextureImportResult CreatorTextureWorkflowService::ImportTexture(
        const std::string& projectRoot,
        const StableId& projectId,
        const std::string& externalSourcePath) const
    {
        if (projectRoot.empty() || !IsValidStableId(projectId))
            return Failure("Texture import requires an active Renegade project.");
        if (externalSourcePath.empty())
            return Failure("Texture import requires a source image.");

        std::error_code ec;
        const fs::path external = fs::weakly_canonical(
            fs::absolute(fs::u8path(externalSourcePath), ec), ec);
        if (ec || external.empty() || !fs::is_regular_file(external, ec) || ec)
            return Failure("The selected texture source is unavailable.");

        const ResourceSourceFormat format = DetectResourceSourceFormat(
            external.filename().generic_u8string());
        if (format == ResourceSourceFormat::Unknown ||
            ClassifyResourceSourceFormat(format) != ResourceClass::Texture)
        {
            return Failure(
                "The selected file is not a texture format supported by the pinned Wicked resource manager.");
        }

        const std::uintmax_t sourceBytes = fs::file_size(external, ec);
        if (ec)
            return Failure("Could not inspect the selected texture size: " + ec.message());
        if (sourceBytes == 0)
            return Failure("The selected texture source is empty.");
        if (sourceBytes > MaximumCreatorTextureBytes)
        {
            return Failure(
                "The selected texture exceeds Renegade's 512 MiB creator-import ceiling.");
        }

        const fs::path root = fs::weakly_canonical(
            fs::absolute(fs::u8path(projectRoot), ec), ec);
        if (ec || root.empty() || !fs::is_directory(root, ec) || ec)
            return Failure("The active project root is unavailable.");

        std::string registryError;
        if (!EnsureAssetRegistryDocument(root, projectId, registryError))
            return Failure(std::move(registryError));

        const fs::path sourceDirectory = root / "SourceAssets" / "Textures";
        const fs::path productDirectory = root / "Content" / "Textures";
        fs::create_directories(sourceDirectory, ec);
        if (ec)
            return Failure("Could not create SourceAssets/Textures: " + ec.message());
        fs::create_directories(productDirectory, ec);
        if (ec)
            return Failure("Could not create Content/Textures: " + ec.message());

        std::string stem = external.stem().generic_u8string();
        if (stem.empty())
            stem = "ImportedTexture";
        std::string extension = LowerAscii(external.extension().generic_u8string());
        if (extension.empty())
            return Failure("The selected texture has no supported extension.");

        fs::path retainedSource;
        fs::path governedProduct;
        bool uniqueDestinationFound = false;
        for (std::uint32_t index = 1; index <= MaximumNameAttempts; ++index)
        {
            const std::string candidateStem = index == 1
                ? stem
                : stem + "_" + std::to_string(index);
            retainedSource = sourceDirectory /
                fs::u8path(candidateStem + extension);
            governedProduct = productDirectory /
                fs::u8path(candidateStem + ResourceAssetExtension);
            ec.clear();
            const bool sourceExists = fs::exists(retainedSource, ec);
            if (ec)
                return Failure("Could not inspect the retained texture destination.");
            const bool productExists = fs::exists(governedProduct, ec);
            if (ec)
                return Failure("Could not inspect the governed texture destination.");
            if (!sourceExists && !productExists)
            {
                uniqueDestinationFound = true;
                break;
            }
        }
        if (!uniqueDestinationFound)
        {
            return Failure(
                "Could not allocate a unique texture name after 1024 attempts.");
        }

        fs::copy_file(external, retainedSource, fs::copy_options::none, ec);
        if (ec)
        {
            if (ec == std::make_error_code(std::errc::file_exists))
            {
                return Failure(
                    "The retained texture destination became occupied during import; retry the import.");
            }
            return Failure("Could not retain the original texture source: " + ec.message());
        }

        CreatorTextureImportResult result;
        result.sourceFormat = format;
        result.sourceProjectRelativePath = retainedSource.lexically_relative(root)
            .lexically_normal().generic_u8string();
        result.assetProjectRelativePath = governedProduct.lexically_relative(root)
            .lexically_normal().generic_u8string();

        ResourceAssetImportRequest request;
        request.projectRoot = root.generic_u8string();
        request.projectId = projectId;
        request.sourceProjectRelativePath = result.sourceProjectRelativePath;
        request.assetProjectRelativePath = result.assetProjectRelativePath;
        request.expectedFormat = format;

        ResourceAssetService assets;
        result.asset = assets.ImportResourceAsset(request);
        result.committed = result.asset.transaction.committed;
        result.assetId = result.asset.assetId;
        result.sourceAssetId = result.asset.sourceAssetId;
        if (!result.asset.succeeded)
        {
            result.error = result.asset.error.empty()
                ? "The governed texture transaction failed."
                : result.asset.error;
            if (!result.asset.transaction.committed)
            {
                std::error_code cleanupError;
                fs::remove(retainedSource, cleanupError);
            }
            return result;
        }

        result.succeeded = true;
        result.committed = true;
        result.error.clear();
        return result;
    }

    bool CreatorTextureWorkflowService::EnrichTextureCatalogue(
        const std::string& projectRoot,
        const StableId& projectId,
        AssetCatalogue& catalogue,
        std::string& warning) const
    {
        warning.clear();
        ResourceAssetMetadataDocument metadata;
        std::string metadataError;
        if (!ReadResourceAssetMetadata(
                projectRoot, projectId, metadata, metadataError))
        {
            std::size_t affected = 0;
            for (auto& entry : catalogue.entries)
            {
                if (!IsGovernedTextureEntry(entry))
                    continue;
                entry.state = AssetCatalogueState::Invalid;
                entry.sourceFormat.clear();
                ++affected;
            }
            warning = "Texture resource metadata could not be read; " +
                std::to_string(affected) +
                " governed texture(s) were left visible as INVALID. " +
                metadataError;
            return true;
        }

        std::size_t failureCount = 0;
        StableId firstFailure;
        for (auto& entry : catalogue.entries)
        {
            if (!IsGovernedTextureEntry(entry))
                continue;

            const auto found = std::find_if(
                metadata.records.begin(), metadata.records.end(),
                [&entry](const ResourceAssetMetadataRecord& record)
                {
                    return record.assetId == entry.assetId;
                });
            if (found == metadata.records.end() ||
                found->resourceClass != ResourceClass::Texture ||
                found->sourceFormat == ResourceSourceFormat::Unknown)
            {
                entry.state = AssetCatalogueState::Invalid;
                entry.sourceFormat.clear();
                ++failureCount;
                if (firstFailure.empty())
                    firstFailure = entry.assetId;
                continue;
            }
            entry.sourceFormat = LowerAscii(
                ResourceSourceFormatLabel(found->sourceFormat));
        }

        if (failureCount != 0)
        {
            warning = std::to_string(failureCount) +
                " governed texture(s) are missing accepted resource metadata and remain visible as INVALID. First asset: " +
                firstFailure;
        }
        return true;
    }
}
