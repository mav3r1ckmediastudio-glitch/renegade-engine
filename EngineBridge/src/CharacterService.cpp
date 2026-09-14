#include "renegade/bridge/CharacterService.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace
{
    using namespace renegade::bridge;

    constexpr float DefaultCharacterWidth = 0.3f;
    constexpr float DefaultCharacterHeight = 1.8f;

    bool EntityExists(const wi::scene::Scene& scene, const wi::ecs::Entity entity)
    {
        if (entity == wi::ecs::INVALID_ENTITY)
            return false;
        wi::unordered_set<wi::ecs::Entity> entities;
        scene.FindAllEntities(entities);
        return entities.count(entity) != 0;
    }

    bool MetadataEmpty(const wi::scene::MetadataComponent& metadata)
    {
        return metadata.bool_values.names.empty() &&
            metadata.int_values.names.empty() &&
            metadata.float_values.names.empty() &&
            metadata.string_values.names.empty();
    }

    template<typename T>
    T ReadEnum(
        const wi::scene::MetadataComponent* metadata,
        const char* key,
        const T fallback,
        const T minimum,
        const T maximum) noexcept
    {
        if (metadata == nullptr || !metadata->int_values.has(key))
            return fallback;
        const int value = metadata->int_values.get(key);
        const int low = static_cast<int>(minimum);
        const int high = static_cast<int>(maximum);
        return value >= low && value <= high ? static_cast<T>(value) : fallback;
    }

    std::string ReadString(
        const wi::scene::MetadataComponent* metadata,
        const char* key,
        std::string fallback = {})
    {
        if (metadata == nullptr || !metadata->string_values.has(key))
            return fallback;
        return metadata->string_values.get(key);
    }

    bool ReadBool(
        const wi::scene::MetadataComponent* metadata,
        const char* key,
        const bool fallback) noexcept
    {
        if (metadata == nullptr || !metadata->bool_values.has(key))
            return fallback;
        return metadata->bool_values.get(key);
    }

    void SetOrErase(
        wi::scene::MetadataComponent& metadata,
        const char* key,
        const std::string& value)
    {
        if (value.empty())
            metadata.string_values.erase(key);
        else
            metadata.string_values.set(key, value);
    }

    bool HasHumanoidInHierarchy(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity root) noexcept
    {
        if (scene.humanoids.Contains(root))
            return true;
        for (std::size_t index = 0; index < scene.humanoids.GetCount(); ++index)
        {
            const wi::ecs::Entity entity = scene.humanoids.GetEntity(index);
            if (scene.Entity_IsDescendant(entity, root))
                return true;
        }
        return false;
    }

    bool HasNavigationGrid(const wi::scene::Scene& scene) noexcept
    {
        return scene.voxel_grids.GetCount() != 0;
    }

    void WriteCharacterSettings(
        wi::scene::MetadataComponent& metadata,
        const CharacterAuthoringSettings& settings)
    {
        metadata.string_values.set(CharacterMetadataKey, "1");
        metadata.int_values.set(CharacterSchemaVersionMetadataKey, CharacterSchemaVersion);
        metadata.int_values.set(CharacterTypeMetadataKey, static_cast<int>(settings.type));
        metadata.int_values.set(CharacterRoleMetadataKey, static_cast<int>(settings.role));
        metadata.int_values.set(CharacterPersonalityMetadataKey, static_cast<int>(settings.personality));
        metadata.string_values.set(CharacterFactionMetadataKey, settings.factionId);
        SetOrErase(metadata, CharacterAnimationSetMetadataKey, settings.animationSetId);
        SetOrErase(metadata, CharacterPatrolRouteMetadataKey, settings.patrolRouteEntityId);
        SetOrErase(metadata, CharacterSquadMetadataKey, settings.squadId);
        SetOrErase(metadata, CharacterWeaponMetadataKey, settings.weaponEntityId);
        metadata.int_values.set(CharacterCombatStyleMetadataKey, static_cast<int>(settings.combatStyle));
        metadata.int_values.set(CharacterSkillMetadataKey, static_cast<int>(settings.skill));
        metadata.int_values.set(CharacterAwarenessMetadataKey, static_cast<int>(settings.awareness));
        metadata.bool_values.set(CharacterAutonomousMetadataKey, settings.autonomous);
        metadata.bool_values.set(CharacterCanFleeMetadataKey, settings.canFlee);
        metadata.bool_values.set(CharacterCanSurrenderMetadataKey, settings.canSurrender);
        metadata.bool_values.set(CharacterCanUseCoverMetadataKey, settings.canUseCover);
        metadata.bool_values.set(CharacterCanCommunicateMetadataKey, settings.canCommunicate);
    }

    void EraseCharacterSettings(wi::scene::MetadataComponent& metadata)
    {
        metadata.string_values.erase(CharacterMetadataKey);
        metadata.int_values.erase(CharacterSchemaVersionMetadataKey);
        metadata.int_values.erase(CharacterTypeMetadataKey);
        metadata.int_values.erase(CharacterRoleMetadataKey);
        metadata.int_values.erase(CharacterPersonalityMetadataKey);
        metadata.string_values.erase(CharacterFactionMetadataKey);
        metadata.string_values.erase(CharacterAnimationSetMetadataKey);
        metadata.string_values.erase(CharacterPatrolRouteMetadataKey);
        metadata.string_values.erase(CharacterSquadMetadataKey);
        metadata.string_values.erase(CharacterWeaponMetadataKey);
        metadata.int_values.erase(CharacterCombatStyleMetadataKey);
        metadata.int_values.erase(CharacterSkillMetadataKey);
        metadata.int_values.erase(CharacterAwarenessMetadataKey);
        metadata.bool_values.erase(CharacterAutonomousMetadataKey);
        metadata.bool_values.erase(CharacterCanFleeMetadataKey);
        metadata.bool_values.erase(CharacterCanSurrenderMetadataKey);
        metadata.bool_values.erase(CharacterCanUseCoverMetadataKey);
        metadata.bool_values.erase(CharacterCanCommunicateMetadataKey);
        metadata.bool_values.erase(CharacterControllerOwnedMetadataKey);
        metadata.bool_values.erase(CharacterAdoptedControllerWasActiveMetadataKey);
    }

    bool ApplySettings(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const CharacterAuthoringSettings& settings,
        std::string& error) noexcept
    {
        if (!IsRenegadeCharacter(scene, entity))
        {
            error = "Selected entity is not a Renegade Character.";
            return false;
        }
        if (!ValidateCharacterSettings(settings, error))
            return false;
        auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata == nullptr)
        {
            error = "Character metadata is missing.";
            return false;
        }
        WriteCharacterSettings(*metadata, settings);
        error.clear();
        return true;
    }

    bool RemoveCharacterDirect(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata == nullptr || !IsRenegadeCharacter(scene, entity))
            return false;

        const bool controllerOwned = ReadBool(
            metadata, CharacterControllerOwnedMetadataKey, false);
        const bool adoptedWasActive = ReadBool(
            metadata, CharacterAdoptedControllerWasActiveMetadataKey, false);

        EraseCharacterSettings(*metadata);

        if (controllerOwned)
        {
            scene.characters.Remove(entity);
        }
        else if (auto* character = scene.characters.GetComponent(entity); character != nullptr)
        {
            character->SetActive(adoptedWasActive);
        }

        metadata = scene.metadatas.GetComponent(entity);
        if (metadata != nullptr && MetadataEmpty(*metadata))
            scene.metadatas.Remove(entity);
        return true;
    }
}

