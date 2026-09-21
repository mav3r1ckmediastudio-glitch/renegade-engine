#pragma once

#include "RuntimeCharacterDecision.h"
#include "RuntimeCharacterSystem.h"
#include "RuntimeCombatService.h"

#include "renegade/bridge/AnimationService.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace renegade::runtime
{
    enum class CharacterAnimationSemantic : std::uint8_t
    {
        Idle = 0,
        Locomotion,
        Run,
        Attack,
        Reload,
        Hit,
        Death,
        Count,
    };

    inline constexpr std::size_t CharacterAnimationSemanticCount =
        static_cast<std::size_t>(CharacterAnimationSemantic::Count);

    [[nodiscard]] inline const char* CharacterAnimationSemanticName(
        const CharacterAnimationSemantic semantic) noexcept
    {
        switch (semantic)
        {
        case CharacterAnimationSemantic::Idle: return "Idle";
        case CharacterAnimationSemantic::Locomotion: return "Locomotion";
        case CharacterAnimationSemantic::Run: return "Run";
        case CharacterAnimationSemantic::Attack: return "Attack";
        case CharacterAnimationSemantic::Reload: return "Reload";
        case CharacterAnimationSemantic::Hit: return "Hit";
        case CharacterAnimationSemantic::Death: return "Death";
        default: return "Unknown";
        }
    }

    struct RuntimeAnimationClip
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        std::string name;
    };

    struct RuntimeCharacterAnimationRecord
    {
        bridge::StableId characterId;
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        std::array<std::vector<RuntimeAnimationClip>, CharacterAnimationSemanticCount> clips;
        wi::ecs::Entity activeClip = wi::ecs::INVALID_ENTITY;
        CharacterAnimationSemantic activeSemantic = CharacterAnimationSemantic::Idle;
        std::string resolvedClipName;
        std::string lastRequest;
        std::uint64_t variantSequence = 0;
        std::uint64_t observedShots = 0;
        std::uint64_t observedReloads = 0;
        std::uint64_t observedDamage = 0;
        float observedHealth = 0.0f;
        std::uint64_t playbackRequests = 0;
        std::uint64_t missingRequests = 0;
    };

    struct RuntimeCharacterAnimationState
    {
        std::vector<RuntimeCharacterAnimationRecord> characters;
        std::uint64_t playbackRequests = 0;
        std::uint64_t missingRequests = 0;
    };

    [[nodiscard]] inline std::size_t CharacterAnimationIndex(
        const CharacterAnimationSemantic semantic) noexcept
    {
        return static_cast<std::size_t>(semantic);
    }

    [[nodiscard]] inline std::string NormalizeAnimationClipName(
        std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](const unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        return value;
    }

    [[nodiscard]] inline bool AnimationNameContains(
        const std::string& name,
        const std::initializer_list<const char*> tokens) noexcept
    {
        for (const char* token : tokens)
        {
            if (name.find(token) != std::string::npos)
                return true;
        }
        return false;
    }
    [[nodiscard]] inline CharacterAnimationSemantic InferCharacterAnimationSemantic(
        const std::string& nativeName) noexcept
    {
        const std::string name = NormalizeAnimationClipName(nativeName);
        // The order is deliberate: names such as "death_hit" must remain a
        // death action, and "attack_reload" must never be mistaken for a loop.
        if (AnimationNameContains(name, {"death", "dead", "die", "dying"}))
            return CharacterAnimationSemantic::Death;
        if (AnimationNameContains(name, {"reload"}))
            return CharacterAnimationSemantic::Reload;
        if (AnimationNameContains(name, {"hit", "hurt", "damage", "stagger"}))
            return CharacterAnimationSemantic::Hit;
        if (AnimationNameContains(name, {
                "attack", "fire", "shoot", "punch", "kick", "claw", "bite",
                "swing", "slash", "swipe", "swiping", "melee"}))
        {
            return CharacterAnimationSemantic::Attack;
        }
        if (AnimationNameContains(name, {"run", "sprint", "jog"}))
            return CharacterAnimationSemantic::Run;
        if (AnimationNameContains(name, {
                "walk", "move", "locomotion", "strafe"}))
        {
            return CharacterAnimationSemantic::Locomotion;
        }
        return CharacterAnimationSemantic::Idle;
    }

    [[nodiscard]] inline RuntimeCharacterAnimationRecord* FindCharacterAnimation(
        RuntimeCharacterAnimationState& state,
        const bridge::StableId& characterId) noexcept
    {
        const auto iterator = std::lower_bound(
            state.characters.begin(), state.characters.end(), characterId,
            [](const RuntimeCharacterAnimationRecord& record,
                const bridge::StableId& value)
            {
                return record.characterId < value;
            });
        return iterator != state.characters.end() &&
            iterator->characterId == characterId
            ? &*iterator
            : nullptr;
    }

    [[nodiscard]] inline bool InitializeRuntimeCharacterAnimations(
        const wi::scene::Scene& scene,
        const RuntimeCharacterSystemState& characters,
        const RuntimeCombatState& combat,
        RuntimeCharacterAnimationState& state,
        std::string& error)
    {
        RuntimeCharacterAnimationState candidate;
        candidate.characters.reserve(characters.characters.size());
        for (const auto& character : characters.characters)
        {
            const auto* combatRecord = FindCharacterCombat(
                combat, character.stableEntityId);
            if (combatRecord == nullptr)
            {
                state = {};
                error = "AI-06 animation setup is missing its AI-05 combat record.";
                return false;
            }

            RuntimeCharacterAnimationRecord record;
            record.characterId = character.stableEntityId;
            record.entity = character.entity;
            record.observedShots = combatRecord->shotsFired;
            record.observedReloads = combatRecord->reloads;
            record.observedDamage = combatRecord->damageTaken;
            record.observedHealth = combatRecord->health;

            for (const auto& clip : bridge::CollectAnimationClips(
                     scene, character.entity, true))
            {
                if (clip.entity == wi::ecs::INVALID_ENTITY)
                    continue;
                record.clips[CharacterAnimationIndex(
                    InferCharacterAnimationSemantic(clip.name))]
                    .push_back({clip.entity, clip.name});
            }

            for (auto& variants : record.clips)
            {
                std::sort(variants.begin(), variants.end(),
                    [](const RuntimeAnimationClip& lhs,
                        const RuntimeAnimationClip& rhs)
                    {
                        if (lhs.name != rhs.name)
                            return lhs.name < rhs.name;
                        return lhs.entity < rhs.entity;
                    });
            }
            candidate.characters.push_back(std::move(record));
        }

        std::sort(candidate.characters.begin(), candidate.characters.end(),
            [](const RuntimeCharacterAnimationRecord& lhs,
                const RuntimeCharacterAnimationRecord& rhs)
            {
                return lhs.characterId < rhs.characterId;
            });
        state = std::move(candidate);
        error.clear();
        return true;
    }

    inline void ResetRuntimeCharacterAnimations(
        RuntimeCharacterAnimationState& state) noexcept
    {
        state = {};
    }
    [[nodiscard]] inline bool RequestCharacterAnimation(
        wi::scene::Scene& scene,
        RuntimeCharacterAnimationState& state,
        RuntimeCharacterAnimationRecord& record,
        const CharacterAnimationSemantic semantic) noexcept
    {
        record.lastRequest = CharacterAnimationSemanticName(semantic);
        const auto* variants =
            &record.clips[CharacterAnimationIndex(semantic)];
        // Run is optional in an authored asset: fall back to walk without
        // suppressing Chase/Flee animation when no running clip is present.
        if (semantic == CharacterAnimationSemantic::Run && variants->empty())
            variants = &record.clips[CharacterAnimationIndex(CharacterAnimationSemantic::Locomotion)];
        if (variants->empty())
        {
            record.resolvedClipName.clear();
            ++record.missingRequests;
            ++state.missingRequests;
            return false;
        }

        const RuntimeAnimationClip& next = (*variants)[
            record.variantSequence++ % variants->size()];
        auto* active = scene.animations.GetComponent(record.activeClip);
        if (record.activeClip == next.entity &&
            record.activeSemantic == semantic &&
            active != nullptr && active->IsPlaying())
        {
            return true;
        }

        if (record.activeClip != wi::ecs::INVALID_ENTITY &&
            record.activeClip != next.entity)
        {
            (void)bridge::StopAnimation(scene, record.activeClip);
        }

        auto* animation = scene.animations.GetComponent(next.entity);
        if (animation == nullptr)
        {
            record.resolvedClipName.clear();
            ++record.missingRequests;
            ++state.missingRequests;
            return false;
        }

        if (semantic == CharacterAnimationSemantic::Idle ||
            semantic == CharacterAnimationSemantic::Locomotion ||
            semantic == CharacterAnimationSemantic::Run)
        {
            animation->SetLooped(true);
        }
        else
        {
            animation->SetPlayOnce();
        }
        if (!bridge::PlayAnimation(scene, next.entity, true))
        {
            record.resolvedClipName.clear();
            ++record.missingRequests;
            ++state.missingRequests;
            return false;
        }

        record.activeClip = next.entity;
        record.activeSemantic = semantic;
        record.resolvedClipName = next.name;
        ++record.playbackRequests;
        ++state.playbackRequests;
        return true;
    }

    inline void UpdateRuntimeCharacterAnimations(
        wi::scene::Scene& scene,
        const RuntimeCharacterSystemState& characters,
        const RuntimeCharacterDecisionState& decisions,
        const RuntimeCombatState& combat,
        RuntimeCharacterAnimationState& state) noexcept
    {
        if (state.characters.size() != characters.characters.size() ||
            decisions.characters.size() != characters.characters.size() ||
            combat.characters.size() != characters.characters.size())
        {
            return;
        }

        for (const auto& character : characters.characters)
        {
            auto* record = FindCharacterAnimation(
                state, character.stableEntityId);
            const auto* decision = FindCharacterDecision(
                decisions, character.stableEntityId);
            const auto* combatRecord = FindCharacterCombat(
                combat, character.stableEntityId);
            if (record == nullptr || decision == nullptr ||
                combatRecord == nullptr)
            {
                continue;
            }

            CharacterAnimationSemantic requested =
                CharacterAnimationSemantic::Idle;
            bool requestPlayback = false;
            if (combatRecord->dead || decision->intent == CharacterIntent::Dead)
            {
                requested = CharacterAnimationSemantic::Death;
                requestPlayback = record->activeSemantic != requested;
            }
            else if (combatRecord->damageTaken != record->observedDamage ||
                combatRecord->health + 0.01f < record->observedHealth)
            {
                requested = CharacterAnimationSemantic::Hit;
                requestPlayback = true;
            }
            else if (combatRecord->shotsFired != record->observedShots)
            {
                requested = CharacterAnimationSemantic::Attack;
                requestPlayback = true;
            }
            else if (combatRecord->reloads != record->observedReloads)
            {
                requested = CharacterAnimationSemantic::Reload;
                requestPlayback = true;
            }
            else if ((record->activeSemantic == CharacterAnimationSemantic::Attack ||
                record->activeSemantic == CharacterAnimationSemantic::Reload ||
                record->activeSemantic == CharacterAnimationSemantic::Hit) &&
                scene.animations.GetComponent(record->activeClip) != nullptr &&
                scene.animations.GetComponent(record->activeClip)->IsPlaying() &&
                scene.animations.GetComponent(record->activeClip)->IsPlayingOnce())
            {
                // Finish authored one-shot actions before choosing Idle/Run.
                requested = record->activeSemantic;
                requestPlayback = false;
            }
            else if (decision->intent == CharacterIntent::Chase ||
                decision->intent == CharacterIntent::Retreat ||
                decision->intent == CharacterIntent::Flee)
            {
                requested = CharacterAnimationSemantic::Run;
                requestPlayback = record->activeSemantic != requested;
            }
            else if (decision->intent == CharacterIntent::Patrol ||
                decision->intent == CharacterIntent::Investigate ||
                decision->intent == CharacterIntent::Search)
            {
                requested = CharacterAnimationSemantic::Locomotion;
                requestPlayback = record->activeSemantic != requested;
            }
            else
            {
                requested = CharacterAnimationSemantic::Idle;
                requestPlayback = record->activeSemantic != requested;
            }

            if (requestPlayback)
                (void)RequestCharacterAnimation(scene, state, *record, requested);

            record->observedShots = combatRecord->shotsFired;
            record->observedReloads = combatRecord->reloads;
            record->observedDamage = combatRecord->damageTaken;
            record->observedHealth = combatRecord->health;
        }
    }
}
