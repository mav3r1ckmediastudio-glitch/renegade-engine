#pragma once

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <WickedEngine.h>

namespace renegade::bridge
{
    // Wicked writes CharacterComponent pose to TransformComponent even when
    // inactive. Mirror authored edits into the controller immediately so an
    // editor frame cannot teleport or reverse an NPC after promotion/gizmo.
    inline void SyncNativeCharacterPoseFromTransform(
        wi::scene::Scene& scene, const wi::ecs::Entity entity) noexcept
    {
        auto* transform = scene.transforms.GetComponent(entity);
        auto* character = scene.characters.GetComponent(entity);
        if (transform == nullptr || character == nullptr)
            return;
        transform->UpdateTransform();
        character->SetPosition(transform->GetPosition());
        const XMFLOAT3 forward = transform->GetForward();
        const float horizontal = std::sqrt(forward.x * forward.x + forward.z * forward.z);
        if (horizontal > 0.0001f)
        {
            // Wicked applies an additional PI yaw in RunCharacterUpdateSystem.
            character->SetFacing(XMFLOAT3(
                -forward.x / horizontal, 0.0f, -forward.z / horizontal));
        }
        // CharacterComponent supplies a uniform scale when updating Transform.
        character->scale = transform->scale_local.x;
    }

    inline constexpr const char* CharacterMetadataKey = "renegade.character";
    inline constexpr const char* CharacterSchemaVersionMetadataKey = "renegade.character.version";
    inline constexpr const char* CharacterTypeMetadataKey = "renegade.character.type";
    inline constexpr const char* CharacterRoleMetadataKey = "renegade.character.role";
    inline constexpr const char* CharacterPersonalityMetadataKey = "renegade.character.personality";
    inline constexpr const char* CharacterFactionMetadataKey = "renegade.character.faction";
    inline constexpr const char* CharacterAnimationSetMetadataKey = "renegade.character.animation_set";
    inline constexpr const char* CharacterPatrolRouteMetadataKey = "renegade.character.patrol_route_id";
    inline constexpr const char* CharacterSquadMetadataKey = "renegade.character.squad";
    inline constexpr const char* CharacterWeaponMetadataKey = "renegade.character.weapon_id";
    inline constexpr const char* CharacterCombatStyleMetadataKey = "renegade.character.combat_style";
    inline constexpr const char* CharacterSkillMetadataKey = "renegade.character.skill";
    inline constexpr const char* CharacterAwarenessMetadataKey = "renegade.character.awareness";
    inline constexpr const char* CharacterAutonomousMetadataKey = "renegade.character.autonomous";
    inline constexpr const char* CharacterCanFleeMetadataKey = "renegade.character.can_flee";
    inline constexpr const char* CharacterCanSurrenderMetadataKey = "renegade.character.can_surrender";
    inline constexpr const char* CharacterCanUseCoverMetadataKey = "renegade.character.can_use_cover";
    inline constexpr const char* CharacterCanCommunicateMetadataKey = "renegade.character.can_communicate";
    inline constexpr const char* CharacterControllerOwnedMetadataKey = "renegade.character.controller_owned";
    inline constexpr const char* CharacterAdoptedControllerWasActiveMetadataKey =
        "renegade.character.adopted_controller_was_active";
    inline constexpr const char* CharacterAdvancedPayloadMetadataKey =
        "renegade.character.advanced";
    inline constexpr int CharacterSchemaVersion = 1;

    // CW-04 asset-origin marker. This is deliberately distinct from
    // CharacterMetadataKey: a prepared reusable Character Asset is a template,
    // while CharacterMetadataKey identifies a concrete scene Character
    // instance with its own persistent identity and gameplay authoring state.
    // PrepareModelAssetPlacement stamps this marker into the in-memory reusable
    // payload after reading the durable import recipe, so already-imported CW-01
    // Character products gain the new placement behaviour without requiring a
    // destructive product rewrite.
    inline constexpr const char* CharacterAssetTemplateMetadataKey =
        "renegade.character_asset_template";
    inline constexpr const char* CharacterAssetTemplateVersionMetadataKey =
        "renegade.character_asset_template.version";
    inline constexpr int CharacterAssetTemplateVersion = 1;

    [[nodiscard]] inline bool IsCharacterAssetTemplateMarker(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        if (entity == wi::ecs::INVALID_ENTITY)
            return false;
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata != nullptr &&
            metadata->bool_values.has(CharacterAssetTemplateMetadataKey) &&
            metadata->bool_values.get(CharacterAssetTemplateMetadataKey) &&
            metadata->int_values.has(CharacterAssetTemplateVersionMetadataKey) &&
            metadata->int_values.get(CharacterAssetTemplateVersionMetadataKey) ==
                CharacterAssetTemplateVersion;
    }

    [[nodiscard]] inline bool IsCharacterAssetTemplateScene(
        const wi::scene::Scene& scene) noexcept
    {
        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            if (IsCharacterAssetTemplateMarker(
                    scene, scene.metadatas.GetEntity(index)))
            {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] inline bool CharacterAssetTemplateInHierarchy(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity root) noexcept
    {
        if (root == wi::ecs::INVALID_ENTITY)
            return false;
        if (IsCharacterAssetTemplateMarker(scene, root))
            return true;
        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            const wi::ecs::Entity entity = scene.metadatas.GetEntity(index);
            if (scene.Entity_IsDescendant(entity, root) &&
                IsCharacterAssetTemplateMarker(scene, entity))
            {
                return true;
            }
        }
        return false;
    }