namespace renegade::bridge
{
    bool operator==(
        const CharacterAuthoringSettings& lhs,
        const CharacterAuthoringSettings& rhs) noexcept
    {
        return lhs.type == rhs.type && lhs.role == rhs.role &&
            lhs.personality == rhs.personality && lhs.factionId == rhs.factionId &&
            lhs.animationSetId == rhs.animationSetId &&
            lhs.patrolRouteEntityId == rhs.patrolRouteEntityId &&
            lhs.squadId == rhs.squadId && lhs.weaponEntityId == rhs.weaponEntityId &&
            lhs.combatStyle == rhs.combatStyle && lhs.skill == rhs.skill &&
            lhs.awareness == rhs.awareness && lhs.autonomous == rhs.autonomous &&
            lhs.canFlee == rhs.canFlee && lhs.canSurrender == rhs.canSurrender &&
            lhs.canUseCover == rhs.canUseCover &&
            lhs.canCommunicate == rhs.canCommunicate;
    }

    bool IsRenegadeCharacter(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        if (entity == wi::ecs::INVALID_ENTITY)
            return false;
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata != nullptr && metadata->string_values.has(CharacterMetadataKey) &&
            metadata->string_values.get(CharacterMetadataKey) == "1" &&
            metadata->int_values.has(CharacterSchemaVersionMetadataKey) &&
            metadata->int_values.get(CharacterSchemaVersionMetadataKey) == CharacterSchemaVersion;
    }

