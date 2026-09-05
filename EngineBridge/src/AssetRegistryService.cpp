#include "renegade/bridge/AssetRegistryService.h"

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
        const char* RequirementName(const DependencyRequirement requirement)
        {
            switch (requirement)
            {
            case DependencyRequirement::Required: return "required";
            case DependencyRequirement::Optional: return "optional";
            case DependencyRequirement::EditorOnly: return "editor_only";
            }
            return "";
        }

        bool TryParseRequirement(
            const std::string& name,
            DependencyRequirement& requirement)
        {
            if (name == "required")
                requirement = DependencyRequirement::Required;
            else if (name == "optional")
                requirement = DependencyRequirement::Optional;
            else if (name == "editor_only")
                requirement = DependencyRequirement::EditorOnly;
            else
                return false;
            return true;
        }

        bool IsProjectAssetNode(const DependencyNode& node)
        {
            return !node.runtimeSupport &&
                node.dependencyClass != DependencyClass::RuntimeSupport;
        }

        bool IsSafeCanonicalProjectPath(const std::string& value)
        {
            if (value.empty() || value.find('\\') != std::string::npos)
                return false;
            const std::filesystem::path path = std::filesystem::u8path(value);
            if (path.is_absolute() || path.has_root_name() ||
                path.generic_u8string() != value ||
                path.lexically_normal().generic_u8string() != value)
                return false;
            return std::none_of(path.begin(), path.end(),
                [](const std::filesystem::path& part)
                {
                    return part == "." || part == "..";
                });
        }

        bool IsContentHash(const std::string& value)
        {
            constexpr const char* Prefix = "fnv1a64:";
            if (value == "missing")
                return true;
            if (value.size() != 24 || value.compare(0, 8, Prefix) != 0)
                return false;
            return std::all_of(value.begin() + 8, value.end(),
                [](const unsigned char character)
                {
                    return (character >= '0' && character <= '9') ||
                        (character >= 'a' && character <= 'f');
                });
        }

        bool IsCanonicalSettingsJson(const std::string& value)
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

        bool RecordLess(const AssetRecord& left, const AssetRecord& right)
        {
            if (left.projectRelativePath != right.projectRelativePath)
                return left.projectRelativePath < right.projectRelativePath;
            return left.assetId < right.assetId;
        }

        bool ImportedProductLess(
            const ImportedProductRecord& left,
            const ImportedProductRecord& right)
        {
            if (left.sourceAssetId != right.sourceAssetId)
                return left.sourceAssetId < right.sourceAssetId;
            return left.productAssetId < right.productAssetId;
        }

        bool MissingAssetLess(
            const MissingAssetRecord& left,
            const MissingAssetRecord& right)
        {
            if (left.lastKnownPath != right.lastKnownPath)
                return left.lastKnownPath < right.lastKnownPath;
            return left.assetId < right.assetId;
        }

        bool CanRecoverAsset(
            const AssetRecord& previous,
            const DependencyNode& discovered)
        {
            return previous.contentHash != "missing" &&
                previous.contentHash == discovered.contentHash &&
                previous.dependencyClass == discovered.dependencyClass &&
                previous.requirement == discovered.requirement &&
                previous.applicability == discovered.applicability &&
                previous.provider == discovered.provider &&
                previous.providerVersion == discovered.providerVersion;
        }

        bool CanRecoverAsset(
            const MissingAssetRecord& previous,
            const DependencyNode& discovered)
        {
            return previous.contentHash == discovered.contentHash &&
                previous.dependencyClass == discovered.dependencyClass &&
                previous.requirement == discovered.requirement &&
                previous.applicability == discovered.applicability &&
                previous.provider == discovered.provider &&
                previous.providerVersion == discovered.providerVersion;
        }

        bool SameTrackedState(const AssetRecord& left, const AssetRecord& right)
        {
            return left.dependencyNodeId == right.dependencyNodeId &&
                left.projectRelativePath == right.projectRelativePath &&
                left.dependencyClass == right.dependencyClass &&
                left.requirement == right.requirement &&
                left.applicability == right.applicability &&
                left.provider == right.provider &&
                left.providerVersion == right.providerVersion &&
                left.contentHash == right.contentHash &&
                left.root == right.root &&
                left.sourceAvailable == right.sourceAvailable &&
                left.dependencyAssetIds == right.dependencyAssetIds;
        }

        bool ResolveProjectRoot(
            const std::string& projectRoot,
            std::filesystem::path& resolved,
            std::string& error)
        {
            error.clear();
            resolved.clear();
            if (projectRoot.empty())
            {
                error = "Asset registry persistence requires a project root.";
                return false;
            }
            std::error_code pathError;
            const std::filesystem::path absolute = std::filesystem::absolute(
                std::filesystem::u8path(projectRoot), pathError);
            if (pathError || absolute.empty())
            {
                error = "Could not resolve asset registry project root: " +
                    projectRoot;
                return false;
            }
            resolved = std::filesystem::weakly_canonical(absolute, pathError);
            if (pathError || resolved.empty() ||
                !std::filesystem::is_directory(resolved, pathError) || pathError)
            {
                error = "Asset registry project root is not a directory: " +
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
                error = std::string("Asset registry is missing '") + name + "'.";
                return false;
            }
            try
            {
                value = found->get<T>();
                return true;
            }
            catch (const nlohmann::json::exception&)
            {
                error = std::string("Asset registry field '") + name +
                    "' has the wrong type.";
                return false;
            }
        }
    }

    bool ValidateAssetRegistry(
        const AssetRegistry& registry,
        std::string& error)
    {
        if (registry.formatIdentifier != "renegade-asset-registry" ||
            (registry.schemaVersion != AssetRegistry::LegacySchemaVersion &&
                registry.schemaVersion != AssetRegistry::ProvenanceSchemaVersion &&
                registry.schemaVersion != AssetRegistry::CurrentSchemaVersion))
        {
            error = "Unsupported asset registry schema.";
            return false;
        }
        if (registry.schemaVersion == AssetRegistry::LegacySchemaVersion &&
            (!registry.importedProducts.empty() || !registry.missingAssets.empty()))
        {
            error = "Legacy asset registry cannot contain import provenance.";
            return false;
        }
        if (!IsValidStableId(registry.projectId))
        {
            error = "Asset registry project ID is invalid.";
            return false;
        }

        std::set<StableId> ids;
        std::set<std::string> dependencyNodeIds;
        std::set<std::string> activePaths;
        DependencyPathRegistry paths;
        for (const auto& record : registry.records)
        {
            if (!IsValidStableId(record.assetId) ||
                !ids.insert(record.assetId).second)
            {
                error = "Asset registry contains an invalid or duplicate asset ID.";
                return false;
            }
            const auto pathRegistration = paths.Register(
                record.assetId, record.projectRelativePath);
            if (record.dependencyNodeId.empty() ||
                !dependencyNodeIds.insert(record.dependencyNodeId).second ||
                !IsSafeCanonicalProjectPath(record.projectRelativePath) ||
                !activePaths.insert(record.projectRelativePath).second ||
                !pathRegistration.inserted)
            {
                error = "Asset registry contains an invalid or duplicate node/path.";
                return false;
            }
            if (DependencyClassName(record.dependencyClass)[0] == '\0' ||
                RequirementName(record.requirement)[0] == '\0' ||
                record.applicability.empty() || record.provider.empty() ||
                record.providerVersion == 0 || !IsContentHash(record.contentHash))
            {
                error = "Asset registry contains incomplete source tracking.";
                return false;
            }
            if (record.sourceAvailable == (record.contentHash == "missing"))
            {
                error = "Asset registry source availability contradicts its hash.";
                return false;
            }
        }

        for (const auto& record : registry.records)
        {
            std::set<StableId> dependencies;
            for (const auto& dependencyId : record.dependencyAssetIds)
            {
                if (ids.find(dependencyId) == ids.end() ||
                    !dependencies.insert(dependencyId).second)
                {
                    error = "Asset registry contains an invalid dependency ID.";
                    return false;
                }
            }
        }

        if (registry.schemaVersion != AssetRegistry::CurrentSchemaVersion &&
            !registry.missingAssets.empty())
        {
            error = "Legacy asset registry cannot contain recovery tombstones.";
            return false;
        }
        std::set<StableId> missingIds;
        std::set<std::string> missingPaths;
        for (const auto& missing : registry.missingAssets)
        {
            if (!IsValidStableId(missing.assetId) ||
                ids.find(missing.assetId) != ids.end() ||
                !missingIds.insert(missing.assetId).second ||
                !IsSafeCanonicalProjectPath(missing.lastKnownPath) ||
                activePaths.find(missing.lastKnownPath) != activePaths.end() ||
                !missingPaths.insert(missing.lastKnownPath).second ||
                DependencyClassName(missing.dependencyClass)[0] == '\0' ||
                RequirementName(missing.requirement)[0] == '\0' ||
                missing.applicability.empty() ||
                missing.provider.empty() || missing.providerVersion == 0 ||
                !IsContentHash(missing.contentHash) || missing.contentHash == "missing")
            {
                error = "Asset registry contains invalid missing-asset recovery state.";
                return false;
            }
        }

        std::set<StableId> knownIds = ids;
        knownIds.insert(missingIds.begin(), missingIds.end());
        std::set<StableId> importedProductIds;
        for (const auto& imported : registry.importedProducts)
        {
            if (!IsValidStableId(imported.sourceAssetId) ||
                !IsValidStableId(imported.productAssetId) ||
                imported.sourceAssetId == imported.productAssetId ||
                knownIds.find(imported.sourceAssetId) == knownIds.end() ||
                knownIds.find(imported.productAssetId) == knownIds.end() ||
                !importedProductIds.insert(imported.productAssetId).second ||
                imported.importer.empty() || imported.importerVersion == 0 ||
                imported.settingsSchema.empty() ||
                imported.settingsVersion == 0 ||
                !IsCanonicalSettingsJson(imported.settingsJson) ||
                !IsContentHash(imported.sourceContentHashAtImport) ||
                !IsContentHash(imported.productContentHashAtImport) ||
                imported.sourceContentHashAtImport == "missing" ||
                imported.productContentHashAtImport == "missing")
            {
                error = "Asset registry contains invalid imported-product provenance.";
                return false;
            }
        }

        error.clear();
        return true;
    }

    bool RefreshAssetRegistry(
        const StableId& projectId,
        const DependencyGraph& graph,
        const AssetRegistry* existingRegistry,
        AssetRegistryRefresh& refresh,
        std::string& error,
        AssetIdGenerator generateId)
    {
        refresh = {};
        if (!IsValidStableId(projectId))
        {
            error = "Cannot refresh an asset registry with an invalid project ID.";
            return false;
        }
        if (!generateId)
        {
            error = "Cannot refresh an asset registry without an ID generator.";
            return false;
        }

        std::string graphJson;
        if (!SerializeDependencyGraph(graph, graphJson, error))
            return false;

        std::map<std::string, AssetRecord> existingByPath;
        std::vector<MissingAssetRecord> availableTombstones;
        if (existingRegistry != nullptr)
        {
            if (!ValidateAssetRegistry(*existingRegistry, error))
                return false;
            if (existingRegistry->projectId != projectId)
            {
                error = "Existing asset registry belongs to another project.";
                return false;
            }
            for (const auto& record : existingRegistry->records)
                existingByPath.emplace(record.projectRelativePath, record);
            // Provenance is keyed by durable asset IDs.  Retain it through a
            // normal dependency refresh; if a refreshed graph removes an
            // endpoint, final registry validation fails closed rather than
            // silently leaving a recipe pointing at an unrelated new asset.
            refresh.registry.importedProducts =
                existingRegistry->importedProducts;
            availableTombstones = existingRegistry->missingAssets;
            refresh.registry.schemaVersion = existingRegistry->schemaVersion;
        }

        const std::set<std::string> rootIds(
            graph.rootIds.begin(), graph.rootIds.end());
        std::map<std::string, StableId> assetIdByNodeId;
        std::set<StableId> assignedIds;
        for (const auto& item : existingByPath)
            assignedIds.insert(item.second.assetId);
        for (const auto& item : availableTombstones)
            assignedIds.insert(item.assetId);

        refresh.registry.projectId = projectId;
        std::vector<const DependencyNode*> nodes;
        nodes.reserve(graph.nodes.size());
        for (const auto& node : graph.nodes)
            nodes.push_back(&node);
        std::sort(nodes.begin(), nodes.end(),
            [](const DependencyNode* left, const DependencyNode* right)
            {
                return left->id < right->id;
            });
        std::set<std::string> currentPaths;
        for (const DependencyNode* node : nodes)
            if (IsProjectAssetNode(*node))
                currentPaths.insert(node->projectRelativePath);
        std::map<std::string, std::vector<StableId>> recoveryMatchesByNodeId;
        std::map<StableId, std::size_t> recoveryCandidateCounts;
        for (const DependencyNode* node : nodes)
        {
            if (!IsProjectAssetNode(*node) ||
                existingByPath.find(node->projectRelativePath) != existingByPath.end())
                continue;
            auto& matches = recoveryMatchesByNodeId[node->id];
            for (const auto& previous : existingByPath)
            {
                if (currentPaths.find(previous.first) == currentPaths.end() &&
                    CanRecoverAsset(previous.second, *node))
                    matches.push_back(previous.second.assetId);
            }
            for (const auto& previous : availableTombstones)
                if (CanRecoverAsset(previous, *node))
                    matches.push_back(previous.assetId);
            std::sort(matches.begin(), matches.end());
            matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
            for (const auto& match : matches)
                ++recoveryCandidateCounts[match];
        }
        std::set<StableId> consumedExistingIds;
        std::set<StableId> consumedTombstoneIds;
        for (const DependencyNode* nodePointer : nodes)
        {
            const DependencyNode& node = *nodePointer;
            if (!IsProjectAssetNode(node))
                continue;

            AssetRecord record;
            const auto existing = existingByPath.find(node.projectRelativePath);
            if (existing != existingByPath.end())
            {
                record.assetId = existing->second.assetId;
                consumedExistingIds.insert(record.assetId);
            }
            else
            {
                const auto matches = recoveryMatchesByNodeId.find(node.id);
                const std::vector<StableId> noMatches;
                const auto& recoveryMatches = matches == recoveryMatchesByNodeId.end()
                    ? noMatches : matches->second;
                if (recoveryMatches.size() == 1 &&
                    recoveryCandidateCounts.at(recoveryMatches.front()) == 1)
                {
                    record.assetId = recoveryMatches.front();
                    if (std::any_of(existingByPath.begin(), existingByPath.end(),
                            [&record](const auto& item)
                            { return item.second.assetId == record.assetId; }))
                        consumedExistingIds.insert(record.assetId);
                    else
                        consumedTombstoneIds.insert(record.assetId);
                    refresh.recoveredAssetIds.push_back(record.assetId);
                }
                else
                {
                    record.assetId = generateId();
                    if (!IsValidStableId(record.assetId) ||
                        !assignedIds.insert(record.assetId).second)
                    {
                        error = "Asset ID generator returned an invalid or duplicate ID.";
                        refresh = {};
                        return false;
                    }
                    refresh.addedAssetIds.push_back(record.assetId);
                    if (!recoveryMatches.empty())
                        refresh.ambiguousRecoveryAssetIds.push_back(record.assetId);
                }
            }

            record.dependencyNodeId = node.id;
            record.projectRelativePath = node.projectRelativePath;
            record.dependencyClass = node.dependencyClass;
            record.requirement = node.requirement;
            record.applicability = node.applicability;
            record.provider = node.provider;
            record.providerVersion = node.providerVersion;
            record.contentHash = node.contentHash;
            record.root = rootIds.find(node.id) != rootIds.end();
            record.sourceAvailable = node.contentHash != "missing";
            assetIdByNodeId.emplace(node.id, record.assetId);
            refresh.registry.records.push_back(std::move(record));
        }

        std::map<StableId, AssetRecord*> recordById;
        for (auto& record : refresh.registry.records)
            recordById.emplace(record.assetId, &record);
        for (const auto& edge : graph.edges)
        {
            const auto source = assetIdByNodeId.find(edge.sourceId);
            const auto target = assetIdByNodeId.find(edge.targetId);
            if (source == assetIdByNodeId.end() ||
                target == assetIdByNodeId.end())
                continue;
            recordById.at(source->second)->dependencyAssetIds.push_back(
                target->second);
        }
        for (auto& record : refresh.registry.records)
        {
            std::sort(record.dependencyAssetIds.begin(),
                record.dependencyAssetIds.end());
            record.dependencyAssetIds.erase(std::unique(
                record.dependencyAssetIds.begin(),
                record.dependencyAssetIds.end()),
                record.dependencyAssetIds.end());
        }
        std::sort(refresh.registry.records.begin(),
            refresh.registry.records.end(), RecordLess);

        for (const auto& record : refresh.registry.records)
        {
            const auto existing = existingByPath.find(record.projectRelativePath);
            if (existing != existingByPath.end() &&
                !SameTrackedState(existing->second, record))
                refresh.changedAssetIds.push_back(record.assetId);
            existingByPath.erase(record.projectRelativePath);
        }
        for (const auto& removed : existingByPath)
        {
            if (consumedExistingIds.find(removed.second.assetId) !=
                consumedExistingIds.end())
                continue;
            refresh.removedAssetIds.push_back(removed.second.assetId);
            if (removed.second.sourceAvailable &&
                removed.second.contentHash != "missing")
            {
                refresh.registry.missingAssets.push_back({
                    removed.second.assetId,
                    removed.second.projectRelativePath,
                    removed.second.dependencyClass,
                    removed.second.requirement,
                    removed.second.applicability,
                    removed.second.provider,
                    removed.second.providerVersion,
                    removed.second.contentHash,
                });
            }
        }
        for (const auto& tombstone : availableTombstones)
        {
            if (consumedTombstoneIds.find(tombstone.assetId) ==
                consumedTombstoneIds.end())
                refresh.registry.missingAssets.push_back(tombstone);
        }
        std::sort(refresh.registry.missingAssets.begin(),
            refresh.registry.missingAssets.end(), MissingAssetLess);
        if (!refresh.registry.missingAssets.empty())
            refresh.registry.schemaVersion = AssetRegistry::CurrentSchemaVersion;
        std::sort(refresh.addedAssetIds.begin(), refresh.addedAssetIds.end());
        std::sort(refresh.changedAssetIds.begin(), refresh.changedAssetIds.end());
        std::sort(refresh.removedAssetIds.begin(), refresh.removedAssetIds.end());
        std::sort(refresh.recoveredAssetIds.begin(), refresh.recoveredAssetIds.end());
        std::sort(refresh.ambiguousRecoveryAssetIds.begin(),
            refresh.ambiguousRecoveryAssetIds.end());

        if (!ValidateAssetRegistry(refresh.registry, error))
        {
            refresh = {};
            return false;
        }
        return true;
    }

    bool SerializeAssetRegistry(
        const AssetRegistry& registry,
        std::string& json,
        std::string& error)
    {
        if (!ValidateAssetRegistry(registry, error))
            return false;

        auto records = registry.records;
        std::sort(records.begin(), records.end(), RecordLess);
        nlohmann::json document;
        document["schema"] = registry.formatIdentifier;
        document["version"] = registry.schemaVersion;
        document["project_id"] = registry.projectId;
        document["assets"] = nlohmann::json::array();
        if (registry.schemaVersion == AssetRegistry::CurrentSchemaVersion)
        {
            document["imported_products"] = nlohmann::json::array();
            document["missing_assets"] = nlohmann::json::array();
        }
        for (auto& record : records)
        {
            std::sort(record.dependencyAssetIds.begin(),
                record.dependencyAssetIds.end());
            document["assets"].push_back({
                {"asset_id", record.assetId},
                {"dependency_node_id", record.dependencyNodeId},
                {"path", record.projectRelativePath},
                {"class", DependencyClassName(record.dependencyClass)},
                {"requirement", RequirementName(record.requirement)},
                {"applicability", record.applicability},
                {"provider", record.provider},
                {"provider_version", record.providerVersion},
                {"content_hash", record.contentHash},
                {"root", record.root},
                {"source_available", record.sourceAvailable},
                {"dependencies", record.dependencyAssetIds},
            });
        }
        auto importedProducts = registry.importedProducts;
        std::sort(importedProducts.begin(), importedProducts.end(),
            ImportedProductLess);
        for (const auto& imported : importedProducts)
        {
            document["imported_products"].push_back({
                {"source_asset_id", imported.sourceAssetId},
                {"product_asset_id", imported.productAssetId},
                {"importer", imported.importer},
                {"importer_version", imported.importerVersion},
                {"settings_schema", imported.settingsSchema},
                {"settings_version", imported.settingsVersion},
                {"settings", nlohmann::json::parse(imported.settingsJson)},
                {"source_content_hash_at_import",
                    imported.sourceContentHashAtImport},
                {"product_content_hash_at_import",
                    imported.productContentHashAtImport},
            });
        }
        for (const auto& missing : registry.missingAssets)
        {
            document["missing_assets"].push_back({
                {"asset_id", missing.assetId},
                {"last_known_path", missing.lastKnownPath},
                {"class", DependencyClassName(missing.dependencyClass)},
                {"requirement", RequirementName(missing.requirement)},
                {"applicability", missing.applicability},
                {"provider", missing.provider},
                {"provider_version", missing.providerVersion},
                {"content_hash", missing.contentHash},
            });
        }
        json = document.dump(2) + "\n";
        error.clear();
        return true;
    }

    bool DeserializeAssetRegistry(
        const std::string& json,
        AssetRegistry& registry,
        std::string& error)
    {
        registry = {};
        try
        {
            const nlohmann::json document = nlohmann::json::parse(json);
            if (!document.is_object() ||
                !RequiredField(document, "schema", registry.formatIdentifier, error) ||
                !RequiredField(document, "version", registry.schemaVersion, error) ||
                !RequiredField(document, "project_id", registry.projectId, error))
                return false;

            const auto assets = document.find("assets");
            if (assets == document.end() || !assets->is_array())
            {
                error = "Asset registry field 'assets' has the wrong type.";
                return false;
            }
            for (const auto& value : *assets)
            {
                if (!value.is_object())
                {
                    error = "Asset registry asset entry is not an object.";
                    return false;
                }
                AssetRecord record;
                std::string className;
                std::string requirementName;
                if (!RequiredField(value, "asset_id", record.assetId, error) ||
                    !RequiredField(value, "dependency_node_id",
                        record.dependencyNodeId, error) ||
                    !RequiredField(value, "path", record.projectRelativePath, error) ||
                    !RequiredField(value, "class", className, error) ||
                    !RequiredField(value, "requirement", requirementName, error) ||
                    !RequiredField(value, "applicability", record.applicability, error) ||
                    !RequiredField(value, "provider", record.provider, error) ||
                    !RequiredField(value, "provider_version",
                        record.providerVersion, error) ||
                    !RequiredField(value, "content_hash", record.contentHash, error) ||
                    !RequiredField(value, "root", record.root, error) ||
                    !RequiredField(value, "source_available",
                        record.sourceAvailable, error) ||
                    !RequiredField(value, "dependencies",
                        record.dependencyAssetIds, error))
                    return false;
                if (!TryParseDependencyClassName(
                        className, record.dependencyClass) ||
                    !TryParseRequirement(requirementName, record.requirement))
                {
                    error = "Asset registry contains an unknown class or requirement.";
                    return false;
                }
                registry.records.push_back(std::move(record));
            }
            const auto importedProducts = document.find("imported_products");
            if (registry.schemaVersion == AssetRegistry::CurrentSchemaVersion &&
                (importedProducts == document.end() ||
                    !importedProducts->is_array()))
            {
                error = "Asset registry field 'imported_products' has the wrong type.";
                return false;
            }
            if (registry.schemaVersion == AssetRegistry::LegacySchemaVersion &&
                importedProducts != document.end())
            {
                error = "Legacy asset registry must not contain import provenance.";
                return false;
            }
            if (importedProducts != document.end())
            {
                for (const auto& value : *importedProducts)
                {
                    if (!value.is_object())
                    {
                        error = "Asset registry imported-product entry is not an object.";
                        return false;
                    }
                    ImportedProductRecord imported;
                    nlohmann::json settings;
                    if (!RequiredField(value, "source_asset_id",
                            imported.sourceAssetId, error) ||
                        !RequiredField(value, "product_asset_id",
                            imported.productAssetId, error) ||
                        !RequiredField(value, "importer", imported.importer, error) ||
                        !RequiredField(value, "importer_version",
                            imported.importerVersion, error) ||
                        !RequiredField(value, "settings_schema",
                            imported.settingsSchema, error) ||
                        !RequiredField(value, "settings_version",
                            imported.settingsVersion, error) ||
                        !RequiredField(value, "settings", settings, error) ||
                        !RequiredField(value, "source_content_hash_at_import",
                            imported.sourceContentHashAtImport, error) ||
                        !RequiredField(value, "product_content_hash_at_import",
                            imported.productContentHashAtImport, error))
                        return false;
                    if (!settings.is_object())
                    {
                        error = "Asset registry import settings must be an object.";
                        return false;
                    }
                    imported.settingsJson = settings.dump();
                    registry.importedProducts.push_back(std::move(imported));
                }
            }
            const auto missingAssets = document.find("missing_assets");
            if (registry.schemaVersion == AssetRegistry::CurrentSchemaVersion &&
                (missingAssets == document.end() || !missingAssets->is_array()))
            {
                error = "Asset registry field 'missing_assets' has the wrong type.";
                return false;
            }
            if (registry.schemaVersion != AssetRegistry::CurrentSchemaVersion &&
                missingAssets != document.end())
            {
                error = "Legacy asset registry must not contain recovery tombstones.";
                return false;
            }
            if (missingAssets != document.end())
            for (const auto& value : *missingAssets)
            {
                MissingAssetRecord missing;
                std::string className;
                std::string requirementName;
                if (!value.is_object() ||
                    !RequiredField(value, "asset_id", missing.assetId, error) ||
                    !RequiredField(value, "last_known_path", missing.lastKnownPath, error) ||
                    !RequiredField(value, "class", className, error) ||
                    !RequiredField(value, "requirement", requirementName, error) ||
                    !RequiredField(value, "applicability", missing.applicability, error) ||
                    !RequiredField(value, "provider", missing.provider, error) ||
                    !RequiredField(value, "provider_version", missing.providerVersion, error) ||
                    !RequiredField(value, "content_hash", missing.contentHash, error) ||
                    !TryParseDependencyClassName(className, missing.dependencyClass) ||
                    !TryParseRequirement(requirementName, missing.requirement))
                    return false;
                registry.missingAssets.push_back(std::move(missing));
            }
            std::sort(registry.records.begin(), registry.records.end(), RecordLess);
            std::sort(registry.importedProducts.begin(),
                registry.importedProducts.end(), ImportedProductLess);
            std::sort(registry.missingAssets.begin(), registry.missingAssets.end(),
                MissingAssetLess);
            return ValidateAssetRegistry(registry, error);
        }
        catch (const nlohmann::json::exception& exception)
        {
            error = std::string("Could not parse asset registry JSON: ") +
                exception.what();
            return false;
        }
    }

    bool SetImportedProductRecords(
        AssetRegistry& registry,
        std::vector<ImportedProductRecord> records,
        std::string& error)
    {
        AssetRegistry candidate = registry;
        std::sort(records.begin(), records.end(), ImportedProductLess);
        candidate.importedProducts = std::move(records);
        if (!candidate.importedProducts.empty())
            candidate.schemaVersion = AssetRegistry::CurrentSchemaVersion;
        if (!ValidateAssetRegistry(candidate, error))
            return false;
        for (const auto& imported : candidate.importedProducts)
        {
            const auto source = std::find_if(candidate.records.begin(),
                candidate.records.end(), [&imported](const AssetRecord& record)
                { return record.assetId == imported.sourceAssetId; });
            const auto product = std::find_if(candidate.records.begin(),
                candidate.records.end(), [&imported](const AssetRecord& record)
                { return record.assetId == imported.productAssetId; });
            if (!source->sourceAvailable || !product->sourceAvailable ||
                source->contentHash != imported.sourceContentHashAtImport ||
                product->contentHash != imported.productContentHashAtImport)
            {
                error = "Imported-product provenance must snapshot the current source and product hashes.";
                return false;
            }
        }
        registry = std::move(candidate);
        error.clear();
        return true;
    }

    bool GetImportedProductStatus(
        const AssetRegistry& registry,
        const ImportedProductRecord& imported,
        ImportedProductStatus& status,
        std::string& error)
    {
        status = {};
        if (!ValidateAssetRegistry(registry, error))
            return false;
        const auto found = std::find_if(registry.importedProducts.begin(),
            registry.importedProducts.end(),
            [&imported](const ImportedProductRecord& candidate)
            {
                return candidate.sourceAssetId == imported.sourceAssetId &&
                    candidate.productAssetId == imported.productAssetId;
            });
        if (found == registry.importedProducts.end())
        {
            error = "Imported-product provenance record is not in this registry.";
            return false;
        }
        const auto source = std::find_if(registry.records.begin(),
            registry.records.end(), [&imported](const AssetRecord& candidate)
            { return candidate.assetId == imported.sourceAssetId; });
        const auto product = std::find_if(registry.records.begin(),
            registry.records.end(), [&imported](const AssetRecord& candidate)
            { return candidate.assetId == imported.productAssetId; });
        status.sourceAvailable = source != registry.records.end() &&
            source->sourceAvailable;
        status.productAvailable = product != registry.records.end() &&
            product->sourceAvailable;
        status.sourceChanged = !status.sourceAvailable ||
            source->contentHash != found->sourceContentHashAtImport;
        status.productChanged = !status.productAvailable ||
            product->contentHash != found->productContentHashAtImport;
        error.clear();
        return true;
    }

    bool ResolveAssetRegistryDocumentPath(
        const std::string& projectRoot,
        std::string& documentPath,
        std::string& error)
    {
        std::filesystem::path root;
        if (!ResolveProjectRoot(projectRoot, root, error))
        {
            documentPath.clear();
            return false;
        }
        documentPath = (root / AssetRegistryDocumentName).generic_u8string();
        error.clear();
        return true;
    }

    ProjectDocumentTransactionResult WriteAssetRegistry(
        const std::string& projectRoot,
        const AssetRegistry& registry,
        AssetRegistryPersistenceOptions options)
    {
        std::filesystem::path root;
        std::string error;
        if (!ResolveProjectRoot(projectRoot, root, error))
            return PersistenceFailure("asset_registry_root", std::move(error));

        std::string json;
        if (!SerializeAssetRegistry(registry, json, error))
            return PersistenceFailure("asset_registry_invalid", std::move(error));

        ProjectDocumentWrite write;
        write.destinationPath =
            (root / AssetRegistryDocumentName).generic_u8string();
        write.content.assign(json.begin(), json.end());
        const StableId expectedProjectId = registry.projectId;
        const std::string expectedJson = json;
        write.validator = [expectedProjectId, expectedJson](
            const std::string& stagedPath,
            std::string& validationError)
        {
            std::ifstream stream(
                std::filesystem::u8path(stagedPath), std::ios::binary);
            const std::string staged{
                std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>()};
            if (!stream && !stream.eof())
            {
                validationError =
                    "Could not read the staged asset registry.";
                return false;
            }
            AssetRegistry parsed;
            if (!DeserializeAssetRegistry(staged, parsed, validationError))
                return false;
            if (parsed.projectId != expectedProjectId)
            {
                validationError =
                    "Staged asset registry belongs to another project.";
                return false;
            }
            std::string canonical;
            if (!SerializeAssetRegistry(parsed, canonical, validationError))
                return false;
            if (canonical != staged)
            {
                validationError =
                    "Staged asset registry is not canonical.";
                return false;
            }
            if (staged != expectedJson)
            {
                validationError =
                    "Staged asset registry does not match the requested write.";
                return false;
            }
            validationError.clear();
            return true;
        };

        ProjectDocumentTransactionOptions transactionOptions;
        transactionOptions.transactionId = std::move(options.transactionId);
        transactionOptions.journalDirectory =
            (root / "Intermediate" / "Transactions").generic_u8string();
        transactionOptions.allowedRoot = root.generic_u8string();
        transactionOptions.operationHook = std::move(options.operationHook);
        ProjectDocumentTransaction transaction;
        return transaction.Execute(
            {std::move(write)}, std::move(transactionOptions));
    }

    bool ReadAssetRegistry(
        const std::string& projectRoot,
        const StableId& expectedProjectId,
        AssetRegistry& registry,
        std::string& error)
    {
        registry = {};
        if (!IsValidStableId(expectedProjectId))
        {
            error = "Expected asset registry project ID is invalid.";
            return false;
        }
        std::string documentPath;
        if (!ResolveAssetRegistryDocumentPath(
                projectRoot, documentPath, error))
            return false;

        const std::filesystem::path path =
            std::filesystem::u8path(documentPath);
        std::error_code fileError;
        if (!std::filesystem::is_regular_file(path, fileError) || fileError)
        {
            error = "Asset registry document is missing: " + documentPath;
            return false;
        }
        std::ifstream stream(path, std::ios::binary);
        const std::string json{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
        if (!stream && !stream.eof())
        {
            error = "Could not read asset registry document: " + documentPath;
            return false;
        }

        AssetRegistry parsed;
        if (!DeserializeAssetRegistry(json, parsed, error))
            return false;
        if (parsed.projectId != expectedProjectId)
        {
            error = "Asset registry document belongs to another project.";
            return false;
        }
        std::string canonical;
        if (!SerializeAssetRegistry(parsed, canonical, error))
            return false;
        if (canonical != json)
        {
            error = "Asset registry document is valid but not canonical.";
            return false;
        }
        registry = std::move(parsed);
        error.clear();
        return true;
    }
}
