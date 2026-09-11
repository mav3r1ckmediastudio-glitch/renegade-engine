#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
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
        Once,
    };

    enum class AnimationPreviewAction
    {
        Play,
        Pause,
        Stop,
        PlayFromStart,
        PlayFromEnd,
    };

    struct AnimationAuthoringState
    {
        float start = 0.0f;
        float end = 0.0f;
        float amount = 1.0f;
        float speed = 1.0f;
        AnimationPlaybackMode playbackMode = AnimationPlaybackMode::Loop;
        bool rootMotion = false;
        wi::ecs::Entity rootMotionBone = wi::ecs::INVALID_ENTITY;
    };

    struct AnimationClipInfo
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        std::string name;
        AnimationAuthoringState authored;
        bool playing = false;
        float timer = 0.0f;
        float length = 0.0f;
        std::size_t channelCount = 0;
        std::size_t samplerCount = 0;
    };

    [[nodiscard]] AnimationAuthoringState CaptureAnimationAuthoring(
        const wi::scene::AnimationComponent& animation) noexcept;
    [[nodiscard]] AnimationAuthoringState SanitizeAnimationAuthoring(
        const wi::scene::Scene& scene,
        const AnimationAuthoringState& state) noexcept;
    [[nodiscard]] bool HasAnimationAuthoringChange(
        const AnimationAuthoringState& before,
        const AnimationAuthoringState& after) noexcept;
    void ApplyAnimationAuthoring(
        wi::scene::Scene& scene,
        wi::scene::AnimationComponent& animation,
        const AnimationAuthoringState& state) noexcept;

    // Resolves the nearest selected hierarchy root that owns native Wicked
    // AnimationComponents. Selecting a mesh or bone inside an imported model
    // therefore still exposes its sibling clip entities.
    [[nodiscard]] wi::ecs::Entity ResolveAnimationOwner(
        const wi::scene::Scene& scene,
        wi::ecs::Entity selected) noexcept;
    [[nodiscard]] std::vector<AnimationClipInfo> CollectAnimationClips(
        const wi::scene::Scene& scene,
        wi::ecs::Entity selected);

    // Preview operations deliberately bypass CommandService. Wicked serializes
    // playback flags/timer, but creator preview transport must not make the
    // document dirty. Authored loop/speed/blend/range/root-motion state uses
    // SetAnimationAuthoringCommand below.
    [[nodiscard]] bool PreviewAnimation(
        wi::scene::Scene& scene,
        wi::ecs::Entity animationEntity,
        AnimationPreviewAction action) noexcept;
    [[nodiscard]] bool PreviewAnimationTimer(
        wi::scene::Scene& scene,
        wi::ecs::Entity animationEntity,
        float timer) noexcept;

    class SetAnimationAuthoringCommand final : public ICommand
    {
    public:
        SetAnimationAuthoringCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity animationEntity,
            AnimationAuthoringState state);
        SetAnimationAuthoringCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity animationEntity,
            AnimationAuthoringState before,
            AnimationAuthoringState after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const AnimationAuthoringState& state) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        AnimationAuthoringState before_;
        AnimationAuthoringState after_;
    };

    // ----- Gate 7B: native Armature/Humanoid mapping and retargeting -----

    struct HumanoidMappingState
    {
        static constexpr std::size_t BoneCount =
            static_cast<std::size_t>(
                wi::scene::HumanoidComponent::HumanoidBone::Count);
        std::array<wi::ecs::Entity, BoneCount> bones{};
    };

    struct HumanoidLookState
    {
        bool enabled = true;
        bool capsuleShadowDisabled = false;
        wi::ecs::Entity target = wi::ecs::INVALID_ENTITY;
        float headHorizontalDegrees = 60.0f;
        float headVerticalDegrees = 30.0f;
        float headSpeed = 0.1f;
        float eyeHorizontalDegrees = 9.0f;
        float eyeVerticalDegrees = 9.0f;
        float eyeSpeed = 0.1f;
        float armSpacing = 0.0f;
        float legSpacing = 0.0f;
    };

    [[nodiscard]] HumanoidMappingState CaptureHumanoidMapping(
        const wi::scene::HumanoidComponent& humanoid) noexcept;
    [[nodiscard]] HumanoidLookState CaptureHumanoidLook(
        const wi::scene::HumanoidComponent& humanoid) noexcept;
    [[nodiscard]] HumanoidLookState SanitizeHumanoidLook(
        const wi::scene::Scene& scene,
        const HumanoidLookState& state) noexcept;
    void ApplyHumanoidLook(
        wi::scene::HumanoidComponent& humanoid,
        const HumanoidLookState& state) noexcept;

    [[nodiscard]] bool BuildAutomaticHumanoidMapping(
        const wi::scene::Scene& scene,
        wi::ecs::Entity armatureEntity,
        HumanoidMappingState& mapping,
        std::string& error);

    class SetHumanoidMappingCommand final : public ICommand
    {
    public:
        SetHumanoidMappingCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity humanoidEntity,
            HumanoidMappingState mapping);
        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const HumanoidMappingState& mapping, bool create) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        HumanoidMappingState before_;
        HumanoidMappingState after_;
        bool existedBefore_ = false;
    };

    class SetHumanoidBoneCommand final : public ICommand
    {
    public:
        SetHumanoidBoneCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity humanoidEntity,
            wi::scene::HumanoidComponent::HumanoidBone bone,
            wi::ecs::Entity target);
        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(wi::ecs::Entity target) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::scene::HumanoidComponent::HumanoidBone bone_ =
            wi::scene::HumanoidComponent::HumanoidBone::Hips;
        wi::ecs::Entity before_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity after_ = wi::ecs::INVALID_ENTITY;
    };

    class SetHumanoidLookCommand final : public ICommand
    {
    public:
        SetHumanoidLookCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity humanoidEntity,
            HumanoidLookState state);
        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const HumanoidLookState& state) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        HumanoidLookState before_;
        HumanoidLookState after_;
    };

    [[nodiscard]] bool ResetHumanoidPosePreview(
        wi::scene::Scene& scene,
        wi::ecs::Entity humanoidOrArmatureEntity) noexcept;

    class RetargetAnimationsCommand final : public ICommand
    {
    public:
        RetargetAnimationsCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity humanoidEntity,
            std::string sourcePath);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] const std::vector<wi::ecs::Entity>& CreatedAnimations()
            const noexcept;
        [[nodiscard]] const std::string& LastError() const noexcept;

    private:
        bool ImportFirstTime();
        bool RestoreSnapshots();
        void CaptureSnapshots();

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity humanoid_ = wi::ecs::INVALID_ENTITY;
        std::string sourcePath_;
        std::string lastError_;
        std::vector<wi::ecs::Entity> animations_;
        std::vector<wi::ecs::Entity> animationData_;
        std::vector<wi::Archive> animationSnapshots_;
        std::vector<wi::Archive> dataSnapshots_;
        bool snapshotsReady_ = false;
    };

    // ----- Gate 7C: IK, look-at and expressions -----

    struct InverseKinematicsState
    {
        bool enabled = true;
        wi::ecs::Entity target = wi::ecs::INVALID_ENTITY;
        std::uint32_t chainLength = 0;
        std::uint32_t iterations = 1;
    };

    [[nodiscard]] InverseKinematicsState CaptureInverseKinematics(
        const wi::scene::InverseKinematicsComponent& ik) noexcept;
    [[nodiscard]] InverseKinematicsState SanitizeInverseKinematics(
        const wi::scene::Scene& scene,
        const InverseKinematicsState& state) noexcept;
    void ApplyInverseKinematics(
        wi::scene::InverseKinematicsComponent& ik,
        const InverseKinematicsState& state) noexcept;

    class SetInverseKinematicsCommand final : public ICommand
    {
    public:
        SetInverseKinematicsCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            InverseKinematicsState state,
            bool createIfMissing = true);
        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const InverseKinematicsState& state, bool create) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        InverseKinematicsState before_;
        InverseKinematicsState after_;
        bool existedBefore_ = false;
        bool createIfMissing_ = true;
    };

    struct ExpressionMasterState
    {
        bool forceTalking = false;
        float blinkFrequency = 0.0f;
        float blinkLength = 0.1f;
        int blinkCount = 2;
        float lookFrequency = 0.0f;
        float lookLength = 0.6f;
    };

    struct ExpressionItemState
    {
        std::size_t index = 0;
        float weight = 0.0f;
        bool binary = false;
        wi::scene::ExpressionComponent::Override mouth =
            wi::scene::ExpressionComponent::Override::None;
        wi::scene::ExpressionComponent::Override blink =
            wi::scene::ExpressionComponent::Override::None;
        wi::scene::ExpressionComponent::Override look =
            wi::scene::ExpressionComponent::Override::None;
    };

    [[nodiscard]] ExpressionMasterState CaptureExpressionMaster(
        const wi::scene::ExpressionComponent& expressions) noexcept;
    [[nodiscard]] ExpressionItemState CaptureExpressionItem(
        const wi::scene::ExpressionComponent& expressions,
        std::size_t index) noexcept;

    class SetExpressionMasterCommand final : public ICommand
    {
    public:
        SetExpressionMasterCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            ExpressionMasterState state);
        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const ExpressionMasterState& state) noexcept;
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        ExpressionMasterState before_;
        ExpressionMasterState after_;
    };

    class SetExpressionItemCommand final : public ICommand
    {
    public:
        SetExpressionItemCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            ExpressionItemState state);
        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const ExpressionItemState& state) noexcept;
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        ExpressionItemState before_;
        ExpressionItemState after_;
    };

    // ----- Gate 7D: native Wicked timeline/keyframe authoring -----

    enum class AnimationInterpolation
    {
        Step,
        Linear,
        CubicSpline,
    };

    struct TimelineChannelInfo
    {
        std::size_t channelIndex = 0;
        wi::ecs::Entity target = wi::ecs::INVALID_ENTITY;
        wi::scene::AnimationComponent::AnimationChannel::Path path =
            wi::scene::AnimationComponent::AnimationChannel::Path::UNKNOWN;
        AnimationInterpolation interpolation = AnimationInterpolation::Linear;
        std::size_t keyCount = 0;
    };

    [[nodiscard]] wi::ecs::Entity CreateNativeAnimation(
        wi::scene::Scene& scene,
        wi::ecs::Entity owner,
        const std::string& name);
    [[nodiscard]] std::vector<TimelineChannelInfo> CollectTimelineChannels(
        const wi::scene::Scene& scene,
        wi::ecs::Entity animationEntity);

    // Adds or replaces a native key at `time`. `values` must match the native
    // PathDataType arity. CubicSpline expects in/value/out triplets per key;
    // imported spline data remains fully supported while creator recording can
    // deliberately reject malformed data instead of corrupting a clip.
    [[nodiscard]] bool RecordAnimationKey(
        wi::scene::Scene& scene,
        wi::ecs::Entity animationEntity,
        wi::ecs::Entity target,
        wi::scene::AnimationComponent::AnimationChannel::Path path,
        AnimationInterpolation interpolation,
        float time,
        const std::vector<float>& values,
        std::string& error);

    // Captures the current native value for a supported target/path in the
    // same form expected by RecordAnimationKey. This covers transform, morph,
    // light, sound, emitter, camera, script and material paths supported by the
    // pinned Wicked AnimationWindow.
    [[nodiscard]] bool CaptureCurrentAnimationValue(
        const wi::scene::Scene& scene,
        wi::ecs::Entity target,
        wi::scene::AnimationComponent::AnimationChannel::Path path,
        std::vector<float>& values,
        std::string& error);

    class RecordAnimationKeyCommand final : public ICommand
    {
    public:
        RecordAnimationKeyCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity animationEntity,
            wi::ecs::Entity target,
            wi::scene::AnimationComponent::AnimationChannel::Path path,
            AnimationInterpolation interpolation,
            float time,
            std::vector<float> values);
        bool Execute() override;
        void Undo() override;
        [[nodiscard]] const std::string& LastError() const noexcept;

    private:
        bool Restore(const wi::scene::AnimationComponent& animation,
            const std::vector<std::pair<wi::ecs::Entity,
                wi::scene::AnimationDataComponent>>& data) noexcept;
        void CaptureBefore();
        void CaptureAfter();

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity animation_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity target_ = wi::ecs::INVALID_ENTITY;
        wi::scene::AnimationComponent::AnimationChannel::Path path_ =
            wi::scene::AnimationComponent::AnimationChannel::Path::UNKNOWN;
        AnimationInterpolation interpolation_ = AnimationInterpolation::Linear;
        float time_ = 0.0f;
        std::vector<float> values_;
        wi::scene::AnimationComponent beforeAnimation_;
        wi::scene::AnimationComponent afterAnimation_;
        std::vector<std::pair<wi::ecs::Entity,
            wi::scene::AnimationDataComponent>> beforeData_;
        std::vector<std::pair<wi::ecs::Entity,
            wi::scene::AnimationDataComponent>> afterData_;
        bool captured_ = false;
        std::string lastError_;
    };
}