    CharacterPromotionStatus InspectCharacterPromotion(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
    {
        CharacterPromotionStatus status;
        status.selectionExists = EntityExists(scene, entity);
        if (!status.selectionExists)
        {
            status.summary = "No valid Scene entity is selected.";
            return status;
        }

        status.hasTransform = scene.transforms.Contains(entity);
        status.hasPersistentIdentity = IsValidStableId(PersistentEntityId(scene, entity));
        status.hasNativeCharacterController = scene.characters.Contains(entity);
        status.hasHumanoid = HasHumanoidInHierarchy(scene, entity);
        status.hasNavigationGrid = HasNavigationGrid(scene);
        status.alreadyCharacter = IsRenegadeCharacter(scene, entity);
        status.canPromote = status.hasTransform && !status.alreadyCharacter;

        std::ostringstream summary;
        summary << (status.hasPersistentIdentity ? "[OK]" : "[ADD]") << " stable identity  "
                << (status.hasHumanoid ? "[OK]" : "[WARN]") << " humanoid  "
                << (status.hasNativeCharacterController ? "[OK]" : "[ADD]") << " native controller  "
                << (status.hasNavigationGrid ? "[OK]" : "[WARN]") << " navigation";
        status.summary = summary.str();
        return status;
    }

    CharacterAuthoringSettings CaptureCharacterSettings(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        CharacterAuthoringSettings settings;
        const auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata == nullptr || !IsRenegadeCharacter(scene, entity))
            return settings;

        settings.type = ReadEnum(
            metadata, CharacterTypeMetadataKey, CharacterType::Human,
            CharacterType::Human, CharacterType::Custom);
        settings.role = ReadEnum(
            metadata, CharacterRoleMetadataKey, CharacterRole::Guard,
            CharacterRole::Guard, CharacterRole::Custom);
        settings.personality = ReadEnum(
            metadata, CharacterPersonalityMetadataKey, PersonalityPreset::Balanced,
            PersonalityPreset::Cautious, PersonalityPreset::Custom);
        settings.factionId = ReadString(metadata, CharacterFactionMetadataKey, "Neutral");
        settings.animationSetId = ReadString(metadata, CharacterAnimationSetMetadataKey);
        settings.patrolRouteEntityId = ReadString(metadata, CharacterPatrolRouteMetadataKey);
        settings.squadId = ReadString(metadata, CharacterSquadMetadataKey);
        settings.weaponEntityId = ReadString(metadata, CharacterWeaponMetadataKey);
        settings.combatStyle = ReadEnum(
            metadata, CharacterCombatStyleMetadataKey, CombatStyle::None,
            CombatStyle::None, CombatStyle::Custom);
        settings.skill = ReadEnum(
            metadata, CharacterSkillMetadataKey, SkillPreset::Trained,
            SkillPreset::Untrained, SkillPreset::Elite);
        settings.awareness = ReadEnum(
            metadata, CharacterAwarenessMetadataKey, AwarenessPreset::Normal,
            AwarenessPreset::Relaxed, AwarenessPreset::Vigilant);
        settings.autonomous = ReadBool(metadata, CharacterAutonomousMetadataKey, true);
        settings.canFlee = ReadBool(metadata, CharacterCanFleeMetadataKey, true);
        settings.canSurrender = ReadBool(metadata, CharacterCanSurrenderMetadataKey, false);
        settings.canUseCover = ReadBool(metadata, CharacterCanUseCoverMetadataKey, true);
        settings.canCommunicate = ReadBool(metadata, CharacterCanCommunicateMetadataKey, true);
        return settings;
    }

