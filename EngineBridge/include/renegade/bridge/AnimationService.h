#pragma once

#include <WickedEngine.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    enum class AnimationPlaybackMode
    {
        Loop,
        PingPong,
        PlayOnce,
    };

    struct AnimationClipInfo
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        std::string name;
        std::size_t channelCount = 0;
        float start = 0.0f;
        float end = 0.0f;
        float timer = 0.0f;
        float speed = 1.0f;
        float amount = 1.0f;
        bool playing = false;
        bool rootMotion = false;
        AnimationPlaybackMode mode = AnimationPlaybackMode::Loop;
    };

    struct AnimationAuthoredState
    {
        float start = 0.0f;
        float end = 0.0f;
        float speed = 1.0f;
        float amount = 1.0f;
        bool rootMotion = false;
        AnimationPlaybackMode mode = AnimationPlaybackMode::Loop;
    };

    namespace animation_detail
    {
        inline constexpr float Epsilon = 0.00001f;

        inline float FiniteOr(const float value, const float fallback) noexcept
        {
            return std::isfinite(value) ? value : fallback;
        }

        inline bool NearlyEqual(const float left, const float right) noexcept
        {
            return std::abs(left - right) <= Epsilon;
        }

        inline bool IsAncestorOrSelf(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity ancestor,
            wi::ecs::Entity entity) noexcept
        {
            if (ancestor == wi::ecs::INVALID_ENTITY || entity == wi::ecs::INVALID_ENTITY)
                return false;
            for (std::size_t depth = 0; depth < 512; ++depth)
            {
                if (entity == ancestor)
                    return true;
                const auto* hierarchy = scene.hierarchy.GetComponent(entity);
                if (hierarchy == nullptr || hierarchy->parentID == wi::ecs::INVALID_ENTITY || hierarchy->parentID == entity)
                    break;
                entity = hierarchy->parentID;
            }
            return false;
        }

        inline bool ClipTouchesSelection(
            const wi::scene::Scene& scene,
            const wi::scene::AnimationComponent& animation,
            const wi::ecs::Entity selected) noexcept
        {
            if (selected == wi::ecs::INVALID_ENTITY)
                return false;
            for (const auto& channel : animation.channels)
            {
                if (channel.target == wi::ecs::INVALID_ENTITY)
                    continue;
                if (IsAncestorOrSelf(scene, selected, channel.target) ||
                    IsAncestorOrSelf(scene, channel.target, selected))
                {
                    return true;
                }
            }
            return false;
        }
    }

    [[nodiscard]] inline AnimationPlaybackMode AnimationMode(
        const wi::scene::AnimationComponent& animation) noexcept
    {
        if (animation.IsPingPong())
            return AnimationPlaybackMode::PingPong;
        if (animation.IsPlayingOnce())
            return AnimationPlaybackMode::PlayOnce;
        return AnimationPlaybackMode::Loop;
    }

    [[nodiscard]] inline AnimationAuthoredState CaptureAnimationAuthoredState(
        const wi::scene::AnimationComponent& animation) noexcept
    {
        AnimationAuthoredState state;
        state.start = animation.start;
        state.end = animation.end;
        state.speed = animation.speed;
        state.amount = animation.amount;
        state.rootMotion = animation.IsRootMotion();
        state.mode = AnimationMode(animation);
        return state;
    }

    [[nodiscard]] inline AnimationAuthoredState SanitizeAnimationAuthoredState(
        const AnimationAuthoredState& state) noexcept
    {
        AnimationAuthoredState result = state;
        result.start = animation_detail::FiniteOr(result.start, 0.0f);
        result.end = animation_detail::FiniteOr(result.end, result.start);
        if (result.end < result.start)
            std::swap(result.start, result.end);
        result.speed = std::clamp(animation_detail::FiniteOr(result.speed, 1.0f), -16.0f, 16.0f);
        result.amount = std::clamp(animation_detail::FiniteOr(result.amount, 1.0f), 0.0f, 1.0f);
        return result;
    }

    [[nodiscard]] inline bool HasAnimationAuthoredStateChange(
        const AnimationAuthoredState& before,
        const AnimationAuthoredState& after) noexcept
    {
        const auto left = SanitizeAnimationAuthoredState(before);
        const auto right = SanitizeAnimationAuthoredState(after);
        return !animation_detail::NearlyEqual(left.start, right.start) ||
            !animation_detail::NearlyEqual(left.end, right.end) ||
            !animation_detail::NearlyEqual(left.speed, right.speed) ||
            !animation_detail::NearlyEqual(left.amount, right.amount) ||
            left.rootMotion != right.rootMotion || left.mode != right.mode;
    }

    inline void ApplyAnimationAuthoredState(
        wi::scene::AnimationComponent& animation,
        const AnimationAuthoredState& state) noexcept
    {
        const auto safe = SanitizeAnimationAuthoredState(state);
        animation.start = safe.start;
        animation.end = safe.end;
        animation.speed = safe.speed;
        animation.amount = safe.amount;
        if (animation.timer < safe.start || animation.timer > safe.end)
        {
            animation.timer = safe.start;
            animation.last_update_time = animation.timer;
        }
        if (safe.rootMotion)
            animation.RootMotionOn();
        else
            animation.RootMotionOff();
        switch (safe.mode)
        {
        case AnimationPlaybackMode::PingPong:
            animation.SetPingPong(true);
            break;
        case AnimationPlaybackMode::PlayOnce:
            animation.SetPlayOnce();
            break;
        case AnimationPlaybackMode::Loop:
        default:
            animation.SetLooped(true);
            break;
        }
    }

    class SetAnimationAuthoredStateCommand final : public ICommand
    {
    public:
        SetAnimationAuthoredStateCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const AnimationAuthoredState& state)
            : scene_(&scene), entity_(entity), after_(SanitizeAnimationAuthoredState(state))
        {
            if (const auto* animation = scene.animations.GetComponent(entity))
                before_ = CaptureAnimationAuthoredState(*animation);
        }

        bool Execute() override
        {
            return HasAnimationAuthoredStateChange(before_, after_) && Apply(after_);
        }

        void Undo() override
        {
            (void)Apply(before_);
        }

    private:
        bool Apply(const AnimationAuthoredState& state) noexcept
        {
            if (scene_ == nullptr)
                return false;
            auto* animation = scene_->animations.GetComponent(entity_);
            if (animation == nullptr)
                return false;
            ApplyAnimationAuthoredState(*animation, state);
            return true;
        }

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        AnimationAuthoredState before_;
        AnimationAuthoredState after_;
    };

    [[nodiscard]] inline std::vector<AnimationClipInfo> CollectAnimationClips(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity selected,
        const bool relatedOnly = true)
    {
        std::vector<AnimationClipInfo> result;
        result.reserve(scene.animations.GetCount());
        for (std::size_t index = 0; index < scene.animations.GetCount(); ++index)
        {
            const auto entity = scene.animations.GetEntity(index);
            const auto& animation = scene.animations[index];
            if (relatedOnly && !animation_detail::ClipTouchesSelection(scene, animation, selected))
                continue;

            AnimationClipInfo info;
            info.entity = entity;
            if (const auto* name = scene.names.GetComponent(entity); name != nullptr && !name->name.empty())
                info.name = name->name;
            else
                info.name = "Animation " + std::to_string(index + 1);
            info.channelCount = animation.channels.size();
            info.start = animation.start;
            info.end = animation.end;
            info.timer = animation.timer;
            info.speed = animation.speed;
            info.amount = animation.amount;
            info.playing = animation.IsPlaying();
            info.rootMotion = animation.IsRootMotion();
            info.mode = AnimationMode(animation);
            result.push_back(std::move(info));
        }
        return result;
    }

    inline bool PlayAnimation(wi::scene::Scene& scene, const wi::ecs::Entity entity, const bool fromStart = false) noexcept
    {
        auto* animation = scene.animations.GetComponent(entity);
        if (animation == nullptr)
            return false;
        if (fromStart)
        {
            animation->timer = animation->start;
            animation->last_update_time = animation->timer;
        }
        animation->Play();
        return true;
    }

    inline bool PauseAnimation(wi::scene::Scene& scene, const wi::ecs::Entity entity) noexcept
    {
        auto* animation = scene.animations.GetComponent(entity);
        if (animation == nullptr)
            return false;
        animation->Pause();
        return true;
    }

    inline bool StopAnimation(wi::scene::Scene& scene, const wi::ecs::Entity entity) noexcept
    {
        auto* animation = scene.animations.GetComponent(entity);
        if (animation == nullptr)
            return false;
        animation->Stop();
        animation->timer = animation->start;
        animation->last_update_time = animation->timer;
        return true;
    }

    inline bool ScrubAnimation(wi::scene::Scene& scene, const wi::ecs::Entity entity, const float timer) noexcept
    {
        auto* animation = scene.animations.GetComponent(entity);
        if (animation == nullptr)
            return false;
        animation->timer = std::clamp(animation_detail::FiniteOr(timer, animation->start), animation->start, animation->end);
        animation->last_update_time = animation->timer;
        return true;
    }
}
