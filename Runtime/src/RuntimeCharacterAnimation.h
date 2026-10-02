#pragma once

#include "RuntimeCharacterDecision.h"
#include "RuntimeCharacterSystem.h"
#include "RuntimeCombatService.h"

#include "renegade/bridge/AnimationService.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <limits>
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

    struct RuntimeAnimationBlendClip
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        float weight = 0.0f;
        float startWeight = 0.0f;
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
        std::vector<RuntimeAnimationBlendClip> blendClips;
        float blendElapsed = 0.0f;
        float blendDuration = 0.0f;
        std::uint64_t crossfadeTransitions = 0;
        std::uint64_t incompatibleTransitions = 0;
        wi::ecs::Entity baseIdle = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity replayOutgoing = wi::ecs::INVALID_ENTITY;
        float idleElapsed = 0.0f;
        bool idleVariation = false;
        std::uint64_t idleVariationSequence = 0;
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
    [[nodiscard]] inline bool IsExplicitCharacterIdleName(const std::string& nativeName)
    {
        const std::string name = NormalizeAnimationClipName(nativeName);
        return AnimationNameContains(name, {"idle", "breath", "rest", "stand"});
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

    [[nodiscard]] inline float CharacterAnimationSecondsRemaining(
        const wi::scene::AnimationComponent& animation) noexcept
    {
        const float speed = std::abs(animation.speed);
        return speed > 0.0001f ? std::max(0.0f, animation.end - animation.timer) / speed :
            std::numeric_limits<float>::infinity();
    }

    [[nodiscard]] inline double CharacterIdleMotionScore(
        const wi::scene::Scene& scene, const wi::ecs::Entity entity) noexcept
    {
        const auto* animation = scene.animations.GetComponent(entity);
        if (!animation) return std::numeric_limits<double>::infinity();
        double distance = 0.0;
        for (const auto& channel : animation->channels)
        {
            if (channel.path != wi::scene::AnimationComponent::AnimationChannel::Path::ROTATION ||
                channel.samplerIndex < 0 || size_t(channel.samplerIndex) >= animation->samplers.size()) continue;
            const auto& sampler = animation->samplers[channel.samplerIndex];
            const auto* dataScene = sampler.scene ? static_cast<const wi::scene::Scene*>(sampler.scene) : &scene;
            const auto* data = dataScene->animation_datas.GetComponent(sampler.data);
            if (!data || data->keyframe_times.size() < 2) continue;
            const size_t stride = sampler.mode == wi::scene::AnimationComponent::AnimationSampler::CUBICSPLINE ? 12 : 4;
            const size_t offset = stride == 12 ? 4 : 0;
            if (data->keyframe_data.size() < data->keyframe_times.size() * stride) continue;
            for (size_t i = 1; i < data->keyframe_times.size(); ++i)
            {
                const auto* a = data->keyframe_data.data() + (i - 1) * stride + offset;
                const auto* c = data->keyframe_data.data() + i * stride + offset;
                const double norm = std::sqrt((a[0]*a[0]+a[1]*a[1]+a[2]*a[2]+a[3]*a[3]) *
                    (c[0]*c[0]+c[1]*c[1]+c[2]*c[2]+c[3]*c[3]));
                if (norm > 0.000001)
                    distance += 2.0 * std::acos(std::clamp(std::abs(
                        a[0]*c[0]+a[1]*c[1]+a[2]*c[2]+a[3]*c[3]) / norm, 0.0, 1.0));
            }
        }
        return distance / std::max(0.001f, animation->GetLength());
    }

    // Complete transform coverage in the transient Runtime scene before playback.
    // Native sequential amounts need every contributing clip to write each path.
    inline void CompleteCharacterAnimationCoverage(
        wi::scene::Scene& scene, const RuntimeCharacterAnimationRecord& record)
    {
        using Channel = wi::scene::AnimationComponent::AnimationChannel;
        std::vector<Channel> coverage;
        for (const auto& variants : record.clips)
            for (const auto& clip : variants)
            {
                const auto* animation = scene.animations.GetComponent(clip.entity);
                if (!animation) return;
                for (const auto& channel : animation->channels)
                {
                    if ((channel.path != Channel::Path::TRANSLATION &&
                         channel.path != Channel::Path::ROTATION &&
                         channel.path != Channel::Path::SCALE) ||
                        !scene.transforms.GetComponent(channel.target))
                        return; // Events and non-transform tracks retain the guarded fallback.
                    if (std::none_of(coverage.begin(), coverage.end(), [&channel](const auto& other)
                        { return other.target == channel.target && other.path == channel.path; }))
                        coverage.push_back(channel);
                }
            }
        for (const auto& variants : record.clips)
            for (const auto& clip : variants)
            {
                auto* animation = scene.animations.GetComponent(clip.entity);
                for (auto channel : coverage)
                {
                    if (std::any_of(animation->channels.begin(), animation->channels.end(),
                        [&channel](const auto& other)
                        { return other.target == channel.target && other.path == channel.path; }))
                        continue;
                    const auto* transform = scene.transforms.GetComponent(channel.target);
                    const auto dataEntity = wi::ecs::CreateEntity();
                    auto& data = scene.animation_datas.Create(dataEntity);
                    data.keyframe_times = {animation->start};
                    if (channel.path == Channel::Path::ROTATION)
                        data.keyframe_data = {transform->rotation_local.x, transform->rotation_local.y,
                            transform->rotation_local.z, transform->rotation_local.w};
                    else
                    {
                        const auto value = channel.path == Channel::Path::TRANSLATION ?
                            transform->translation_local : transform->scale_local;
                        data.keyframe_data = {value.x, value.y, value.z};
                    }
                    wi::scene::AnimationComponent::AnimationSampler sampler;
                    sampler.data = dataEntity;
                    sampler.mode = wi::scene::AnimationComponent::AnimationSampler::STEP;
                    channel.samplerIndex = static_cast<int>(animation->samplers.size());
                    channel.retargetIndex = -1; // Values already belong to the destination skeleton.
                    animation->samplers.push_back(sampler);
                    animation->channels.push_back(channel);
                }
            }
    }

    [[nodiscard]] inline bool InitializeRuntimeCharacterAnimations(
        wi::scene::Scene& scene,
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

            const auto available = bridge::CollectAnimationClips(
                scene, character.entity, true);
            const bool hasAuthoredActions = std::any_of(
                available.begin(), available.end(), [&scene](const auto& clip)
                {
                    const auto* metadata = scene.metadatas.GetComponent(clip.entity);
                    return metadata != nullptr && metadata->string_values.has(
                        bridge::CreatorCharacterAnimationActionMetadataKey);
                });
            for (const auto& clip : available)
            {
                if (clip.entity == wi::ecs::INVALID_ENTITY)
                    continue;
                CharacterAnimationSemantic semantic = CharacterAnimationSemantic::Idle;
                if (hasAuthoredActions)
                {
                    const auto* metadata = scene.metadatas.GetComponent(clip.entity);
                    if (metadata == nullptr || !metadata->string_values.has(
                            bridge::CreatorCharacterAnimationActionMetadataKey))
                        continue;
                    const std::string action = metadata->string_values.get(
                        bridge::CreatorCharacterAnimationActionMetadataKey);
                    if (action == "Idle") semantic = CharacterAnimationSemantic::Idle;
                    else if (action == "Walk") semantic = CharacterAnimationSemantic::Locomotion;
                    else if (action == "Run") semantic = CharacterAnimationSemantic::Run;
                    else if (action == "Attack") semantic = CharacterAnimationSemantic::Attack;
                    else if (action == "Reload") semantic = CharacterAnimationSemantic::Reload;
                    else if (action == "Hit") semantic = CharacterAnimationSemantic::Hit;
                    else if (action == "Death") semantic = CharacterAnimationSemantic::Death;
                    else continue; // Unassigned/custom actions cannot become Idle.
                }
                else
                {
                    semantic = InferCharacterAnimationSemantic(clip.name);
                    if (semantic == CharacterAnimationSemantic::Idle &&
                        !IsExplicitCharacterIdleName(clip.name))
                        continue; // Never treat an unnamed bind/turn clip as Idle.
                }
                record.clips[CharacterAnimationIndex(semantic)]
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
            CompleteCharacterAnimationCoverage(scene, record);
            const auto& idles = record.clips[CharacterAnimationIndex(CharacterAnimationSemantic::Idle)];
            if (!idles.empty())
                record.baseIdle = std::min_element(idles.begin(), idles.end(),
                    [&scene](const auto& a, const auto& b)
                    { return CharacterIdleMotionScore(scene, a.entity) < CharacterIdleMotionScore(scene, b.entity); })->entity;
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

    // Native animation amounts blend sequentially in component order. Logical
    // weights are converted to cumulative amounts so the result is independent
    // of whether the incoming clip precedes or follows the outgoing clip.
    inline void ApplyCharacterAnimationBlend(
        wi::scene::Scene& scene, RuntimeCharacterAnimationRecord& record) noexcept
    {
        std::sort(record.blendClips.begin(), record.blendClips.end(),
            [&scene](const auto& a, const auto& b)
            { return scene.animations.GetIndex(a.entity) < scene.animations.GetIndex(b.entity); });
        float cumulative = 0.0f;
        for (const auto& clip : record.blendClips)
        {
            auto* animation = scene.animations.GetComponent(clip.entity);
            if (animation == nullptr) continue;
            cumulative += clip.weight;
            animation->amount = clip.weight > 0.0f ? clip.weight / cumulative : 0.0f;
            // A completed outgoing one-shot still contributes its final pose.
            if (clip.weight > 0.0f && !animation->IsPlaying())
                animation->last_update_time = animation->timer - 1.0f;
        }
    }

    inline void StopCharacterAnimationBlend(
        wi::scene::Scene& scene, RuntimeCharacterAnimationRecord& record) noexcept
    {
        for (const auto& clip : record.blendClips)
        {
            (void)bridge::StopAnimation(scene, clip.entity);
            if (auto* animation = scene.animations.GetComponent(clip.entity))
                animation->amount = 0.0f;
        }
        record.blendClips.clear();
        record.blendElapsed = record.blendDuration = 0.0f;
    }

    [[nodiscard]] inline bool MatchingCharacterAnimationChannels(
        const wi::scene::AnimationComponent& first,
        const wi::scene::AnimationComponent& second) noexcept
    {
        if (first.channels.size() != second.channels.size()) return false;
        for (const auto& channel : first.channels)
        {
            if (channel.GetPathDataType() == wi::scene::AnimationComponent::AnimationChannel::PathDataType::Event)
                return false; // Gameplay events must not fire from fading clips.
            if (std::none_of(second.channels.begin(), second.channels.end(),
                [&channel](const auto& other)
                { return other.target == channel.target && other.path == channel.path; }))
                return false;
        }
        return true;
    }

    inline void BeginCharacterAnimationBlend(
        wi::scene::Scene& scene, RuntimeCharacterAnimationRecord& record,
        const wi::ecs::Entity incoming, const CharacterAnimationSemantic semantic)
    {
        auto* animation = scene.animations.GetComponent(incoming);
        if (animation == nullptr) return;
        bool compatible = !record.blendClips.empty();
        for (const auto& clip : record.blendClips)
        {
            const auto* prior = scene.animations.GetComponent(clip.entity);
            compatible = compatible && prior != nullptr &&
                MatchingCharacterAnimationChannels(*prior, *animation);
        }
        if (!compatible)
        {
            if (!record.blendClips.empty()) ++record.incompatibleTransitions;
            // Events and non-transform coverage retain an immediate switch.
            for (const auto& clip : record.blendClips)
                if (clip.entity != incoming)
                {
                    (void)bridge::StopAnimation(scene, clip.entity);
                    if (auto* prior = scene.animations.GetComponent(clip.entity))
                        prior->amount = 0.0f;
                }
            record.blendClips = {{incoming, 1.0f, 1.0f}};
            record.blendDuration = 0.0f;
        }
        else
        {
            ++record.crossfadeTransitions;
            auto found = std::find_if(record.blendClips.begin(), record.blendClips.end(),
                [incoming](const auto& clip) { return clip.entity == incoming; });
            if (found == record.blendClips.end())
                record.blendClips.push_back({incoming, 0.0f, 0.0f});
            for (auto& clip : record.blendClips) clip.startWeight = clip.weight;
            const bool loop = semantic == CharacterAnimationSemantic::Idle ||
                semantic == CharacterAnimationSemantic::Locomotion ||
                semantic == CharacterAnimationSemantic::Run;
            record.blendDuration = loop ? 0.20f :
                semantic == CharacterAnimationSemantic::Death ? 0.05f :
                semantic == CharacterAnimationSemantic::Attack ? 0.18f : 0.08f;
        }
        record.blendElapsed = 0.0f;
        ApplyCharacterAnimationBlend(scene, record);
    }

    inline void AdvanceCharacterAnimationBlend(
        wi::scene::Scene& scene, RuntimeCharacterAnimationRecord& record,
        const float dt) noexcept
    {
        if (record.blendDuration > 0.0f)
        {
            if (std::isfinite(dt) && dt > 0.0f) record.blendElapsed += dt;
            const float t = std::clamp(record.blendElapsed / record.blendDuration, 0.0f, 1.0f);
            for (auto& clip : record.blendClips)
                clip.weight = clip.startWeight * (1.0f - t) +
                    (clip.entity == record.activeClip ? t : 0.0f);
            if (t >= 1.0f)
            {
                for (const auto& clip : record.blendClips)
                    if (clip.entity != record.activeClip)
                    {
                        (void)bridge::StopAnimation(scene, clip.entity);
                        if (auto* animation = scene.animations.GetComponent(clip.entity))
                            animation->amount = 0.0f;
                    }
                record.blendClips = {{record.activeClip, 1.0f, 1.0f}};
                record.blendDuration = 0.0f;
            }
        }
        ApplyCharacterAnimationBlend(scene, record);
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
        const CharacterAnimationSemantic semantic,
        const bool forceTransition = false,
        const wi::ecs::Entity preferredClip = wi::ecs::INVALID_ENTITY) noexcept
    {
        record.lastRequest = CharacterAnimationSemanticName(semantic);
        auto* active = scene.animations.GetComponent(record.activeClip);
        if (record.activeSemantic == CharacterAnimationSemantic::Death && active != nullptr)
            return semantic == CharacterAnimationSemantic::Death;
        const auto* variants =
            &record.clips[CharacterAnimationIndex(semantic)];
        // Run is optional in an authored asset: fall back to walk without
        // suppressing Chase/Flee animation when no running clip is present.
        if (semantic == CharacterAnimationSemantic::Run && variants->empty())
            variants = &record.clips[CharacterAnimationIndex(CharacterAnimationSemantic::Locomotion)];
        if (variants->empty())
        {
            if (semantic == CharacterAnimationSemantic::Death)
            {
                StopCharacterAnimationBlend(scene, record);
                record.activeClip = wi::ecs::INVALID_ENTITY;
                record.activeSemantic = semantic;
            }
            record.resolvedClipName.clear();
            ++record.missingRequests;
            ++state.missingRequests;
            return false;
        }

        // Repeated requests must not restart an active loop or one-shot.
        if (!forceTransition && record.activeSemantic == semantic && active != nullptr &&
            active->IsPlaying())
            return true;
        const wi::ecs::Entity preferred = preferredClip != wi::ecs::INVALID_ENTITY ? preferredClip :
            semantic == CharacterAnimationSemantic::Idle ? record.baseIdle : wi::ecs::INVALID_ENTITY;
        const auto selected = std::find_if(variants->begin(), variants->end(),
            [preferred](const auto& clip) { return clip.entity == preferred; });
        const RuntimeAnimationClip next = selected != variants->end() ? *selected :
            (*variants)[record.variantSequence++ % variants->size()];
        if (!forceTransition && record.activeClip == next.entity && active != nullptr && active->IsPlaying())
        {
            // Optional Run falls back to the same Walk without restarting it.
            record.activeSemantic = semantic;
            record.resolvedClipName = next.name;
            return true;
        }

        if (record.activeClip == next.entity && active != nullptr && forceTransition)
        {
            const auto outgoing = *active;
            if (record.replayOutgoing == wi::ecs::INVALID_ENTITY)
                record.replayOutgoing = wi::ecs::CreateEntity();
            auto* copy = scene.animations.GetComponent(record.replayOutgoing);
            if (!copy) copy = &scene.animations.Create(record.replayOutgoing);
            *copy = outgoing;
            for (auto& clip : record.blendClips)
                if (clip.entity == next.entity) clip.entity = record.replayOutgoing;
        }
        auto* animation = scene.animations.GetComponent(next.entity);
        if (animation == nullptr)
        {
            record.resolvedClipName.clear();
            ++record.missingRequests;
            ++state.missingRequests;
            return false;
        }

        record.idleVariation = semantic == CharacterAnimationSemantic::Idle &&
            record.baseIdle != wi::ecs::INVALID_ENTITY && next.entity != record.baseIdle;
        if ((semantic == CharacterAnimationSemantic::Idle && !record.idleVariation) ||
            semantic == CharacterAnimationSemantic::Locomotion ||
            semantic == CharacterAnimationSemantic::Run)
        {
            animation->SetLooped(true);
        }
        else
        {
            animation->SetPlayOnce();
        }
        const bool alreadyContributing = std::any_of(record.blendClips.begin(), record.blendClips.end(),
            [&next](const auto& clip) { return clip.entity == next.entity && clip.weight > 0.0f; });
        // The Character controller owns world movement, including during a fade.
        animation->RootMotionOff();
        if (!bridge::PlayAnimation(scene, next.entity, !alreadyContributing || animation->IsPlayingOnce()))
        {
            record.resolvedClipName.clear();
            ++record.missingRequests;
            ++state.missingRequests;
            return false;
        }

        BeginCharacterAnimationBlend(scene, record, next.entity, semantic);
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
        RuntimeCharacterAnimationState& state,
        const float dt = 0.0f) noexcept
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
            bool forceTransition = false;
            wi::ecs::Entity preferredClip = wi::ecs::INVALID_ENTITY;
            const auto* activeAnimation = scene.animations.GetComponent(record->activeClip);
            const float remaining = activeAnimation ? CharacterAnimationSecondsRemaining(*activeAnimation) : 0.0f;
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
            else if (record->activeSemantic == CharacterAnimationSemantic::Hit && remaining > 0.08f &&
                scene.animations.GetComponent(record->activeClip) != nullptr &&
                scene.animations.GetComponent(record->activeClip)->IsPlaying() &&
                scene.animations.GetComponent(record->activeClip)->IsPlayingOnce())
            {
                requested = CharacterAnimationSemantic::Hit;
                requestPlayback = false;
            }
            else if (combatRecord->shotsFired != record->observedShots)
            {
                requested = CharacterAnimationSemantic::Attack;
                requestPlayback = true;
                forceTransition = combatRecord->weapon.style == bridge::WeaponAiStyle::Melee;
            }
            else if (combatRecord->reloads != record->observedReloads)
            {
                requested = CharacterAnimationSemantic::Reload;
                requestPlayback = true;
            }
            else if ((record->activeSemantic == CharacterAnimationSemantic::Attack ||
                record->activeSemantic == CharacterAnimationSemantic::Reload ||
                record->activeSemantic == CharacterAnimationSemantic::Hit ||
                (record->idleVariation && (decision->intent == CharacterIntent::Idle ||
                    (decision->intent == CharacterIntent::Guard && !decision->hasGoal)))) &&
                remaining > (record->activeSemantic == CharacterAnimationSemantic::Attack &&
                    decision->intent == CharacterIntent::Attack && !combat.playerDead ? 0.18f : 0.20f) && scene.animations.GetComponent(record->activeClip) != nullptr &&
                scene.animations.GetComponent(record->activeClip)->IsPlaying() &&
                scene.animations.GetComponent(record->activeClip)->IsPlayingOnce())
            {
                // Finish authored one-shot actions before choosing Idle/Run.
                requested = record->activeSemantic;
                requestPlayback = false;
            }
            else if (decision->intent == CharacterIntent::Guard && decision->hasGoal && !decision->arrived)
            {
                requested = CharacterAnimationSemantic::Locomotion;
                requestPlayback = record->activeSemantic != requested;
            }
            else if (decision->intent == CharacterIntent::Chase ||
                decision->intent == CharacterIntent::Retreat ||
                decision->intent == CharacterIntent::Flee)
            {
                requested = CharacterAnimationSemantic::Run;
                requestPlayback = record->activeSemantic != requested;
            }
            else if ((decision->intent == CharacterIntent::Wander &&
                      decision->hasGoal && !decision->arrived) ||
                decision->intent == CharacterIntent::Patrol ||
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

            const bool quiet = requested == CharacterAnimationSemantic::Idle &&
                (decision->intent == CharacterIntent::Idle ||
                 (decision->intent == CharacterIntent::Guard && !decision->hasGoal));
            if (quiet && record->baseIdle != wi::ecs::INVALID_ENTITY)
            {
                if (record->idleVariation)
                {
                    if (remaining <= 0.20f || !activeAnimation || !activeAnimation->IsPlaying())
                    {
                        requestPlayback = forceTransition = true;
                        preferredClip = record->baseIdle;
                    }
                }
                else if (record->activeClip == record->baseIdle)
                {
                    if (dt > 0 && std::isfinite(dt)) record->idleElapsed += dt;
                    const auto& idles = record->clips[CharacterAnimationIndex(CharacterAnimationSemantic::Idle)];
                    if (record->idleElapsed >= 12.0f && idles.size() > 1)
                    {
                        do { preferredClip = idles[record->idleVariationSequence++ % idles.size()].entity; }
                        while (preferredClip == record->baseIdle);
                        requestPlayback = forceTransition = true;
                        record->idleElapsed = 0.0f;
                    }
                }
            }
            else record->idleElapsed = 0.0f;

            // The default enum is Idle even when no native clip has ever
            // started. Likewise a stopped loop must restart without requiring
            // a new AI intent. Previously both conditions silently glided.
            if (!requestPlayback &&
                (requested == CharacterAnimationSemantic::Idle ||
                 requested == CharacterAnimationSemantic::Locomotion ||
                 requested == CharacterAnimationSemantic::Run))
            {
                const auto* active = scene.animations.GetComponent(record->activeClip);
                const auto& variants = record->clips[CharacterAnimationIndex(requested)];
                const bool fallbackWalk = requested == CharacterAnimationSemantic::Run &&
                    !record->clips[CharacterAnimationIndex(
                        CharacterAnimationSemantic::Locomotion)].empty();
                requestPlayback = (!variants.empty() || fallbackWalk) &&
                    (active == nullptr || !active->IsPlaying());
            }
            // No authored Idle means no Idle. Never substitute the source bind pose,
            // and do not count a missing optional loop as a failure every frame.
            if (requestPlayback && requested == CharacterAnimationSemantic::Idle &&
                record->clips[CharacterAnimationIndex(requested)].empty())
            {
                StopCharacterAnimationBlend(scene, *record);
                record->activeClip = wi::ecs::INVALID_ENTITY;
                record->activeSemantic = requested;
                record->resolvedClipName.clear();
                record->lastRequest = "Idle (unassigned)";
                requestPlayback = false;
            }
            if (requestPlayback)
                (void)RequestCharacterAnimation(scene, state, *record, requested, forceTransition, preferredClip);

            AdvanceCharacterAnimationBlend(scene, *record, dt);
            record->observedShots = combatRecord->shotsFired;
            record->observedReloads = combatRecord->reloads;
            record->observedDamage = combatRecord->damageTaken;
            record->observedHealth = combatRecord->health;
        }
    }
}
