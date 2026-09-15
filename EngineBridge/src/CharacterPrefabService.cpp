#include "renegade/bridge/CharacterPrefabService.h"

#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "renegade/bridge/IdentityService.h"

#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <set>
#include <sstream>
#include <utility>

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;
        using json = nlohmann::json;

        constexpr std::uint64_t FnvOffset = 1469598103934665603ull;
        constexpr std::uint64_t FnvPrime = 1099511628211ull;

        std::string HashText(const std::string& text)
        {
            std::uint64_t hash = FnvOffset;
            for (const unsigned char value : text)
            {
                hash ^= value;
                hash *= FnvPrime;
            }
            std::ostringstream stream;
            stream << "fnv1a64:" << std::hex << std::setfill('0')
                   << std::setw(16) << hash;
            return stream.str();
        }

        bool ReadTextFile(
            const fs::path& path,
            std::string& text,
            std::string& error)
        {
            text.clear();
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                error = "Could not read Character Prefab file: " +
                    path.generic_u8string();
                return false;
            }
            text.assign(
                std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>());
            if (!stream && !stream.eof())
            {
                text.clear();
                error = "Could not read the complete Character Prefab file.";
                return false;
            }
            error.clear();
            return true;
        }

        std::vector<std::uint8_t> Bytes(const std::string& text)
        {
            return std::vector<std::uint8_t>(text.begin(), text.end());
        }

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

        std::string SanitizePrefabName(std::string value)
        {
            while (!value.empty() && std::isspace(
                    static_cast<unsigned char>(value.front())) != 0)
                value.erase(value.begin());
            while (!value.empty() && std::isspace(
                    static_cast<unsigned char>(value.back())) != 0)
                value.pop_back();

            for (char& character : value)
            {
                const unsigned char c = static_cast<unsigned char>(character);
                if (std::isalnum(c) != 0 || character == ' ' ||
                    character == '-' || character == '_' ||
                    character == '(' || character == ')')
                {
                    continue;
                }
                character = '_';
            }
            while (!value.empty() &&
                (value.back() == '.' || value.back() == ' '))
                value.pop_back();
            if (value.empty())
                value = "Character Prefab";
            if (value.size() > 96)
                value.resize(96);
            return value;
        }

        json UnknownFieldsToJson(
            const std::unordered_map<std::string, std::string>& fields)
        {
            json object = json::object();
            for (const auto& [key, value] : fields)
                object[key] = value;
            return object;
        }

        void UnknownFieldsFromJson(
            const json& object,
            std::unordered_map<std::string, std::string>& fields)
        {
            fields.clear();
            if (!object.is_object())
                return;
            for (auto it = object.begin(); it != object.end(); ++it)
            {
                if (it.value().is_string())
                    fields[it.key()] = it.value().get<std::string>();
            }
        }

        json SerializeProvenance(const ScriptProvenance& provenance)
        {
            json value;
            value["kind"] = static_cast<int>(provenance.kind);
            value["library_id"] = provenance.libraryId;
            value["library_version"] = provenance.libraryVersion;
            value["content_hash"] = provenance.contentHash;
            value["unknown"] = UnknownFieldsToJson(provenance.unknownFields);
            return value;
        }

        bool DeserializeProvenance(
            const json& value,
            ScriptProvenance& provenance,
            std::string& error)
        {
            if (!value.is_object() || !value.contains("kind") ||
                !value.at("kind").is_number_integer())
            {
                error = "Character Prefab script provenance is malformed.";
                return false;
            }
            const int kind = value.at("kind").get<int>();
            if (kind < static_cast<int>(ScriptProvenanceKind::Project) ||
                kind > static_cast<int>(ScriptProvenanceKind::PersonalLibrary))
            {
                error = "Character Prefab script provenance kind is unsupported.";
                return false;
            }
            provenance.kind = static_cast<ScriptProvenanceKind>(kind);
            provenance.libraryId = value.value("library_id", std::string{});
            provenance.libraryVersion = value.value("library_version", std::string{});
            provenance.contentHash = value.value("content_hash", std::string{});
            if (value.contains("unknown"))
                UnknownFieldsFromJson(value.at("unknown"), provenance.unknownFields);
            error.clear();
            return true;
        }

        json SerializeDependency(const ScriptDependency& dependency)
        {
            json value;
            value["kind"] = static_cast<int>(dependency.kind);
            value["id"] = dependency.id;
            value["path_hint"] = dependency.pathHint;
            value["optional"] = dependency.optional;
            value["unknown"] = UnknownFieldsToJson(dependency.unknownFields);
            return value;
        }

        bool DeserializeDependency(
            const json& value,
            ScriptDependency& dependency,
            std::string& error)
        {
            if (!value.is_object() || !value.contains("kind") ||
                !value.at("kind").is_number_integer())
            {
                error = "Character Prefab script dependency is malformed.";
                return false;
            }
            const int kind = value.at("kind").get<int>();
            if (kind < static_cast<int>(ScriptDependencyKind::ScriptModule) ||
                kind > static_cast<int>(ScriptDependencyKind::Asset))
            {
                error = "Character Prefab script dependency kind is unsupported.";
                return false;
            }
            dependency.kind = static_cast<ScriptDependencyKind>(kind);
            dependency.id = value.value("id", std::string{});
            dependency.pathHint = value.value("path_hint", std::string{});
            dependency.optional = value.value("optional", false);
            if (!dependency.id.empty() && !IsValidStableId(dependency.id))
            {
                error = "Character Prefab script dependency contains an invalid stable ID.";
                return false;
            }
            if (value.contains("unknown"))
                UnknownFieldsFromJson(value.at("unknown"), dependency.unknownFields);
            error.clear();
            return true;
        }

        json SerializeSource(const ScriptSourceBinding& source)
        {
            json value;
            value["source_id"] = source.sourceId;
            value["source_path"] = source.sourcePath;
            value["presentation"] = static_cast<int>(source.presentation);
            value["api_version"] = source.apiVersion;
            value["unsafe"] = source.unsafe;
            value["provenance"] = SerializeProvenance(source.provenance);
            value["dependencies"] = json::array();
            for (const auto& dependency : source.dependencies)
                value["dependencies"].push_back(SerializeDependency(dependency));
            value["capabilities"] = source.capabilities;
            return value;
        }

        bool DeserializeSource(
            const json& value,
            ScriptSourceBinding& source,
            std::string& error)
        {
            if (!value.is_object() ||
                !value.contains("source_id") ||
                !value.at("source_id").is_string() ||
                !value.contains("source_path") ||
                !value.at("source_path").is_string() ||
                !value.contains("presentation") ||
                !value.at("presentation").is_number_integer())
            {
                error = "Character Prefab script source is malformed.";
                return false;
            }
            source = {};
            source.sourceId = value.at("source_id").get<std::string>();
            source.sourcePath = value.at("source_path").get<std::string>();
            if (!IsValidStableId(source.sourceId) || source.sourcePath.empty())
            {
                error = "Character Prefab script source has invalid governed identity.";
                return false;
            }
            const int presentation = value.at("presentation").get<int>();
            if (presentation < static_cast<int>(ScriptPresentation::Action) ||
                presentation > static_cast<int>(ScriptPresentation::GlobalScript))
            {
                error = "Character Prefab script presentation is unsupported.";
                return false;
            }
            source.presentation = static_cast<ScriptPresentation>(presentation);
            source.apiVersion = value.value("api_version", 1u);
            source.unsafe = value.value("unsafe", false);
            if (value.contains("provenance") &&
                !DeserializeProvenance(value.at("provenance"), source.provenance, error))
                return false;
            if (value.contains("dependencies"))
            {
                if (!value.at("dependencies").is_array())
                {
                    error = "Character Prefab script dependency list is malformed.";
                    return false;
                }
                for (const auto& item : value.at("dependencies"))
                {
                    ScriptDependency dependency;
                    if (!DeserializeDependency(item, dependency, error))
                        return false;
                    source.dependencies.push_back(std::move(dependency));
                }
            }
            if (value.contains("capabilities"))
            {
                if (!value.at("capabilities").is_array())
                {
                    error = "Character Prefab script capabilities are malformed.";
                    return false;
                }
                for (const auto& capability : value.at("capabilities"))
                {
                    if (!capability.is_string())
                    {
                        error = "Character Prefab script capability must be text.";
                        return false;
                    }
                    source.capabilities.push_back(capability.get<std::string>());
                }
            }
            error.clear();
            return true;
        }

        json SerializeProperty(const CharacterPrefabScriptProperty& property)
        {
            const auto& source = property.value;
            json value;
            value["name"] = source.name;
            value["type"] = static_cast<int>(source.type);
            value["bool"] = source.booleanValue;
            value["integer"] = source.integerValue;
            value["number"] = source.numberValue;
            value["x"] = source.x;
            value["y"] = source.y;
            value["z"] = source.z;
            value["w"] = source.w;
            value["text"] = source.textValue;
            value["reference_id"] = source.referenceId;
            value["path_hint"] = source.pathHint;
            value["self_entity_reference"] = property.selfEntityReference;
            value["unknown"] = UnknownFieldsToJson(source.unknownFields);
            return value;
        }

        bool DeserializeProperty(
            const json& value,
            CharacterPrefabScriptProperty& property,
            std::string& error)
        {
            if (!value.is_object() || !value.contains("name") ||
                !value.at("name").is_string() || !value.contains("type") ||
                !value.at("type").is_number_integer())
            {
                error = "Character Prefab script property is malformed.";
                return false;
            }
            property = {};
            auto& target = property.value;
            target.name = value.at("name").get<std::string>();
            const int type = value.at("type").get<int>();
            if (type < static_cast<int>(ScriptPropertyType::Boolean) ||
                type > static_cast<int>(ScriptPropertyType::Enum))
            {
                error = "Character Prefab script property type is unsupported.";
                return false;
            }
            target.type = static_cast<ScriptPropertyType>(type);
            target.booleanValue = value.value("bool", false);
            target.integerValue = value.value("integer", std::int64_t{0});
            target.numberValue = value.value("number", 0.0f);
            target.x = value.value("x", 0.0f);
            target.y = value.value("y", 0.0f);
            target.z = value.value("z", 0.0f);
            target.w = value.value("w", 0.0f);
            target.textValue = value.value("text", std::string{});
            target.referenceId = value.value("reference_id", std::string{});
            target.pathHint = value.value("path_hint", std::string{});
            property.selfEntityReference = value.value("self_entity_reference", false);
            if (value.contains("unknown"))
                UnknownFieldsFromJson(value.at("unknown"), target.unknownFields);

            if (property.selfEntityReference)
            {
                if (target.type != ScriptPropertyType::EntityReference ||
                    !target.referenceId.empty())
                {
                    error = "Character Prefab self-reference property is inconsistent.";
                    return false;
                }
            }
            else if (target.type == ScriptPropertyType::EntityReference &&
                !target.referenceId.empty())
            {
                error =
                    "Character Prefab contains a non-portable scene entity reference.";
                return false;
            }
            else if (!target.referenceId.empty() &&
                !IsValidStableId(target.referenceId))
            {
                error = "Character Prefab property contains an invalid stable reference.";
                return false;
            }
            error.clear();
            return true;
        }

        json SerializeSettings(const CharacterAuthoringSettings& settings)
        {
            json value;
            value["type"] = static_cast<int>(settings.type);
            value["role"] = static_cast<int>(settings.role);
            value["personality"] = static_cast<int>(settings.personality);
            value["faction"] = settings.factionId;
            value["animation_set_id"] = settings.animationSetId;
            value["patrol_route_id"] = settings.patrolRouteEntityId;
            value["squad"] = settings.squadId;
            value["weapon_id"] = settings.weaponEntityId;
            value["combat_style"] = static_cast<int>(settings.combatStyle);
            value["skill"] = static_cast<int>(settings.skill);
            value["awareness"] = static_cast<int>(settings.awareness);
            value["autonomous"] = settings.autonomous;
            value["can_flee"] = settings.canFlee;
            value["can_surrender"] = settings.canSurrender;
            value["can_use_cover"] = settings.canUseCover;
            value["can_communicate"] = settings.canCommunicate;
            return value;
        }

        bool DeserializeSettings(
            const json& value,
            CharacterAuthoringSettings& settings,
            std::string& error)
        {
            if (!value.is_object())
            {
                error = "Character Prefab settings are malformed.";
                return false;
            }
            settings = {};
            settings.type = static_cast<CharacterType>(value.value("type", 0));
            settings.role = static_cast<CharacterRole>(value.value("role", 0));
            settings.personality = static_cast<PersonalityPreset>(
                value.value("personality", static_cast<int>(PersonalityPreset::Balanced)));
            settings.factionId = value.value("faction", std::string("Neutral"));
            settings.animationSetId = value.value("animation_set_id", std::string{});
            settings.patrolRouteEntityId = value.value("patrol_route_id", std::string{});
            settings.squadId = value.value("squad", std::string{});
            settings.weaponEntityId = value.value("weapon_id", std::string{});
            settings.combatStyle = static_cast<CombatStyle>(value.value("combat_style", 0));
            settings.skill = static_cast<SkillPreset>(
                value.value("skill", static_cast<int>(SkillPreset::Trained)));
            settings.awareness = static_cast<AwarenessPreset>(
                value.value("awareness", static_cast<int>(AwarenessPreset::Normal)));
            settings.autonomous = value.value("autonomous", true);
            settings.canFlee = value.value("can_flee", true);
            settings.canSurrender = value.value("can_surrender", false);
            settings.canUseCover = value.value("can_use_cover", true);
            settings.canCommunicate = value.value("can_communicate", true);
            return ValidateCharacterSettings(settings, error);
        }

        bool ValidateDocument(
            const CharacterPrefabDocument& document,
            std::string& error)
        {
            if (document.formatIdentifier != CharacterPrefabFormat ||
                document.schemaVersion != CharacterPrefabSchemaVersion)
            {
                error = "Character Prefab format/schema is unsupported.";
                return false;
            }
            if (!IsValidStableId(document.projectId) ||
                !IsValidStableId(document.assetId) ||
                !IsValidStableId(document.baseCharacterAssetId))
            {
                error = "Character Prefab requires valid project, prefab and base Character IDs.";
                return false;
            }
            if (!ValidateCharacterSettings(document.settings, error))
                return false;
            if (!document.settings.patrolRouteEntityId.empty() ||
                !document.settings.weaponEntityId.empty())
            {
                error =
                    "Character Prefab contains a scene-local patrol or weapon entity reference.";
                return false;
            }
            CharacterAdvancedOverrides overrides;
            if (!DeserializeCharacterAdvancedOverrides(
                    document.advancedOverrides, overrides, error))
                return false;
            for (const auto& script : document.scripts)
            {
                if (!IsValidStableId(script.source.sourceId) ||
                    script.source.sourcePath.empty())
                {
                    error = "Character Prefab contains an invalid script source binding.";
                    return false;
                }
                for (const auto& property : script.properties)
                {
                    if (property.selfEntityReference &&
                        property.value.type != ScriptPropertyType::EntityReference)
                    {
                        error = "Character Prefab contains an invalid script self reference.";
                        return false;
                    }
                    if (!property.selfEntityReference &&
                        property.value.type == ScriptPropertyType::EntityReference &&
                        !property.value.referenceId.empty())
                    {
                        error = "Character Prefab contains a non-portable scene entity reference.";
                        return false;
                    }
                }
            }
            error.clear();
            return true;
        }

        StableId CharacterBaseAssetId(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity character)
        {
            const auto* metadata = scene.metadatas.GetComponent(character);
            if (metadata == nullptr)
                return {};
            if (metadata->string_values.has(CharacterPrefabBaseAssetMetadataKey))
                return metadata->string_values.get(CharacterPrefabBaseAssetMetadataKey);
            if (metadata->string_values.has(ReusableAssetInstanceIdMetadataKey))
                return metadata->string_values.get(ReusableAssetInstanceIdMetadataKey);
            return {};
        }

        bool RegistryContainsId(
            const AssetRegistry& registry,
            const StableId& id) noexcept
        {
            return std::any_of(
                registry.records.begin(), registry.records.end(),
                [&id](const AssetRecord& record) { return record.assetId == id; }) ||
                std::any_of(
                    registry.missingAssets.begin(), registry.missingAssets.end(),
                    [&id](const MissingAssetRecord& record) { return record.assetId == id; });
        }

        const AssetRecord* FindRecord(
            const AssetRegistry& registry,
            const StableId& id) noexcept
        {
            const auto found = std::find_if(
                registry.records.begin(), registry.records.end(),
                [&id](const AssetRecord& record) { return record.assetId == id; });
            return found == registry.records.end() ? nullptr : &*found;
        }

        bool HasSourceEntityScripts(
            const ScriptDocument& document,
            const StableId& sourceId) noexcept
        {
            return std::any_of(
                document.attachments.begin(), document.attachments.end(),
                [&sourceId](const ScriptAttachment& attachment)
                {
                    return attachment.scope == ScriptScope::Entity &&
                        attachment.ownerEntityId == sourceId;
                });
        }

        bool StampPrefabOrigin(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const CharacterPrefabDocument& document)
        {
            auto* metadata = scene.metadatas.GetComponent(entity);
            if (metadata == nullptr)
                metadata = &scene.metadatas.Create(entity);
            metadata->string_values.set(
                CharacterPrefabOriginAssetMetadataKey, document.assetId);
            metadata->string_values.set(
                CharacterPrefabBaseAssetMetadataKey, document.baseCharacterAssetId);
            metadata->int_values.set(
                CharacterPrefabVersionMetadataKey,
                static_cast<int>(CharacterPrefabSchemaVersion));
            return true;
        }
    }

    bool SerializeCharacterPrefab(
        const CharacterPrefabDocument& document,
        std::string& output,
        std::string& error)
    {
        output.clear();
        if (!ValidateDocument(document, error))
            return false;

        json root;
        root["format"] = document.formatIdentifier;
        root["schema_version"] = document.schemaVersion;
        root["project_id"] = document.projectId;
        root["asset_id"] = document.assetId;
        root["base_character_asset_id"] = document.baseCharacterAssetId;
        root["reference_policy"] = "scene-local-cleared-v1";
        root["character"] = SerializeSettings(document.settings);
        root["advanced_overrides"] = document.advancedOverrides;
        root["scripts"] = json::array();
        for (const auto& script : document.scripts)
        {
            json item;
            item["source"] = SerializeSource(script.source);
            item["enabled"] = script.enabled;
            item["order"] = script.order;
            item["properties"] = json::array();
            for (const auto& property : script.properties)
                item["properties"].push_back(SerializeProperty(property));
            root["scripts"].push_back(std::move(item));
        }
        output = root.dump();
        error.clear();
        return true;
    }

    bool DeserializeCharacterPrefab(
        const std::string& input,
        CharacterPrefabDocument& document,
        std::string& error)
    {
        document = {};
        try
        {
            const json root = json::parse(input);
            if (!root.is_object() || root.dump() != input ||
                root.value("reference_policy", std::string{}) !=
                    "scene-local-cleared-v1")
            {
                error = "Character Prefab document is not canonical version-1 JSON.";
                return false;
            }
            document.formatIdentifier = root.at("format").get<std::string>();
            document.schemaVersion = root.at("schema_version").get<std::uint32_t>();
            document.projectId = root.at("project_id").get<std::string>();
            document.assetId = root.at("asset_id").get<std::string>();
            document.baseCharacterAssetId =
                root.at("base_character_asset_id").get<std::string>();
            if (!DeserializeSettings(root.at("character"), document.settings, error))
                return false;
            document.advancedOverrides =
                root.value("advanced_overrides", std::string{});
            if (root.contains("scripts"))
            {
                if (!root.at("scripts").is_array())
                {
                    error = "Character Prefab script list is malformed.";
                    return false;
                }
                for (const auto& item : root.at("scripts"))
                {
                    if (!item.is_object())
                    {
                        error = "Character Prefab script template is malformed.";
                        return false;
                    }
                    CharacterPrefabScriptTemplate script;
                    if (!DeserializeSource(item.at("source"), script.source, error))
                        return false;
                    script.enabled = item.value("enabled", true);
                    script.order = item.value("order", 0u);
                    if (item.contains("properties"))
                    {
                        if (!item.at("properties").is_array())
                        {
                            error = "Character Prefab script properties are malformed.";
                            return false;
                        }
                        for (const auto& propertyValue : item.at("properties"))
                        {
                            CharacterPrefabScriptProperty property;
                            if (!DeserializeProperty(propertyValue, property, error))
                                return false;
                            script.properties.push_back(std::move(property));
                        }
                    }
                    document.scripts.push_back(std::move(script));
                }
            }
        }
        catch (const json::exception&)
        {
            document = {};
            error = "Character Prefab document is malformed or incomplete.";
            return false;
        }
        return ValidateDocument(document, error);
    }

    bool IsCharacterPrefabProjectPath(
        const std::string& projectRelativePath) noexcept
    {
        if (projectRelativePath.empty() ||
            projectRelativePath.find('\\') != std::string::npos)
            return false;
        const fs::path path = fs::u8path(projectRelativePath).lexically_normal();
        if (path.is_absolute() || path.generic_u8string() != projectRelativePath ||
            path.extension().generic_u8string() != CharacterPrefabExtension)
            return false;
        auto part = path.begin();
        if (part == path.end() || part->generic_u8string() != "Content")
            return false;
        ++part;
        return part != path.end() && part->generic_u8string() == "Prefabs";
    }

    CharacterPrefabSaveResult SaveCharacterPrefab(
        const CharacterPrefabSaveRequest& request)
    {
        CharacterPrefabSaveResult result;
        if (request.scene == nullptr ||
            !IsValidStableId(request.projectId) || request.projectRoot.empty())
        {
            result.error = "Save Character Prefab requires an active project and Scene.";
            return result;
        }
        if (!IsRenegadeCharacter(*request.scene, request.character))
        {
            result.error = "Save Character Prefab requires a configured Renegade Character.";
            return result;
        }

        const StableId characterId =
            PersistentEntityId(*request.scene, request.character);
        if (!IsValidStableId(characterId))
        {
            result.error = "Character is missing a valid persistent Scene identity.";
            return result;
        }
        result.baseCharacterAssetId =
            CharacterBaseAssetId(*request.scene, request.character);
        if (!IsValidStableId(result.baseCharacterAssetId))
        {
            result.error =
                "Character has no prepared base Character Asset. Import it as a Character Asset before saving a reusable Character Prefab.";
            return result;
        }

        ReusableModelPlacementRequest baseRequest;
        baseRequest.projectRoot = request.projectRoot;
        baseRequest.projectId = request.projectId;
        baseRequest.assetId = result.baseCharacterAssetId;
        auto preparedBase =
            ReusableAssetService().PrepareModelAssetPlacement(baseRequest);
        if (!preparedBase.IsReady() || preparedBase.PeekScene() == nullptr ||
            !IsCharacterAssetTemplateScene(*preparedBase.PeekScene()))
        {
            result.error =
                "Character Prefab base asset is not a prepared Character Asset.";
            if (!preparedBase.Result().error.empty())
                result.error += " " + preparedBase.Result().error;
            return result;
        }

        CharacterPrefabDocument document;
        document.projectId = request.projectId;
        document.baseCharacterAssetId = result.baseCharacterAssetId;
        document.settings = CaptureCharacterSettings(
            *request.scene, request.character);
        if (!document.settings.patrolRouteEntityId.empty())
        {
            document.settings.patrolRouteEntityId.clear();
            result.warnings.push_back(
                "Patrol Route was scene-local and was cleared from the reusable Character Prefab.");
        }
        if (!document.settings.weaponEntityId.empty())
        {
            document.settings.weaponEntityId.clear();
            result.warnings.push_back(
                "Weapon reference was scene-local and was cleared from the reusable Character Prefab.");
        }

        CharacterAdvancedOverrides advanced;
        if (!CaptureCharacterAdvancedOverrides(
                *request.scene, request.character, advanced, result.error))
            return result;
        document.advancedOverrides = SerializeCharacterAdvancedOverrides(advanced);

        if (request.scriptDocument != nullptr)
        {
            std::vector<const ScriptAttachment*> attachments;
            for (const auto& attachment : request.scriptDocument->attachments)
            {
                if (attachment.scope == ScriptScope::Entity &&
                    attachment.ownerEntityId == characterId)
                {
                    attachments.push_back(&attachment);
                }
            }
            std::sort(
                attachments.begin(), attachments.end(),
                [](const ScriptAttachment* left, const ScriptAttachment* right)
                {
                    return left->order < right->order;
                });

            for (const auto* attachment : attachments)
            {
                CharacterPrefabScriptTemplate script;
                script.source = CaptureScriptSourceBinding(*attachment);
                script.enabled = attachment->enabled;
                script.order = attachment->order;
                for (const auto& sourceProperty : attachment->properties)
                {
                    CharacterPrefabScriptProperty property;
                    property.value = sourceProperty;
                    if (sourceProperty.type == ScriptPropertyType::EntityReference)
                    {
                        if (sourceProperty.referenceId == characterId)
                        {
                            property.selfEntityReference = true;
                            property.value.referenceId.clear();
                            property.value.pathHint.clear();
                        }
                        else if (!sourceProperty.referenceId.empty())
                        {
                            property.value.referenceId.clear();
                            property.value.pathHint.clear();
                            result.warnings.push_back(
                                "Script property '" + sourceProperty.name +
                                "' referenced another scene entity and was cleared from the portable Character Prefab.");
                        }
                    }
                    script.properties.push_back(std::move(property));
                }
                document.scripts.push_back(std::move(script));
            }
        }

        std::error_code ec;
        const fs::path root = fs::weakly_canonical(
            fs::absolute(fs::u8path(request.projectRoot), ec), ec);
        if (ec || root.empty() || !fs::is_directory(root, ec) || ec)
        {
            result.error = "Character Prefab project root is unavailable.";
            return result;
        }
        const fs::path prefabRoot = root / "Content" / "Prefabs";
        fs::create_directories(prefabRoot, ec);
        if (ec)
        {
            result.error = "Could not create Content/Prefabs: " + ec.message();
            return result;
        }

        AssetRegistry registry;
        if (!ReadAssetRegistry(
                root.generic_u8string(), request.projectId,
                registry, result.error))
            return result;
        if (FindRecord(registry, result.baseCharacterAssetId) == nullptr)
        {
            result.error = "Character Prefab base Character Asset is not registered.";
            return result;
        }

        do
        {
            document.assetId = GenerateStableId();
        } while (!IsValidStableId(document.assetId) ||
            RegistryContainsId(registry, document.assetId));
        result.assetId = document.assetId;

        const std::string baseName = SanitizePrefabName(request.prefabName);
        fs::path destination;
        for (std::size_t suffix = 1; suffix < 100000; ++suffix)
        {
            const std::string filename = suffix == 1
                ? baseName + CharacterPrefabExtension
                : baseName + " (" + std::to_string(suffix) + ")" +
                    CharacterPrefabExtension;
            const fs::path candidate = prefabRoot / fs::u8path(filename);
            const std::string relative = candidate.lexically_relative(root)
                .lexically_normal().generic_u8string();
            const bool registryPathUsed = std::any_of(
                registry.records.begin(), registry.records.end(),
                [&relative](const AssetRecord& record)
                { return record.projectRelativePath == relative; });
            if (!fs::exists(candidate, ec) && !ec && !registryPathUsed)
            {
                destination = candidate;
                result.projectRelativePath = relative;
                break;
            }
            ec.clear();
        }
        if (destination.empty() ||
            !IsCharacterPrefabProjectPath(result.projectRelativePath))
        {
            result.error = "Could not allocate a safe unique Character Prefab path.";
            return result;
        }

        std::string prefabJson;
        if (!SerializeCharacterPrefab(document, prefabJson, result.error))
            return result;

        AssetRecord record;
        record.assetId = document.assetId;
        record.dependencyNodeId = "character-prefab:" + document.assetId;
        record.projectRelativePath = result.projectRelativePath;
        record.dependencyClass = DependencyClass::Data;
        record.requirement = DependencyRequirement::Required;
        record.applicability = "windows-x64";
        record.provider = "renegade.character_prefab";
        record.providerVersion = CharacterPrefabSchemaVersion;
        record.contentHash = HashText(prefabJson);
        record.root = false;
        record.sourceAvailable = true;
        record.dependencyAssetIds = {document.baseCharacterAssetId};
        registry.records.push_back(std::move(record));

        std::string registryJson;
        if (!SerializeAssetRegistry(registry, registryJson, result.error))
            return result;
        std::string registryPath;
        if (!ResolveAssetRegistryDocumentPath(
                root.generic_u8string(), registryPath, result.error))
            return result;

        ProjectDocumentWrite prefabWrite;
        prefabWrite.destinationPath = destination.generic_u8string();
        prefabWrite.content = Bytes(prefabJson);
        prefabWrite.validator = [projectId = request.projectId,
                                    assetId = document.assetId](
            const std::string& stagedPath, std::string& error)
        {
            std::string staged;
            if (!ReadTextFile(fs::u8path(stagedPath), staged, error))
                return false;
            CharacterPrefabDocument parsed;
            if (!DeserializeCharacterPrefab(staged, parsed, error))
                return false;
            if (parsed.projectId != projectId || parsed.assetId != assetId)
            {
                error = "Staged Character Prefab identity changed during validation.";
                return false;
            }
            return true;
        };

        ProjectDocumentWrite registryWrite;
        registryWrite.destinationPath = registryPath;
        registryWrite.content = Bytes(registryJson);
        registryWrite.validator = [projectId = request.projectId,
                                      assetId = document.assetId](
            const std::string& stagedPath, std::string& error)
        {
            std::string staged;
            if (!ReadTextFile(fs::u8path(stagedPath), staged, error))
                return false;
            AssetRegistry parsed;
            if (!DeserializeAssetRegistry(staged, parsed, error) ||
                parsed.projectId != projectId ||
                FindRecord(parsed, assetId) == nullptr)
            {
                if (error.empty())
                    error = "Staged asset registry lost the Character Prefab record.";
                return false;
            }
            return true;
        };

        ProjectDocumentTransactionOptions options;
        options.transactionId = "cw05-character-prefab-" + document.assetId;
        options.journalDirectory =
            (root / "Intermediate" / "Transactions").generic_u8string();
        options.allowedRoot = root.generic_u8string();
        options.operationHook = request.transactionHook;

        ProjectDocumentTransaction transaction;
        result.transaction = transaction.Execute(
            {std::move(prefabWrite), std::move(registryWrite)},
            std::move(options));
        if (!result.transaction.success)
        {
            result.error = result.transaction.message.empty()
                ? "Character Prefab transaction failed."
                : result.transaction.message;
            return result;
        }

        result.succeeded = true;
        result.error.clear();
        return result;
    }

    PreparedCharacterPrefabPlacement PrepareCharacterPrefabPlacement(
        const std::string& projectRoot,
        const StableId& projectId,
        const StableId& prefabAssetId)
    {
        PreparedCharacterPrefabPlacement result;
        if (projectRoot.empty() || !IsValidStableId(projectId) ||
            !IsValidStableId(prefabAssetId))
        {
            result.error = "Character Prefab placement requires valid project and prefab identity.";
            return result;
        }

        std::error_code ec;
        const fs::path root = fs::weakly_canonical(
            fs::absolute(fs::u8path(projectRoot), ec), ec);
        if (ec || root.empty() || !fs::is_directory(root, ec) || ec)
        {
            result.error = "Character Prefab placement project root is unavailable.";
            return result;
        }

        AssetRegistry registry;
        if (!ReadAssetRegistry(root.generic_u8string(), projectId, registry, result.error))
            return result;
        const AssetRecord* record = FindRecord(registry, prefabAssetId);
        if (record == nullptr || !record->sourceAvailable ||
            record->provider != "renegade.character_prefab" ||
            record->providerVersion != CharacterPrefabSchemaVersion ||
            !IsCharacterPrefabProjectPath(record->projectRelativePath))
        {
            result.error = "Selected asset is not a current registered Character Prefab.";
            return result;
        }

        const fs::path contentRoot = fs::weakly_canonical(root / "Content", ec);
        const fs::path path = fs::weakly_canonical(
            root / fs::u8path(record->projectRelativePath), ec);
        if (ec || !fs::is_regular_file(path, ec) || ec ||
            !IsWithin(path, contentRoot))
        {
            result.error = "Character Prefab file is missing or resolves outside Content.";
            return result;
        }
        std::string text;
        if (!ReadTextFile(path, text, result.error))
            return result;
        if (HashText(text) != record->contentHash)
        {
            result.error = "Character Prefab bytes no longer match the asset registry.";
            return result;
        }
        if (!DeserializeCharacterPrefab(text, result.document, result.error))
            return result;
        if (result.document.projectId != projectId ||
            result.document.assetId != prefabAssetId ||
            std::find(record->dependencyAssetIds.begin(),
                record->dependencyAssetIds.end(),
                result.document.baseCharacterAssetId) ==
                    record->dependencyAssetIds.end())
        {
            result.error = "Character Prefab identity/dependency metadata contradicts the asset registry.";
            return result;
        }

        ReusableModelPlacementRequest baseRequest;
        baseRequest.projectRoot = root.generic_u8string();
        baseRequest.projectId = projectId;
        baseRequest.assetId = result.document.baseCharacterAssetId;
        auto base = ReusableAssetService().PrepareModelAssetPlacement(baseRequest);
        if (!base.IsReady() || base.PeekScene() == nullptr ||
            !IsCharacterAssetTemplateScene(*base.PeekScene()))
        {
            result.error = "Character Prefab base Character Asset cannot be prepared.";
            if (!base.Result().error.empty())
                result.error += " " + base.Result().error;
            return result;
        }
        result.scale = HasCreatorAuthoredTransform(*base.PeekScene())
            ? 1.0f
            : ImportService::ResolveScaleFactor(
                ModelScaleMode::Automatic, *base.PeekScene());
        result.bounds = ImportService::MeasureModelBounds(*base.PeekScene());
        result.scene = base.ReleaseScene();
        result.succeeded = result.scene.IsValid();
        if (!result.succeeded)
            result.error = "Character Prefab base Character scene was unavailable.";
        return result;
    }

    DuplicateCharacterInstanceCommand::DuplicateCharacterInstanceCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity source,
        ScriptDocument* scriptDocument)
        : scene_(&scene)
        , source_(source)
        , scriptDocument_(scriptDocument)
    {
    }

    bool DuplicateCharacterInstanceCommand::ApplyCharacterAuthoring()
    {
        if (scene_ == nullptr || duplicate_ == wi::ecs::INVALID_ENTITY ||
            !IsRenegadeCharacter(*scene_, duplicate_))
            return false;

        // Entity_Duplicate copied the native controller byte-for-byte. Remove
        // the copied Character semantic/controller and recreate it through the
        // accepted Character foundation so no transient native controller state
        // can leak into the new actor. Authoring settings/advanced overrides are
        // then restored explicitly.
        RemoveCharacterCommand remove(*scene_, duplicate_);
        if (!remove.Execute())
            return false;
        MakeCharacterCommand make(*scene_, duplicate_, settings_);
        if (!make.Execute())
            return false;
        std::string error;
        return ApplyCharacterAdvancedOverrides(
            *scene_, duplicate_, advancedOverrides_, error);
    }

    bool DuplicateCharacterInstanceCommand::ApplyScriptDuplication()
    {
        scriptChanged_ = false;
        if (scriptDocument_ == nullptr)
            return true;

        const StableId sourceId = PersistentEntityId(*scene_, source_);
        const StableId duplicateId = PersistentEntityId(*scene_, duplicate_);
        if (!HasSourceEntityScripts(*scriptDocument_, sourceId))
            return true;

        scriptBefore_ = *scriptDocument_;
        std::vector<StableId> created;
        std::string error;
        if (!DuplicateEntityScriptAttachments(
                *scriptDocument_, sourceId, duplicateId, created, error))
        {
            *scriptDocument_ = scriptBefore_;
            return false;
        }
        scriptAfter_ = *scriptDocument_;
        scriptChanged_ = true;
        return true;
    }

    bool DuplicateCharacterInstanceCommand::Execute()
    {
        if (!captured_)
        {
            if (scene_ == nullptr || !IsRenegadeCharacter(*scene_, source_))
                return false;
            settings_ = CaptureCharacterSettings(*scene_, source_);
            std::string error;
            if (!CaptureCharacterAdvancedOverrides(
                    *scene_, source_, advancedOverrides_, error))
                return false;

            duplicateCommand_ =
                std::make_unique<DuplicateEntityCommand>(*scene_, source_);
            if (!duplicateCommand_->Execute())
                return false;
            duplicate_ = duplicateCommand_->DuplicatedEntity();
            if (!ApplyCharacterAuthoring() || !ApplyScriptDuplication())
            {
                if (scriptChanged_)
                    *scriptDocument_ = scriptBefore_;
                duplicateCommand_->Undo();
                duplicate_ = wi::ecs::INVALID_ENTITY;
                return false;
            }
            captured_ = true;
            return true;
        }

        if (duplicateCommand_ == nullptr || !duplicateCommand_->Execute())
            return false;
        duplicate_ = duplicateCommand_->DuplicatedEntity();
        if (!ApplyCharacterAuthoring())
        {
            duplicateCommand_->Undo();
            return false;
        }
        if (scriptChanged_ && scriptDocument_ != nullptr)
            *scriptDocument_ = scriptAfter_;
        return true;
    }

    void DuplicateCharacterInstanceCommand::Undo()
    {
        if (scriptChanged_ && scriptDocument_ != nullptr)
            *scriptDocument_ = scriptBefore_;
        if (duplicateCommand_ != nullptr)
            duplicateCommand_->Undo();
    }

    wi::ecs::Entity DuplicateCharacterInstanceCommand::DuplicatedEntity() const noexcept
    {
        return duplicate_;
    }

    PlaceCharacterPrefabCommand::PlaceCharacterPrefabCommand(
        wi::scene::Scene& scene,
        wi::allocator::shared_ptr<wi::scene::Scene> preparedBaseCharacter,
        CharacterPrefabDocument document,
        const XMFLOAT3& placementPosition,
        const float scaleFactor,
        std::string displayName,
        ScriptDocument* scriptDocument)
        : scene_(&scene)
        , document_(std::move(document))
        , scriptDocument_(scriptDocument)
    {
        placementCommand_ = std::make_unique<PlaceReusableModelCommand>(
            scene, std::move(preparedBaseCharacter),
            document_.baseCharacterAssetId, placementPosition, scaleFactor,
            std::move(displayName));
    }

    PlaceCharacterPrefabCommand::PlaceCharacterPrefabCommand(
        wi::scene::Scene& scene,
        CharacterPrefabDocument document,
        const wi::ecs::Entity existingInstanceRoot,
        const wi::ecs::Entity existingPayloadRoot,
        const std::size_t firstMaterialIndex,
        std::string displayName,
        ScriptDocument* scriptDocument)
        : scene_(&scene)
        , document_(std::move(document))
        , scriptDocument_(scriptDocument)
    {
        placementCommand_ = std::make_unique<PlaceReusableModelCommand>(
            scene, document_.baseCharacterAssetId,
            existingInstanceRoot, existingPayloadRoot, firstMaterialIndex,
            std::move(displayName));
    }

    bool PlaceCharacterPrefabCommand::ApplyPrefabLayer()
    {
        if (scene_ == nullptr || placed_ == wi::ecs::INVALID_ENTITY ||
            !IsRenegadeCharacter(*scene_, placed_))
            return false;

        SetCharacterSettingsCommand settings(
            *scene_, placed_, document_.settings);
        if (CaptureCharacterSettings(*scene_, placed_) != document_.settings &&
            !settings.Execute())
            return false;

        CharacterAdvancedOverrides overrides;
        std::string error;
        if (!DeserializeCharacterAdvancedOverrides(
                document_.advancedOverrides, overrides, error) ||
            !ApplyCharacterAdvancedOverrides(*scene_, placed_, overrides, error))
            return false;
        return StampPrefabOrigin(*scene_, placed_, document_);
    }

    bool PlaceCharacterPrefabCommand::InstantiateScripts()
    {
        scriptChanged_ = false;
        if (document_.scripts.empty())
            return true;
        if (scriptDocument_ == nullptr)
            return false;

        const StableId ownerId = PersistentEntityId(*scene_, placed_);
        if (!IsValidStableId(ownerId))
            return false;

        scriptBefore_ = *scriptDocument_;
        for (const auto& scriptTemplate : document_.scripts)
        {
            ScriptAttachment attachment = CreateScriptAttachment(
                ScriptScope::Entity, ownerId, scriptTemplate.source);
            attachment.enabled = scriptTemplate.enabled;
            attachment.order = scriptTemplate.order;
            for (const auto& propertyTemplate : scriptTemplate.properties)
            {
                ScriptPropertyValue property = propertyTemplate.value;
                if (propertyTemplate.selfEntityReference)
                {
                    property.referenceId = ownerId;
                    property.pathHint.clear();
                }
                attachment.properties.push_back(std::move(property));
            }
            std::string error;
            if (!AddScriptAttachment(
                    *scriptDocument_, std::move(attachment), error))
            {
                *scriptDocument_ = scriptBefore_;
                return false;
            }
        }
        scriptAfter_ = *scriptDocument_;
        scriptChanged_ = true;
        return true;
    }

    bool PlaceCharacterPrefabCommand::Execute()
    {
        if (placementCommand_ == nullptr)
            return false;

        if (!captured_)
        {
            std::string error;
            if (!ValidateDocument(document_, error) ||
                !placementCommand_->Execute())
                return false;
            placed_ = placementCommand_->PlacedEntity();
            if (!ApplyPrefabLayer() || !InstantiateScripts())
            {
                if (scriptChanged_ && scriptDocument_ != nullptr)
                    *scriptDocument_ = scriptBefore_;
                placementCommand_->Undo();
                placed_ = wi::ecs::INVALID_ENTITY;
                return false;
            }
            captured_ = true;
            return true;
        }

        if (!placementCommand_->Execute())
            return false;
        placed_ = placementCommand_->PlacedEntity();
        if (!ApplyPrefabLayer())
        {
            placementCommand_->Undo();
            return false;
        }
        if (scriptChanged_ && scriptDocument_ != nullptr)
            *scriptDocument_ = scriptAfter_;
        return true;
    }

    void PlaceCharacterPrefabCommand::Undo()
    {
        if (scriptChanged_ && scriptDocument_ != nullptr)
            *scriptDocument_ = scriptBefore_;
        if (placementCommand_ != nullptr)
            placementCommand_->Undo();
    }

    wi::ecs::Entity PlaceCharacterPrefabCommand::PlacedEntity() const noexcept
    {
        return placed_;
    }
}
