#pragma once

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"

#include <cstdint>
#include <string>
#include <vector>

#include <WickedEngine.h>

namespace renegade::bridge
{
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
    inline constexpr int CharacterSchemaVersion = 1;

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
        wi::Archive controllerSnapshot_;
        bool captured_ = false;
        bool controllerOwned_ = false;
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
