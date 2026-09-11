#pragma once

#include <string>
#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    enum class AnimationPlaybackMode
    {
        Loop,
        PingPong,
        PlayOnce,
    };

    // Curated creator-authored state over Wicked's native AnimationComponent.
    // Channels, samplers, AnimationData and root-motion bone ownership remain
    // entirely Wicked-native and are intentionally not copied into Renegade.
    struct AnimationAuthoredState
    {
        float start = 0.0f;
        float end = 0.0f;
        float amount = 1.0f;
        float speed = 1.0f;
        AnimationPlaybackMode playbackMode = AnimationPlaybackMode::Loop;
        bool rootMotion = false;
    };

    struct AnimationClipState
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        std::string name;
        AnimationAuthoredState authored;
        float timer = 0.0f;
        float length = 0.0f;
        bool playing = false;
        bool ended = false;
    };

    [[nodiscard]] AnimationAuthoredState CaptureAnimationAuthoredState(
        const wi::scene::Scene& scene,
        wi::ecs::Entity animationEntity) noexcept;

    [[nodiscard]] AnimationAuthoredState SanitizeAnimationAuthoredState(
        AnimationAuthoredState state) noexcept;

    [[nodiscard]] bool AnimationAuthoredStateEquals(
        const AnimationAuthoredState& a,
        const AnimationAuthoredState& b) noexcept;

    // Resolves the selected creator entity through the existing reusable-asset
    // authoring root and returns every native Wicked AnimationComponent owned by
    // that hierarchy. This makes selecting either an imported model root or one
    // of its descendants expose the same clip set without a parallel clip DB.
    [[nodiscard]] std::vector<AnimationClipState> InspectAnimationsForSelection(
        const wi::scene::Scene& scene,
        wi::ecs::Entity selected);

    [[nodiscard]] bool ApplyAnimationAuthoredState(
        wi::scene::Scene& scene,
        wi::ecs::Entity animationEntity,
        const AnimationAuthoredState& state) noexcept;

    // Preview transport is deliberately transient: these helpers mutate only
    // Wicked runtime playback state and never enter CommandService, so audition,
    // pause and scrub cannot make the scene dirty.
    [[nodiscard]] bool PlayAnimationPreview(
        wi::scene::Scene& scene,
        wi::ecs::Entity animationEntity) noexcept;
    [[nodiscard]] bool PlayAnimationPreviewFromStart(
        wi::scene::Scene& scene,
        wi::ecs::Entity animationEntity) noexcept;
    [[nodiscard]] bool PauseAnimationPreview(
        wi::scene::Scene& scene,
        wi::ecs::Entity animationEntity) noexcept;
    [[nodiscard]] bool StopAnimationPreview(
        wi::scene::Scene& scene,
        wi::ecs::Entity animationEntity) noexcept;
    [[nodiscard]] bool ScrubAnimationPreview(
        wi::scene::Scene& scene,
        wi::ecs::Entity animationEntity,
        float timer) noexcept;

    class SetAnimationAuthoredStateCommand final : public ICommand
    {
    public:
        SetAnimationAuthoredStateCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity animationEntity,
            AnimationAuthoredState state) noexcept;

        SetAnimationAuthoredStateCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity animationEntity,
            AnimationAuthoredState before,
            AnimationAuthoredState after) noexcept;

        bool Execute() override;
        void Undo() override;

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        AnimationAuthoredState before_;
        AnimationAuthoredState after_;
    };
}
