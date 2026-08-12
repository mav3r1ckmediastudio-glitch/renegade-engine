#include "renegade/bridge/ResourceAssetService.h"

#include "renegade/bridge/AssetBrowserService.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <utility>

#include "json.hpp"

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    bool IsWithin(const fs::path& child, const fs::path& parent)
    {
        auto childPart = child.begin();
        for (auto parentPart = parent.begin(); parentPart != parent.end();
            ++parentPart, ++childPart)
        {
            if (childPart == child.end() || *childPart != *parentPart)
                return false;
        }
        return true;
    }

    bool IsSafeProjectRelativePath(const fs::path& path)
    {
        if (path.empty() || path.is_absolute() || path.has_root_name())
            return false;
        return std::none_of(path.begin(), path.end(), [](const fs::path& part)
        {
            return part == "." || part == "..";
        });
    }

    bool PathStartsWith(const fs::path& path, const fs::path& prefix)
    {
        auto pathPart = path.begin();
        for (auto prefixPart = prefix.begin(); prefixPart != prefix.end();
            ++prefixPart, ++pathPart)
        {
            if (pathPart == path.end() || *pathPart != *prefixPart)
                return false;
        }
        return pathPart != path.end();
    }

    bool ResolveProjectRoot(
        const std::string& projectRoot,
        fs::path& root,
        std::string& error)
    {
        root.clear();
        if (projectRoot.empty())
        {
            error = "Resource asset reimport requires an active project root.";
            return false;
        }
        std::error_code ec;
        const fs::path absolute = fs::absolute(fs::u8path(projectRoot), ec);
        if (ec || absolute.empty())
        {
            error = "Could not resolve the resource reimport project root.";
            return false;
        }
        root = fs::weakly_canonical(absolute, ec);
        if (ec || root.empty() || !fs::is_directory(root, ec) || ec)
        {
            root.clear();
            error = "Resource reimport project root is unavailable.";
            return false;
        }
        error.clear();
        return true;
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
            error = "Could not read file: " + path.generic_u8string();
            return false;
        }
        stream.seekg(0, std::ios::end);
        const std::streamoff size = stream.tellg();
        if (size < 0)
        {
            error = "Could not inspect file size: " + path.generic_u8string();
            return false;
        }
        stream.seekg(0, std::ios::beg);
        bytes.resize(static_cast<std::size_t>(size));
        if (!bytes.empty())
        {
            stream.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }
        if (!stream && !bytes.empty())
        {
            bytes.clear();
            error = "Could not read complete file: " + path.generic_u8string();
            return false;
        }
        error.clear();
        return true;
    }

    std::string HashBytes(const std::vector<std::uint8_t>& bytes)
    {
        constexpr std::uint64_t Offset = 1469598103934665603ull;
        constexpr std::uint64_t Prime = 1099511628211ull;
        std::uint64_t hash = Offset;
        for (const auto value : bytes)
        {
            hash ^= value;
            hash *= Prime;
        }
        std::ostringstream stream;
        stream << "fnv1a64:" << std::hex << std::setfill('0')
               << std::setw(16) << hash;
        return stream.str();
    }

    const AssetRecord* FindRecord(
        const AssetRegistry& registry,
        const StableId& assetId)
    {
        const auto found = std::find_if(
            registry.records.begin(), registry.records.end(),
            [&assetId](const AssetRecord& record)
            { return record.assetId == assetId; });
        return found == registry.records.end() ? nullptr : &*found;
    }

    AssetRecord* FindRecord(
        AssetRegistry& registry,
        const StableId& assetId)
    {
        const auto found = std::find_if(
            registry.records.begin(), registry.records.end(),
            [&assetId](const AssetRecord& record)
            { return record.assetId == assetId; });
        return found == registry.records.end() ? nullptr : &*found;
    }

    const MissingAssetRecord* FindMissingRecord(
        const AssetRegistry& registry,
        const StableId& assetId)
    {
        const auto found = std::find_if(
            registry.missingAssets.begin(), registry.missingAssets.end(),
            [&assetId](const MissingAssetRecord& record)
            { return record.assetId == assetId; });
        return found == registry.missingAssets.end() ? nullptr : &*found;
    }

    const ImportedProductRecord* FindProvenance(
        const AssetRegistry& registry,
        const StableId& assetId)
    {
        const auto found = std::find_if(
            registry.importedProducts.begin(), registry.importedProducts.end(),
            [&assetId](const ImportedProductRecord& record)
            { return record.productAssetId == assetId; });
        return found == registry.importedProducts.end() ? nullptr : &*found;
    }

    ImportedProductRecord* FindProvenance(
        AssetRegistry& registry,
        const StableId& assetId)
    {
        const auto found = std::find_if(
            registry.importedProducts.begin(), registry.importedProducts.end(),
            [&assetId](const ImportedProductRecord& record)
            { return record.productAssetId == assetId; });
        return found == registry.importedProducts.end() ? nullptr : &*found;
    }

    DependencyClass ResourceDependencyClass(const ResourceClass resourceClass)
    {
        switch (resourceClass)
        {
        case ResourceClass::Texture: return DependencyClass::Texture;
        case ResourceClass::Audio: return DependencyClass::Audio;
        case ResourceClass::Script: return DependencyClass::Script;
        case ResourceClass::Video: return DependencyClass::Video;
        case ResourceClass::Font: return DependencyClass::Font;
        default: return DependencyClass::Data;
        }
    }

    fs::path SourcePrefix(const ResourceClass resourceClass)
    {
        switch (resourceClass)
        {
        case ResourceClass::Texture: return "SourceAssets/Textures";
        case ResourceClass::Audio: return "SourceAssets/Audio";
        case ResourceClass::Script: return "SourceAssets/Scripts";
        case ResourceClass::Video: return "SourceAssets/Video";
        case ResourceClass::Font: return "SourceAssets/Fonts";
        default: return {};
        }
    }

    fs::path ProductPrefix(const ResourceClass resourceClass)
    {
        switch (resourceClass)
        {
        case ResourceClass::Texture: return "Content/Textures";
        case ResourceClass::Audio: return "Content/Audio";
        case ResourceClass::Script: return "Content/Scripts";
        case ResourceClass::Video: return "Content/Video";
        case ResourceClass::Font: return "Content/Fonts";
        default: return {};
        }
    }

    bool ParseResourceClassToken(
        const std::string& token,
        ResourceClass& resourceClass)
    {
        if (token == "texture") resourceClass = ResourceClass::Texture;
        else if (token == "audio") resourceClass = ResourceClass::Audio;
        else if (token == "script") resourceClass = ResourceClass::Script;
        else if (token == "video") resourceClass = ResourceClass::Video;
        else if (token == "font") resourceClass = ResourceClass::Font;
        else
        {
            resourceClass = ResourceClass::Unknown;
            return false;
        }
        return true;
    }

    bool ParseResourceFormatToken(
        const std::string& token,
        ResourceSourceFormat& format)
    {
        for (const auto& capability : GetSupportedResourceFormats())
        {
            std::string extension = capability.wickedExtension;
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](const unsigned char value)
                {
                    return value >= 'A' && value <= 'Z'
                        ? static_cast<char>(value + ('a' - 'A'))
                        : static_cast<char>(value);
                });
            if (extension == token)
            {
                format = capability.format;
                return true;
            }
        }
        format = ResourceSourceFormat::Unknown;
        return false;
    }

    bool ParseStoredRecipe(
        const ImportedProductRecord& provenance,
        ResourceClass& resourceClass,
        ResourceSourceFormat& sourceFormat,
        std::string& error)
    {
        resourceClass = ResourceClass::Unknown;
        sourceFormat = ResourceSourceFormat::Unknown;
        if (provenance.importer != "wicked.resourcemanager" ||
            provenance.importerVersion != 1 ||
            provenance.settingsSchema != ResourceAssetImportSettingsSchema ||
            provenance.settingsVersion != 1)
        {
            error = "Stored resource import recipe version/backend is unsupported.";
            return false;
        }
        try
        {
            const auto recipe = nlohmann::json::parse(provenance.settingsJson);
            if (!recipe.is_object() || recipe.dump() != provenance.settingsJson ||
                recipe.size() != 3 ||
                !recipe.contains("options") || !recipe.at("options").is_object() ||
                !recipe.at("options").empty() ||
                !recipe.contains("resource_class") ||
                !recipe.at("resource_class").is_string() ||
                !recipe.contains("source_format") ||
                !recipe.at("source_format").is_string())
            {
                error = "Stored resource import recipe is not the canonical version-1 contract.";
                return false;
            }
            const std::string classToken =
                recipe.at("resource_class").get<std::string>();
            const std::string formatToken =
                recipe.at("source_format").get<std::string>();
            if (!ParseResourceClassToken(classToken, resourceClass) ||
                !ParseResourceFormatToken(formatToken, sourceFormat) ||
                ClassifyResourceSourceFormat(sourceFormat) != resourceClass)
            {
                error = "Stored resource import recipe contains an unsupported class/format.";
                return false;
            }
        }
        catch (const nlohmann::json::exception&)
        {
            error = "Stored resource import recipe is malformed.";
            return false;
        }
        error.clear();
        return true;
    }

    std::uint32_t ReadBigEndian32(
        const std::vector<std::uint8_t>& bytes,
        const std::size_t offset)
    {
        return (static_cast<std::uint32_t>(bytes[offset]) << 24) |
            (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
            (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
            static_cast<std::uint32_t>(bytes[offset + 3]);
    }

    std::uint32_t ReadLittleEndian32(
        const std::vector<std::uint8_t>& bytes,
        const std::size_t offset)
    {
        return static_cast<std::uint32_t>(bytes[offset]) |
            (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
            (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
            (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    }

    ResourceAssetDerivedMetadata DeriveMetadata(
        const ResourceClass resourceClass,
        const ResourceSourceFormat format,
        const std::vector<std::uint8_t>& bytes)
    {
        ResourceAssetDerivedMetadata metadata;
        metadata.known = !bytes.empty();
        metadata.byteCount = static_cast<std::uint64_t>(bytes.size());
        if (resourceClass != ResourceClass::Texture)
            return metadata;
        if (format == ResourceSourceFormat::Png && bytes.size() >= 24 &&
            bytes[12] == 'I' && bytes[13] == 'H' &&
            bytes[14] == 'D' && bytes[15] == 'R')
        {
            metadata.width = ReadBigEndian32(bytes, 16);
            metadata.height = ReadBigEndian32(bytes, 20);
            metadata.mipCount = 1;
            metadata.dimensionsKnown =
                metadata.width > 0 && metadata.height > 0;
        }
        else if (format == ResourceSourceFormat::Dds && bytes.size() >= 32 &&
            bytes[0] == 'D' && bytes[1] == 'D' && bytes[2] == 'S' && bytes[3] == ' ')
        {
            metadata.height = ReadLittleEndian32(bytes, 12);
            metadata.width = ReadLittleEndian32(bytes, 16);
            metadata.mipCount = std::max<std::uint32_t>(
                1, ReadLittleEndian32(bytes, 28));
            metadata.dimensionsKnown =
                metadata.width > 0 && metadata.height > 0;
        }
        if (!metadata.dimensionsKnown)
        {
            metadata.width = 0;
            metadata.height = 0;
            metadata.mipCount = 0;
        }
        return metadata;
    }

    bool UpsertMetadata(
        ResourceAssetMetadataDocument& document,
        const ResourceAssetMetadataRecord& record,
        std::string& error)
    {
        const auto found = std::find_if(
            document.records.begin(), document.records.end(),
            [&record](const ResourceAssetMetadataRecord& existing)
            { return existing.assetId == record.assetId; });
        if (found == document.records.end())
            document.records.push_back(record);
        else
            *found = record;
        std::sort(document.records.begin(), document.records.end(),
            [](const ResourceAssetMetadataRecord& left,
               const ResourceAssetMetadataRecord& right)
            { return left.assetId < right.assetId; });
        return ValidateResourceAssetMetadata(document, error);
    }

    std::vector<std::uint8_t> StringBytes(const std::string& value)
    {
        return {value.begin(), value.end()};
    }

    ProjectDocumentWrite RegistryWrite(
        const fs::path& root,
        const AssetRegistry& registry,
        const std::string& json)
    {
        ProjectDocumentWrite write;
        write.destinationPath = (root / AssetRegistryDocumentName).generic_u8string();
        write.content = StringBytes(json);
        const StableId projectId = registry.projectId;
        write.validator = [projectId, json](
            const std::string& path, std::string& error)
        {
            std::vector<std::uint8_t> bytes;
            if (!ReadBytes(fs::u8path(path), bytes, error))
                return false;
            const std::string staged(bytes.begin(), bytes.end());
            AssetRegistry parsed;
            if (!DeserializeAssetRegistry(staged, parsed, error) ||
                parsed.projectId != projectId)
            {
                if (error.empty()) error = "Staged asset registry belongs to another project.";
                return false;
            }
            std::string canonical;
            if (!SerializeAssetRegistry(parsed, canonical, error) ||
                canonical != staged || staged != json)
            {
                if (error.empty()) error = "Staged asset registry is not the requested canonical state.";
                return false;
            }
            error.clear();
            return true;
        };
        return write;
    }

    ProjectDocumentWrite MetadataWrite(
        const fs::path& root,
        const ResourceAssetMetadataDocument& metadata,
        const std::string& json)
    {
        ProjectDocumentWrite write;
        write.destinationPath =
            (root / ResourceAssetMetadataDocumentName).generic_u8string();
        write.content = StringBytes(json);
        const StableId projectId = metadata.projectId;
        write.validator = [projectId, json](
            const std::string& path, std::string& error)
        {
            std::vector<std::uint8_t> bytes;
            if (!ReadBytes(fs::u8path(path), bytes, error))
                return false;
            const std::string staged(bytes.begin(), bytes.end());
            ResourceAssetMetadataDocument parsed;
            if (!DeserializeResourceAssetMetadata(staged, parsed, error) ||
                parsed.projectId != projectId)
            {
                if (error.empty()) error = "Staged resource metadata belongs to another project.";
                return false;
            }
            std::string canonical;
            if (!SerializeResourceAssetMetadata(parsed, canonical, error) ||
                canonical != staged || staged != json)
            {
                if (error.empty()) error = "Staged resource metadata is not the requested canonical state.";
                return false;
            }
            error.clear();
            return true;
        };
        return write;
    }

    const ResourceAssetMetadataRecord* FindMetadata(
        const ResourceAssetMetadataDocument& document,
        const StableId& assetId)
    {
        const auto found = std::find_if(
            document.records.begin(), document.records.end(),
            [&assetId](const ResourceAssetMetadataRecord& record)
            { return record.assetId == assetId; });
        return found == document.records.end() ? nullptr : &*found;
    }
}

namespace renegade::bridge
{
    ResourceAssetReimportResult ResourceAssetService::ReimportResourceAsset(
        const ResourceAssetReimportRequest& request,
        ResourceAssetReimportOptions options) const
    {
        ResourceAssetReimportResult result;
        result.assetId = request.assetId;
        if (!IsValidStableId(request.projectId) || !IsValidStableId(request.assetId))
        {
            result.error =
                "Resource reimport requires valid project and product asset IDs.";
            return result;
        }

        fs::path root;
        if (!ResolveProjectRoot(request.projectRoot, root, result.error))
            return result;

        AssetRegistry registry;
        if (!ReadAssetRegistry(
                root.generic_u8string(), request.projectId, registry, result.error))
            return result;

        const ImportedProductRecord* foundProvenance =
            FindProvenance(registry, request.assetId);
        if (foundProvenance == nullptr)
        {
            result.error = "Only a registered governed resource product can be reimported.";
            return result;
        }
        const ImportedProductRecord acceptedProvenance = *foundProvenance;
        result.sourceAssetId = acceptedProvenance.sourceAssetId;

        ResourceClass resourceClass = ResourceClass::Unknown;
        ResourceSourceFormat sourceFormat = ResourceSourceFormat::Unknown;
        if (!ParseStoredRecipe(
                acceptedProvenance, resourceClass, sourceFormat, result.error))
            return result;
        result.resourceClass = resourceClass;
        result.sourceFormat = sourceFormat;

        const AssetRecord* sourceRecord = FindRecord(
            static_cast<const AssetRegistry&>(registry), acceptedProvenance.sourceAssetId);
        const AssetRecord* productRecord = FindRecord(
            static_cast<const AssetRegistry&>(registry), request.assetId);
        const MissingAssetRecord* missingProduct = FindMissingRecord(
            registry, request.assetId);
        if (sourceRecord == nullptr)
        {
            result.error =
                "Resource reimport source is missing; recover its LC01 stable ID first.";
            return result;
        }
        if (productRecord == nullptr && missingProduct == nullptr)
        {
            result.error =
                "Resource product identity is neither active nor recoverable in LC01.";
            return result;
        }
        const bool recoveringMissingProduct = productRecord == nullptr;
        result.sourceProjectRelativePath = sourceRecord->projectRelativePath;
        result.assetProjectRelativePath = recoveringMissingProduct
            ? missingProduct->lastKnownPath
            : productRecord->projectRelativePath;
        result.previousProductHash = recoveringMissingProduct
            ? missingProduct->contentHash
            : productRecord->contentHash;

        if (sourceRecord->dependencyClass != DependencyClass::ImportedContent ||
            sourceRecord->requirement != DependencyRequirement::EditorOnly ||
            sourceRecord->provider != "lp08.source_asset" ||
            sourceRecord->providerVersion != 1)
        {
            result.error =
                "Registered resource source is not the accepted LP08 retained-source relationship.";
            return result;
        }

        const DependencyClass trackedProductClass = recoveringMissingProduct
            ? missingProduct->dependencyClass
            : productRecord->dependencyClass;
        const DependencyRequirement trackedProductRequirement = recoveringMissingProduct
            ? missingProduct->requirement
            : productRecord->requirement;
        const std::string& trackedProductProvider = recoveringMissingProduct
            ? missingProduct->provider
            : productRecord->provider;
        const std::uint32_t trackedProductProviderVersion = recoveringMissingProduct
            ? missingProduct->providerVersion
            : productRecord->providerVersion;
        if (trackedProductClass != ResourceDependencyClass(resourceClass) ||
            trackedProductRequirement != DependencyRequirement::Required ||
            trackedProductProvider != "lp08.rasset" ||
            trackedProductProviderVersion != 1)
        {
            result.error =
                "Registered resource product identity is not the accepted LP08 governed relationship.";
            return result;
        }
        if (recoveringMissingProduct &&
            missingProduct->contentHash != acceptedProvenance.productContentHashAtImport)
        {
            result.error =
                "Missing resource tombstone does not match the accepted last-good provenance hash.";
            return result;
        }

        const fs::path sourceRelative =
            fs::u8path(sourceRecord->projectRelativePath).lexically_normal();
        const fs::path productRelative =
            fs::u8path(result.assetProjectRelativePath).lexically_normal();
        if (!IsSafeProjectRelativePath(sourceRelative) ||
            !IsSafeProjectRelativePath(productRelative) ||
            !PathStartsWith(sourceRelative, SourcePrefix(resourceClass)) ||
            !PathStartsWith(productRelative, ProductPrefix(resourceClass)) ||
            productRelative.extension() != ResourceAssetExtension ||
            AssetBrowserService::Classify(productRelative.generic_u8string()) !=
                ResourceClassAssetType(resourceClass))
        {
            result.error =
                "Registered resource paths do not match the canonical class folders.";
            return result;
        }

        std::error_code ec;
        const fs::path sourceRoot =
            fs::weakly_canonical(root / "SourceAssets", ec);
        if (ec || !fs::is_directory(sourceRoot, ec) || ec)
        {
            result.error = "Project SourceAssets folder is unavailable.";
            return result;
        }
        const fs::path contentRoot = fs::weakly_canonical(root / "Content", ec);
        if (ec || !fs::is_directory(contentRoot, ec) || ec)
        {
            result.error = "Project Content folder is unavailable.";
            return result;
        }
        const fs::path sourcePath = fs::weakly_canonical(root / sourceRelative, ec);
        if (ec || !fs::is_regular_file(sourcePath, ec) || ec ||
            !IsWithin(sourcePath, sourceRoot))
        {
            result.error =
                "Resource reimport source is unavailable or resolves outside SourceAssets.";
            return result;
        }

        fs::path productPath;
        if (!recoveringMissingProduct)
        {
            productPath = fs::weakly_canonical(root / productRelative, ec);
            if (ec || !fs::is_regular_file(productPath, ec) || ec ||
                !IsWithin(productPath, contentRoot))
            {
                result.error =
                    "Resource last-good product is unavailable or resolves outside Content.";
                return result;
            }
        }
        else
        {
            const fs::path requestedProductPath = root / productRelative;
            ec.clear();
            const bool destinationExists = fs::exists(requestedProductPath, ec);
            if (ec)
            {
                result.error =
                    "Could not inspect the missing resource product destination.";
                return result;
            }
            if (destinationExists)
            {
                result.error =
                    "Missing resource recovery destination is occupied; refresh LC01 before reimport.";
                return result;
            }
            const fs::path productParent = fs::weakly_canonical(
                requestedProductPath.parent_path(), ec);
            if (ec || !fs::is_directory(productParent, ec) || ec ||
                !IsWithin(productParent, contentRoot))
            {
                result.error =
                    "Missing resource recovery destination is outside project Content.";
                return result;
            }
            productPath = productParent / requestedProductPath.filename();
        }

        if (!GetImportedProductStatus(
                registry, acceptedProvenance, result.statusBefore, result.error))
            return result;
        if (!result.statusBefore.sourceAvailable)
        {
            result.error = "Resource source is unavailable; recover it before reimport.";
            return result;
        }
        if (!recoveringMissingProduct &&
            (!result.statusBefore.productAvailable || result.statusBefore.productChanged))
        {
            result.error =
                "Resource last-good product is missing or changed outside the governed transaction.";
            return result;
        }
        if (recoveringMissingProduct && result.statusBefore.productAvailable)
        {
            result.error =
                "LC01 reports the recovery product as available; refresh state before reimport.";
            return result;
        }

        ResourceSourceInspectionRequest inspectionRequest;
        inspectionRequest.projectRoot = root.generic_u8string();
        inspectionRequest.sourceProjectRelativePath =
            sourceRecord->projectRelativePath;
        inspectionRequest.expectedFormat = sourceFormat;
        const ResourceSourceInspectionResult inspection =
            InspectResourceSource(inspectionRequest);
        if (!inspection.succeeded || inspection.resourceClass != resourceClass ||
            inspection.format != sourceFormat)
        {
            result.error = inspection.error.empty()
                ? "Resource source no longer matches the stored import recipe."
                : inspection.error;
            return result;
        }

        std::vector<std::uint8_t> payload;
        if (!ReadBytes(sourcePath, payload, result.error) || payload.empty())
            return result;
        result.sourceHash = HashBytes(payload);
        if (result.sourceHash != sourceRecord->contentHash)
        {
            result.error =
                "Resource source registry state is not current; refresh LC01 state before reimport.";
            return result;
        }

        ResourceAssetDocument replacement;
        if (!recoveringMissingProduct)
        {
            std::vector<std::uint8_t> existingProductBytes;
            if (!ReadBytes(productPath, existingProductBytes, result.error))
                return result;
            const std::string existingProductHash = HashBytes(existingProductBytes);
            if (existingProductHash != productRecord->contentHash ||
                existingProductHash != acceptedProvenance.productContentHashAtImport)
            {
                result.error =
                    "Resource last-good product bytes do not match LC01 provenance.";
                return result;
            }

            ResourceAssetDocument existingProduct;
            if (!DeserializeResourceAssetDocument(
                    existingProductBytes, existingProduct, result.error))
                return result;
            if (existingProduct.manifest.projectId != request.projectId ||
                existingProduct.manifest.assetId != request.assetId ||
                existingProduct.manifest.sourceAssetId != acceptedProvenance.sourceAssetId ||
                existingProduct.manifest.resourceClass != resourceClass ||
                existingProduct.manifest.sourceFormat != sourceFormat ||
                existingProduct.manifest.importer != acceptedProvenance.importer ||
                existingProduct.manifest.importerVersion != acceptedProvenance.importerVersion ||
                existingProduct.manifest.settingsSchema != acceptedProvenance.settingsSchema ||
                existingProduct.manifest.settingsVersion != acceptedProvenance.settingsVersion ||
                existingProduct.manifest.settingsJson != acceptedProvenance.settingsJson)
            {
                result.error =
                    "Resource last-good .rasset manifest contradicts LC01 provenance/recipe.";
                return result;
            }
            replacement = std::move(existingProduct);
        }
        else
        {
            replacement.manifest.projectId = request.projectId;
            replacement.manifest.assetId = request.assetId;
            replacement.manifest.sourceAssetId = acceptedProvenance.sourceAssetId;
            replacement.manifest.resourceClass = resourceClass;
            replacement.manifest.sourceFormat = sourceFormat;
            replacement.manifest.importer = acceptedProvenance.importer;
            replacement.manifest.importerVersion = acceptedProvenance.importerVersion;
            replacement.manifest.settingsSchema = acceptedProvenance.settingsSchema;
            replacement.manifest.settingsVersion = acceptedProvenance.settingsVersion;
            replacement.manifest.settingsJson = acceptedProvenance.settingsJson;
        }

        result.derived = DeriveMetadata(resourceClass, sourceFormat, payload);
        replacement.payload = payload;
        replacement.manifest.payloadHash = result.sourceHash;
        replacement.manifest.derived = result.derived;
        std::vector<std::uint8_t> replacementBytes;
        if (!SerializeResourceAssetDocument(
                replacement, replacementBytes, result.error))
            return result;
        result.productHash = HashBytes(replacementBytes);

        AssetRegistry candidate = registry;
        AssetRecord* candidateSource =
            FindRecord(candidate, acceptedProvenance.sourceAssetId);
        ImportedProductRecord* candidateProvenance =
            FindProvenance(candidate, request.assetId);
        if (candidateSource == nullptr || candidateProvenance == nullptr)
        {
            result.error =
                "Resource identity/provenance disappeared while preparing reimport.";
            return result;
        }

        if (!recoveringMissingProduct)
        {
            AssetRecord* candidateProduct = FindRecord(candidate, request.assetId);
            if (candidateProduct == nullptr)
            {
                result.error =
                    "Resource product identity disappeared while preparing reimport.";
                return result;
            }
            candidateProduct->contentHash = result.productHash;
        }
        else
        {
            const MissingAssetRecord tombstone = *missingProduct;
            candidate.missingAssets.erase(std::remove_if(
                candidate.missingAssets.begin(), candidate.missingAssets.end(),
                [&request](const MissingAssetRecord& record)
                { return record.assetId == request.assetId; }),
                candidate.missingAssets.end());

            AssetRecord restoredProduct;
            restoredProduct.assetId = request.assetId;
            restoredProduct.dependencyNodeId = "lp08.rasset:" + request.assetId;
            restoredProduct.projectRelativePath = tombstone.lastKnownPath;
            restoredProduct.dependencyClass = tombstone.dependencyClass;
            restoredProduct.requirement = tombstone.requirement;
            restoredProduct.applicability = tombstone.applicability;
            restoredProduct.provider = tombstone.provider;
            restoredProduct.providerVersion = tombstone.providerVersion;
            restoredProduct.contentHash = result.productHash;
            restoredProduct.sourceAvailable = true;
            candidate.records.push_back(std::move(restoredProduct));
            candidate.schemaVersion = AssetRegistry::CurrentSchemaVersion;
        }

        candidateProvenance->sourceContentHashAtImport = result.sourceHash;
        candidateProvenance->productContentHashAtImport = result.productHash;
        const AssetRecord* candidateProduct = FindRecord(
            static_cast<const AssetRegistry&>(candidate), request.assetId);
        if (candidateSource->contentHash != result.sourceHash ||
            candidateProduct == nullptr ||
            candidateProduct->contentHash != result.productHash ||
            !ValidateAssetRegistry(candidate, result.error))
        {
            if (result.error.empty())
                result.error = "Resource replacement provenance is inconsistent.";
            return result;
        }

        ResourceAssetMetadataDocument metadata;
        if (!ReadResourceAssetMetadata(
                root.generic_u8string(), request.projectId, metadata, result.error))
            return result;
        ResourceAssetMetadataRecord metadataRecord;
        metadataRecord.assetId = request.assetId;
        metadataRecord.resourceClass = resourceClass;
        metadataRecord.sourceFormat = sourceFormat;
        metadataRecord.derived = result.derived;
        if (!UpsertMetadata(metadata, metadataRecord, result.error))
            return result;

        std::string registryJson;
        std::string metadataJson;
        if (!SerializeAssetRegistry(candidate, registryJson, result.error) ||
            !SerializeResourceAssetMetadata(metadata, metadataJson, result.error))
            return result;

        ProjectDocumentWrite productWrite;
        productWrite.destinationPath = productPath.generic_u8string();
        productWrite.content = replacementBytes;
        const StableId expectedProjectId = request.projectId;
        const StableId expectedAssetId = request.assetId;
        const StableId expectedSourceId = acceptedProvenance.sourceAssetId;
        const std::string expectedImporter = acceptedProvenance.importer;
        const std::uint32_t expectedImporterVersion = acceptedProvenance.importerVersion;
        const std::string expectedSettingsSchema = acceptedProvenance.settingsSchema;
        const std::uint32_t expectedSettingsVersion = acceptedProvenance.settingsVersion;
        const std::string expectedRecipe = acceptedProvenance.settingsJson;
        productWrite.validator = [sourcePath, payload, replacementBytes,
            expectedProjectId, expectedAssetId, expectedSourceId,
            resourceClass, sourceFormat, expectedImporter,
            expectedImporterVersion, expectedSettingsSchema,
            expectedSettingsVersion, expectedRecipe](
            const std::string& stagedPath, std::string& error)
        {
            std::vector<std::uint8_t> currentSource;
            if (!ReadBytes(sourcePath, currentSource, error) ||
                currentSource != payload)
            {
                if (error.empty())
                    error = "Retained resource source changed during reimport.";
                return false;
            }
            std::vector<std::uint8_t> staged;
            if (!ReadBytes(fs::u8path(stagedPath), staged, error) ||
                staged != replacementBytes)
            {
                if (error.empty())
                    error = "Staged resource replacement bytes changed.";
                return false;
            }
            ResourceAssetDocument parsed;
            if (!DeserializeResourceAssetDocument(staged, parsed, error))
                return false;
            if (parsed.manifest.projectId != expectedProjectId ||
                parsed.manifest.assetId != expectedAssetId ||
                parsed.manifest.sourceAssetId != expectedSourceId ||
                parsed.manifest.resourceClass != resourceClass ||
                parsed.manifest.sourceFormat != sourceFormat ||
                parsed.manifest.importer != expectedImporter ||
                parsed.manifest.importerVersion != expectedImporterVersion ||
                parsed.manifest.settingsSchema != expectedSettingsSchema ||
                parsed.manifest.settingsVersion != expectedSettingsVersion ||
                parsed.manifest.settingsJson != expectedRecipe ||
                parsed.payload != payload)
            {
                error =
                    "Staged resource replacement identity/recipe/payload changed during reimport.";
                return false;
            }
            error.clear();
            return true;
        };

        std::vector<ProjectDocumentWrite> writes;
        writes.push_back(std::move(productWrite));
        writes.push_back(RegistryWrite(root, candidate, registryJson));
        writes.push_back(MetadataWrite(root, metadata, metadataJson));

        ProjectDocumentTransactionOptions transactionOptions;
        transactionOptions.transactionId = std::move(options.transactionId);
        transactionOptions.journalDirectory =
            (root / "Intermediate/Transactions").generic_u8string();
        transactionOptions.allowedRoot = root.generic_u8string();
        transactionOptions.operationHook = std::move(options.operationHook);
        ProjectDocumentTransaction transaction;
        result.transaction = transaction.Execute(
            std::move(writes), std::move(transactionOptions));
        if (!result.transaction.success || !result.transaction.committed)
        {
            result.error = "Resource reimport transaction failed [" +
                result.transaction.code + "]: " + result.transaction.message;
            return result;
        }

        ResourceAssetDocument reopenedProduct;
        AssetRegistry reopenedRegistry;
        ResourceAssetMetadataDocument reopenedMetadata;
        if (!ReadResourceAssetDocument(
                productPath.generic_u8string(), reopenedProduct, result.error) ||
            !ReadAssetRegistry(
                root.generic_u8string(), request.projectId,
                reopenedRegistry, result.error) ||
            !ReadResourceAssetMetadata(
                root.generic_u8string(), request.projectId,
                reopenedMetadata, result.error))
        {
            result.error =
                "Committed resource replacement did not reopen: " + result.error;
            return result;
        }
        const ImportedProductRecord* reopenedProvenance =
            FindProvenance(reopenedRegistry, request.assetId);
        const AssetRecord* reopenedRecord = FindRecord(
            static_cast<const AssetRegistry&>(reopenedRegistry), request.assetId);
        const MissingAssetRecord* reopenedMissing =
            FindMissingRecord(reopenedRegistry, request.assetId);
        const ResourceAssetMetadataRecord* reopenedMetadataRecord =
            FindMetadata(reopenedMetadata, request.assetId);
        if (reopenedProduct.manifest.projectId != request.projectId ||
            reopenedProduct.manifest.assetId != request.assetId ||
            reopenedProduct.manifest.sourceAssetId != result.sourceAssetId ||
            reopenedProduct.manifest.resourceClass != resourceClass ||
            reopenedProduct.manifest.sourceFormat != sourceFormat ||
            reopenedProduct.manifest.importer != acceptedProvenance.importer ||
            reopenedProduct.manifest.importerVersion != acceptedProvenance.importerVersion ||
            reopenedProduct.manifest.settingsSchema != acceptedProvenance.settingsSchema ||
            reopenedProduct.manifest.settingsVersion != acceptedProvenance.settingsVersion ||
            reopenedProduct.manifest.settingsJson != acceptedProvenance.settingsJson ||
            reopenedProduct.payload != payload ||
            reopenedRecord == nullptr || reopenedMissing != nullptr ||
            reopenedRecord->projectRelativePath != result.assetProjectRelativePath ||
            reopenedRecord->contentHash != result.productHash ||
            reopenedProvenance == nullptr ||
            reopenedProvenance->sourceAssetId != result.sourceAssetId ||
            reopenedProvenance->sourceContentHashAtImport != result.sourceHash ||
            reopenedProvenance->productContentHashAtImport != result.productHash ||
            reopenedMetadataRecord == nullptr ||
            reopenedMetadataRecord->resourceClass != resourceClass ||
            reopenedMetadataRecord->sourceFormat != sourceFormat ||
            reopenedMetadataRecord->derived != result.derived)
        {
            result.error =
                "Committed resource replacement reopen evidence is inconsistent.";
            return result;
        }

        result.recoveredMissingProduct = recoveringMissingProduct;
        result.succeeded = true;
        result.error.clear();
        return result;
    }
}
