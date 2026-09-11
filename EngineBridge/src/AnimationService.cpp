#include "renegade/bridge/AnimationService.h"

#include "renegade/bridge/SceneComponentService.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace renegade::bridge
{
    namespace
    {
        [[nodiscard]] bool IsOwnedByRoot(
            const wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            wi::ecs::Entity root) noexcept
        {
            if (entity == wi::ecs::INVALID_ENTITY ||
                root == wi::ecs::INVALID_ENTITY)
            {
                return false;
            }

            wi::ecs::Entity current = entity;
            for (std::size_t depth = 0; depth < 512; ++depth)
            {
                if (current == root)
                    return true;
                const auto* hierarchy = scene.hierarchy.GetComponent(current);
                if (hierarchy == nullptr ||
                    hierarchy->parentID == wi::ecs::INVALID_ENTITY ||
                    hierarchy->parentID == current)
                {
                    return false;
                }
                current = hierarchy->parentID;
            }
            return false;
        }

        [[nodiscard]] AnimationPlaybackMode PlaybackMode(
            const wi::scene::AnimationComponent& animation) noexcept
        {
            if (animation.IsLooped())
                return AnimationPlaybackMode::Loop;
            if (animation.IsPingPong())
                return AnimationPlaybackMode::PingPong;
            return AnimationPlaybackMode::PlayOnce;
        }

        [[nodiscard]] std::string AnimationName(
            const wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            std::size_t ordinal)
        {
            if (const auto* name = scene.names.GetComponent(entity);
                name != nullptr && !name->name.empty())
            {
                return name->name;
            }
            return "Animation " + std::to_string(ordinal + 1);
        }

        [[nodiscard]] wi::scene::AnimationComponent* FindAnimation(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity) noexcept
        {
            return scene.animations.GetComponent(entity);
        }
    }

    AnimationAuthoredState CaptureAnimationAuthoredState(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity) noexcept
    {
        AnimationAuthoredState state;
        const auto* animation = scene.animations.GetComponent(animationEntity);
        if (animation == nullptr)
            return state;

        state.start = animation->start;
        state.end = animation->end;
        state.amount = animation->amount;
        state.speed = animation->speed;
        state.playbackMode = PlaybackMode(*animation);
        state.rootMotion = animation->IsRootMotion();
        return state;
    }

    AnimationAuthoredState SanitizeAnimationAuthoredState(
        AnimationAuthoredState state) noexcept
    {
        if (!std::isfinite(state.start))
            state.start = 0.0f;
        if (!std::isfinite(state.end))
            state.end = state.start;
        state.end = std::max(state.start, state.end);

        if (!std::isfinite(state.amount))
            state.amount = 1.0f;
        state.amount = std::clamp(state.amount, 0.0f, 1.0f);

        if (!std::isfinite(state.speed))
            state.speed = 1.0f;
        state.speed = std::clamp(state.speed, -4.0f, 4.0f);
        return state;
    }

    bool AnimationAuthoredStateEquals(
        const AnimationAuthoredState& a,
        const AnimationAuthoredState& b) noexcept
    {
        return a.start == b.start &&
            a.end == b.end &&
            a.amount == b.amount &&
            a.speed == b.speed &&
            a.playbackMode == b.playbackMode &&
            a.rootMotion == b.rootMotion;
    }

    std::vector<AnimationClipState> InspectAnimationsForSelection(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity selected)
    {
        std::vector<AnimationClipState> clips;
        if (selected == wi::ecs::INVALID_ENTITY)
            return clips;

        const wi::ecs::Entity root =
            ResolveSceneComponentAuthoringRoot(scene, selected);
        const std::size_t count = scene.animations.GetCount();
        clips.reserve(count);

        for (std::size_t index = 0; index < count; ++index)
        {
            const wi::ecs::Entity entity = scene.animations.GetEntity(index);
            if (!IsOwnedByRoot(scene, entity, root))
                continue;

            const auto* animation = scene.animations.GetComponent(entity);
            if (animation == nullptr)
                continue;

            AnimationClipState clip;
            clip.entity = entity;
            clip.name = AnimationName(scene, entity, clips.size());
            clip.authored = CaptureAnimationAuthoredState(scene, entity);
            clip.timer = animation->timer;
            clip.length = animation->GetLength();
            clip.playing = animation->IsPlaying();
            clip.ended = animation->IsEnded();
            clips.push_back(std::move(clip));
        }
        return clips;
    }

    bool ApplyAnimationAuthoredState(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity,
        const AnimationAuthoredState& requested) noexcept
    {
        auto* animation = FindAnimation(scene, animationEntity);
        if (animation == nullptr)
            return false;

        const AnimationAuthoredState state =
            SanitizeAnimationAuthoredState(requested);
        animation->start = state.start;
        animation->end = state.end;
        animation->amount = state.amount;
        animation->speed = state.speed;

        switch (state.playbackMode)
        {
        case AnimationPlaybackMode::Loop:
            animation->SetLooped();
            break;
        case AnimationPlaybackMode::PingPong:
            animation->SetPingPong();
            break;
        case AnimationPlaybackMode::PlayOnce:
            animation->SetPlayOnce();
            break;
        }

        if (state.rootMotion)
            animation->RootMotionOn();
        else
            animation->RootMotionOff();

        const float clampedTimer =
            std::clamp(animation->timer, animation->start, animation->end);
        if (animation->timer != clampedTimer)
        {
            animation->timer = clampedTimer;
            animation->last_update_time = clampedTimer;
        }
        return true;
    }

    bool PlayAnimationPreview(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity) noexcept
    {
        auto* animation = FindAnimation(scene, animationEntity);
        if (animation == nullptr || animation->timer >= animation->end)
            return false;
        animation->Play();
        return true;
    }

    bool PlayAnimationPreviewFromStart(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity) noexcept
    {
        auto* animation = FindAnimation(scene, animationEntity);
        if (animation == nullptr)
            return false;
        animation->timer = animation->start;
        animation->last_update_time = animation->start;
        animation->Play();
        return true;
    }

    bool PauseAnimationPreview(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity) noexcept
    {
        auto* animation = FindAnimation(scene, animationEntity);
        if (animation == nullptr)
            return false;
        animation->Pause();
        return true;
    }

    bool StopAnimationPreview(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity) noexcept
    {
        auto* animation = FindAnimation(scene, animationEntity);
        if (animation == nullptr)
            return false;
        animation->Stop();
        animation->timer = animation->start;
        animation->last_update_time = animation->start;
        return true;
    }

    bool ScrubAnimationPreview(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity,
        float timer) noexcept
    {
        auto* animation = FindAnimation(scene, animationEntity);
        if (animation == nullptr)
            return false;
        if (!std::isfinite(timer))
            timer = animation->start;
        timer = std::clamp(timer, animation->start, animation->end);
        animation->timer = timer;
        animation->last_update_time = timer;
        return true;
    }

    SetAnimationAuthoredStateCommand::SetAnimationAuthoredStateCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity,
        AnimationAuthoredState state) noexcept
        : scene_(&scene)
        , entity_(animationEntity)
        , before_(CaptureAnimationAuthoredState(scene, animationEntity))
        , after_(SanitizeAnimationAuthoredState(state))
    {
    }

    SetAnimationAuthoredStateCommand::SetAnimationAuthoredStateCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity,
        AnimationAuthoredState before,
        AnimationAuthoredState after) noexcept
        : scene_(&scene)
        , entity_(animationEntity)
        , before_(SanitizeAnimationAuthoredState(before))
        , after_(SanitizeAnimationAuthoredState(after))
    {
    }

    bool SetAnimationAuthoredStateCommand::Execute()
    {
        if (scene_ == nullptr || entity_ == wi::ecs::INVALID_ENTITY ||
            AnimationAuthoredStateEquals(before_, after_))
        {
            return false;
        }
        return ApplyAnimationAuthoredState(*scene_, entity_, after_);
    }

    void SetAnimationAuthoredStateCommand::Undo()
    {
        if (scene_ != nullptr && entity_ != wi::ecs::INVALID_ENTITY)
            (void)ApplyAnimationAuthoredState(*scene_, entity_, before_);
    }
}