    bool ValidateCharacterSettings(
        const CharacterAuthoringSettings& settings,
        std::string& error) noexcept
    {
        if (settings.factionId.empty() || settings.factionId.size() > 64)
        {
            error = "Character faction must be between 1 and 64 characters.";
            return false;
        }
        if (settings.squadId.size() > 64)
        {
            error = "Character squad must be at most 64 characters.";
            return false;
        }
        for (const auto* id : {
            &settings.animationSetId,
            &settings.patrolRouteEntityId,
            &settings.weaponEntityId})
        {
            if (!id->empty() && !IsValidStableId(*id))
            {
                error = "Character asset/entity references must use Renegade stable IDs.";
                return false;
            }
        }
        error.clear();
        return true;
    }

    std::vector<wi::ecs::Entity> CollectCharacters(const wi::scene::Scene& scene)
    {
        std::vector<wi::ecs::Entity> result;
        result.reserve(scene.metadatas.GetCount());
        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            const wi::ecs::Entity entity = scene.metadatas.GetEntity(index);
            if (IsRenegadeCharacter(scene, entity))
                result.push_back(entity);
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    bool InitializeRuntimeCharacters(
        wi::scene::Scene& scene,
        CharacterRuntimeState& state,
        std::string& error)
    {
        state.characters.clear();
        const auto entities = CollectCharacters(scene);
        state.characters.reserve(entities.size());
        for (const wi::ecs::Entity entity : entities)
        {
            const StableId characterId = PersistentEntityId(scene, entity);
            if (!IsValidStableId(characterId))
            {
                state.characters.clear();
                error = "Character is missing a valid persistent Renegade identity.";
                return false;
            }
            auto* transform = scene.transforms.GetComponent(entity);
            auto* character = scene.characters.GetComponent(entity);
            if (transform == nullptr || character == nullptr)
            {
                state.characters.clear();
                error = "Character lost its required Transform or native CharacterComponent.";
                return false;
            }

            character->SetPosition(transform->GetPosition());
            const XMFLOAT3 facing = transform->GetForward();
            character->SetFacing(facing);
            character->SetActive(true);

            CharacterRecord record;
            record.entity = entity;
            record.characterId = characterId;
            record.settings = CaptureCharacterSettings(scene, entity);
            state.characters.push_back(std::move(record));
        }
        error.clear();
        return true;
    }

    void ResetRuntimeCharacters(
        wi::scene::Scene& scene,
        CharacterRuntimeState& state) noexcept
    {
        for (const auto& record : state.characters)
        {
            if (auto* character = scene.characters.GetComponent(record.entity);
                character != nullptr)
            {
                character->SetActive(false);
            }
        }
        state.characters.clear();
    }

    MakeCharacterCommand::MakeCharacterCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        CharacterAuthoringSettings settings)
        : scene_(&scene), entity_(entity), settings_(std::move(settings))
    {
    }

