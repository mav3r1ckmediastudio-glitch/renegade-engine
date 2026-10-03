#pragma once

#include "RuntimePlayerViewRig.h"

#include "renegade/bridge/AnimationService.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <string>
#include <vector>

namespace renegade::runtime
{
    struct RuntimePlayerViewAnimationClip
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        std::string name;
    };

    struct RuntimePlayerViewAnimationState
    {
        std::array<std::vector<RuntimePlayerViewAnimationClip>, 3> clips;
        wi::ecs::Entity activeClip = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity outgoingClip = wi::ecs::INVALID_ENTITY;
        PlayerViewAction activeAction = PlayerViewAction::Idle;
        std::string resolvedClipName;
        float blendElapsed = 0.0f;
        float blendDuration = 0.20f;
        bool initialized = false;
    };

    [[nodiscard]] inline std::size_t PlayerViewActionIndex(
        const PlayerViewAction action) noexcept
    {
        switch (action)
        {
        case PlayerViewAction::Walk: return 1;
        case PlayerViewAction::Sprint: return 2;
        case PlayerViewAction::Idle:
        default:
            return 0;
        }
    }

    [[nodiscard]] inline std::string NormalizePlayerViewAnimationName(
        std::string value)
    {
        std::transform(
            value.begin(), value.end(), value.begin(),
            [](const unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        return value;
    }

    [[nodiscard]] inline bool PlayerViewAnimationNameContains(
        const std::string& value,
        const std::initializer_list<const char*> tokens) noexcept
    {
        for (const char* token : tokens)
        {
            if (value.find(token) != std::string::npos)
                return true;
        }
        return false;
    }

    [[nodiscard]] inline bool ResolvePlayerViewAnimationAction(
        const std::string& nativeName,
        PlayerViewAction& action)
    {
        const std::string name =
            NormalizePlayerViewAnimationName(nativeName);
        if (PlayerViewAnimationNameContains(
                name, {"run", "sprint", "jog"}))
        {
            action = PlayerViewAction::Sprint;
            return true;
        }
        if (PlayerViewAnimationNameContains(
                name, {"walk", "move", "locomotion", "strafe"}))
        {
            action = PlayerViewAction::Walk;
            return true;
        }
        if (PlayerViewAnimationNameContains(
                name, {"idle", "breath", "rest", "stand"}))
        {
            action = PlayerViewAction::Idle;
            return true;
        }
        return false;
    }

    [[nodiscard]] inline bool ResolveAuthoredPlayerViewAnimationAction(
        const std::string& authoredAction,
        PlayerViewAction& action) noexcept
    {
        if (authoredAction == "Idle")
        {
            action = PlayerViewAction::Idle;
            return true;
        }
        if (authoredAction == "Walk" ||
            authoredAction == "Locomotion")
        {
            action = PlayerViewAction::Walk;
            return true;
        }
        if (authoredAction == "Run" ||
            authoredAction == "Sprint")
        {
            action = PlayerViewAction::Sprint;
            return true;
        }
        return false;
    }

    [[nodiscard]] inline bool MatchingPlayerViewAnimationChannels(
        const wi::scene::AnimationComponent& first,
        const wi::scene::AnimationComponent& second) noexcept
    {
        if (first.channels.size() != second.channels.size())
            return false;

        for (const auto& channel : first.channels)
        {
            if (channel.GetPathDataType() ==
                wi::scene::AnimationComponent::AnimationChannel::
                    PathDataType::Event)
            {
                return false;
            }

            if (std::none_of(
                    second.channels.begin(),
                    second.channels.end(),
                    [&channel](const auto& other)
                    {
                        return other.target == channel.target &&
                            other.path == channel.path;
                    }))
            {
                return false;
            }
        }
        return true;
    }

    inline void ApplyPlayerViewAnimationBlend(
        wi::scene::Scene& scene,
        const RuntimePlayerViewAnimationState& state,
        const float activeWeight,
        const float outgoingWeight) noexcept
    {
        struct WeightedClip
        {
            wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
            float weight = 0.0f;
        };

        std::array<WeightedClip, 2> weighted = {{
            {state.activeClip, activeWeight},
            {state.outgoingClip, outgoingWeight},
        }};
        std::sort(
            weighted.begin(), weighted.end(),
            [&scene](const WeightedClip& left, const WeightedClip& right)
            {
                return scene.animations.GetIndex(left.entity) <
                    scene.animations.GetIndex(right.entity);
            });

        float cumulative = 0.0f;
        for (const auto& clip : weighted)
        {
            auto* animation = scene.animations.GetComponent(clip.entity);
            if (animation == nullptr)
                continue;

            cumulative += clip.weight;
            animation->amount =
                clip.weight > 0.0f && cumulative > 0.0f
                ? clip.weight / cumulative
                : 0.0f;
        }
    }

    inline void ResetRuntimePlayerViewAnimations(
        wi::scene::Scene& scene,
        RuntimePlayerViewAnimationState& state) noexcept
    {
        for (const auto& variants : state.clips)
        {
            for (const auto& clip : variants)
            {
                (void)bridge::StopAnimation(scene, clip.entity);
                if (auto* animation =
                        scene.animations.GetComponent(clip.entity))
                {
                    animation->amount = 0.0f;
                }
            }
        }
        state = {};
    }

    [[nodiscard]] inline bool InitializeRuntimePlayerViewAnimations(
        wi::scene::Scene& scene,
        const RuntimePlayerViewRigState& rig,
        RuntimePlayerViewAnimationState& state,
        std::string& error)
    {
        ResetRuntimePlayerViewAnimations(scene, state);
        error.clear();

        if (rig.viewModelRoot == wi::ecs::INVALID_ENTITY ||
            !scene.transforms.Contains(rig.viewModelRoot))
        {
            error =
                "Player View animation setup requires a loaded view-model root.";
            return false;
        }

        const auto available =
            bridge::CollectAnimationClips(scene, rig.viewModelRoot, true);
        const bool hasAuthoredActions = std::any_of(
            available.begin(), available.end(),
            [&scene](const auto& clip)
            {
                const auto* metadata =
                    scene.metadatas.GetComponent(clip.entity);
                return metadata != nullptr &&
                    metadata->string_values.has(
                        bridge::CreatorCharacterAnimationActionMetadataKey);
            });

        for (const auto& clip : available)
        {
            if (clip.entity == wi::ecs::INVALID_ENTITY)
                continue;

            PlayerViewAction action = PlayerViewAction::Idle;
            bool accepted = false;
            if (hasAuthoredActions)
            {
                const auto* metadata =
                    scene.metadatas.GetComponent(clip.entity);
                if (metadata != nullptr &&
                    metadata->string_values.has(
                        bridge::CreatorCharacterAnimationActionMetadataKey))
                {
                    accepted = ResolveAuthoredPlayerViewAnimationAction(
                        metadata->string_values.get(
                            bridge::CreatorCharacterAnimationActionMetadataKey),
                        action);
                }
            }
            else
            {
                accepted =
                    ResolvePlayerViewAnimationAction(clip.name, action);
            }

            if (!accepted)
                continue;

            auto* animation =
                scene.animations.GetComponent(clip.entity);
            if (animation == nullptr)
                continue;

            animation->RootMotionOff();
            animation->SetLooped(true);
            (void)bridge::StopAnimation(scene, clip.entity);
            animation->amount = 0.0f;

            state.clips[PlayerViewActionIndex(action)].push_back(
                {clip.entity, clip.name});
        }

        for (auto& variants : state.clips)
        {
            std::sort(
                variants.begin(), variants.end(),
                [](const RuntimePlayerViewAnimationClip& left,
                    const RuntimePlayerViewAnimationClip& right)
                {
                    if (left.name != right.name)
                        return left.name < right.name;
                    return left.entity < right.entity;
                });
        }

        state.initialized = true;
        return true;
    }

    [[nodiscard]] inline const std::vector<RuntimePlayerViewAnimationClip>*
    ResolvePlayerViewAnimationVariants(
        const RuntimePlayerViewAnimationState& state,
        const PlayerViewAction requested) noexcept
    {
        const auto* variants =
            &state.clips[PlayerViewActionIndex(requested)];

        if (requested == PlayerViewAction::Sprint && variants->empty())
        {
            variants =
                &state.clips[PlayerViewActionIndex(PlayerViewAction::Walk)];
        }

        if (requested != PlayerViewAction::Idle && variants->empty())
        {
            variants =
                &state.clips[PlayerViewActionIndex(PlayerViewAction::Idle)];
        }
        return variants;
    }

    [[nodiscard]] inline bool RequestRuntimePlayerViewAnimation(
        wi::scene::Scene& scene,
        RuntimePlayerViewAnimationState& state,
        const PlayerViewAction requested) noexcept
    {
        if (!state.initialized)
            return false;

        const auto* variants =
            ResolvePlayerViewAnimationVariants(state, requested);
        if (variants == nullptr || variants->empty())
        {
            state.activeAction = requested;
            state.resolvedClipName.clear();
            return false;
        }

        const RuntimePlayerViewAnimationClip& next = variants->front();
        auto* nextAnimation =
            scene.animations.GetComponent(next.entity);
        if (nextAnimation == nullptr)
            return false;

        if (state.activeClip == next.entity)
        {
            state.activeAction = requested;
            state.resolvedClipName = next.name;
            if (!nextAnimation->IsPlaying())
            {
                nextAnimation->RootMotionOff();
                nextAnimation->SetLooped(true);
                nextAnimation->amount = 1.0f;
                return bridge::PlayAnimation(scene, next.entity, true);
            }
            return true;
        }

        wi::ecs::Entity outgoing = wi::ecs::INVALID_ENTITY;
        if (auto* active =
                scene.animations.GetComponent(state.activeClip))
        {
            if (active->IsPlaying() &&
                MatchingPlayerViewAnimationChannels(
                    *active, *nextAnimation))
            {
                outgoing = state.activeClip;
            }
            else
            {
                (void)bridge::StopAnimation(scene, state.activeClip);
                active->amount = 0.0f;
            }
        }

        if (state.outgoingClip != wi::ecs::INVALID_ENTITY &&
            state.outgoingClip != outgoing)
        {
            (void)bridge::StopAnimation(scene, state.outgoingClip);
            if (auto* prior =
                    scene.animations.GetComponent(state.outgoingClip))
            {
                prior->amount = 0.0f;
            }
        }

        state.outgoingClip = outgoing;
        state.activeClip = next.entity;
        state.activeAction = requested;
        state.resolvedClipName = next.name;
        state.blendElapsed = 0.0f;
        state.blendDuration =
            outgoing == wi::ecs::INVALID_ENTITY ? 0.0f : 0.20f;

        nextAnimation->RootMotionOff();
        nextAnimation->SetLooped(true);
        nextAnimation->amount =
            outgoing == wi::ecs::INVALID_ENTITY ? 1.0f : 0.0f;
        if (!bridge::PlayAnimation(scene, next.entity, true))
            return false;

        if (state.blendDuration > 0.0f)
        {
            ApplyPlayerViewAnimationBlend(
                scene, state, 0.0f, 1.0f);
        }
        return true;
    }

    inline void UpdateRuntimePlayerViewAnimations(
        wi::scene::Scene& scene,
        RuntimePlayerViewAnimationState& state,
        const PlayerViewAction requested,
        const float dt) noexcept
    {
        if (!state.initialized)
            return;

        auto* active =
            scene.animations.GetComponent(state.activeClip);
        if (state.activeClip == wi::ecs::INVALID_ENTITY ||
            state.activeAction != requested ||
            active == nullptr || !active->IsPlaying())
        {
            (void)RequestRuntimePlayerViewAnimation(
                scene, state, requested);
        }

        if (state.blendDuration <= 0.0f ||
            state.outgoingClip == wi::ecs::INVALID_ENTITY)
        {
            return;
        }

        if (std::isfinite(dt) && dt > 0.0f)
            state.blendElapsed += dt;

        const float t = std::clamp(
            state.blendElapsed / state.blendDuration,
            0.0f, 1.0f);
        ApplyPlayerViewAnimationBlend(
            scene, state, t, 1.0f - t);

        if (t >= 1.0f)
        {
            (void)bridge::StopAnimation(
                scene, state.outgoingClip);
            if (auto* outgoingAnimation =
                    scene.animations.GetComponent(state.outgoingClip))
            {
                outgoingAnimation->amount = 0.0f;
            }
            state.outgoingClip = wi::ecs::INVALID_ENTITY;
            state.blendElapsed = 0.0f;
            state.blendDuration = 0.20f;
            if (auto* current =
                    scene.animations.GetComponent(state.activeClip))
            {
                current->amount = 1.0f;
            }
        }
    }
}
