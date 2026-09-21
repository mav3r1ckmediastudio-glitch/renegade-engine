#pragma once

#include "RuntimeCharacterPerception.h"
#include "RuntimeCharacterSystem.h"
#include "renegade/bridge/GameplayEventService.h"
#include "renegade/bridge/WeaponCombatService.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace renegade::runtime
{
    using CombatEventEmitter =
        std::function<bool(bridge::GameplayEvent event, std::string& error)>;

    struct CharacterCombatRecord
    {
        bridge::StableId characterId;
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity weaponEntity = wi::ecs::INVALID_ENTITY;
        bridge::WeaponAiDescriptor weapon;
        bridge::WeaponAiRangeBand effectiveRange;
        float maxHealth = 100.0f;
        float health = 100.0f;
        int magazineAmmo = 0;
        int reserveAmmo = 0;
        float fireCooldownSeconds = 0.0f;
        float reloadRemainingSeconds = 0.0f;
        float targetDistance = std::numeric_limits<float>::infinity();
        bool hasDirectHostileTarget = false;
        bool dead = false;
        std::uint64_t shotSequence = 0;
        std::uint64_t shotsFired = 0;
        std::uint64_t shotsHit = 0;
        std::uint64_t reloads = 0;
        std::uint64_t damageTaken = 0;
    };

    struct RuntimeCombatState
    {
        std::vector<CharacterCombatRecord> characters;
        float playerMaxHealth = 100.0f;
        float playerHealth = 100.0f;
        bool playerDead = false;
        std::uint64_t shotsFired = 0;
        std::uint64_t shotsHit = 0;
        std::uint64_t reloads = 0;
        std::uint64_t damageEvents = 0;
        std::uint64_t combatEventsRejected = 0;
    };

    struct CombatFireResult
    {
        bool fired = false;
        bool hit = false;
        float distance = 0.0f;
        float hitChance = 0.0f;
        float damage = 0.0f;
        bool targetDied = false;
    };

    [[nodiscard]] inline CharacterCombatRecord* FindCharacterCombat(
        RuntimeCombatState& state,
        const bridge::StableId& id) noexcept
    {
        const auto iterator = std::lower_bound(
            state.characters.begin(), state.characters.end(), id,
            [](const CharacterCombatRecord& record, const bridge::StableId& value)
            {
                return record.characterId < value;
            });
        return iterator != state.characters.end() && iterator->characterId == id
            ? &*iterator
            : nullptr;
    }

    [[nodiscard]] inline const CharacterCombatRecord* FindCharacterCombat(
        const RuntimeCombatState& state,
        const bridge::StableId& id) noexcept
    {
        const auto iterator = std::lower_bound(
            state.characters.begin(), state.characters.end(), id,
            [](const CharacterCombatRecord& record, const bridge::StableId& value)
            {
                return record.characterId < value;
            });
        return iterator != state.characters.end() && iterator->characterId == id
            ? &*iterator
            : nullptr;
    }

    [[nodiscard]] inline float HealthFraction(
        const CharacterCombatRecord& combat) noexcept
    {
        return combat.maxHealth <= 0.0f
            ? 0.0f
            : std::clamp(combat.health / combat.maxHealth, 0.0f, 1.0f);
    }

    [[nodiscard]] inline bool UsesAmmunition(
        const CharacterCombatRecord& combat) noexcept
    {
        return combat.weapon.magazineSize > 0;
    }

    [[nodiscard]] inline bool CanReload(
        const CharacterCombatRecord& combat) noexcept
    {
        return UsesAmmunition(combat) && !combat.dead &&
            combat.reloadRemainingSeconds <= 0.0f &&
            combat.magazineAmmo < combat.weapon.magazineSize &&
            combat.reserveAmmo > 0;
    }

    [[nodiscard]] inline bool HasUsableWeapon(
        const CharacterCombatRecord& combat) noexcept
    {
        if (combat.dead || combat.weapon.style == bridge::WeaponAiStyle::None ||
            combat.weapon.damage <= 0.0f || combat.effectiveRange.maxRange <= 0.0f)
        {
            return false;
        }
        if (!UsesAmmunition(combat))
            return true;
        return combat.magazineAmmo > 0 || combat.reserveAmmo > 0;
    }

    [[nodiscard]] inline bool InitializeRuntimeCombat(
        const wi::scene::Scene& scene,
        const RuntimeCharacterSystemState& characters,
        RuntimeCombatState& state,
        std::string& error)
    {
        RuntimeCombatState candidate;
        candidate.characters.reserve(characters.characters.size());
        for (const auto& character : characters.characters)
        {
            const auto* nativeCharacter = scene.characters.GetComponent(character.entity);
            if (nativeCharacter == nullptr)
            {
                state = {};
                error = "AI-05 combat requires the native Wicked CharacterComponent.";
                return false;
            }

            CharacterCombatRecord combat;
            combat.characterId = character.stableEntityId;
            combat.entity = character.entity;
            combat.weaponEntity = character.references.weaponEntity;
            if (!bridge::CaptureWeaponAiDescriptor(
                    scene,
                    combat.weaponEntity,
                    character.authoring.combatStyle,
                    combat.weapon,
                    error))
            {
                state = {};
                error = "Character '" + character.stableEntityId +
                    "' weapon descriptor is invalid: " + error;
                return false;
            }

            combat.effectiveRange = bridge::ResolveEffectiveWeaponAiRange(
                character.tuning, combat.weapon);

            combat.maxHealth = nativeCharacter->health > 0
                ? static_cast<float>(nativeCharacter->health)
                : 100.0f;
            combat.health = static_cast<float>(std::max(0, nativeCharacter->health));
            combat.dead = combat.health <= 0.0f;
            combat.magazineAmmo = std::max(0, combat.weapon.magazineSize);
            combat.reserveAmmo = std::max(0, combat.weapon.reserveAmmo);

            // Enemy is a hostile gameplay classification, not a request for an
            // unarmed retreating NPC. Require an actual combat capability before
            // Runtime starts. Intrinsic melee is represented by no weapon entity
            // plus CombatStyle::Melee, which resolves to the built-in fists /
            // claws / teeth descriptor above. Governed weapon entities resolve
            // through their normal Weapon AI descriptor.
            if (character.authoring.factionId == "Enemy" && !HasUsableWeapon(combat))
            {
                state = {};
                error = "Enemy Character '" + character.stableEntityId +
                    "' requires FISTS / CLAWS / TEETH or a usable weapon.";
                return false;
            }

            candidate.characters.push_back(std::move(combat));
        }

        std::sort(
            candidate.characters.begin(), candidate.characters.end(),
            [](const CharacterCombatRecord& lhs, const CharacterCombatRecord& rhs)
            {
                return lhs.characterId < rhs.characterId;
            });
        for (std::size_t index = 1; index < candidate.characters.size(); ++index)
        {
            if (candidate.characters[index - 1].characterId ==
                candidate.characters[index].characterId)
            {
                state = {};
                error = "AI-05 combat received duplicate Character stable identities.";
                return false;
            }
        }

        state = std::move(candidate);
        error.clear();
        return true;
    }

    inline void ResetRuntimeCombat(RuntimeCombatState& state) noexcept
    {
        state = {};
    }

    [[nodiscard]] inline std::uint64_t CombatStableHash(
        const std::string& value,
        const std::uint64_t sequence) noexcept
    {
        std::uint64_t hash = 1469598103934665603ull;
        for (const unsigned char c : value)
        {
            hash ^= c;
            hash *= 1099511628211ull;
        }
        hash ^= sequence + 0x9e3779b97f4a7c15ull;
        hash *= 1099511628211ull;
        return hash;
    }

    [[nodiscard]] inline float DeterministicCombatUnit(
        const std::string& stableId,
        const std::uint64_t sequence) noexcept
    {
        const std::uint64_t hash = CombatStableHash(stableId, sequence);
        return static_cast<float>(hash & 0xFFFFFFu) /
            static_cast<float>(0x1000000u);
    }

    [[nodiscard]] inline float ComputeCombatHitChance(
        const bridge::CharacterTuning& tuning,
        const bridge::WeaponAiDescriptor& weapon,
        const float distance) noexcept
    {
        if (!std::isfinite(distance) || distance < 0.0f || distance > weapon.maxRange)
            return 0.0f;
        float rangeQuality = 1.0f;
        if (distance < weapon.minRange && weapon.minRange > 0.0f)
        {
            rangeQuality = std::clamp(distance / weapon.minRange, 0.35f, 1.0f);
        }
        else if (distance > weapon.preferredRange && weapon.maxRange > weapon.preferredRange)
        {
            const float span = weapon.maxRange - weapon.preferredRange;
            rangeQuality = std::clamp(
                1.0f - ((distance - weapon.preferredRange) / span) * 0.55f,
                0.35f,
                1.0f);
        }
        return std::clamp(
            tuning.accuracy * weapon.baseAccuracy * rangeQuality,
            0.02f,
            0.98f);
    }

    inline void RefreshRuntimeCombat(
        wi::scene::Scene& scene,
        const RuntimeCharacterSystemState& characters,
        const RuntimeCharacterPerceptionState& perception,
        RuntimeCombatState& state,
        const float dt) noexcept
    {
        if (!(dt >= 0.0f) || !std::isfinite(dt))
            return;
        for (const auto& character : characters.characters)
        {
            auto* combat = FindCharacterCombat(state, character.stableEntityId);
            if (combat == nullptr)
                continue;
            auto* nativeCharacter = scene.characters.GetComponent(character.entity);
            if (nativeCharacter == nullptr)
                continue;

            combat->fireCooldownSeconds = std::max(
                0.0f, combat->fireCooldownSeconds - dt);
            const float previousReload = combat->reloadRemainingSeconds;
            combat->reloadRemainingSeconds = std::max(
                0.0f, combat->reloadRemainingSeconds - dt);
            if (previousReload > 0.0f && combat->reloadRemainingSeconds <= 0.0f &&
                UsesAmmunition(*combat))
            {
                const int needed = std::max(
                    0, combat->weapon.magazineSize - combat->magazineAmmo);
                const int transferred = std::min(needed, combat->reserveAmmo);
                combat->magazineAmmo += transferred;
                combat->reserveAmmo -= transferred;
            }

            const float nativeHealth = static_cast<float>(std::max(0, nativeCharacter->health));
            if (std::abs(nativeHealth - combat->health) > 0.01f)
                combat->health = std::clamp(nativeHealth, 0.0f, combat->maxHealth);
            combat->dead = combat->health <= 0.0f;

            combat->hasDirectHostileTarget = false;
            combat->targetDistance = std::numeric_limits<float>::infinity();
            const auto* cognition = FindCharacterCognition(
                perception, character.stableEntityId);
            if (cognition == nullptr)
                continue;
            const auto* memory = FindCharacterMemory(*cognition, RuntimePlayerKnowledgeId);
            if (memory == nullptr || !memory->hostile || !memory->directSight ||
                !memory->hasPosition)
            {
                continue;
            }
            const XMFLOAT3 position = nativeCharacter->GetPositionInterpolated();
            const float dx = memory->lastKnownPosition.x - position.x;
            const float dy = memory->lastKnownPosition.y - position.y;
            const float dz = memory->lastKnownPosition.z - position.z;
            combat->targetDistance = std::sqrt(dx * dx + dy * dy + dz * dz);
            combat->hasDirectHostileTarget = std::isfinite(combat->targetDistance);
        }
    }

    [[nodiscard]] inline bool BeginCombatReload(
        CharacterCombatRecord& combat) noexcept
    {
        if (!CanReload(combat))
            return false;
        combat.reloadRemainingSeconds = std::max(0.01f, combat.weapon.reloadSeconds);
        ++combat.reloads;
        return true;
    }

    inline bool EmitCombatGameplayEvent(
        RuntimeCombatState& state,
        const CombatEventEmitter& emitter,
        bridge::GameplayEvent event) noexcept
    {
        if (!emitter)
            return false;
        std::string error;
        if (!emitter(std::move(event), error))
        {
            ++state.combatEventsRejected;
            return false;
        }
        return true;
    }

    [[nodiscard]] inline bool ApplyCombatDamage(
        wi::scene::Scene& scene,
        RuntimeCombatState& state,
        const std::string& targetId,
        const float amount,
        bool& died) noexcept
    {
        died = false;
        if (!std::isfinite(amount) || amount <= 0.0f)
            return false;
        if (targetId == RuntimePlayerKnowledgeId)
        {
            if (state.playerDead)
                return false;
            state.playerHealth = std::max(0.0f, state.playerHealth - amount);
            state.playerDead = state.playerHealth <= 0.0f;
            died = state.playerDead;
            ++state.damageEvents;
            return true;
        }

        auto* combat = FindCharacterCombat(state, targetId);
        if (combat == nullptr || combat->dead)
            return false;
        combat->health = std::max(0.0f, combat->health - amount);
        combat->dead = combat->health <= 0.0f;
        combat->damageTaken += static_cast<std::uint64_t>(std::ceil(amount));
        if (auto* nativeCharacter = scene.characters.GetComponent(combat->entity))
            nativeCharacter->health = static_cast<int>(std::lround(combat->health));
        died = combat->dead;
        ++state.damageEvents;
        return true;
    }

    [[nodiscard]] inline bool TryFireAtRuntimePlayer(
        wi::scene::Scene& scene,
        const RuntimeCharacterRecord& character,
        const CharacterCognitionRecord& cognition,
        CharacterCombatRecord& combat,
        RuntimeCharacterPerceptionState& perception,
        RuntimeCombatState& state,
        const CombatEventEmitter& emitter,
        CombatFireResult& result) noexcept
    {
        result = {};
        if (combat.dead || state.playerDead || combat.reloadRemainingSeconds > 0.0f ||
            combat.fireCooldownSeconds > 0.0f || !HasUsableWeapon(combat))
        {
            return false;
        }
        const auto* memory = FindCharacterMemory(cognition, RuntimePlayerKnowledgeId);
        if (memory == nullptr || !memory->hostile || !memory->directSight ||
            !memory->hasPosition || memory->confidence < MinimumMemoryConfidence)
        {
            return false;
        }
        const auto* nativeCharacter = scene.characters.GetComponent(character.entity);
        if (nativeCharacter == nullptr)
            return false;

        const XMFLOAT3 shooterPosition = nativeCharacter->GetPositionInterpolated();
        const float dx = memory->lastKnownPosition.x - shooterPosition.x;
        const float dy = memory->lastKnownPosition.y - shooterPosition.y;
        const float dz = memory->lastKnownPosition.z - shooterPosition.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (!std::isfinite(distance) || distance > combat.weapon.maxRange)
            return false;
        if (UsesAmmunition(combat) && combat.magazineAmmo <= 0)
            return false;

        if (UsesAmmunition(combat))
            --combat.magazineAmmo;
        combat.fireCooldownSeconds = std::max(0.02f, combat.weapon.fireIntervalSeconds);
        ++combat.shotSequence;
        ++combat.shotsFired;
        ++state.shotsFired;

        result.fired = true;
        result.distance = distance;
        result.hitChance = ComputeCombatHitChance(character.tuning, combat.weapon, distance);
        result.hit = DeterministicCombatUnit(
            combat.characterId, combat.shotSequence) < result.hitChance;

        (void)EmitSoundStimulus(
            perception,
            combat.characterId,
            character.authoring.factionId,
            shooterPosition,
            XMFLOAT3(0.0f, 0.0f, 0.0f),
            combat.weapon.style == bridge::WeaponAiStyle::Melee ? 6.0f : 40.0f,
            combat.weapon.style == bridge::WeaponAiStyle::Melee ? 0.35f : 1.0f,
            2.0f);

        const std::string firedPayload =
            std::string("target=") + RuntimePlayerKnowledgeId +
            ";distance=" + std::to_string(distance) +
            ";hit=" + std::string(result.hit ? "1" : "0") +
            ";ammo=" + std::to_string(combat.magazineAmmo);
        (void)EmitCombatGameplayEvent(
            state,
            emitter,
            {0, "ai.weapon_fired", firedPayload,
             combat.characterId, {}});

        if (!result.hit)
            return true;

        result.damage = combat.weapon.damage;
        bool died = false;
        if (ApplyCombatDamage(
                scene, state, RuntimePlayerKnowledgeId, result.damage, died))
        {
            result.targetDied = died;
            ++combat.shotsHit;
            ++state.shotsHit;
            const std::string damagePayload =
                std::string("target=") + RuntimePlayerKnowledgeId +
                ";damage=" + std::to_string(result.damage) +
                ";health=" + std::to_string(state.playerHealth) +
                ";dead=" + std::string(died ? "1" : "0");
            (void)EmitCombatGameplayEvent(
                state,
                emitter,
                {0, "ai.damage", damagePayload,
                 combat.characterId, {}});
        }
        return true;
    }
}
