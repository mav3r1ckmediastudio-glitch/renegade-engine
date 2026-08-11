#include "renegade/bridge/ReusableAssetService.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <utility>

#include "json.hpp"

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

        bool IsSafeCanonicalProjectPath(const std::string& value)
        {
            if (value.empty() || value.find('\\') != std::string::npos)
                return false;
            const fs::path path = fs::u8path(value);
            if (path.is_absolute() || path.has_root_name() ||
                path.generic_u8string() != value ||
                path.lexically_normal().generic_u8string() != value)
                return false;
            return std::none_of(path.begin(), path.end(),
                [](const fs::path& part)
                {
                    return part == "." || part == "..";
                });
        }

        bool HasTopLevelFolder(const std::string& path, const char* folder)
        {
            const fs::path parsed = fs::u8path(path);
            return parsed.begin() != parsed.end() &&
                parsed.begin()->generic_u8string() == folder;
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

        bool ResolveProjectRoot(
            const std::string& projectRoot,
            fs::path& root,
            std::string& error)
        {
            root.clear();
            if (projectRoot.empty())
            {
                error = "Reusable asset reimport requires a project root.";
                return false;
            }
            std::error_code ec;
            root = fs::weakly_canonical(fs::absolute(fs::u8path(projectRoot), ec), ec);
            if (ec || root.empty() || !fs::is_directory(root, ec) || ec)
            {
                error = "Reusable asset reimport project root is unavailable: " + projectRoot;
                root.clear();
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
            if (size < 0 || static_cast<std::uintmax_t>(size) >
                    (std::numeric_limits<std::size_t>::max)())
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

        bool HashFile(
            const fs::path& path,
            std::string& hash,
            std::string& error)
        {
            std::vector<std::uint8_t> bytes;
            if (!ReadBytes(path, bytes, error))
                return false;
            hash = HashBytes(bytes);
            error.clear();
            return true;
        }

        AssetRecord* FindRecordById(AssetRegistry& registry, const StableId& id)
        {
            const auto found = std::find_if(registry.records.begin(), registry.records.end(),
                [&id](const AssetRecord& record) { return record.assetId == id; });
            return found == registry.records.end() ? nullptr : &*found;
        }

        const ImportedProductRecord* FindImportedProduct(
            const AssetRegistry& registry,
            const StableId& productAssetId)
        {
            const auto found = std::find_if(
                registry.importedProducts.begin(), registry.importedProducts.end(),
                [&productAssetId](const ImportedProductRecord& record)
                { return record.productAssetId == productAssetId; });
            return found == registry.importedProducts.end() ? nullptr : &*found;
        }

        const char* SourceFormatToken(const ModelSourceFormat format) noexcept
        {
            switch (format)
            {
            case ModelSourceFormat::Fbx: return "fbx";
            case ModelSourceFormat::Gltf: return "gltf";
            case ModelSourceFormat::Glb: return "glb";
            default: return "";
            }
        }

        bool ParseSourceFormatToken(
            const std::string& token,
            ModelSourceFormat& format)
        {
            if (token == "fbx") format = ModelSourceFormat::Fbx;
            else if (token == "gltf") format = ModelSourceFormat::Gltf;
            else if (token == "glb") format = ModelSourceFormat::Glb;
            else return false;
            return true;
        }

        const char* ExpectedBackend(const ModelSourceFormat format) noexcept
        {
            switch (format)
            {
            case ModelSourceFormat::Fbx: return "wicked.ufbx";
            case ModelSourceFormat::Gltf:
            case ModelSourceFormat::Glb: return "wicked.gltf";
            default: return "";
            }
        }

        bool ParseStoredRecipe(
            const ImportedProductRecord& provenance,
            ModelSourceFormat& format,
            std::string& error)
        {
            format = ModelSourceFormat::Unknown;
            if (provenance.importerVersion != 1 ||
                provenance.settingsSchema != ReusableModelImportSettingsSchema ||
                provenance.settingsVersion != 1)
            {
                error = "Stored reusable-model import recipe version is unsupported.";
                return false;
            }
            try
            {
                const nlohmann::json recipe = nlohmann::json::parse(provenance.settingsJson);
                if (!recipe.is_object() || recipe.dump() != provenance.settingsJson ||
                    recipe.size() != 2 || !recipe.contains("source_format") ||
                    !recipe.at("source_format").is_string() ||
                    !recipe.contains("options") || !recipe.at("options").is_object())
                {
                    error = "Stored reusable-model import recipe is not the canonical version-1 contract.";
                    return false;
                }
                const std::string token = recipe.at("source_format").get<std::string>();
                if (!ParseSourceFormatToken(token, format) ||
                    !ImportService::IsModelSourceFormatSupported(format))
                {
                    error = "Stored reusable-model source format is not enabled by LP07.";
                    format = ModelSourceFormat::Unknown;
                    return false;
                }
            }
            catch (const nlohmann::json::exception&)
            {
                error = "Stored reusable-model import recipe is malformed.";
                return false;
            }
            if (provenance.importer != ExpectedBackend(format))
            {
                error = "Stored reusable-model importer backend contradicts its source-format recipe.";
                format = ModelSourceFormat::Unknown;
                return false;
            }
            error.clear();
            return true;
        }

        bool ConvertCount(
            const std::size_t value,
            std::uint32_t& converted,
            std::string& error)
        {
            if (value > (std::numeric_limits<std::uint32_t>::max)())
            {
                error = "Reimported model metadata exceeds the supported 32-bit count range.";
                return false;
            }
            converted = static_cast<std::uint32_t>(value);
            return true;
        }

        bool BuildModelMetadata(
            const ImportResult& result,
            const wi::scene::Scene& scene,
            ModelDerivedMetadata& metadata,
            std::string& error)
        {
            metadata = {};
            metadata.known = true;
            if (!ConvertCount(result.imported.meshes, metadata.meshCount, error) ||
                !ConvertCount(result.imported.materials, metadata.materialCount, error) ||
                !ConvertCount(result.imported.armatures, metadata.armatureCount, error) ||
                !ConvertCount(result.importedEvidence.armatureBones, metadata.boneCount, error) ||
                !ConvertCount(result.imported.animations, metadata.animationClipCount, error) ||
                !ConvertCount(result.importedEvidence.animationChannels,
                    metadata.animationChannelCount, error))
            {
                metadata = {};
                return false;
            }

            std::size_t morphTargets = 0;
            for (std::size_t index = 0; index < scene.meshes.GetCount(); ++index)
            {
                const auto count = scene.meshes[index].morph_targets.size();
                if (count > (std::numeric_limits<std::size_t>::max)() - morphTargets)
                {
                    error = "Reimported model morph-target count overflowed.";
                    metadata = {};
                    return false;
                }
                morphTargets += count;
            }
            if (!ConvertCount(morphTargets, metadata.morphTargetCount, error))
            {
                metadata = {};
                return false;
            }
            metadata.skinned = result.importedEvidence.skinnedMeshes > 0;
            metadata.animated = result.imported.animations > 0 &&
                result.importedEvidence.animationChannels > 0;
            error.clear();
            return true;
        }

        ProjectDocumentWrite RegistryWrite(
            const fs::path& root,
            const AssetRegistry& registry,
            const std::string& json)
        {
            ProjectDocumentWrite write;
            write.destinationPath = (root / AssetRegistryDocumentName).generic_u8string();
            write.content.assign(json.begin(), json.end());
            const StableId projectId = registry.projectId;
            write.validator = [projectId, json](const std::string& path, std::string& error)
            {
                std::ifstream stream(fs::u8path(path), std::ios::binary);
                const std::string staged{
                    std::istreambuf_iterator<char>(stream),
                    std::istreambuf_iterator<char>()};
                if (!stream && !stream.eof())
                {
                    error = "Could not read staged asset registry.";
                    return false;
                }
                AssetRegistry parsed;
                if (!DeserializeAssetRegistry(staged, parsed, error) || parsed.projectId != projectId)
                {
                    if (error.empty()) error = "Staged asset registry belongs to another project.";
                    return false;
                }
                std::string canonical;
                if (!SerializeAssetRegistry(parsed, canonical, error) ||
                    canonical != staged || staged != json)
                {
                    if (error.empty()) error = "Staged asset registry is not the requested canonical document.";
                    return false;
                }
                error.clear();
                return true;
            };
            return write;
        }

        ProjectDocumentWrite MetadataWrite(
            const fs::path& root,
            const AssetCatalogueMetadataDocument& metadata,
            const std::string& json)
        {
            ProjectDocumentWrite write;
            write.destinationPath =
                (root / AssetCatalogueMetadataDocumentName).generic_u8string();
            write.content.assign(json.begin(), json.end());
            const StableId projectId = metadata.projectId;
            write.validator = [projectId, json](const std::string& path, std::string& error)
            {
                std::ifstream stream(fs::u8path(path), std::ios::binary);
                const std::string staged{
                    std::istreambuf_iterator<char>(stream),
                    std::istreambuf_iterator<char>()};
                if (!stream && !stream.eof())
                {
                    error = "Could not read staged asset metadata.";
                    return false;
                }
                AssetCatalogueMetadataDocument parsed;
                if (!DeserializeAssetCatalogueMetadata(staged, parsed, error) ||
                    parsed.projectId != projectId)
                {
                    if (error.empty()) error = "Staged asset metadata belongs to another project.";
                    return false;
                }
                std::string canonical;
                if (!SerializeAssetCatalogueMetadata(parsed, canonical, error) ||
                    canonical != staged || staged != json)
                {
                    if (error.empty()) error = "Staged asset metadata is not the requested canonical document.";
                    return false;
                }
                error.clear();
                return true;
            };
            return write;
        }
    }

    ReusableModelReimportResult ReusableAssetService::ReimportModelAsset(
        const ReusableModelReimportRequest& request,
        ReusableModelReimportOptions options) const
    {
        ReusableModelReimportResult result;
        result.assetId = request.assetId;

        if (!IsValidStableId(request.projectId) || !IsValidStableId(request.assetId))
        {
            result.error = "Reusable model reimport requires valid project and product asset IDs.";
            return result;
        }

        fs::path root;
        if (!ResolveProjectRoot(request.projectRoot, root, result.error))
            return result;

        AssetRegistry registry;
        if (!ReadAssetRegistry(root.generic_u8string(), request.projectId,
                registry, result.error))
            return result;

        const ImportedProductRecord* foundProvenance =
            FindImportedProduct(registry, request.assetId);
        if (foundProvenance == nullptr)
        {
            result.error = "Only a registered imported product can be reimported.";
            return result;
        }
        // Copy the accepted recipe before any candidate registry mutation so no
        // pointer/reference into registry.importedProducts survives replacement.
        const ImportedProductRecord acceptedProvenance = *foundProvenance;
        result.sourceAssetId = acceptedProvenance.sourceAssetId;

        AssetRecord* sourceRecord = FindRecordById(registry, acceptedProvenance.sourceAssetId);
        AssetRecord* productRecord = FindRecordById(registry, acceptedProvenance.productAssetId);
        if (sourceRecord == nullptr)
        {
            result.error = "Reusable model reimport source is missing; recover its LC01 stable ID first.";
            return result;
        }
        if (productRecord == nullptr)
        {
            result.error = "Reusable model product is missing; last-good product cannot be replaced safely.";
            return result;
        }
        result.sourceProjectRelativePath = sourceRecord->projectRelativePath;
        result.assetProjectRelativePath = productRecord->projectRelativePath;
        result.previousProductHash = productRecord->contentHash;

        if (sourceRecord->dependencyClass != DependencyClass::ImportedContent ||
            sourceRecord->requirement != DependencyRequirement::EditorOnly ||
            sourceRecord->provider != "lp07.source_asset" ||
            productRecord->dependencyClass != DependencyClass::ImportedContent ||
            productRecord->requirement != DependencyRequirement::Required ||
            productRecord->provider != "lp07.rasset")
        {
            result.error = "Registered source/product records are not an LP07 reusable-model relationship.";
            return result;
        }
        if (!IsSafeCanonicalProjectPath(sourceRecord->projectRelativePath) ||
            !HasTopLevelFolder(sourceRecord->projectRelativePath, "SourceAssets") ||
            !IsSafeCanonicalProjectPath(productRecord->projectRelativePath) ||
            !HasTopLevelFolder(productRecord->projectRelativePath, "Content") ||
            LowerExtension(productRecord->projectRelativePath) != ReusableAssetExtension)
        {
            result.error = "Registered reusable-model paths are not canonical SourceAssets/Content paths.";
            return result;
        }

        std::error_code ec;
        const fs::path sourceRoot = fs::weakly_canonical(root / "SourceAssets", ec);
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
        const fs::path sourcePath = fs::weakly_canonical(
            root / fs::u8path(sourceRecord->projectRelativePath), ec);
        if (ec || !fs::is_regular_file(sourcePath, ec) || ec ||
            !IsWithin(sourcePath, sourceRoot))
        {
            result.error = "Reusable model reimport source is unavailable or resolves outside SourceAssets.";
            return result;
        }
        const fs::path assetPath = fs::weakly_canonical(
            root / fs::u8path(productRecord->projectRelativePath), ec);
        if (ec || !fs::is_regular_file(assetPath, ec) || ec ||
            !IsWithin(assetPath, contentRoot))
        {
            result.error = "Reusable model last-good product is unavailable or resolves outside Content.";
            return result;
        }

        if (!GetImportedProductStatus(registry, acceptedProvenance,
                result.statusBefore, result.error))
            return result;
        if (!result.statusBefore.sourceAvailable)
        {
            result.error = "Reusable model source is unavailable; recover it before reimport.";
            return result;
        }
        if (!result.statusBefore.productAvailable || result.statusBefore.productChanged)
        {
            result.error = "Reusable model last-good product is missing or changed outside the governed transaction.";
            return result;
        }

        std::string sourceHash;
        if (!HashFile(sourcePath, sourceHash, result.error))
            return result;
        if (sourceHash != sourceRecord->contentHash)
        {
            result.error = "Reusable model source registry state is not current; refresh LC01 state before reimport.";
            return result;
        }

        std::vector<std::uint8_t> existingAssetBytes;
        if (!ReadBytes(assetPath, existingAssetBytes, result.error))
            return result;
        const std::string existingAssetHash = HashBytes(existingAssetBytes);
        if (existingAssetHash != productRecord->contentHash ||
            existingAssetHash != acceptedProvenance.productContentHashAtImport)
        {
            result.error = "Reusable model last-good product bytes do not match LC01 provenance.";
            return result;
        }

        ReusableModelAssetDocument existingAsset;
        if (!DeserializeReusableModelAssetDocument(
                existingAssetBytes, existingAsset, result.error))
            return result;
        if (existingAsset.manifest.projectId != request.projectId ||
            existingAsset.manifest.assetId != request.assetId ||
            existingAsset.manifest.sourceAssetId != acceptedProvenance.sourceAssetId ||
            existingAsset.manifest.importer != acceptedProvenance.importer ||
            existingAsset.manifest.importerVersion != acceptedProvenance.importerVersion ||
            existingAsset.manifest.settingsSchema != acceptedProvenance.settingsSchema ||
            existingAsset.manifest.settingsVersion != acceptedProvenance.settingsVersion ||
            existingAsset.manifest.settingsJson != acceptedProvenance.settingsJson)
        {
            result.error = "Reusable model last-good RAsset manifest contradicts LC01 provenance.";
            return result;
        }

        ModelSourceFormat format = ModelSourceFormat::Unknown;
        if (!ParseStoredRecipe(acceptedProvenance, format, result.error))
            return result;
        if (existingAsset.manifest.sourceFormat != SourceFormatToken(format))
        {
            result.error = "Reusable model RAsset source format contradicts the stored import recipe.";
            return result;
        }

        const fs::path importDirectory = root / "Intermediate" / "Imports";
        fs::create_directories(importDirectory, ec);
        if (ec)
        {
            result.error = "Could not create reusable reimport working directory: " + ec.message();
            return result;
        }
        const fs::path temporaryWiscene =
            importDirectory / fs::u8path(request.assetId + ".reimport.wiscene");
        const auto cleanupTemporary = [&temporaryWiscene]()
        {
            std::error_code ignored;
            fs::remove(temporaryWiscene, ignored);
        };
        cleanupTemporary();

        ImportService importer;
        ModelImportRequest importRequest;
        importRequest.sourcePath = sourcePath.generic_u8string();
        importRequest.assetPath = temporaryWiscene.generic_u8string();
        // Recipe is authoritative: the current path is not used to choose a
        // different importer. ImportService only accepts the path if it remains
        // consistent with this stored format.
        importRequest.expectedFormat = format;
        auto prepared = importer.PrepareModelAsset(importRequest);
        if (!prepared.IsReady())
        {
            result.import = prepared.Result();
            result.error = result.import.error.empty()
                ? "Reusable model reimport conversion did not produce a prepared scene."
                : result.import.error;
            cleanupTemporary();
            return result;
        }
        const wi::scene::Scene* preparedScene = prepared.PeekScene();
        if (preparedScene == nullptr)
        {
            result.error = "Reusable model reimport lost its prepared scene before validation.";
            cleanupTemporary();
            return result;
        }

        result.import = importer.SavePreparedModelAsset(prepared);
        if (!result.import.succeeded)
        {
            result.error = result.import.error;
            cleanupTemporary();
            return result;
        }
        if (result.import.sourceFormat != format ||
            result.import.importerBackend != acceptedProvenance.importer)
        {
            result.error = "Reusable model converter did not execute the stored importer recipe.";
            cleanupTemporary();
            return result;
        }
        if (!BuildModelMetadata(result.import, *preparedScene,
                result.modelMetadata, result.error))
        {
            cleanupTemporary();
            return result;
        }

        std::string sourceHashAfter;
        if (!HashFile(sourcePath, sourceHashAfter, result.error) ||
            sourceHashAfter != sourceHash)
        {
            if (result.error.empty())
                result.error = "Reusable model reimport modified the authoritative source bytes.";
            cleanupTemporary();
            return result;
        }

        ReusableModelAssetDocument replacement;
        replacement.manifest.projectId = request.projectId;
        replacement.manifest.assetId = request.assetId;
        replacement.manifest.sourceAssetId = acceptedProvenance.sourceAssetId;
        replacement.manifest.sourceFormat = SourceFormatToken(format);
        replacement.manifest.importer = acceptedProvenance.importer;
        replacement.manifest.importerVersion = acceptedProvenance.importerVersion;
        replacement.manifest.settingsSchema = acceptedProvenance.settingsSchema;
        replacement.manifest.settingsVersion = acceptedProvenance.settingsVersion;
        replacement.manifest.settingsJson = acceptedProvenance.settingsJson;
        if (!ReadBytes(temporaryWiscene, replacement.payload, result.error))
        {
            cleanupTemporary();
            return result;
        }
        cleanupTemporary();
        replacement.manifest.payloadHash = HashBytes(replacement.payload);

        std::vector<std::uint8_t> replacementBytes;
        if (!SerializeReusableModelAssetDocument(
                replacement, replacementBytes, result.error))
            return result;
        result.productHash = HashBytes(replacementBytes);

        productRecord->contentHash = result.productHash;
        auto updatedProvenance = std::find_if(
            registry.importedProducts.begin(), registry.importedProducts.end(),
            [&request](const ImportedProductRecord& record)
            { return record.productAssetId == request.assetId; });
        if (updatedProvenance == registry.importedProducts.end())
        {
            result.error = "Reusable model provenance disappeared while preparing reimport.";
            return result;
        }
        updatedProvenance->sourceContentHashAtImport = sourceHash;
        updatedProvenance->productContentHashAtImport = result.productHash;

        // Validate the complete registry schema, but deliberately update only
        // this product's snapshot. Reimport of one product must not require
        // unrelated imported products to be current or even presently active.
        if (sourceRecord->contentHash != updatedProvenance->sourceContentHashAtImport ||
            productRecord->contentHash != updatedProvenance->productContentHashAtImport ||
            !ValidateAssetRegistry(registry, result.error))
        {
            if (result.error.empty())
                result.error = "Reusable model target provenance does not snapshot the accepted replacement.";
            return result;
        }

        AssetCatalogueMetadataDocument metadata;
        if (!ReadAssetCatalogueMetadata(root.generic_u8string(), request.projectId,
                metadata, result.error))
            return result;
        if (!SetAssetModelDerivedMetadata(
                metadata, request.assetId, result.modelMetadata, result.error))
            return result;

        std::string registryJson;
        std::string metadataJson;
        if (!SerializeAssetRegistry(registry, registryJson, result.error) ||
            !SerializeAssetCatalogueMetadata(metadata, metadataJson, result.error))
            return result;

        ProjectDocumentWrite assetWrite;
        assetWrite.destinationPath = assetPath.generic_u8string();
        assetWrite.content = replacementBytes;
        const StableId expectedProjectId = request.projectId;
        const StableId expectedAssetId = request.assetId;
        const StableId expectedSourceId = result.sourceAssetId;
        const std::string expectedImporter = acceptedProvenance.importer;
        const std::uint32_t expectedImporterVersion = acceptedProvenance.importerVersion;
        const std::string expectedSettingsSchema = acceptedProvenance.settingsSchema;
        const std::uint32_t expectedSettingsVersion = acceptedProvenance.settingsVersion;
        const std::string expectedSettingsJson = acceptedProvenance.settingsJson;
        assetWrite.validator = [expectedProjectId, expectedAssetId,
            expectedSourceId, expectedImporter, expectedImporterVersion,
            expectedSettingsSchema, expectedSettingsVersion, expectedSettingsJson,
            replacementBytes](const std::string& path, std::string& error)
        {
            std::vector<std::uint8_t> staged;
            if (!ReadBytes(fs::u8path(path), staged, error))
                return false;
            if (staged != replacementBytes)
            {
                error = "Staged replacement RAsset bytes do not match the accepted candidate.";
                return false;
            }
            ReusableModelAssetDocument parsed;
            if (!DeserializeReusableModelAssetDocument(staged, parsed, error))
                return false;
            if (parsed.manifest.projectId != expectedProjectId ||
                parsed.manifest.assetId != expectedAssetId ||
                parsed.manifest.sourceAssetId != expectedSourceId ||
                parsed.manifest.importer != expectedImporter ||
                parsed.manifest.importerVersion != expectedImporterVersion ||
                parsed.manifest.settingsSchema != expectedSettingsSchema ||
                parsed.manifest.settingsVersion != expectedSettingsVersion ||
                parsed.manifest.settingsJson != expectedSettingsJson)
            {
                error = "Staged replacement RAsset identity/recipe changed during reimport.";
                return false;
            }
            error.clear();
            return true;
        };

        std::vector<ProjectDocumentWrite> writes;
        writes.push_back(std::move(assetWrite));
        writes.push_back(RegistryWrite(root, registry, registryJson));
        writes.push_back(MetadataWrite(root, metadata, metadataJson));

        ProjectDocumentTransactionOptions transactionOptions;
        transactionOptions.transactionId = std::move(options.transactionId);
        transactionOptions.journalDirectory =
            (root / "Intermediate" / "Transactions").generic_u8string();
        transactionOptions.allowedRoot = root.generic_u8string();
        transactionOptions.operationHook = std::move(options.operationHook);
        ProjectDocumentTransaction transaction;
        result.transaction = transaction.Execute(
            std::move(writes), std::move(transactionOptions));
        if (!result.transaction.success || !result.transaction.committed)
        {
            result.error = "Reusable model reimport transaction failed [" +
                result.transaction.code + "]: " + result.transaction.message;
            return result;
        }

        result.succeeded = true;
        result.error.clear();
        return result;
    }
}