    bool MakeCharacterCommand::ApplyPromotion()
    {
        if (scene_ == nullptr || !EntityExists(*scene_, entity_) ||
            !scene_->transforms.Contains(entity_) || IsRenegadeCharacter(*scene_, entity_))
        {
            return false;
        }

        std::string error;
        if (!ValidateCharacterSettings(settings_, error))
            return false;

        const StableId existingId = PersistentEntityId(*scene_, entity_);
        if (!existingId.empty() && !IsValidStableId(existingId))
            return false;
        if (existingId.empty())
        {
            if (assignedPersistentId_.empty())
                assignedPersistentId_ = GenerateStableId();
            if (!AssignPersistentEntityId(*scene_, entity_, assignedPersistentId_, error))
                return false;
        }
        else if (assignedPersistentId_.empty())
        {
            assignedPersistentId_ = existingId;
        }

        auto* character = scene_->characters.GetComponent(entity_);
        const bool controllerOwned = character == nullptr;
        const bool adoptedWasActive = character != nullptr && character->IsActive();
        if (character == nullptr)
        {
            character = &scene_->characters.Create(entity_);
            character->width = DefaultCharacterWidth;
            character->height = DefaultCharacterHeight;
            character->SetFootPlacementEnabled(false);
        }
        character->SetActive(false);

        auto* metadata = scene_->metadatas.GetComponent(entity_);
        if (metadata == nullptr)
            metadata = &scene_->metadatas.Create(entity_);
        WriteCharacterSettings(*metadata, settings_);
        metadata->bool_values.set(CharacterControllerOwnedMetadataKey, controllerOwned);
        metadata->bool_values.set(
            CharacterAdoptedControllerWasActiveMetadataKey,
            adoptedWasActive);
        return true;
    }

    bool MakeCharacterCommand::Execute()
    {
        if (scene_ == nullptr)
            return false;
        if (!captured_)
        {
            if (IsRenegadeCharacter(*scene_, entity_))
                return false;
            const StableId persistentId = PersistentEntityId(*scene_, entity_);
            hadPersistentId_ = IsValidStableId(persistentId);
            if (hadPersistentId_)
                assignedPersistentId_ = persistentId;
            if (auto* character = scene_->characters.GetComponent(entity_); character != nullptr)
            {
                hadCharacterComponent_ = true;
                previousCharacterActive_ = character->IsActive();
            }
            captured_ = true;
        }
        return ApplyPromotion();
    }

    void MakeCharacterCommand::RemovePromotion() noexcept
    {
        if (scene_ == nullptr || !IsRenegadeCharacter(*scene_, entity_))
            return;
        (void)RemoveCharacterDirect(*scene_, entity_);

        if (hadCharacterComponent_)
        {
            if (auto* character = scene_->characters.GetComponent(entity_); character != nullptr)
                character->SetActive(previousCharacterActive_);
        }
        else
        {
            scene_->characters.Remove(entity_);
        }

        if (!hadPersistentId_)
        {
            if (auto* metadata = scene_->metadatas.GetComponent(entity_); metadata != nullptr)
            {
                metadata->string_values.erase(PersistentEntityIdMetadataKey);
                if (MetadataEmpty(*metadata))
                    scene_->metadatas.Remove(entity_);
            }
        }
    }

    void MakeCharacterCommand::Undo()
    {
        RemovePromotion();
    }

    RemoveCharacterCommand::RemoveCharacterCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
        : scene_(&scene), entity_(entity)
    {
    }

    bool RemoveCharacterCommand::Execute()
    {
        if (scene_ == nullptr || !IsRenegadeCharacter(*scene_, entity_))
            return false;
        if (!captured_)
        {
            settings_ = CaptureCharacterSettings(*scene_, entity_);
            captured_ = true;
        }
        return RemoveCharacterDirect(*scene_, entity_);
    }

    void RemoveCharacterCommand::Undo()
    {
        if (scene_ == nullptr || IsRenegadeCharacter(*scene_, entity_))
            return;
        MakeCharacterCommand restore(*scene_, entity_, settings_);
        (void)restore.Execute();
    }

    SetCharacterSettingsCommand::SetCharacterSettingsCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        CharacterAuthoringSettings after)
        : scene_(&scene), entity_(entity),
          before_(CaptureCharacterSettings(scene, entity)),
          after_(std::move(after))
    {
    }

    bool SetCharacterSettingsCommand::Apply(
        const CharacterAuthoringSettings& settings) noexcept
    {
        if (scene_ == nullptr)
            return false;
        std::string error;
        return ApplySettings(*scene_, entity_, settings, error);
    }

    bool SetCharacterSettingsCommand::Execute()
    {
        if (before_ == after_)
            return false;
        return Apply(after_);
    }

    void SetCharacterSettingsCommand::Undo()
    {
        (void)Apply(before_);
    }
}
