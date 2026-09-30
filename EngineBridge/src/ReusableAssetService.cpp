#include "renegade/bridge/ReusableAssetService.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"

#include <chrono>
#include <WickedEngine.h>
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

        bool IsPngBytes(const std::vector<std::uint8_t>& bytes)
    {
        constexpr std::array<std::uint8_t, 8> signature = {
            0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
        };
        return bytes.size() >= signature.size() &&
            std::equal(signature.begin(), signature.end(), bytes.begin());
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
                CreatorModelImportRecipe creatorRecipe;
                if (!ParseCreatorModelImportOptions(optionsJson, creatorRecipe, error))
                    return {};
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
            const std::string& json)
        {
            ProjectDocumentWrite write;
            write.destinationPath = (root / AssetRegistryDocumentName).generic_u8string();
            write.content.assign(json.begin(), json.end());
            // SerializeAssetRegistry already validates every record, project ID,
            // provenance link and canonicalizes this exact JSON before this write.
            // ProjectDocumentTransaction separately verifies staged/committed bytes
            // and retains its journal, backup, rollback and recovery guarantees.
            // Do not deserialize and reserialize the same registry on both passes.
            write.validator = [json](const std::string& path, std::string& error)
            {
                std::ifstream stream(fs::u8path(path), std::ios::binary);
                const std::string staged{
                    std::istreambuf_iterator<char>(stream),
                    std::istreambuf_iterator<char>()};
                if ((!stream && !stream.eof()) || staged != json)
                {
                    error = "Staged asset registry differs from the validated canonical document.";
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

    std::string ResolveReusableModelManagedProjectionPath(
        const std::string& assetProjectRelativePath)
    {
        if (assetProjectRelativePath.empty())
            return {};
        return assetProjectRelativePath + ".json";
    }

    std::string ResolveReusableModelThumbnailPath(
        const std::string& assetProjectRelativePath)
    {
        if (assetProjectRelativePath.empty())
            return {};
        fs::path path = fs::u8path(assetProjectRelativePath);
        path.replace_extension(".thumbnail.png");
        return path.generic_u8string();
    }

    bool ValidateReusableModelManagedProjection(
        const ReusableModelManagedProjection& projection,
        std::string& error)
    {
        if (projection.formatIdentifier != ReusableModelManagedProjectionFormat ||
            projection.schemaVersion != ReusableModelManagedProjection::CurrentSchemaVersion)
        {
            error = "Unsupported managed reusable-asset projection schema.";
            return false;
        }
        if (!IsValidStableId(projection.projectId) ||
            !IsValidStableId(projection.assetId) ||
            !IsValidStableId(projection.sourceAssetId) ||
            projection.assetId == projection.sourceAssetId)
        {
            error = "Managed reusable-asset projection identity is invalid.";
            return false;
        }
        if (!IsSafeCanonicalProjectPath(projection.sourceProjectRelativePath) ||
            !HasTopLevelFolder(projection.sourceProjectRelativePath, "SourceAssets") ||
            !IsSafeCanonicalProjectPath(projection.assetProjectRelativePath) ||
            !HasTopLevelFolder(projection.assetProjectRelativePath, "Content") ||
            LowerExtension(projection.assetProjectRelativePath) != ReusableAssetExtension)
        {
            error = "Managed reusable-asset projection paths are invalid.";
            return false;
        }
        if (!IsSupportedSourceFormatToken(projection.sourceFormat) ||
            projection.importer.empty() || projection.importerVersion == 0 ||
            projection.settingsSchema != ReusableModelImportSettingsSchema ||
            projection.settingsVersion != 1 ||
            !IsCanonicalJsonObject(projection.settingsJson) ||
            projection.payloadHash.empty() || !projection.modelMetadata.known)
        {
            error = "Managed reusable-asset projection recipe/metadata is incomplete.";
            return false;
        }
        try
        {
            const auto recipe = nlohmann::json::parse(projection.settingsJson);
            if (!recipe.contains("source_format") ||
                recipe.at("source_format").get<std::string>() != projection.sourceFormat ||
                !recipe.contains("options") || !recipe.at("options").is_object())
            {
                error = "Managed reusable-asset projection recipe contradicts its source format.";
                return false;
            }
        }
        catch (const nlohmann::json::exception&)
        {
            error = "Managed reusable-asset projection recipe is malformed.";
            return false;
        }
        if (!projection.thumbnailProjectRelativePath.empty() &&
            (!IsSafeCanonicalProjectPath(projection.thumbnailProjectRelativePath) ||
             !HasTopLevelFolder(projection.thumbnailProjectRelativePath, "Content") ||
             projection.thumbnailProjectRelativePath !=
                 ResolveReusableModelThumbnailPath(projection.assetProjectRelativePath)))
        {
            error = "Managed reusable-asset projection thumbnail association is invalid.";
            return false;
        }
        error.clear();
        return true;
    }

    bool SerializeReusableModelManagedProjection(
        const ReusableModelManagedProjection& projection,
        std::string& json,
        std::string& error)
    {
        json.clear();
        if (!ValidateReusableModelManagedProjection(projection, error))
            return false;
        try
        {
            nlohmann::json model;
            model["animated"] = projection.modelMetadata.animated;
            model["animation_channel_count"] = projection.modelMetadata.animationChannelCount;
            model["animation_clip_count"] = projection.modelMetadata.animationClipCount;
            model["armature_count"] = projection.modelMetadata.armatureCount;
            model["bone_count"] = projection.modelMetadata.boneCount;
            model["known"] = projection.modelMetadata.known;
            model["material_count"] = projection.modelMetadata.materialCount;
            model["mesh_count"] = projection.modelMetadata.meshCount;
            model["morph_target_count"] = projection.modelMetadata.morphTargetCount;
            model["skinned"] = projection.modelMetadata.skinned;

            nlohmann::json document;
            document["asset_id"] = projection.assetId;
            document["asset_path"] = projection.assetProjectRelativePath;
            document["derived_model"] = std::move(model);
            document["format"] = projection.formatIdentifier;
            document["importer"] = projection.importer;
            document["importer_version"] = projection.importerVersion;
            document["payload_hash"] = projection.payloadHash;
            document["project_id"] = projection.projectId;
            document["schema_version"] = projection.schemaVersion;
            document["settings"] = nlohmann::json::parse(projection.settingsJson);
            document["settings_schema"] = projection.settingsSchema;
            document["settings_version"] = projection.settingsVersion;
            document["source_asset_id"] = projection.sourceAssetId;
            document["source_format"] = projection.sourceFormat;
            document["source_path"] = projection.sourceProjectRelativePath;
            document["thumbnail_path"] = projection.thumbnailProjectRelativePath;
            json = document.dump();
        }
        catch (const nlohmann::json::exception&)
        {
            error = "Could not serialize managed reusable-asset projection.";
            json.clear();
            return false;
        }
        error.clear();
        return true;
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


}