    // Marks every top-level transform in the prepared reusable payload. The
    // marker stays on the replaceable asset payload; the scene wrapper receives
    // Character instance metadata only when PlaceReusableModelCommand commits
    // the placement. This keeps base Character Asset state separate from scene
    // gameplay configuration.
    [[nodiscard]] inline bool MarkCharacterAssetTemplate(
        wi::scene::Scene& scene) noexcept
    {
        bool marked = false;
        for (std::size_t index = 0; index < scene.transforms.GetCount(); ++index)
        {
            const wi::ecs::Entity entity = scene.transforms.GetEntity(index);
            const auto* hierarchy = scene.hierarchy.GetComponent(entity);
            if (hierarchy != nullptr &&
                hierarchy->parentID != wi::ecs::INVALID_ENTITY)
            {
                continue;
            }

            auto* metadata = scene.metadatas.GetComponent(entity);
            if (metadata == nullptr)
                metadata = &scene.metadatas.Create(entity);
            metadata->bool_values.set(CharacterAssetTemplateMetadataKey, true);
            metadata->int_values.set(
                CharacterAssetTemplateVersionMetadataKey,
                CharacterAssetTemplateVersion);
            marked = true;
        }
        return marked;
    }

    enum class CharacterType : std::int32_t
    {
        Human = 0,
        Creature,
        Custom,
    };

    enum class CharacterRole : std::int32_t
    {
        Guard = 0,
        PatrolGuard,
        Soldier,
        Civilian,
        Companion,
        Predator,
        Passive,
        Custom,
    };

    enum class PersonalityPreset : std::int32_t
    {
        Cautious = 0,
        Balanced,
        Aggressive,
        Timid,
        Veteran,
        Reckless,
        Custom,
    };

    enum class CombatStyle : std::int32_t
    {
        None = 0,
        Melee,
        Ranged,
        Mixed,
        Custom,
    };

    enum class SkillPreset : std::int32_t
    {
        Untrained = 0,
        Novice,
        Trained,
        Veteran,
        Elite,
    };

    enum class AwarenessPreset : std::int32_t
    {
        Relaxed = 0,
        Normal,
        Alert,
        Vigilant,
    };

    struct CharacterAuthoringSettings
    {
        CharacterType type = CharacterType::Human;
        CharacterRole role = CharacterRole::Guard;
        PersonalityPreset personality = PersonalityPreset::Balanced;
        std::string factionId = "Neutral";
        StableId animationSetId;
        StableId patrolRouteEntityId;
        std::string squadId;
        StableId weaponEntityId;
        CombatStyle combatStyle = CombatStyle::None;
        SkillPreset skill = SkillPreset::Trained;
        AwarenessPreset awareness = AwarenessPreset::Normal;
        bool autonomous = true;
        bool canFlee = true;
        bool canSurrender = false;
        bool canUseCover = true;
        bool canCommunicate = true;
    };

    [[nodiscard]] bool operator==(
        const CharacterAuthoringSettings& lhs,
        const CharacterAuthoringSettings& rhs) noexcept;
    [[nodiscard]] inline bool operator!=(
        const CharacterAuthoringSettings& lhs,
        const CharacterAuthoringSettings& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    struct CharacterPromotionStatus
    {
        bool selectionExists = false;
        bool hasTransform = false;
        bool hasPersistentIdentity = false;
        bool hasNativeCharacterController = false;
        bool hasHumanoid = false;
        bool hasNavigationGrid = false;
        bool incompatibleSemantic = false;
        bool alreadyCharacter = false;
        bool canPromote = false;
        std::string summary;
    };

    struct CharacterRecord
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        StableId characterId;
        CharacterAuthoringSettings settings;
    };

    struct CharacterRuntimeState
    {
        std::vector<CharacterRecord> characters;
    };

    [[nodiscard]] bool IsRenegadeCharacter(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;

    [[nodiscard]] CharacterPromotionStatus InspectCharacterPromotion(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity);

    [[nodiscard]] CharacterAuthoringSettings CaptureCharacterSettings(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;

    [[nodiscard]] bool ValidateCharacterSettings(
        const CharacterAuthoringSettings& settings,
        std::string& error) noexcept;

    [[nodiscard]] std::vector<wi::ecs::Entity> CollectCharacters(
        const wi::scene::Scene& scene);

    [[nodiscard]] bool InitializeRuntimeCharacters(
        wi::scene::Scene& scene,
        CharacterRuntimeState& state,
        std::string& error);

    void ResetRuntimeCharacters(
        wi::scene::Scene& scene,
        CharacterRuntimeState& state) noexcept;

    class MakeCharacterCommand final : public ICommand
    {
    public:
        MakeCharacterCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            CharacterAuthoringSettings settings = {});

        bool Execute() override;
        void Undo() override;

    private:
        bool ApplyPromotion();
        void RemovePromotion() noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        CharacterAuthoringSettings settings_;
        StableId assignedPersistentId_;
        bool captured_ = false;
        bool hadPersistentId_ = false;
        bool hadCharacterComponent_ = false;
        bool previousCharacterActive_ = false;
    };

    class RemoveCharacterCommand final : public ICommand
    {
    public:
        RemoveCharacterCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity);

        bool Execute() override;
        void Undo() override;

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        CharacterAuthoringSettings settings_;
        std::string advancedMetadataSnapshot_;
        wi::Archive controllerSnapshot_;
        bool captured_ = false;
        bool controllerOwned_ = false;
        bool hasAdvancedMetadataSnapshot_ = false;
        bool hasControllerSnapshot_ = false;
        bool controllerFootPlacementEnabled_ = true;
    };

    class SetCharacterSettingsCommand final : public ICommand
    {
    public:
        SetCharacterSettingsCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            CharacterAuthoringSettings after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const CharacterAuthoringSettings& settings) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        CharacterAuthoringSettings before_;
        CharacterAuthoringSettings after_;
    };
}
