#include "renegade/bridge/ScriptReferenceAuthoringService.h"

#include "renegade/bridge/AssetBrowserService.h"
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/ScriptAuthoringService.h"

#include <algorithm>
#include <filesystem>
#include <utility>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    std::string FilenameLabel(const std::string& path)
    {
        const std::string filename =
            fs::u8path(path).filename().generic_u8string();
        return filename.empty() ? path : filename;
    }

    bool ReferenceLess(
        const ScriptReferenceOption& left,
        const ScriptReferenceOption& right)
    {
        if (left.label != right.label)
            return left.label < right.label;
        if (left.pathHint != right.pathHint)
            return left.pathHint < right.pathHint;
        return left.referenceId < right.referenceId;
    }
}

namespace renegade::bridge
{
    bool IsS4DReferencePropertyType(const ScriptPropertyType type) noexcept
    {
        switch (type)
        {
        case ScriptPropertyType::EntityReference:
        case ScriptPropertyType::AssetReference:
        case ScriptPropertyType::Animation:
        case ScriptPropertyType::Audio:
            return true;
        default:
            return false;
        }
    }

    bool BuildSceneScriptReferenceOptions(
        const SceneService& scenes,
        const ScriptPropertyType type,
        std::vector<ScriptReferenceOption>& options,
        std::string& error)
    {
        options.clear();
        if (type != ScriptPropertyType::EntityReference &&
            type != ScriptPropertyType::Animation)
        {
            error = "Scene reference enumeration requires EntityReference or Animation metadata.";
            return false;
        }

        const auto& scene = scenes.GetScene();
        if (type == ScriptPropertyType::EntityReference)
        {
            for (const auto& item : scenes.ListEntities())
            {
                if (item.entity == wi::ecs::INVALID_ENTITY ||
                    !scenes.IsHierarchyVisible(item.entity))
                {
                    continue;
                }
                const StableId id = PersistentEntityId(scene, item.entity);
                if (!IsValidStableId(id))
                    continue;

                ScriptReferenceOption option;
                option.referenceId = id;
                option.label = item.name.empty() ? "Unnamed Entity" : item.name;
                options.push_back(std::move(option));
            }
        }
        else
        {
            for (std::size_t index = 0; index < scene.animations.GetCount(); ++index)
            {
                const auto entity = scene.animations.GetEntity(index);
                const StableId id = PersistentEntityId(scene, entity);
                if (!IsValidStableId(id))
                    continue;

                ScriptReferenceOption option;
                option.referenceId = id;
                const auto* name = scene.names.GetComponent(entity);
                option.label = name != nullptr && !name->name.empty()
                    ? name->name
                    : "Animation " + std::to_string(index + 1);
                options.push_back(std::move(option));
            }
        }

        std::sort(options.begin(), options.end(), ReferenceLess);
        error.clear();
        return true;
    }

    bool BuildRegistryScriptReferenceOptions(
        const AssetRegistry& registry,
        const ScriptPropertyType type,
        std::vector<ScriptReferenceOption>& options,
        std::string& error)
    {
        options.clear();
        if (type != ScriptPropertyType::AssetReference &&
            type != ScriptPropertyType::Audio)
        {
            error = "Registry reference enumeration requires AssetReference or Audio metadata.";
            return false;
        }

        for (const auto& record : registry.records)
        {
            if (!IsValidStableId(record.assetId) ||
                record.projectRelativePath.empty() ||
                !record.sourceAvailable)
            {
                continue;
            }
            const AssetType assetType =
                AssetBrowserService::Classify(record.projectRelativePath);
            if (type == ScriptPropertyType::Audio && assetType != AssetType::Audio)
                continue;

            ScriptReferenceOption option;
            option.referenceId = record.assetId;
            option.pathHint = record.projectRelativePath;
            option.label = FilenameLabel(record.projectRelativePath);
            options.push_back(std::move(option));
        }

        std::sort(options.begin(), options.end(), ReferenceLess);
        error.clear();
        return true;
    }

    bool EnumerateScriptReferenceOptions(
        const SceneService& scenes,
        const ProjectService& projects,
        const ScriptPropertyType type,
        std::vector<ScriptReferenceOption>& options,
        std::string& error)
    {
        if (!IsS4DReferencePropertyType(type))
        {
            options.clear();
            error = "Script property is not an S4D reference type.";
            return false;
        }
        if (type == ScriptPropertyType::EntityReference ||
            type == ScriptPropertyType::Animation)
        {
            return BuildSceneScriptReferenceOptions(scenes, type, options, error);
        }
        if (!projects.HasProject())
        {
            options.clear();
            error = "Asset reference enumeration requires an active Renegade project.";
            return false;
        }

        const auto& project = projects.CurrentProject();
        AssetRegistry registry;
        if (!ReadAssetRegistry(
                project.rootPath,
                project.projectId,
                registry,
                error))
        {
            options.clear();
            error = "Could not read the project asset registry for scripting references: " +
                error;
            return false;
        }
        return BuildRegistryScriptReferenceOptions(registry, type, options, error);
    }

    bool BuildScriptReferenceProperty(
        const ScriptMetadataPropertyDescriptor& metadata,
        const ScriptReferenceOption& option,
        ScriptPropertyValue& value,
        std::string& error)
    {
        if (!IsS4DReferencePropertyType(metadata.type))
        {
            error = "Script metadata is not a reference-backed property.";
            return false;
        }
        if (metadata.name.empty())
        {
            error = "Script reference metadata has no stable property name.";
            return false;
        }
        const bool clearing = option.referenceId.empty() && option.pathHint.empty();
        if (!clearing)
        {
            if (!option.resolved)
            {
                error = "An unresolved reference can be preserved but not assigned as a new picker choice.";
                return false;
            }
            if (!IsValidStableId(option.referenceId))
            {
                error = "Script reference picker returned an invalid stable ID.";
                return false;
            }
        }

        value = {};
        value.name = metadata.name;
        value.type = metadata.type;
        value.referenceId = option.referenceId;
        value.pathHint = option.pathHint;
        error.clear();
        return true;
    }

    bool CommitScriptReferenceAuthoringEdit(
        ScriptAuthoringService& scripts,
        CommandService& commands,
        const StableId& scriptInstanceId,
        const ScriptMetadataPropertyDescriptor& metadata,
        const ScriptReferenceOption& option,
        std::string& error)
    {
        if (!IsValidStableId(scriptInstanceId))
        {
            error = "Script reference edit requires a valid ScriptInstanceId.";
            return false;
        }
        ScriptPropertyValue value;
        if (!BuildScriptReferenceProperty(metadata, option, value, error))
            return false;
        if (!scripts.EnsureCurrent(error))
            return false;

        ScriptDocument* document = scripts.Document();
        if (document == nullptr)
        {
            error = "No scripting document is loaded for reference authoring.";
            return false;
        }
        if (FindScriptAttachment(*document, scriptInstanceId) == nullptr)
        {
            error = "Script reference owner is no longer attached.";
            return false;
        }

        auto command = MakeSetScriptPropertyCommand(
            *document,
            scriptInstanceId,
            std::move(value),
            error);
        if (!command)
            return false;
        if (!commands.Execute(std::move(command)))
        {
            error = "Script reference edit could not be committed to Undo/Redo history.";
            return false;
        }
        error.clear();
        return true;
    }
}
