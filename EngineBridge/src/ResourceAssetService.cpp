#include "renegade/bridge/ResourceAssetService.h"

#include "renegade/bridge/AssetBrowserService.h"

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
#include <system_error>
#include <utility>

#include "json.hpp"

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    constexpr std::array<std::uint8_t, 8> RAssetMagic = {
        'R', 'A', 'S', 'S', 'E', 'T', '0', '1'};
    constexpr std::uint32_t MaximumManifestBytes = 1024u * 1024u;
    constexpr std::size_t RAssetHeaderBytes = 8u + 4u + 8u;

    std::string LowerAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](const unsigned char character)
            {
                if (character >= 'A' && character <= 'Z')
                    return static_cast<char>(character - 'A' + 'a');
                return static_cast<char>(character);
            });
        return value;
    }

    bool IsSafeProjectRelativePath(const fs::path& path)
    {
        if (path.empty() || path.is_absolute())
            return false;
        return std::none_of(path.begin(), path.end(), [](const fs::path& part)
        {
            return part == ".." || part == ".";
        });
    }

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
            error = "Resource asset import requires an active project root.";
            return false;
        }
        std::error_code pathError;
        const fs::path absolute = fs::absolute(fs::u8path(projectRoot), pathError);
        if (pathError || absolute.empty())
        {
            error = "Could not resolve the resource asset project root.";
            return false;
        }
        root = fs::weakly_canonical(absolute, pathError);
        if (pathError || root.empty() || !fs::is_directory(root, pathError) ||
            pathError)
        {
            error = "Resource asset project root is not a directory.";
            root.clear();
            return false;
        }
        error.clear();
        return true;
    }

    bool ReadFileBytes(
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
            error = "Could not read complete file: " + path.generic_u8string();
            bytes.clear();
            return false;
        }
        error.clear();
        return true;
    }

    std::string HashBytes(const std::vector<std::uint8_t>& bytes)
    {
        std::uint64_t hash = 1469598103934665603ull;
        for (const std::uint8_t byte : bytes)
        {
            hash ^= byte;
            hash *= 1099511628211ull;
        }
        std::ostringstream stream;
        stream << "fnv1a64:" << std::hex << std::setfill('0')
            << std::setw(16) << hash;
        return stream.str();
    }

    const char* ResourceClassToken(const ResourceClass resourceClass)
    {
        switch (resourceClass)
        {
        case ResourceClass::Texture: return "texture";
        case ResourceClass::Audio: return "audio";
        case ResourceClass::Script: return "script";
        case ResourceClass::Video: return "video";
        case ResourceClass::Font: return "font";
        default: return "unknown";
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

    const char* ResourceFormatToken(const ResourceSourceFormat format)
    {
        switch (format)
        {
        case ResourceSourceFormat::Jpg: return "jpg";
        case ResourceSourceFormat::Jpeg: return "jpeg";
        case ResourceSourceFormat::Png: return "png";
        case ResourceSourceFormat::Bmp: return "bmp";
        case ResourceSourceFormat::Dds: return "dds";
        case ResourceSourceFormat::Tga: return "tga";
        case ResourceSourceFormat::Hdr: return "hdr";
        case ResourceSourceFormat::Wav: return "wav";
        case ResourceSourceFormat::Ogg: return "ogg";
        case ResourceSourceFormat::Lua: return "lua";
        case ResourceSourceFormat::Mp4: return "mp4";
        case ResourceSourceFormat::H264: return "h264";
        case ResourceSourceFormat::Ttf: return "ttf";
        default: return "unknown";
        }
    }

    bool ParseResourceFormatToken(
        const std::string& token,
        ResourceSourceFormat& format)
    {
        for (const auto& capability : GetSupportedResourceFormats())
        {
            if (LowerAscii(capability.wickedExtension) == token)
            {
                format = capability.format;
                return true;
            }
        }
        format = ResourceSourceFormat::Unknown;
        return false;
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

    std::string CanonicalRecipe(
        const ResourceClass resourceClass,
        const ResourceSourceFormat format)
    {
        nlohmann::json recipe;
        recipe["options"] = nlohmann::json::object();
        recipe["resource_class"] = ResourceClassToken(resourceClass);
        recipe["source_format"] = ResourceFormatToken(format);
        return recipe.dump();
    }

    bool ValidateV1SettingsInput(const std::string& settingsJson)
    {
        try
        {
            const auto parsed = nlohmann::json::parse(settingsJson);
            return parsed.is_object() && parsed.empty() && parsed.dump() == settingsJson;
        }
        catch (const nlohmann::json::exception&)
        {
            return false;
        }
    }

    bool ValidateDerivedMetadata(
        const ResourceAssetDerivedMetadata& metadata,
        const ResourceClass resourceClass,
        std::string& error)
    {
        if (!metadata.known)
        {
            if (metadata.byteCount != 0 || metadata.dimensionsKnown ||
                metadata.width != 0 || metadata.height != 0 ||
                metadata.mipCount != 0)
            {
                error = "Unknown resource metadata must not contain derived values.";
                return false;
            }
            error.clear();
            return true;
        }
        if (metadata.byteCount == 0)
        {
            error = "Known resource metadata requires a non-zero byte count.";
            return false;
        }
        if (metadata.dimensionsKnown)
        {
            if (resourceClass != ResourceClass::Texture ||
                metadata.width == 0 || metadata.height == 0 ||
                metadata.mipCount == 0)
            {
                error = "Known resource dimensions require a valid texture size and mip count.";
                return false;
            }
        }
        else if (metadata.width != 0 || metadata.height != 0 || metadata.mipCount != 0)
        {
            error = "Resource dimensions must be zero when dimensions are unknown.";
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
            metadata.dimensionsKnown = metadata.width > 0 && metadata.height > 0;
        }
        else if (format == ResourceSourceFormat::Dds && bytes.size() >= 32 &&
            bytes[0] == 'D' && bytes[1] == 'D' && bytes[2] == 'S' && bytes[3] == ' ')
        {
            metadata.height = ReadLittleEndian32(bytes, 12);
            metadata.width = ReadLittleEndian32(bytes, 16);
            metadata.mipCount = std::max<std::uint32_t>(1, ReadLittleEndian32(bytes, 28));
            metadata.dimensionsKnown = metadata.width > 0 && metadata.height > 0;
        }
        if (!metadata.dimensionsKnown)
        {
            metadata.width = 0;
            metadata.height = 0;
            metadata.mipCount = 0;
        }
        return metadata;
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

    std::uint32_t ReadU32(
        const std::vector<std::uint8_t>& bytes,
        const std::size_t offset)
    {
        return static_cast<std::uint32_t>(bytes[offset]) |
            (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
            (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
            (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    }

    std::uint64_t ReadU64(
        const std::vector<std::uint8_t>& bytes,
        const std::size_t offset)
    {
        std::uint64_t value = 0;
        for (unsigned shift = 0; shift < 64; shift += 8)
            value |= static_cast<std::uint64_t>(bytes[offset + shift / 8]) << shift;
        return value;
    }

    template<typename T>
    bool RequiredField(
        const nlohmann::json& object,
        const char* name,
        T& value,
        std::string& error)
    {
        const auto found = object.find(name);
        if (found == object.end())
        {
            error = std::string("Resource asset metadata is missing '") + name + "'.";
            return false;
        }
        try
        {
            value = found->get<T>();
            return true;
        }
        catch (const nlohmann::json::exception&)
        {
            error = std::string("Resource asset metadata field '") + name + "' has the wrong type.";
            return false;
        }
    }

    nlohmann::json DerivedJson(const ResourceAssetDerivedMetadata& metadata)
    {
        return {
            {"byte_count", metadata.byteCount},
            {"dimensions_known", metadata.dimensionsKnown},
            {"height", metadata.height},
            {"mip_count", metadata.mipCount},
            {"width", metadata.width},
        };
    }

    bool ParseDerivedJson(
        const nlohmann::json& value,
        ResourceAssetDerivedMetadata& metadata,
        std::string& error)
    {
        if (!value.is_object() ||
            !RequiredField(value, "byte_count", metadata.byteCount, error) ||
            !RequiredField(value, "dimensions_known", metadata.dimensionsKnown, error) ||
            !RequiredField(value, "height", metadata.height, error) ||
            !RequiredField(value, "mip_count", metadata.mipCount, error) ||
            !RequiredField(value, "width", metadata.width, error))
        {
            return false;
        }
        metadata.known = true;
        return true;
    }

    std::string SerializeManifest(const ResourceAssetManifest& manifest)
    {
        nlohmann::json root;
        root["asset_id"] = manifest.assetId;
        root["derived"] = DerivedJson(manifest.derived);
        root["format"] = manifest.formatIdentifier;
        root["importer"] = manifest.importer;
        root["importer_version"] = manifest.importerVersion;
        root["payload_format"] = manifest.payloadFormat;
        root["payload_hash"] = manifest.payloadHash;
        root["project_id"] = manifest.projectId;
        root["resource_class"] = ResourceClassToken(manifest.resourceClass);
        root["schema_version"] = manifest.schemaVersion;
        root["settings"] = nlohmann::json::parse(manifest.settingsJson);
        root["settings_schema"] = manifest.settingsSchema;
        root["settings_version"] = manifest.settingsVersion;
        root["source_asset_id"] = manifest.sourceAssetId;
        root["source_format"] = ResourceFormatToken(manifest.sourceFormat);
        return root.dump();
    }

    bool ParseManifest(
        const std::string& text,
        ResourceAssetManifest& manifest,
        std::string& error)
    {
        manifest = {};
        try
        {
            const nlohmann::json root = nlohmann::json::parse(text);
            if (!root.is_object() || root.dump() != text)
            {
                error = "Resource .rasset manifest is not canonical JSON.";
                return false;
            }
            manifest.assetId = root.at("asset_id").get<std::string>();
            const auto derived = root.at("derived");
            manifest.formatIdentifier = root.at("format").get<std::string>();
            manifest.importer = root.at("importer").get<std::string>();
            manifest.importerVersion = root.at("importer_version").get<std::uint32_t>();
            manifest.payloadFormat = root.at("payload_format").get<std::string>();
            manifest.payloadHash = root.at("payload_hash").get<std::string>();
            manifest.projectId = root.at("project_id").get<std::string>();
            const std::string resourceClass = root.at("resource_class").get<std::string>();
            manifest.schemaVersion = root.at("schema_version").get<std::uint32_t>();
            manifest.settingsJson = root.at("settings").dump();
            manifest.settingsSchema = root.at("settings_schema").get<std::string>();
            manifest.settingsVersion = root.at("settings_version").get<std::uint32_t>();
            manifest.sourceAssetId = root.at("source_asset_id").get<std::string>();
            const std::string sourceFormat = root.at("source_format").get<std::string>();
            if (!ParseDerivedJson(derived, manifest.derived, error) ||
                !ParseResourceClassToken(resourceClass, manifest.resourceClass) ||
                !ParseResourceFormatToken(sourceFormat, manifest.sourceFormat))
            {
                if (error.empty())
                    error = "Resource .rasset contains an unsupported class or format.";
                return false;
            }
        }
        catch (const nlohmann::json::exception&)
        {
            error = "Resource .rasset manifest is malformed or missing required fields.";
            manifest = {};
            return false;
        }
        error.clear();
        return true;
    }

    bool MetadataRecordLess(
        const ResourceAssetMetadataRecord& left,
        const ResourceAssetMetadataRecord& right)
    {
        return left.assetId < right.assetId;
    }

    bool UpsertResourceMetadata(
        ResourceAssetMetadataDocument& document,
        ResourceAssetMetadataRecord record,
        std::string& error)
    {
        const auto found = std::find_if(document.records.begin(), document.records.end(),
            [&record](const ResourceAssetMetadataRecord& existing)
            { return existing.assetId == record.assetId; });
        if (found == document.records.end())
            document.records.push_back(std::move(record));
        else
            *found = std::move(record);
        std::sort(document.records.begin(), document.records.end(), MetadataRecordLess);
        return ValidateResourceAssetMetadata(document, error);
    }

    bool ContainsAssetId(const AssetRegistry& registry, const StableId& assetId)
    {
        return std::any_of(registry.records.begin(), registry.records.end(),
            [&assetId](const AssetRecord& record) { return record.assetId == assetId; }) ||
            std::any_of(registry.missingAssets.begin(), registry.missingAssets.end(),
                [&assetId](const MissingAssetRecord& record)
                { return record.assetId == assetId; });
    }

    bool GenerateUniqueAssetId(
        const AssetRegistry& registry,
        const AssetIdGenerator& generateId,
        StableId& assetId,
        std::string& error)
    {
        for (int attempt = 0; attempt < 16; ++attempt)
        {
            assetId = generateId();
            if (IsValidStableId(assetId) && !ContainsAssetId(registry, assetId))
            {
                error.clear();
                return true;
            }
        }
        assetId.clear();
        error = "Asset ID generator did not provide a unique valid stable ID.";
        return false;
    }

    const AssetRecord* FindRecordByPath(const AssetRegistry& registry, const std::string& path)
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

    std::vector<std::uint8_t> StringBytes(const std::string& value)
    {
        return std::vector<std::uint8_t>(value.begin(), value.end());
    }

    ResourceAssetImportResult ImportFailure(
        ResourceAssetImportResult result,
        std::string error)
    {
        result.succeeded = false;
        result.error = std::move(error);
        return result;
    }
}

namespace renegade::bridge
{
    bool ValidateResourceAssetDocument(
        const ResourceAssetDocument& document,
        std::string& error)
    {
        const auto& manifest = document.manifest;
        if (manifest.formatIdentifier != ResourceAssetFormat ||
            manifest.schemaVersion != ResourceAssetManifest::CurrentSchemaVersion)
        {
            error = "Unsupported resource .rasset schema.";
            return false;
        }
        if (!IsValidStableId(manifest.projectId) ||
            !IsValidStableId(manifest.assetId) ||
            !IsValidStableId(manifest.sourceAssetId) ||
            manifest.assetId == manifest.sourceAssetId)
        {
            error = "Resource .rasset contains invalid stable identity.";
            return false;
        }
        if (manifest.resourceClass == ResourceClass::Unknown ||
            manifest.sourceFormat == ResourceSourceFormat::Unknown ||
            ClassifyResourceSourceFormat(manifest.sourceFormat) != manifest.resourceClass)
        {
            error = "Resource .rasset source format does not match its resource class.";
            return false;
        }
        if (manifest.importer != "wicked.resourcemanager" ||
            manifest.importerVersion != 1 ||
            manifest.settingsSchema != ResourceAssetImportSettingsSchema ||
            manifest.settingsVersion != 1 ||
            manifest.payloadFormat != ResourceAssetPayloadFormat)
        {
            error = "Resource .rasset contains an unsupported version-1 import contract.";
            return false;
        }
        if (manifest.settingsJson != CanonicalRecipe(manifest.resourceClass, manifest.sourceFormat))
        {
            error = "Resource .rasset import recipe is not the canonical version-1 recipe.";
            return false;
        }
        if (document.payload.empty() || manifest.payloadHash != HashBytes(document.payload))
        {
            error = "Resource .rasset payload is empty or its hash does not match.";
            return false;
        }
        if (!ValidateDerivedMetadata(manifest.derived, manifest.resourceClass, error))
            return false;
        if (!manifest.derived.known || manifest.derived.byteCount != document.payload.size())
        {
            error = "Resource .rasset derived byte count does not match its payload.";
            return false;
        }
        error.clear();
        return true;
    }

    bool SerializeResourceAssetDocument(
        const ResourceAssetDocument& document,
        std::vector<std::uint8_t>& bytes,
        std::string& error)
    {
        bytes.clear();
        if (!ValidateResourceAssetDocument(document, error))
            return false;
        std::string manifest;
        try
        {
            manifest = SerializeManifest(document.manifest);
        }
        catch (const nlohmann::json::exception&)
        {
            error = "Resource .rasset manifest settings are not valid JSON.";
            return false;
        }
        if (manifest.empty() || manifest.size() > MaximumManifestBytes ||
            manifest.size() > (std::numeric_limits<std::uint32_t>::max)())
        {
            error = "Resource .rasset manifest is too large.";
            return false;
        }
        const std::uint64_t total = static_cast<std::uint64_t>(RAssetHeaderBytes) +
            manifest.size() + document.payload.size();
        if (total > (std::numeric_limits<std::size_t>::max)())
        {
            error = "Resource .rasset container is too large for this platform.";
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

    bool DeserializeResourceAssetDocument(
        const std::vector<std::uint8_t>& bytes,
        ResourceAssetDocument& document,
        std::string& error)
    {
        document = {};
        if (bytes.size() < RAssetHeaderBytes ||
            !std::equal(RAssetMagic.begin(), RAssetMagic.end(), bytes.begin()))
        {
            error = "File is not a version-1 Renegade RAsset container.";
            return false;
        }
        const std::uint32_t manifestBytes = ReadU32(bytes, 8u);
        const std::uint64_t payloadBytes = ReadU64(bytes, 12u);
        if (manifestBytes == 0 || manifestBytes > MaximumManifestBytes || payloadBytes == 0)
        {
            error = "Resource RAsset declares invalid manifest or payload lengths.";
            return false;
        }
        const std::uint64_t expected = static_cast<std::uint64_t>(RAssetHeaderBytes) +
            manifestBytes + payloadBytes;
        if (expected != bytes.size())
        {
            error = "Resource RAsset container length does not match its header.";
            return false;
        }
        const auto manifestStart = bytes.begin() +
            static_cast<std::ptrdiff_t>(RAssetHeaderBytes);
        const auto payloadStart = manifestStart + manifestBytes;
        const std::string manifestText(manifestStart, payloadStart);
        if (!ParseManifest(manifestText, document.manifest, error))
            return false;
        document.payload.assign(payloadStart, bytes.end());
        if (!ValidateResourceAssetDocument(document, error))
        {
            document = {};
            return false;
        }
        error.clear();
        return true;
    }

    bool ReadResourceAssetDocument(
        const std::string& path,
        ResourceAssetDocument& document,
        std::string& error)
    {
        std::vector<std::uint8_t> bytes;
        if (!ReadFileBytes(fs::u8path(path), bytes, error))
        {
            document = {};
            return false;
        }
        return DeserializeResourceAssetDocument(bytes, document, error);
    }

    bool ValidateResourceAssetMetadata(
        const ResourceAssetMetadataDocument& document,
        std::string& error)
    {
        if (document.formatIdentifier != "renegade-resource-asset-metadata" ||
            document.schemaVersion != ResourceAssetMetadataDocument::CurrentSchemaVersion ||
            !IsValidStableId(document.projectId))
        {
            error = "Unsupported or invalid resource asset metadata document.";
            return false;
        }
        std::set<StableId> ids;
        for (const auto& record : document.records)
        {
            if (!IsValidStableId(record.assetId) || !ids.insert(record.assetId).second ||
                record.resourceClass == ResourceClass::Unknown ||
                record.sourceFormat == ResourceSourceFormat::Unknown ||
                ClassifyResourceSourceFormat(record.sourceFormat) != record.resourceClass ||
                !record.derived.known)
            {
                error = "Resource asset metadata contains an invalid record.";
                return false;
            }
            if (!ValidateDerivedMetadata(record.derived, record.resourceClass, error))
                return false;
        }
        error.clear();
        return true;
    }

    bool SerializeResourceAssetMetadata(
        const ResourceAssetMetadataDocument& document,
        std::string& json,
        std::string& error)
    {
        if (!ValidateResourceAssetMetadata(document, error))
            return false;
        auto records = document.records;
        std::sort(records.begin(), records.end(), MetadataRecordLess);
        nlohmann::json root;
        root["assets"] = nlohmann::json::array();
        root["project_id"] = document.projectId;
        root["schema"] = document.formatIdentifier;
        root["version"] = document.schemaVersion;
        for (const auto& record : records)
        {
            root["assets"].push_back({
                {"asset_id", record.assetId},
                {"derived", DerivedJson(record.derived)},
                {"resource_class", ResourceClassToken(record.resourceClass)},
                {"source_format", ResourceFormatToken(record.sourceFormat)},
            });
        }
        json = root.dump(2) + "\n";
        error.clear();
        return true;
    }

    bool DeserializeResourceAssetMetadata(
        const std::string& json,
        ResourceAssetMetadataDocument& document,
        std::string& error)
    {
        document = {};
        try
        {
            const nlohmann::json root = nlohmann::json::parse(json);
            if (!root.is_object() ||
                !RequiredField(root, "project_id", document.projectId, error) ||
                !RequiredField(root, "schema", document.formatIdentifier, error) ||
                !RequiredField(root, "version", document.schemaVersion, error))
                return false;
            const auto assets = root.find("assets");
            if (assets == root.end() || !assets->is_array())
            {
                error = "Resource asset metadata field 'assets' has the wrong type.";
                return false;
            }
            for (const auto& value : *assets)
            {
                ResourceAssetMetadataRecord record;
                std::string resourceClass;
                std::string sourceFormat;
                nlohmann::json derived;
                if (!value.is_object() ||
                    !RequiredField(value, "asset_id", record.assetId, error) ||
                    !RequiredField(value, "derived", derived, error) ||
                    !RequiredField(value, "resource_class", resourceClass, error) ||
                    !RequiredField(value, "source_format", sourceFormat, error) ||
                    !ParseDerivedJson(derived, record.derived, error) ||
                    !ParseResourceClassToken(resourceClass, record.resourceClass) ||
                    !ParseResourceFormatToken(sourceFormat, record.sourceFormat))
                {
                    if (error.empty())
                        error = "Resource asset metadata contains an unsupported class or format.";
                    return false;
                }
                document.records.push_back(std::move(record));
            }
            std::sort(document.records.begin(), document.records.end(), MetadataRecordLess);
            if (!ValidateResourceAssetMetadata(document, error))
            {
                document = {};
                return false;
            }
            std::string canonical;
            if (!SerializeResourceAssetMetadata(document, canonical, error) || canonical != json)
            {
                document = {};
                if (error.empty()) error = "Resource asset metadata document is not canonical.";
                return false;
            }
        }
        catch (const nlohmann::json::exception& exception)
        {
            error = std::string("Could not parse resource asset metadata JSON: ") + exception.what();
            return false;
        }
        error.clear();
        return true;
    }

    bool ReadResourceAssetMetadata(
        const std::string& projectRoot,
        const StableId& expectedProjectId,
        ResourceAssetMetadataDocument& document,
        std::string& error)
    {
        document = {};
        if (!IsValidStableId(expectedProjectId))
        {
            error = "Expected resource metadata project ID is invalid.";
            return false;
        }
        fs::path root;
        if (!ResolveProjectRoot(projectRoot, root, error))
            return false;
        const fs::path path = root / ResourceAssetMetadataDocumentName;
        std::error_code fileError;
        if (!fs::exists(path, fileError))
        {
            if (fileError)
            {
                error = "Could not inspect resource asset metadata document.";
                return false;
            }
            document.projectId = expectedProjectId;
            error.clear();
            return true;
        }
        std::ifstream stream(path, std::ios::binary);
        const std::string json{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
        if (!stream && !stream.eof())
        {
            error = "Could not read resource asset metadata document.";
            return false;
        }
        if (!DeserializeResourceAssetMetadata(json, document, error))
            return false;
        if (document.projectId != expectedProjectId)
        {
            document = {};
            error = "Resource asset metadata belongs to another project.";
            return false;
        }
        error.clear();
        return true;
    }

    ResourceAssetImportResult ResourceAssetService::ImportResourceAsset(
        const ResourceAssetImportRequest& request,
        ResourceAssetImportOptions options) const
    {
        ResourceAssetImportResult result;
        result.sourceProjectRelativePath = request.sourceProjectRelativePath;
        result.assetProjectRelativePath = request.assetProjectRelativePath;

        if (!IsValidStableId(request.projectId))
            return ImportFailure(std::move(result), "Resource asset import requires a valid project ID.");
        if (!options.generateId)
            return ImportFailure(std::move(result), "Resource asset import requires an asset ID generator.");
        if (!ValidateV1SettingsInput(request.settingsJson))
            return ImportFailure(std::move(result),
                "LP08 resource import version 1 accepts only an empty canonical options object.");

        fs::path root;
        std::string error;
        if (!ResolveProjectRoot(request.projectRoot, root, error))
            return ImportFailure(std::move(result), std::move(error));

        const fs::path sourceRelative = fs::u8path(request.sourceProjectRelativePath).lexically_normal();
        const fs::path productRelative = fs::u8path(request.assetProjectRelativePath).lexically_normal();
        if (!IsSafeProjectRelativePath(sourceRelative) || !IsSafeProjectRelativePath(productRelative))
            return ImportFailure(std::move(result),
                "Resource source and product paths must be safe project-relative paths.");

        ResourceSourceInspectionRequest inspectionRequest;
        inspectionRequest.projectRoot = root.generic_u8string();
        inspectionRequest.sourceProjectRelativePath = sourceRelative.generic_u8string();
        inspectionRequest.expectedFormat = request.expectedFormat;
        const ResourceSourceInspectionResult inspection = InspectResourceSource(inspectionRequest);
        if (!inspection.succeeded)
            return ImportFailure(std::move(result), inspection.error);
        result.resourceClass = inspection.resourceClass;
        result.sourceFormat = inspection.format;

        const fs::path expectedSource = SourcePrefix(result.resourceClass);
        const fs::path expectedProduct = ProductPrefix(result.resourceClass);
        if (expectedSource.empty() || expectedProduct.empty() ||
            !PathStartsWith(sourceRelative, expectedSource) ||
            !PathStartsWith(productRelative, expectedProduct))
        {
            return ImportFailure(std::move(result),
                "Resource source/product paths do not match the canonical resource class folders.");
        }
        if (LowerAscii(productRelative.extension().u8string()) != ResourceAssetExtension ||
            AssetBrowserService::Classify(productRelative.generic_u8string()) !=
                ResourceClassAssetType(result.resourceClass))
        {
            return ImportFailure(std::move(result),
                "Resource product must be a class-correct .rasset below project Content.");
        }

        std::error_code pathError;
        const fs::path sourcePath = fs::weakly_canonical(root / sourceRelative, pathError);
        if (pathError || !fs::is_regular_file(sourcePath, pathError) || pathError ||
            !IsWithin(sourcePath, root))
        {
            return ImportFailure(std::move(result),
                "Resource source is unavailable or resolves outside the project.");
        }
        const fs::path productParent = fs::weakly_canonical(
            (root / productRelative).parent_path(), pathError);
        const fs::path contentRoot = fs::weakly_canonical(root / "Content", pathError);
        if (pathError || !fs::is_directory(productParent, pathError) || pathError ||
            !IsWithin(productParent, contentRoot))
        {
            return ImportFailure(std::move(result),
                "Resource product parent must already exist inside project Content.");
        }
        const fs::path productPath = productParent / productRelative.filename();
        if (fs::exists(productPath, pathError) || pathError)
            return ImportFailure(std::move(result),
                "Resource product destination already exists; first import will not overwrite it.");

        std::vector<std::uint8_t> payload;
        if (!ReadFileBytes(sourcePath, payload, error) || payload.empty())
            return ImportFailure(std::move(result), std::move(error));
        result.sourceHash = HashBytes(payload);
        result.derived = DeriveMetadata(result.resourceClass, result.sourceFormat, payload);
        if (!ValidateDerivedMetadata(result.derived, result.resourceClass, error))
            return ImportFailure(std::move(result), std::move(error));

        AssetRegistry registry;
        if (!ReadAssetRegistry(root.generic_u8string(), request.projectId, registry, error))
            return ImportFailure(std::move(result), std::move(error));
        if (HasMissingPath(registry, sourceRelative.generic_u8string()) ||
            HasMissingPath(registry, productRelative.generic_u8string()))
        {
            return ImportFailure(std::move(result),
                "Resource import path collides with LC01 recovery state; refresh/recover the project first.");
        }
        if (FindRecordByPath(registry, productRelative.generic_u8string()) != nullptr)
            return ImportFailure(std::move(result), "Resource product path already has LC01 identity.");

        AssetRegistry candidate = registry;
        const AssetRecord* existingSource = FindRecordByPath(registry, sourceRelative.generic_u8string());
        if (existingSource != nullptr)
        {
            if (!existingSource->sourceAvailable || existingSource->contentHash != result.sourceHash ||
                existingSource->dependencyClass != DependencyClass::ImportedContent ||
                existingSource->requirement != DependencyRequirement::EditorOnly ||
                existingSource->provider != "lp08.source_asset" || existingSource->providerVersion != 1)
            {
                return ImportFailure(std::move(result),
                    "Existing LC01 source identity does not match this retained resource source.");
            }
            result.sourceAssetId = existingSource->assetId;
        }
        else
        {
            if (!GenerateUniqueAssetId(candidate, options.generateId, result.sourceAssetId, error))
                return ImportFailure(std::move(result), std::move(error));
            AssetRecord sourceRecord;
            sourceRecord.assetId = result.sourceAssetId;
            sourceRecord.dependencyNodeId = "lp08.source:" + result.sourceAssetId;
            sourceRecord.projectRelativePath = sourceRelative.generic_u8string();
            sourceRecord.dependencyClass = DependencyClass::ImportedContent;
            sourceRecord.requirement = DependencyRequirement::EditorOnly;
            sourceRecord.provider = "lp08.source_asset";
            sourceRecord.providerVersion = 1;
            sourceRecord.contentHash = result.sourceHash;
            sourceRecord.sourceAvailable = true;
            candidate.records.push_back(std::move(sourceRecord));
        }

        if (!GenerateUniqueAssetId(candidate, options.generateId, result.assetId, error))
            return ImportFailure(std::move(result), std::move(error));

        ResourceAssetDocument product;
        product.manifest.projectId = request.projectId;
        product.manifest.assetId = result.assetId;
        product.manifest.sourceAssetId = result.sourceAssetId;
        product.manifest.resourceClass = result.resourceClass;
        product.manifest.sourceFormat = result.sourceFormat;
        product.manifest.settingsJson = CanonicalRecipe(result.resourceClass, result.sourceFormat);
        product.manifest.payloadHash = result.sourceHash;
        product.manifest.derived = result.derived;
        product.payload = payload;

        std::vector<std::uint8_t> productBytes;
        if (!SerializeResourceAssetDocument(product, productBytes, error))
            return ImportFailure(std::move(result), std::move(error));
        result.productHash = HashBytes(productBytes);

        AssetRecord productRecord;
        productRecord.assetId = result.assetId;
        productRecord.dependencyNodeId = "lp08.rasset:" + result.assetId;
        productRecord.projectRelativePath = productRelative.generic_u8string();
        productRecord.dependencyClass = ResourceDependencyClass(result.resourceClass);
        productRecord.requirement = DependencyRequirement::Required;
        productRecord.provider = "lp08.rasset";
        productRecord.providerVersion = 1;
        productRecord.contentHash = result.productHash;
        productRecord.sourceAvailable = true;
        candidate.records.push_back(std::move(productRecord));
        candidate.schemaVersion = AssetRegistry::CurrentSchemaVersion;

        ImportedProductRecord provenance;
        provenance.sourceAssetId = result.sourceAssetId;
        provenance.productAssetId = result.assetId;
        provenance.importer = "wicked.resourcemanager";
        provenance.importerVersion = 1;
        provenance.settingsSchema = ResourceAssetImportSettingsSchema;
        provenance.settingsVersion = 1;
        provenance.settingsJson = product.manifest.settingsJson;
        provenance.sourceContentHashAtImport = result.sourceHash;
        provenance.productContentHashAtImport = result.productHash;
        auto importedProducts = candidate.importedProducts;
        importedProducts.push_back(std::move(provenance));
        if (!SetImportedProductRecords(candidate, std::move(importedProducts), error) ||
            !ValidateAssetRegistry(candidate, error))
            return ImportFailure(std::move(result), std::move(error));

        ResourceAssetMetadataDocument metadata;
        if (!ReadResourceAssetMetadata(root.generic_u8string(), request.projectId, metadata, error))
            return ImportFailure(std::move(result), std::move(error));
        ResourceAssetMetadataRecord metadataRecord;
        metadataRecord.assetId = result.assetId;
        metadataRecord.resourceClass = result.resourceClass;
        metadataRecord.sourceFormat = result.sourceFormat;
        metadataRecord.derived = result.derived;
        if (!UpsertResourceMetadata(metadata, std::move(metadataRecord), error))
            return ImportFailure(std::move(result), std::move(error));

        std::string registryJson;
        std::string metadataJson;
        if (!SerializeAssetRegistry(candidate, registryJson, error) ||
            !SerializeResourceAssetMetadata(metadata, metadataJson, error))
            return ImportFailure(std::move(result), std::move(error));

        std::string registryPath;
        if (!ResolveAssetRegistryDocumentPath(root.generic_u8string(), registryPath, error))
            return ImportFailure(std::move(result), std::move(error));
        const fs::path metadataPath = root / ResourceAssetMetadataDocumentName;

        ProjectDocumentWrite productWrite;
        productWrite.destinationPath = productPath.generic_u8string();
        productWrite.content = productBytes;
        const auto expectedProductBytes = productBytes;
        const auto expectedPayload = payload;
        const StableId expectedProjectId = request.projectId;
        const StableId expectedAssetId = result.assetId;
        const StableId expectedSourceAssetId = result.sourceAssetId;
        productWrite.validator = [expectedProductBytes, expectedPayload, sourcePath,
            expectedProjectId, expectedAssetId, expectedSourceAssetId](
            const std::string& stagedPath, std::string& validationError)
        {
            std::vector<std::uint8_t> currentSource;
            if (!ReadFileBytes(sourcePath, currentSource, validationError) || currentSource != expectedPayload)
            {
                if (validationError.empty()) validationError = "Retained resource source changed during import.";
                return false;
            }
            std::vector<std::uint8_t> staged;
            if (!ReadFileBytes(fs::u8path(stagedPath), staged, validationError) || staged != expectedProductBytes)
            {
                if (validationError.empty()) validationError = "Staged resource .rasset does not match requested bytes.";
                return false;
            }
            ResourceAssetDocument parsed;
            if (!DeserializeResourceAssetDocument(staged, parsed, validationError))
                return false;
            if (parsed.manifest.projectId != expectedProjectId ||
                parsed.manifest.assetId != expectedAssetId ||
                parsed.manifest.sourceAssetId != expectedSourceAssetId || parsed.payload != expectedPayload)
            {
                validationError = "Staged resource .rasset identity or payload differs from accepted import.";
                return false;
            }
            validationError.clear();
            return true;
        };

        ProjectDocumentWrite registryWrite;
        registryWrite.destinationPath = registryPath;
        registryWrite.content = StringBytes(registryJson);
        const std::string expectedRegistryJson = registryJson;
        registryWrite.validator = [expectedRegistryJson, expectedProjectId](
            const std::string& stagedPath, std::string& validationError)
        {
            std::vector<std::uint8_t> bytes;
            if (!ReadFileBytes(fs::u8path(stagedPath), bytes, validationError)) return false;
            const std::string staged(bytes.begin(), bytes.end());
            AssetRegistry parsed;
            if (!DeserializeAssetRegistry(staged, parsed, validationError) || parsed.projectId != expectedProjectId)
            {
                if (validationError.empty()) validationError = "Staged asset registry belongs to another project.";
                return false;
            }
            std::string canonical;
            if (!SerializeAssetRegistry(parsed, canonical, validationError) ||
                canonical != staged || staged != expectedRegistryJson)
            {
                if (validationError.empty()) validationError = "Staged asset registry is not canonical requested state.";
                return false;
            }
            validationError.clear();
            return true;
        };

        ProjectDocumentWrite metadataWrite;
        metadataWrite.destinationPath = metadataPath.generic_u8string();
        metadataWrite.content = StringBytes(metadataJson);
        const std::string expectedMetadataJson = metadataJson;
        metadataWrite.validator = [expectedMetadataJson, expectedProjectId](
            const std::string& stagedPath, std::string& validationError)
        {
            std::vector<std::uint8_t> bytes;
            if (!ReadFileBytes(fs::u8path(stagedPath), bytes, validationError)) return false;
            const std::string staged(bytes.begin(), bytes.end());
            ResourceAssetMetadataDocument parsed;
            if (!DeserializeResourceAssetMetadata(staged, parsed, validationError) ||
                parsed.projectId != expectedProjectId)
            {
                if (validationError.empty()) validationError = "Staged resource metadata belongs to another project.";
                return false;
            }
            if (staged != expectedMetadataJson)
            {
                validationError = "Staged resource metadata does not match requested state.";
                return false;
            }
            validationError.clear();
            return true;
        };

        ProjectDocumentTransactionOptions transactionOptions;
        transactionOptions.transactionId = std::move(options.transactionId);
        transactionOptions.journalDirectory = (root / "Intermediate/Transactions").generic_u8string();
        transactionOptions.allowedRoot = root.generic_u8string();
        transactionOptions.operationHook = std::move(options.operationHook);
        ProjectDocumentTransaction transaction;
        result.transaction = transaction.Execute({
            std::move(productWrite), std::move(registryWrite), std::move(metadataWrite)},
            std::move(transactionOptions));
        if (!result.transaction.success || !result.transaction.committed)
        {
            result.error = result.transaction.message.empty()
                ? "Resource asset transaction did not commit."
                : result.transaction.message;
            return result;
        }

        ResourceAssetDocument reopenedProduct;
        AssetRegistry reopenedRegistry;
        ResourceAssetMetadataDocument reopenedMetadata;
        if (!ReadResourceAssetDocument(productPath.generic_u8string(), reopenedProduct, error) ||
            !ReadAssetRegistry(root.generic_u8string(), request.projectId, reopenedRegistry, error) ||
            !ReadResourceAssetMetadata(root.generic_u8string(), request.projectId, reopenedMetadata, error))
        {
            result.error = "Committed resource asset did not reopen: " + error;
            return result;
        }
        const auto metadataFound = std::find_if(reopenedMetadata.records.begin(), reopenedMetadata.records.end(),
            [&result](const ResourceAssetMetadataRecord& record)
            { return record.assetId == result.assetId; });
        const auto provenanceFound = std::find_if(reopenedRegistry.importedProducts.begin(),
            reopenedRegistry.importedProducts.end(), [&result](const ImportedProductRecord& record)
            { return record.productAssetId == result.assetId; });
        if (reopenedProduct.manifest.assetId != result.assetId ||
            reopenedProduct.manifest.sourceAssetId != result.sourceAssetId ||
            reopenedProduct.payload != payload || metadataFound == reopenedMetadata.records.end() ||
            metadataFound->derived != result.derived || provenanceFound == reopenedRegistry.importedProducts.end() ||
            provenanceFound->sourceAssetId != result.sourceAssetId ||
            provenanceFound->sourceContentHashAtImport != result.sourceHash ||
            provenanceFound->productContentHashAtImport != result.productHash)
        {
            result.error = "Committed resource asset reopen evidence is inconsistent.";
            return result;
        }

        result.succeeded = true;
        result.error.clear();
        return result;
    }
}
