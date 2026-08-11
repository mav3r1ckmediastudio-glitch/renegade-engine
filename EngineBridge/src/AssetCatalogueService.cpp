#include "renegade/bridge/AssetCatalogueService.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <utility>

#include "json.hpp"

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        bool IsAsciiWhitespace(const unsigned char value) noexcept
        {
            return value == ' ' || value == '\t' || value == '\n' ||
                value == '\r' || value == '\f' || value == '\v';
        }

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

        bool NormaliseTag(
            const std::string& input,
            std::string& output,
            std::string& error)
        {
            std::size_t begin = 0;
            std::size_t end = input.size();
            while (begin < end &&
                IsAsciiWhitespace(static_cast<unsigned char>(input[begin])))
                ++begin;
            while (end > begin &&
                IsAsciiWhitespace(static_cast<unsigned char>(input[end - 1])))
                --end;
            if (begin == end)
            {
                error = "Asset metadata creator tags must not be empty.";
                return false;
            }
            if (end - begin > 64)
            {
                error = "Asset metadata creator tags must be at most 64 bytes.";
                return false;
            }

            output.assign(input.begin() + static_cast<std::ptrdiff_t>(begin),
                input.begin() + static_cast<std::ptrdiff_t>(end));
            for (char& character : output)
            {
                const auto value = static_cast<unsigned char>(character);
                if (value < 0x20 || value == 0x7f)
                {
                    error = "Asset metadata creator tags contain a control character.";
                    return false;
                }
                if (value >= 'A' && value <= 'Z')
                    character = static_cast<char>(value - 'A' + 'a');
            }
            error.clear();
            return true;
        }

        bool ModelMetadataIsEmpty(const ModelDerivedMetadata& model) noexcept
        {
            return !model.known && model.meshCount == 0 &&
                model.materialCount == 0 && model.armatureCount == 0 &&
                model.boneCount == 0 && model.animationClipCount == 0 &&
                model.animationChannelCount == 0 &&
                model.morphTargetCount == 0 && !model.skinned && !model.animated;
        }

        bool ValidateModelMetadata(
            const ModelDerivedMetadata& model,
            std::string& error)
        {
            if (!model.known)
            {
                if (!ModelMetadataIsEmpty(model))
                {
                    error = "Unknown model metadata must not contain derived values.";
                    return false;
                }
                error.clear();
                return true;
            }
            if (model.meshCount == 0)
            {
                error = "Known model metadata must contain at least one mesh.";
                return false;
            }
            if (model.armatureCount == 0 && model.boneCount != 0)
            {
                error = "Model metadata cannot contain bones without an armature.";
                return false;
            }
            if (model.skinned &&
                (model.armatureCount == 0 || model.boneCount == 0))
            {
                error = "Skinned model metadata requires armature and bone evidence.";
                return false;
            }
            if (model.animated &&
                (model.animationClipCount == 0 ||
                    model.animationChannelCount == 0))
            {
                error = "Animated model metadata requires clip and channel evidence.";
                return false;
            }
            error.clear();
            return true;
        }

        bool MetadataRecordLess(
            const AssetCatalogueMetadataRecord& left,
            const AssetCatalogueMetadataRecord& right)
        {
            return left.assetId < right.assetId;
        }

        AssetCatalogueMetadataRecord* FindMetadataRecord(
            AssetCatalogueMetadataDocument& document,
            const StableId& assetId)
        {
            const auto found = std::find_if(document.records.begin(),
                document.records.end(), [&assetId](const auto& record)
                { return record.assetId == assetId; });
            return found == document.records.end() ? nullptr : &*found;
        }

        const AssetCatalogueMetadataRecord* FindMetadataRecord(
            const AssetCatalogueMetadataDocument& document,
            const StableId& assetId)
        {
            const auto found = std::find_if(document.records.begin(),
                document.records.end(), [&assetId](const auto& record)
                { return record.assetId == assetId; });
            return found == document.records.end() ? nullptr : &*found;
        }

        void RemoveEmptyMetadataRecord(
            AssetCatalogueMetadataDocument& document,
            const StableId& assetId)
        {
            document.records.erase(std::remove_if(document.records.begin(),
                document.records.end(), [&assetId](const auto& record)
                {
                    return record.assetId == assetId &&
                        ModelMetadataIsEmpty(record.model) &&
                        record.creatorTags.empty();
                }), document.records.end());
        }

        bool ResolveProjectRoot(
            const std::string& projectRoot,
            fs::path& resolved,
            std::string& error)
        {
            error.clear();
            resolved.clear();
            if (projectRoot.empty())
            {
                error = "Asset metadata persistence requires a project root.";
                return false;
            }
            std::error_code pathError;
            const fs::path absolute = fs::absolute(fs::u8path(projectRoot), pathError);
            if (pathError || absolute.empty())
            {
                error = "Could not resolve asset metadata project root: " + projectRoot;
                return false;
            }
            resolved = fs::weakly_canonical(absolute, pathError);
            if (pathError || resolved.empty() ||
                !fs::is_directory(resolved, pathError) || pathError)
            {
                error = "Asset metadata project root is not a directory: " +
                    projectRoot;
                return false;
            }
            return true;
        }

        ProjectDocumentTransactionResult PersistenceFailure(
            std::string code,
            std::string message)
        {
            ProjectDocumentTransactionResult result;
            result.stage = ProjectDocumentTransactionStage::Prepare;
            result.code = std::move(code);
            result.message = std::move(message);
            return result;
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
                error = std::string("Asset metadata is missing '") + name + "'.";
                return false;
            }
            try
            {
                value = found->get<T>();
                return true;
            }
            catch (const nlohmann::json::exception&)
            {
                error = std::string("Asset metadata field '") + name +
                    "' has the wrong type.";
                return false;
            }
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

        bool IsContentPath(const std::string& projectRelativePath)
        {
            const fs::path path = fs::u8path(projectRelativePath);
            const auto first = path.begin();
            return first != path.end() && first->generic_u8string() == "Content" &&
                std::next(first) != path.end();
        }

        bool IsInternalMetadataPath(const std::string& projectRelativePath)
        {
            const std::string extension = LowerAscii(
                fs::u8path(projectRelativePath).extension().u8string());
            return extension == ".rmeta";
        }

        std::string SourceFormatForPath(const std::string& projectRelativePath)
        {
            std::string extension = LowerAscii(
                fs::u8path(projectRelativePath).extension().u8string());
            if (!extension.empty() && extension.front() == '.')
                extension.erase(extension.begin());
            if (extension == "fbx" || extension == "gltf" ||
                extension == "glb" || extension == "obj" ||
                extension == "ply" || extension == "vrm" ||
                extension == "vrma")
                return extension;
            return {};
        }

        std::string DisplayNameForPath(const std::string& projectRelativePath)
        {
            return fs::u8path(projectRelativePath).filename().u8string();
        }

        bool CatalogueEntryLess(
            const AssetCatalogueEntry& left,
            const AssetCatalogueEntry& right)
        {
            const std::string leftPath = LowerAscii(left.projectRelativePath);
            const std::string rightPath = LowerAscii(right.projectRelativePath);
            if (leftPath != rightPath)
                return leftPath < rightPath;
            if (left.projectRelativePath != right.projectRelativePath)
                return left.projectRelativePath < right.projectRelativePath;
            return left.assetId < right.assetId;
        }

        bool ContainsText(
            const AssetCatalogueEntry& entry,
            const std::string& text)
        {
            if (text.empty())
                return true;
            std::string searchable = LowerAscii(entry.name + "\n" +
                entry.projectRelativePath + "\n" + entry.assetId + "\n" +
                entry.sourceFormat + "\n" + entry.importer + "\n" +
                AssetCatalogueStateLabel(entry.state));
            for (const auto& tag : entry.creatorTags)
                searchable += "\n" + LowerAscii(tag);
            return searchable.find(text) != std::string::npos;
        }
    }

    bool ValidateAssetCatalogueMetadata(
        const AssetCatalogueMetadataDocument& document,
        std::string& error)
    {
        if (document.formatIdentifier != "renegade-asset-metadata" ||
            document.schemaVersion !=
                AssetCatalogueMetadataDocument::CurrentSchemaVersion)
        {
            error = "Unsupported asset metadata schema.";
            return false;
        }
        if (!IsValidStableId(document.projectId))
        {
            error = "Asset metadata project ID is invalid.";
            return false;
        }

        std::set<StableId> ids;
        for (const auto& record : document.records)
        {
            if (!IsValidStableId(record.assetId) ||
                !ids.insert(record.assetId).second)
            {
                error = "Asset metadata contains an invalid or duplicate asset ID.";
                return false;
            }
            if (!ValidateModelMetadata(record.model, error))
                return false;
            if (ModelMetadataIsEmpty(record.model) && record.creatorTags.empty())
            {
                error = "Asset metadata contains an empty record.";
                return false;
            }

            std::string previous;
            for (const auto& tag : record.creatorTags)
            {
                std::string canonical;
                if (!NormaliseTag(tag, canonical, error) || canonical != tag)
                {
                    if (error.empty())
                        error = "Asset metadata contains a non-canonical creator tag.";
                    return false;
                }
                if (!previous.empty() && previous >= tag)
                {
                    error = "Asset metadata creator tags must be sorted and unique.";
                    return false;
                }
                previous = tag;
            }
        }
        error.clear();
        return true;
    }

    bool SerializeAssetCatalogueMetadata(
        const AssetCatalogueMetadataDocument& document,
        std::string& json,
        std::string& error)
    {
        if (!ValidateAssetCatalogueMetadata(document, error))
            return false;

        auto records = document.records;
        std::sort(records.begin(), records.end(), MetadataRecordLess);
        nlohmann::json root;
        root["schema"] = document.formatIdentifier;
        root["version"] = document.schemaVersion;
        root["project_id"] = document.projectId;
        root["assets"] = nlohmann::json::array();
        for (const auto& record : records)
        {
            root["assets"].push_back({
                {"asset_id", record.assetId},
                {"creator_tags", record.creatorTags},
                {"model", {
                    {"known", record.model.known},
                    {"mesh_count", record.model.meshCount},
                    {"material_count", record.model.materialCount},
                    {"armature_count", record.model.armatureCount},
                    {"bone_count", record.model.boneCount},
                    {"animation_clip_count", record.model.animationClipCount},
                    {"animation_channel_count", record.model.animationChannelCount},
                    {"morph_target_count", record.model.morphTargetCount},
                    {"skinned", record.model.skinned},
                    {"animated", record.model.animated},
                }},
            });
        }
        json = root.dump(2) + "\n";
        error.clear();
        return true;
    }

    bool DeserializeAssetCatalogueMetadata(
        const std::string& json,
        AssetCatalogueMetadataDocument& document,
        std::string& error)
    {
        document = {};
        try
        {
            const nlohmann::json root = nlohmann::json::parse(json);
            if (!root.is_object() ||
                !RequiredField(root, "schema", document.formatIdentifier, error) ||
                !RequiredField(root, "version", document.schemaVersion, error) ||
                !RequiredField(root, "project_id", document.projectId, error))
                return false;

            const auto assets = root.find("assets");
            if (assets == root.end() || !assets->is_array())
            {
                error = "Asset metadata field 'assets' has the wrong type.";
                return false;
            }
            for (const auto& value : *assets)
            {
                if (!value.is_object())
                {
                    error = "Asset metadata asset entry is not an object.";
                    return false;
                }
                AssetCatalogueMetadataRecord record;
                nlohmann::json model;
                if (!RequiredField(value, "asset_id", record.assetId, error) ||
                    !RequiredField(value, "creator_tags", record.creatorTags, error) ||
                    !RequiredField(value, "model", model, error))
                    return false;
                if (!model.is_object() ||
                    !RequiredField(model, "known", record.model.known, error) ||
                    !RequiredField(model, "mesh_count", record.model.meshCount, error) ||
                    !RequiredField(model, "material_count", record.model.materialCount, error) ||
                    !RequiredField(model, "armature_count", record.model.armatureCount, error) ||
                    !RequiredField(model, "bone_count", record.model.boneCount, error) ||
                    !RequiredField(model, "animation_clip_count",
                        record.model.animationClipCount, error) ||
                    !RequiredField(model, "animation_channel_count",
                        record.model.animationChannelCount, error) ||
                    !RequiredField(model, "morph_target_count",
                        record.model.morphTargetCount, error) ||
                    !RequiredField(model, "skinned", record.model.skinned, error) ||
                    !RequiredField(model, "animated", record.model.animated, error))
                    return false;
                document.records.push_back(std::move(record));
            }
            std::sort(document.records.begin(), document.records.end(),
                MetadataRecordLess);
            return ValidateAssetCatalogueMetadata(document, error);
        }
        catch (const nlohmann::json::exception& exception)
        {
            error = std::string("Could not parse asset metadata JSON: ") +
                exception.what();
            return false;
        }
    }

    bool SetAssetCreatorTags(
        AssetCatalogueMetadataDocument& document,
        const StableId& assetId,
        std::vector<std::string> tags,
        std::string& error)
    {
        if (!IsValidStableId(assetId))
        {
            error = "Cannot assign creator tags to an invalid asset ID.";
            return false;
        }
        std::vector<std::string> canonicalTags;
        canonicalTags.reserve(tags.size());
        for (const auto& tag : tags)
        {
            std::string canonical;
            if (!NormaliseTag(tag, canonical, error))
                return false;
            canonicalTags.push_back(std::move(canonical));
        }
        std::sort(canonicalTags.begin(), canonicalTags.end());
        canonicalTags.erase(std::unique(canonicalTags.begin(), canonicalTags.end()),
            canonicalTags.end());

        AssetCatalogueMetadataDocument candidate = document;
        auto* record = FindMetadataRecord(candidate, assetId);
        if (record == nullptr)
        {
            AssetCatalogueMetadataRecord created;
            created.assetId = assetId;
            candidate.records.push_back(std::move(created));
            record = &candidate.records.back();
        }
        record->creatorTags = std::move(canonicalTags);
        RemoveEmptyMetadataRecord(candidate, assetId);
        if (!ValidateAssetCatalogueMetadata(candidate, error))
            return false;
        std::sort(candidate.records.begin(), candidate.records.end(),
            MetadataRecordLess);
        document = std::move(candidate);
        error.clear();
        return true;
    }

    bool SetAssetModelDerivedMetadata(
        AssetCatalogueMetadataDocument& document,
        const StableId& assetId,
        ModelDerivedMetadata metadata,
        std::string& error)
    {
        if (!IsValidStableId(assetId))
        {
            error = "Cannot assign model metadata to an invalid asset ID.";
            return false;
        }
        if (!ValidateModelMetadata(metadata, error))
            return false;

        AssetCatalogueMetadataDocument candidate = document;
        auto* record = FindMetadataRecord(candidate, assetId);
        if (record == nullptr)
        {
            AssetCatalogueMetadataRecord created;
            created.assetId = assetId;
            candidate.records.push_back(std::move(created));
            record = &candidate.records.back();
        }
        record->model = metadata;
        RemoveEmptyMetadataRecord(candidate, assetId);
        if (!ValidateAssetCatalogueMetadata(candidate, error))
            return false;
        std::sort(candidate.records.begin(), candidate.records.end(),
            MetadataRecordLess);
        document = std::move(candidate);
        error.clear();
        return true;
    }

    bool ResolveAssetCatalogueMetadataDocumentPath(
        const std::string& projectRoot,
        std::string& documentPath,
        std::string& error)
    {
        fs::path root;
        if (!ResolveProjectRoot(projectRoot, root, error))
        {
            documentPath.clear();
            return false;
        }
        documentPath = (root / AssetCatalogueMetadataDocumentName).generic_u8string();
        error.clear();
        return true;
    }

    ProjectDocumentTransactionResult WriteAssetCatalogueMetadata(
        const std::string& projectRoot,
        const AssetCatalogueMetadataDocument& document,
        AssetCatalogueMetadataPersistenceOptions options)
    {
        fs::path root;
        std::string error;
        if (!ResolveProjectRoot(projectRoot, root, error))
            return PersistenceFailure("asset_metadata_root", std::move(error));

        std::string json;
        if (!SerializeAssetCatalogueMetadata(document, json, error))
            return PersistenceFailure("asset_metadata_invalid", std::move(error));

        ProjectDocumentWrite write;
        write.destinationPath =
            (root / AssetCatalogueMetadataDocumentName).generic_u8string();
        write.content.assign(json.begin(), json.end());
        const StableId expectedProjectId = document.projectId;
        const std::string expectedJson = json;
        write.validator = [expectedProjectId, expectedJson](
            const std::string& stagedPath,
            std::string& validationError)
        {
            std::ifstream stream(fs::u8path(stagedPath), std::ios::binary);
            const std::string staged{
                std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>()};
            if (!stream && !stream.eof())
            {
                validationError = "Could not read the staged asset metadata.";
                return false;
            }
            AssetCatalogueMetadataDocument parsed;
            if (!DeserializeAssetCatalogueMetadata(staged, parsed, validationError))
                return false;
            if (parsed.projectId != expectedProjectId)
            {
                validationError =
                    "Staged asset metadata belongs to another project.";
                return false;
            }
            std::string canonical;
            if (!SerializeAssetCatalogueMetadata(parsed, canonical, validationError))
                return false;
            if (canonical != staged)
            {
                validationError = "Staged asset metadata is not canonical.";
                return false;
            }
            if (staged != expectedJson)
            {
                validationError =
                    "Staged asset metadata does not match the requested write.";
                return false;
            }
            validationError.clear();
            return true;
        };

        ProjectDocumentTransactionOptions transactionOptions;
        transactionOptions.transactionId = std::move(options.transactionId);
        transactionOptions.journalDirectory =
            (root / "Intermediate/Transactions").generic_u8string();
        transactionOptions.allowedRoot = root.generic_u8string();
        transactionOptions.operationHook = std::move(options.operationHook);
        ProjectDocumentTransaction transaction;
        return transaction.Execute({std::move(write)}, std::move(transactionOptions));
    }

    bool ReadAssetCatalogueMetadata(
        const std::string& projectRoot,
        const StableId& expectedProjectId,
        AssetCatalogueMetadataDocument& document,
        std::string& error)
    {
        document = {};
        if (!IsValidStableId(expectedProjectId))
        {
            error = "Expected asset metadata project ID is invalid.";
            return false;
        }
        std::string documentPath;
        if (!ResolveAssetCatalogueMetadataDocumentPath(
                projectRoot, documentPath, error))
            return false;

        std::error_code fileError;
        if (!fs::exists(fs::u8path(documentPath), fileError))
        {
            if (fileError)
            {
                error = "Could not inspect asset metadata document: " +
                    fileError.message();
                return false;
            }
            document.projectId = expectedProjectId;
            error.clear();
            return true;
        }

        std::ifstream stream(fs::u8path(documentPath), std::ios::binary);
        const std::string json{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
        if (!stream && !stream.eof())
        {
            error = "Could not read asset metadata document.";
            return false;
        }
        if (!DeserializeAssetCatalogueMetadata(json, document, error))
            return false;
        if (document.projectId != expectedProjectId)
        {
            error = "Asset metadata document belongs to another project.";
            document = {};
            return false;
        }
        std::string canonical;
        if (!SerializeAssetCatalogueMetadata(document, canonical, error))
            return false;
        if (canonical != json)
        {
            error = "Asset metadata document is not canonical.";
            document = {};
            return false;
        }
        error.clear();
        return true;
    }

    bool BuildAssetCatalogue(
        const std::string& projectRoot,
        const StableId& expectedProjectId,
        const AssetRegistry& registry,
        const AssetCatalogueMetadataDocument& metadata,
        AssetCatalogue& catalogue,
        std::string& error,
        AssetCatalogueBuildOptions options)
    {
        catalogue = {};
        if (!IsValidStableId(expectedProjectId))
        {
            error = "Asset catalogue requires a valid project ID.";
            return false;
        }
        if (!ValidateAssetRegistry(registry, error))
            return false;
        if (registry.projectId != expectedProjectId)
        {
            error = "Asset catalogue registry belongs to another project.";
            return false;
        }
        if (!ValidateAssetCatalogueMetadata(metadata, error))
            return false;
        if (metadata.projectId != expectedProjectId)
        {
            error = "Asset catalogue metadata belongs to another project.";
            return false;
        }

        std::map<StableId, const AssetRecord*> activeById;
        std::map<std::string, const AssetRecord*> activeByPath;
        std::map<StableId, const MissingAssetRecord*> missingById;
        std::set<StableId> knownIds;
        for (const auto& record : registry.records)
        {
            activeById.emplace(record.assetId, &record);
            activeByPath.emplace(record.projectRelativePath, &record);
            knownIds.insert(record.assetId);
        }
        for (const auto& missing : registry.missingAssets)
        {
            missingById.emplace(missing.assetId, &missing);
            knownIds.insert(missing.assetId);
        }
        for (const auto& record : metadata.records)
        {
            if (knownIds.find(record.assetId) == knownIds.end())
            {
                error = "Asset catalogue metadata references an unknown asset ID.";
                return false;
            }
        }

        std::set<StableId> movedIds;
        for (const auto& assetId : options.movedAssetIds)
        {
            if (activeById.find(assetId) == activeById.end() ||
                !movedIds.insert(assetId).second)
            {
                error = "Asset catalogue moved-state input is invalid or duplicated.";
                return false;
            }
        }

        std::map<StableId, std::vector<StableId>> referencedBy;
        for (const auto& record : registry.records)
        {
            for (const auto& dependencyId : record.dependencyAssetIds)
                referencedBy[dependencyId].push_back(record.assetId);
        }
        for (auto& pair : referencedBy)
        {
            auto& ids = pair.second;
            std::sort(ids.begin(), ids.end());
            ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        }

        std::map<StableId, const ImportedProductRecord*> importedByProduct;
        for (const auto& imported : registry.importedProducts)
            importedByProduct.emplace(imported.productAssetId, &imported);

        fs::path root;
        if (!ResolveProjectRoot(projectRoot, root, error))
            return false;
        std::error_code pathError;
        const fs::path canonicalContent =
            fs::weakly_canonical(root / "Content", pathError);
        if (pathError || !fs::is_directory(canonicalContent, pathError) || pathError)
        {
            error = "Asset catalogue project Content folder is unavailable.";
            return false;
        }

        AssetBrowserService browser;
        const AssetBrowserSnapshot rootSnapshot = browser.Scan(
            root.generic_u8string(), "Content");
        if (!rootSnapshot.succeeded)
        {
            error = rootSnapshot.error;
            return false;
        }

        std::map<std::string, AssetEntry> filesystemFiles;
        for (const auto& folder : rootSnapshot.folders)
        {
            const AssetBrowserSnapshot snapshot = browser.Scan(
                root.generic_u8string(), folder.projectRelativePath);
            if (!snapshot.succeeded ||
                snapshot.currentFolder != folder.projectRelativePath)
            {
                error = snapshot.error.empty()
                    ? "Asset catalogue folder resolved outside project Content."
                    : snapshot.error;
                return false;
            }
            for (const auto& asset : snapshot.assets)
            {
                if (asset.directory || IsInternalMetadataPath(asset.projectRelativePath))
                    continue;
                const fs::path canonicalFile = fs::weakly_canonical(
                    root / fs::u8path(asset.projectRelativePath), pathError);
                if (pathError || !IsWithin(canonicalFile, canonicalContent))
                {
                    error =
                        "Asset catalogue refused a filesystem entry outside project Content.";
                    return false;
                }
                if (!filesystemFiles.emplace(
                        asset.projectRelativePath, asset).second)
                {
                    error = "Asset catalogue discovered a duplicate project path.";
                    return false;
                }
            }
        }

        for (const auto& missing : registry.missingAssets)
        {
            if (IsContentPath(missing.lastKnownPath) &&
                !IsInternalMetadataPath(missing.lastKnownPath) &&
                filesystemFiles.find(missing.lastKnownPath) != filesystemFiles.end())
            {
                error = "Asset catalogue found a file at a missing-asset tombstone path; registry refresh is required.";
                return false;
            }
        }

        auto applyMetadata = [&metadata](AssetCatalogueEntry& entry)
        {
            if (const auto* record = FindMetadataRecord(metadata, entry.assetId))
            {
                entry.model = record->model;
                entry.creatorTags = record->creatorTags;
            }
        };

        auto pathForAssetId = [&activeById, &missingById](
            const StableId& assetId) -> std::string
        {
            const auto active = activeById.find(assetId);
            if (active != activeById.end())
                return active->second->projectRelativePath;
            const auto missing = missingById.find(assetId);
            if (missing != missingById.end())
                return missing->second->lastKnownPath;
            return {};
        };

        auto buildRegisteredEntry = [&](const AssetRecord& record,
            const bool foundOnDisk,
            AssetCatalogueEntry& entry) -> bool
        {
            entry.name = DisplayNameForPath(record.projectRelativePath);
            entry.projectRelativePath = record.projectRelativePath;
            entry.type = AssetBrowserService::Classify(record.projectRelativePath);
            entry.registered = true;
            entry.assetId = record.assetId;
            entry.dependencyClass = record.dependencyClass;
            entry.sourceFormat = SourceFormatForPath(record.projectRelativePath);
            entry.sourceAvailable = record.sourceAvailable;
            entry.productAvailable = record.sourceAvailable;
            entry.dependencyAssetIds = record.dependencyAssetIds;
            const auto reverse = referencedBy.find(record.assetId);
            if (reverse != referencedBy.end())
                entry.referencedByAssetIds = reverse->second;
            applyMetadata(entry);

            if (record.sourceAvailable != foundOnDisk)
                entry.state = AssetCatalogueState::Invalid;
            else if (!record.sourceAvailable)
                entry.state = AssetCatalogueState::Missing;
            else
                entry.state = AssetCatalogueState::Current;

            const auto imported = importedByProduct.find(record.assetId);
            if (imported != importedByProduct.end())
            {
                const ImportedProductRecord& provenance = *imported->second;
                entry.importedProduct = true;
                entry.sourceAssetId = provenance.sourceAssetId;
                entry.importer = provenance.importer;
                entry.importerVersion = provenance.importerVersion;
                entry.sourceFormat = SourceFormatForPath(
                    pathForAssetId(provenance.sourceAssetId));

                const auto source = activeById.find(provenance.sourceAssetId);
                entry.sourceAvailable = source != activeById.end() &&
                    source->second->sourceAvailable;
                entry.productAvailable = record.sourceAvailable;
                if (entry.state != AssetCatalogueState::Invalid)
                {
                    if (!entry.productAvailable || !entry.sourceAvailable)
                    {
                        entry.state = AssetCatalogueState::Missing;
                    }
                    else if (record.contentHash !=
                        provenance.productContentHashAtImport)
                    {
                        entry.state = AssetCatalogueState::Invalid;
                    }
                    else if (source->second->contentHash !=
                        provenance.sourceContentHashAtImport)
                    {
                        entry.state = AssetCatalogueState::Stale;
                    }
                }
            }

            if (entry.state == AssetCatalogueState::Current &&
                movedIds.find(record.assetId) != movedIds.end())
                entry.state = AssetCatalogueState::Moved;
            return true;
        };

        std::set<StableId> emittedActiveIds;
        for (const auto& pair : filesystemFiles)
        {
            const auto registered = activeByPath.find(pair.first);
            if (registered == activeByPath.end())
            {
                AssetCatalogueEntry entry;
                entry.name = pair.second.name;
                entry.projectRelativePath = pair.second.projectRelativePath;
                entry.type = pair.second.type;
                entry.state = AssetCatalogueState::Unregistered;
                entry.sourceFormat = SourceFormatForPath(entry.projectRelativePath);
                catalogue.entries.push_back(std::move(entry));
                continue;
            }

            AssetCatalogueEntry entry;
            if (!buildRegisteredEntry(*registered->second, true, entry))
                return false;
            emittedActiveIds.insert(entry.assetId);
            catalogue.entries.push_back(std::move(entry));
        }

        for (const auto& record : registry.records)
        {
            if (!IsContentPath(record.projectRelativePath) ||
                IsInternalMetadataPath(record.projectRelativePath) ||
                emittedActiveIds.find(record.assetId) != emittedActiveIds.end())
                continue;
            AssetCatalogueEntry entry;
            if (!buildRegisteredEntry(record, false, entry))
                return false;
            catalogue.entries.push_back(std::move(entry));
        }

        for (const auto& missing : registry.missingAssets)
        {
            if (!IsContentPath(missing.lastKnownPath) ||
                IsInternalMetadataPath(missing.lastKnownPath))
                continue;
            AssetCatalogueEntry entry;
            entry.name = DisplayNameForPath(missing.lastKnownPath);
            entry.projectRelativePath = missing.lastKnownPath;
            entry.type = AssetBrowserService::Classify(missing.lastKnownPath);
            entry.state = AssetCatalogueState::Missing;
            entry.registered = true;
            entry.assetId = missing.assetId;
            entry.dependencyClass = missing.dependencyClass;
            entry.sourceFormat = SourceFormatForPath(missing.lastKnownPath);
            entry.sourceAvailable = false;
            entry.productAvailable = false;
            const auto reverse = referencedBy.find(missing.assetId);
            if (reverse != referencedBy.end())
                entry.referencedByAssetIds = reverse->second;
            applyMetadata(entry);

            const auto imported = importedByProduct.find(missing.assetId);
            if (imported != importedByProduct.end())
            {
                entry.importedProduct = true;
                entry.sourceAssetId = imported->second->sourceAssetId;
                entry.importer = imported->second->importer;
                entry.importerVersion = imported->second->importerVersion;
                entry.sourceFormat = SourceFormatForPath(
                    pathForAssetId(imported->second->sourceAssetId));
            }
            catalogue.entries.push_back(std::move(entry));
        }

        std::sort(catalogue.entries.begin(), catalogue.entries.end(),
            CatalogueEntryLess);
        catalogue.projectId = expectedProjectId;
        error.clear();
        return true;
    }

    std::vector<AssetCatalogueEntry> QueryAssetCatalogue(
        const AssetCatalogue& catalogue,
        const AssetCatalogueQuery& query)
    {
        std::string text = LowerAscii(query.text);
        std::string sourceFormat = LowerAscii(query.sourceFormat);
        if (!sourceFormat.empty() && sourceFormat.front() == '.')
            sourceFormat.erase(sourceFormat.begin());

        std::vector<std::string> tags;
        tags.reserve(query.tags.size());
        for (const auto& tag : query.tags)
        {
            std::string canonical;
            std::string ignored;
            if (!NormaliseTag(tag, canonical, ignored))
                return {};
            tags.push_back(std::move(canonical));
        }
        std::sort(tags.begin(), tags.end());
        tags.erase(std::unique(tags.begin(), tags.end()), tags.end());

        std::vector<AssetCatalogueEntry> result;
        for (const auto& entry : catalogue.entries)
        {
            if (query.type.has_value() && entry.type != *query.type)
                continue;
            if (query.state.has_value() && entry.state != *query.state)
                continue;
            if (!sourceFormat.empty() &&
                LowerAscii(entry.sourceFormat) != sourceFormat)
                continue;
            if (query.skinned.has_value() &&
                (!entry.model.known || entry.model.skinned != *query.skinned))
                continue;
            if (query.animated.has_value() &&
                (!entry.model.known || entry.model.animated != *query.animated))
                continue;
            if (query.staticModelsOnly &&
                (!entry.model.known || entry.model.skinned || entry.model.animated))
                continue;
            if (!ContainsText(entry, text))
                continue;

            bool hasAllTags = true;
            for (const auto& tag : tags)
            {
                if (!std::binary_search(entry.creatorTags.begin(),
                        entry.creatorTags.end(), tag))
                {
                    hasAllTags = false;
                    break;
                }
            }
            if (!hasAllTags)
                continue;
            result.push_back(entry);
        }
        return result;
    }

    const char* AssetCatalogueStateLabel(
        const AssetCatalogueState state) noexcept
    {
        switch (state)
        {
        case AssetCatalogueState::Unregistered: return "unregistered";
        case AssetCatalogueState::Current: return "current";
        case AssetCatalogueState::Stale: return "stale";
        case AssetCatalogueState::Missing: return "missing";
        case AssetCatalogueState::Moved: return "moved";
        case AssetCatalogueState::Invalid: return "invalid";
        }
        return "invalid";
    }
}
