#include "renegade/bridge/AssetRegistryService.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
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

        bool RecordLess(const AssetRecord& left, const AssetRecord& right)
        {
            if (left.projectRelativePath != right.projectRelativePath)
                return left.projectRelativePath < right.projectRelativePath;
            return left.assetId < right.assetId;
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
            registry.schemaVersion != AssetRegistry::CurrentSchemaVersion)
        {
            error = "Unsupported asset registry schema.";
            return false;
        }
        if (!IsValidStableId(registry.projectId))
        {
            error = "Asset registry project ID is invalid.";
            return false;
        }

        std::set<StableId> ids;
        std::set<std::string> dependencyNodeIds;
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
        }

        const std::set<std::string> rootIds(
            graph.rootIds.begin(), graph.rootIds.end());
        std::map<std::string, StableId> assetIdByNodeId;
        std::set<StableId> assignedIds;
        for (const auto& item : existingByPath)
            assignedIds.insert(item.second.assetId);

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
            refresh.removedAssetIds.push_back(removed.second.assetId);
        std::sort(refresh.addedAssetIds.begin(), refresh.addedAssetIds.end());
        std::sort(refresh.changedAssetIds.begin(), refresh.changedAssetIds.end());
        std::sort(refresh.removedAssetIds.begin(), refresh.removedAssetIds.end());

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
            std::sort(registry.records.begin(), registry.records.end(), RecordLess);
            return ValidateAssetRegistry(registry, error);
        }
        catch (const nlohmann::json::exception& exception)
        {
            error = std::string("Could not parse asset registry JSON: ") +
                exception.what();
            return false;
        }
    }
}
