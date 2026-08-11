#include "renegade/bridge/ReusableAssetService.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <set>
#include <sstream>
#include <utility>

#include "json.hpp"

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr std::array<std::uint8_t, 8> RAssetMagic = {
            'R', 'A', 'S', 'S', 'E', 'T', '0', '1'
        };
        constexpr std::uint32_t MaximumManifestBytes = 1024u * 1024u;
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
                error = "Reusable asset import requires a project root.";
                return false;
            }
            std::error_code ec;
            root = fs::weakly_canonical(fs::absolute(fs::u8path(projectRoot), ec), ec);
            if (ec || root.empty() || !fs::is_directory(root, ec) || ec)
            {
                error = "Reusable asset project root is unavailable: " + projectRoot;
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

        bool IsCanonicalJsonObject(const std::string& value)
        {
            if (value.empty())
                return false;
            try
            {
                const nlohmann::json parsed = nlohmann::json::parse(value);
                return parsed.is_object() && parsed.dump() == value;
            }
            catch (const nlohmann::json::exception&)
            {
                return false;
            }
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

        bool IsSupportedSourceFormatToken(const std::string& value)
        {
            return value == "fbx" || value == "gltf" || value == "glb";
        }

        std::string BuildRecipeJson(
            const ModelSourceFormat format,
            const std::string& optionsJson,
            std::string& error)
        {
            try
            {
                const nlohmann::json options = nlohmann::json::parse(optionsJson);
                if (!options.is_object() || options.dump() != optionsJson)
                {
                    error = "Reusable model import settings must be a canonical JSON object.";
                    return {};
                }
                nlohmann::json recipe;
                recipe["options"] = options;
                recipe["source_format"] = SourceFormatToken(format);
                error.clear();
                return recipe.dump();
            }
            catch (const nlohmann::json::exception&)
            {
                error = "Reusable model import settings are not valid JSON.";
                return {};
            }
        }

        std::string SerializeManifest(
            const ReusableModelAssetManifest& manifest)
        {
            nlohmann::json document;
            document["asset_id"] = manifest.assetId;
            document["format"] = manifest.formatIdentifier;
            document["importer"] = manifest.importer;
            document["importer_version"] = manifest.importerVersion;
            document["payload_format"] = manifest.payloadFormat;
            document["payload_hash"] = manifest.payloadHash;
            document["project_id"] = manifest.projectId;
            document["schema_version"] = manifest.schemaVersion;
            document["settings"] = nlohmann::json::parse(manifest.settingsJson);
            document["settings_schema"] = manifest.settingsSchema;
            document["settings_version"] = manifest.settingsVersion;
            document["source_asset_id"] = manifest.sourceAssetId;
            document["source_format"] = manifest.sourceFormat;
            return document.dump();
        }

        bool ParseManifest(
            const std::string& text,
            ReusableModelAssetManifest& manifest,
            std::string& error)
        {
            manifest = {};
            try
            {
                const nlohmann::json document = nlohmann::json::parse(text);
                if (!document.is_object() || document.dump() != text)
                {
                    error = "RAsset manifest is not canonical JSON.";
                    return false;
                }
                manifest.assetId = document.at("asset_id").get<std::string>();
                manifest.formatIdentifier = document.at("format").get<std::string>();
                manifest.importer = document.at("importer").get<std::string>();
                manifest.importerVersion = document.at("importer_version").get<std::uint32_t>();
                manifest.payloadFormat = document.at("payload_format").get<std::string>();
                manifest.payloadHash = document.at("payload_hash").get<std::string>();
                manifest.projectId = document.at("project_id").get<std::string>();
                manifest.schemaVersion = document.at("schema_version").get<std::uint32_t>();
                manifest.settingsJson = document.at("settings").dump();
                manifest.settingsSchema = document.at("settings_schema").get<std::string>();
                manifest.settingsVersion = document.at("settings_version").get<std::uint32_t>();
                manifest.sourceAssetId = document.at("source_asset_id").get<std::string>();
                manifest.sourceFormat = document.at("source_format").get<std::string>();
            }
            catch (const nlohmann::json::exception&)
            {
                error = "RAsset manifest is malformed or missing required fields.";
                manifest = {};
                return false;
            }
            error.clear();
            return true;
        }

        void AppendU32(std::vector<std::uint8_t>& bytes, const std::uint32_t value)
        {
            bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
            bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
            bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
            bytes.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
        }

        void AppendU64(std::vector<std::uint8_t>& bytes, const std::uint64_t value)
        {
            for (unsigned shift = 0; shift < 64; shift += 8)
                bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
        }

        std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes, const std::size_t offset)
        {
            return static_cast<std::uint32_t>(bytes[offset]) |
                (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
                (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
                (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
        }

        std::uint64_t ReadU64(const std::vector<std::uint8_t>& bytes, const std::size_t offset)
        {
            std::uint64_t value = 0;
            for (unsigned shift = 0; shift < 64; shift += 8)
                value |= static_cast<std::uint64_t>(bytes[offset + shift / 8]) << shift;
            return value;
        }

        bool ConvertCount(
            const std::size_t value,
            std::uint32_t& converted,
            std::string& error)
        {
            if (value > (std::numeric_limits<std::uint32_t>::max)())
            {
                error = "Imported model metadata exceeds the supported 32-bit count range.";
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
                    error = "Imported model morph-target count overflowed.";
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

        bool GenerateUniqueId(
            const AssetRegistry& registry,
            const AssetIdGenerator& generator,
            StableId& id,
            std::string& error)
        {
            if (!generator)
            {
                error = "Reusable asset import requires an asset ID generator.";
                return false;
            }
            std::set<StableId> known;
            for (const auto& record : registry.records)
                known.insert(record.assetId);
            for (const auto& missing : registry.missingAssets)
                known.insert(missing.assetId);
            id = generator();
            if (!IsValidStableId(id) || known.find(id) != known.end())
            {
                error = "Reusable asset ID generator returned an invalid or duplicate ID.";
                id.clear();
                return false;
            }
            error.clear();
            return true;
        }

        AssetRecord* FindRecordByPath(AssetRegistry& registry, const std::string& path)
        {
            const auto found = std::find_if(registry.records.begin(), registry.records.end(),
                [&path](const AssetRecord& record)
                { return record.projectRelativePath == path; });
            return found == registry.records.end() ? nullptr : &*found;
        }

        bool HasMissingPath(const AssetRegistry& registry, const std::string& path)
        {
            return std::any_of(registry.missingAssets.begin(), registry.missingAssets.end(),
                [&path](const MissingAssetRecord& record)
                { return record.lastKnownPath == path; });
        }

        bool ReadRegistryOrCreate(
            const fs::path& root,
            const StableId& projectId,
            AssetRegistry& registry,
            std::string& error)
        {
            const fs::path path = root / AssetRegistryDocumentName;
            std::error_code ec;
            if (!fs::exists(path, ec))
            {
                if (ec)
                {
                    error = "Could not inspect the project asset registry: " + ec.message();
                    return false;
                }
                registry = {};
                registry.projectId = projectId;
                error.clear();
                return true;
            }
            return ReadAssetRegistry(root.generic_u8string(), projectId, registry, error);
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

    bool ValidateReusableModelAssetDocument(
        const ReusableModelAssetDocument& document,
        std::string& error)
    {
        const auto& manifest = document.manifest;
        if (manifest.formatIdentifier != ReusableModelAssetFormat ||
            manifest.schemaVersion != ReusableModelAssetManifest::CurrentSchemaVersion)
        {
            error = "Unsupported RAsset model schema.";
            return false;
        }
        if (!IsValidStableId(manifest.projectId) ||
            !IsValidStableId(manifest.assetId) ||
            !IsValidStableId(manifest.sourceAssetId) ||
            manifest.assetId == manifest.sourceAssetId)
        {
            error = "RAsset model identity is invalid.";
            return false;
        }
        if (!IsSupportedSourceFormatToken(manifest.sourceFormat) ||
            manifest.importer.empty() || manifest.importerVersion == 0 ||
            manifest.settingsSchema != ReusableModelImportSettingsSchema ||
            manifest.settingsVersion != 1 ||
            !IsCanonicalJsonObject(manifest.settingsJson) ||
            manifest.payloadFormat != ReusableModelPayloadFormat ||
            document.payload.empty())
        {
            error = "RAsset model manifest is incomplete or unsupported.";
            return false;
        }
        try
        {
            const auto recipe = nlohmann::json::parse(manifest.settingsJson);
            if (!recipe.contains("source_format") ||
                recipe.at("source_format").get<std::string>() != manifest.sourceFormat ||
                !recipe.contains("options") || !recipe.at("options").is_object())
            {
                error = "RAsset model recipe contradicts its source-format manifest.";
                return false;
            }
        }
        catch (const nlohmann::json::exception&)
        {
            error = "RAsset model recipe is malformed.";
            return false;
        }
        if (manifest.payloadHash != HashBytes(document.payload))
        {
            error = "RAsset model payload hash does not match its payload bytes.";
            return false;
        }
        error.clear();
        return true;
    }

    bool SerializeReusableModelAssetDocument(
        const ReusableModelAssetDocument& document,
        std::vector<std::uint8_t>& bytes,
        std::string& error)
    {
        bytes.clear();
        if (!ValidateReusableModelAssetDocument(document, error))
            return false;
        const std::string manifest = SerializeManifest(document.manifest);
        if (manifest.size() > MaximumManifestBytes ||
            manifest.size() > (std::numeric_limits<std::uint32_t>::max)())
        {
            error = "RAsset model manifest is too large.";
            return false;
        }
        if (document.payload.size() > (std::numeric_limits<std::uint64_t>::max)())
        {
            error = "RAsset model payload is too large.";
            return false;
        }
        const std::uint64_t headerBytes = RAssetMagic.size() + 4u + 8u;
        const std::uint64_t total = headerBytes + manifest.size() + document.payload.size();
        if (total > (std::numeric_limits<std::size_t>::max)())
        {
            error = "RAsset model container is too large for this platform.";
            return false;
        }
        bytes.reserve(static_cast<std::size_t>(total));
        bytes.insert(bytes.end(), RAssetMagic.begin(), RAssetMagic.end());
        AppendU32(bytes, static_cast<std::uint32_t>(manifest.size()));
        AppendU64(bytes, static_cast<std::uint64_t>(document.payload.size()));
        bytes.insert(bytes.end(), manifest.begin(), manifest.end());
        bytes.insert(bytes.end(), document.payload.begin(), document.payload.end());
        error.clear();
        return true;
    }

    bool DeserializeReusableModelAssetDocument(
        const std::vector<std::uint8_t>& bytes,
        ReusableModelAssetDocument& document,
        std::string& error)
    {
        document = {};
        constexpr std::size_t HeaderSize = 8u + 4u + 8u;
        if (bytes.size() < HeaderSize ||
            !std::equal(RAssetMagic.begin(), RAssetMagic.end(), bytes.begin()))
        {
            error = "File is not a version-1 Renegade RAsset container.";
            return false;
        }
        const std::uint32_t manifestBytes = ReadU32(bytes, 8u);
        const std::uint64_t payloadBytes = ReadU64(bytes, 12u);
        if (manifestBytes == 0 || manifestBytes > MaximumManifestBytes ||
            payloadBytes == 0)
        {
            error = "RAsset container declares invalid manifest or payload lengths.";
            return false;
        }
        const std::uint64_t expected = static_cast<std::uint64_t>(HeaderSize) +
            manifestBytes + payloadBytes;
        if (expected != bytes.size())
        {
            error = "RAsset container length does not match its header.";
            return false;
        }
        const auto manifestStart = bytes.begin() + HeaderSize;
        const auto payloadStart = manifestStart + manifestBytes;
        const std::string manifestText(manifestStart, payloadStart);
        if (!ParseManifest(manifestText, document.manifest, error))
            return false;
        document.payload.assign(payloadStart, bytes.end());
        if (!ValidateReusableModelAssetDocument(document, error))
        {
            document = {};
            return false;
        }
        error.clear();
        return true;
    }

    bool ReadReusableModelAssetDocument(
        const std::string& path,
        ReusableModelAssetDocument& document,
        std::string& error)
    {
        std::vector<std::uint8_t> bytes;
        if (!ReadBytes(fs::u8path(path), bytes, error))
            return false;
        return DeserializeReusableModelAssetDocument(bytes, document, error);
    }

    ReusableModelImportResult ReusableAssetService::ImportModelAsset(
        const ReusableModelImportRequest& request,
        ReusableModelImportOptions options) const
    {
        ReusableModelImportResult result;
        result.sourceProjectRelativePath = request.sourceProjectRelativePath;
        result.assetProjectRelativePath = request.assetProjectRelativePath;

        if (!IsValidStableId(request.projectId))
        {
            result.error = "Reusable model import requires a valid project ID.";
            return result;
        }
        if (!IsSafeCanonicalProjectPath(request.sourceProjectRelativePath) ||
            !HasTopLevelFolder(request.sourceProjectRelativePath, "SourceAssets"))
        {
            result.error = "Reusable model source must be a canonical project path below SourceAssets.";
            return result;
        }
        if (!IsSafeCanonicalProjectPath(request.assetProjectRelativePath) ||
            !HasTopLevelFolder(request.assetProjectRelativePath, "Content") ||
            LowerExtension(request.assetProjectRelativePath) != ReusableAssetExtension)
        {
            result.error = "Reusable model product must be a canonical .rasset path below Content.";
            return result;
        }

        fs::path root;
        if (!ResolveProjectRoot(request.projectRoot, root, result.error))
            return result;

        std::error_code ec;
        const fs::path sourceRoot = fs::weakly_canonical(root / "SourceAssets", ec);
        if (ec || !fs::is_directory(sourceRoot, ec) || ec)
        {
            result.error = "Project SourceAssets folder is unavailable.";
            return result;
        }
        const fs::path sourcePath = fs::weakly_canonical(
            root / fs::u8path(request.sourceProjectRelativePath), ec);
        if (ec || !fs::is_regular_file(sourcePath, ec) || ec ||
            !IsWithin(sourcePath, sourceRoot))
        {
            result.error = "Reusable model source is missing or resolves outside SourceAssets.";
            return result;
        }

        const fs::path contentRoot = fs::weakly_canonical(root / "Content", ec);
        if (ec || !fs::is_directory(contentRoot, ec) || ec)
        {
            result.error = "Project Content folder is unavailable.";
            return result;
        }
        const fs::path assetPath = (root / fs::u8path(request.assetProjectRelativePath)).lexically_normal();
        const fs::path assetParent = fs::weakly_canonical(assetPath.parent_path(), ec);
        if (ec || !fs::is_directory(assetParent, ec) || ec ||
            !IsWithin(assetParent, contentRoot))
        {
            result.error = "Reusable model destination folder is missing or resolves outside Content.";
            return result;
        }
        if (fs::exists(assetPath, ec) || ec)
        {
            result.error = ec
                ? "Could not inspect reusable model destination: " + ec.message()
                : "Reusable model destination already exists; Gate 4 reimport/replacement is required.";
            return result;
        }

        const ModelSourceFormat format = ImportService::ClassifyModelSourceFormat(
            request.sourceProjectRelativePath);
        if (!ImportService::IsModelSourceFormatSupported(format))
        {
            result.error = "Reusable model source format is not enabled by LP07.";
            return result;
        }
        if (request.expectedFormat != ModelSourceFormat::Unknown &&
            request.expectedFormat != format)
        {
            result.error = "Reusable model source format does not match the requested format.";
            return result;
        }
        std::string recipeError;
        const std::string recipeJson = BuildRecipeJson(format, request.settingsJson, recipeError);
        if (recipeJson.empty())
        {
            result.error = std::move(recipeError);
            return result;
        }

        AssetRegistry registry;
        if (!ReadRegistryOrCreate(root, request.projectId, registry, result.error))
            return result;
        if (FindRecordByPath(registry, request.assetProjectRelativePath) != nullptr ||
            HasMissingPath(registry, request.assetProjectRelativePath))
        {
            result.error = "Reusable model destination already has LC01 identity; Gate 4 reimport is required.";
            return result;
        }
        if (HasMissingPath(registry, request.sourceProjectRelativePath))
        {
            result.error = "Reusable model source has an LC01 recovery tombstone; refresh/recover it before import.";
            return result;
        }

        std::string sourceHash;
        if (!HashFile(sourcePath, sourceHash, result.error))
            return result;

        AssetRecord* sourceRecord = FindRecordByPath(
            registry, request.sourceProjectRelativePath);
        if (sourceRecord != nullptr)
        {
            if (!sourceRecord->sourceAvailable || sourceRecord->contentHash != sourceHash)
            {
                result.error = "Reusable model source registry state is stale; refresh it before creating another product.";
                return result;
            }
            result.sourceAssetId = sourceRecord->assetId;
        }
        else
        {
            if (!GenerateUniqueId(registry, options.generateId,
                    result.sourceAssetId, result.error))
                return result;
            AssetRecord created;
            created.assetId = result.sourceAssetId;
            created.dependencyNodeId = "lp07.source:" + result.sourceAssetId;
            created.projectRelativePath = request.sourceProjectRelativePath;
            created.dependencyClass = DependencyClass::ImportedContent;
            created.requirement = DependencyRequirement::EditorOnly;
            created.provider = "lp07.source_asset";
            created.providerVersion = 1;
            created.contentHash = sourceHash;
            created.root = false;
            created.sourceAvailable = true;
            registry.records.push_back(std::move(created));
        }

        if (!GenerateUniqueId(registry, options.generateId, result.assetId, result.error))
            return result;
        if (result.assetId == result.sourceAssetId)
        {
            result.error = "Reusable asset ID generator returned the source ID for the product.";
            result.assetId.clear();
            return result;
        }

        const fs::path importDirectory = root / "Intermediate" / "Imports";
        fs::create_directories(importDirectory, ec);
        if (ec)
        {
            result.error = "Could not create reusable import working directory: " + ec.message();
            return result;
        }
        const fs::path temporaryWiscene =
            importDirectory / fs::u8path(result.assetId + ".wiscene");
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
        importRequest.expectedFormat = format;
        auto prepared = importer.PrepareModelAsset(importRequest);
        if (!prepared.IsReady())
        {
            result.import = prepared.Result();
            result.error = result.import.error.empty()
                ? "Reusable model conversion did not produce a prepared scene."
                : result.import.error;
            cleanupTemporary();
            return result;
        }
        const wi::scene::Scene* preparedScene = prepared.PeekScene();
        if (preparedScene == nullptr)
        {
            result.error = "Reusable model conversion lost its prepared scene before validation.";
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
        if (!BuildModelMetadata(result.import, *preparedScene,
                result.modelMetadata, result.error))
        {
            cleanupTemporary();
            return result;
        }

        ReusableModelAssetDocument assetDocument;
        assetDocument.manifest.projectId = request.projectId;
        assetDocument.manifest.assetId = result.assetId;
        assetDocument.manifest.sourceAssetId = result.sourceAssetId;
        assetDocument.manifest.sourceFormat = SourceFormatToken(format);
        assetDocument.manifest.importer = result.import.importerBackend;
        assetDocument.manifest.importerVersion = 1;
        assetDocument.manifest.settingsJson = recipeJson;
        if (!ReadBytes(temporaryWiscene, assetDocument.payload, result.error))
        {
            cleanupTemporary();
            return result;
        }
        cleanupTemporary();
        assetDocument.manifest.payloadHash = HashBytes(assetDocument.payload);

        std::vector<std::uint8_t> assetBytes;
        if (!SerializeReusableModelAssetDocument(assetDocument, assetBytes, result.error))
            return result;
        const std::string assetHash = HashBytes(assetBytes);

        AssetRecord product;
        product.assetId = result.assetId;
        product.dependencyNodeId = "lp07.rasset:" + result.assetId;
        product.projectRelativePath = request.assetProjectRelativePath;
        product.dependencyClass = DependencyClass::ImportedContent;
        product.requirement = DependencyRequirement::Required;
        product.provider = "lp07.rasset";
        product.providerVersion = 1;
        product.contentHash = assetHash;
        product.root = false;
        product.sourceAvailable = true;
        registry.records.push_back(std::move(product));

        auto importedProducts = registry.importedProducts;
        ImportedProductRecord provenance;
        provenance.sourceAssetId = result.sourceAssetId;
        provenance.productAssetId = result.assetId;
        provenance.importer = result.import.importerBackend;
        provenance.importerVersion = 1;
        provenance.settingsSchema = ReusableModelImportSettingsSchema;
        provenance.settingsVersion = 1;
        provenance.settingsJson = recipeJson;
        provenance.sourceContentHashAtImport = sourceHash;
        provenance.productContentHashAtImport = assetHash;
        importedProducts.push_back(std::move(provenance));
        if (!SetImportedProductRecords(registry, std::move(importedProducts), result.error))
            return result;

        AssetCatalogueMetadataDocument metadata;
        if (!ReadAssetCatalogueMetadata(root.generic_u8string(), request.projectId,
                metadata, result.error))
            return result;
        if (!SetAssetModelDerivedMetadata(
                metadata, result.assetId, result.modelMetadata, result.error))
            return result;

        std::string registryJson;
        std::string metadataJson;
        if (!SerializeAssetRegistry(registry, registryJson, result.error) ||
            !SerializeAssetCatalogueMetadata(metadata, metadataJson, result.error))
            return result;

        ProjectDocumentWrite assetWrite;
        assetWrite.destinationPath = assetPath.generic_u8string();
        assetWrite.content = assetBytes;
        const StableId expectedProjectId = request.projectId;
        const StableId expectedAssetId = result.assetId;
        const StableId expectedSourceId = result.sourceAssetId;
        assetWrite.validator = [expectedProjectId, expectedAssetId,
            expectedSourceId, assetBytes](const std::string& path, std::string& error)
        {
            std::vector<std::uint8_t> staged;
            if (!ReadBytes(fs::u8path(path), staged, error))
                return false;
            if (staged != assetBytes)
            {
                error = "Staged RAsset bytes do not match the requested product.";
                return false;
            }
            ReusableModelAssetDocument parsed;
            if (!DeserializeReusableModelAssetDocument(staged, parsed, error))
                return false;
            if (parsed.manifest.projectId != expectedProjectId ||
                parsed.manifest.assetId != expectedAssetId ||
                parsed.manifest.sourceAssetId != expectedSourceId)
            {
                error = "Staged RAsset identity does not match the requested import.";
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
            result.error = "Reusable model import transaction failed [" +
                result.transaction.code + "]: " + result.transaction.message;
            return result;
        }

        result.succeeded = true;
        result.error.clear();
        return result;
    }
}
