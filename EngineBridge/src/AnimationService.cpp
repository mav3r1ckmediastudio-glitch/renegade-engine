#include "renegade/bridge/AnimationService.h"

#include "ModelImporter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace renegade::bridge
{
    namespace
    {
        constexpr float Epsilon = 0.00001f;

        bool NearlyEqual(const float left, const float right) noexcept
        {
            return std::abs(left - right) <= Epsilon;
        }

        float FiniteOr(const float value, const float fallback) noexcept
        {
            return std::isfinite(value) ? value : fallback;
        }

        bool EntityHasTransform(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity entity) noexcept
        {
            return entity != wi::ecs::INVALID_ENTITY &&
                scene.transforms.Contains(entity);
        }

        AnimationPlaybackMode PlaybackModeOf(
            const wi::scene::AnimationComponent& animation) noexcept
        {
            if (animation.IsPingPong())
                return AnimationPlaybackMode::PingPong;
            if (animation.IsLooped())
                return AnimationPlaybackMode::Loop;
            return AnimationPlaybackMode::Once;
        }

        void ApplyPlaybackMode(
            wi::scene::AnimationComponent& animation,
            const AnimationPlaybackMode mode) noexcept
        {
            switch (mode)
            {
            case AnimationPlaybackMode::Loop:
                animation.SetLooped();
                break;
            case AnimationPlaybackMode::PingPong:
                animation.SetPingPong();
                break;
            case AnimationPlaybackMode::Once:
                animation.SetPlayOnce();
                break;
            }
        }

        bool IsAnimationDescendantOf(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity animation,
            const wi::ecs::Entity root) noexcept
        {
            return animation == root || scene.Entity_IsDescendant(animation, root);
        }

        bool HasAnimationBelow(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity root) noexcept
        {
            if (root == wi::ecs::INVALID_ENTITY)
                return false;
            for (std::size_t index = 0; index < scene.animations.GetCount(); ++index)
            {
                if (IsAnimationDescendantOf(
                        scene,
                        scene.animations.GetEntity(index),
                        root))
                {
                    return true;
                }
            }
            return false;
        }

        bool HasAnimationAuthoringStateChange(
            const AnimationAuthoringState& left,
            const AnimationAuthoringState& right) noexcept
        {
            return !NearlyEqual(left.start, right.start) ||
                !NearlyEqual(left.end, right.end) ||
                !NearlyEqual(left.amount, right.amount) ||
                !NearlyEqual(left.speed, right.speed) ||
                left.playbackMode != right.playbackMode ||
                left.rootMotion != right.rootMotion ||
                left.rootMotionBone != right.rootMotionBone;
        }

        bool HasHumanoidLookChange(
            const HumanoidLookState& left,
            const HumanoidLookState& right) noexcept
        {
            return left.enabled != right.enabled ||
                left.capsuleShadowDisabled != right.capsuleShadowDisabled ||
                left.target != right.target ||
                !NearlyEqual(left.headHorizontalDegrees, right.headHorizontalDegrees) ||
                !NearlyEqual(left.headVerticalDegrees, right.headVerticalDegrees) ||
                !NearlyEqual(left.headSpeed, right.headSpeed) ||
                !NearlyEqual(left.eyeHorizontalDegrees, right.eyeHorizontalDegrees) ||
                !NearlyEqual(left.eyeVerticalDegrees, right.eyeVerticalDegrees) ||
                !NearlyEqual(left.eyeSpeed, right.eyeSpeed) ||
                !NearlyEqual(left.armSpacing, right.armSpacing) ||
                !NearlyEqual(left.legSpacing, right.legSpacing);
        }

        bool HasIkChange(
            const InverseKinematicsState& left,
            const InverseKinematicsState& right) noexcept
        {
            return left.enabled != right.enabled ||
                left.target != right.target ||
                left.chainLength != right.chainLength ||
                left.iterations != right.iterations;
        }

        bool HasExpressionMasterChange(
            const ExpressionMasterState& left,
            const ExpressionMasterState& right) noexcept
        {
            return left.forceTalking != right.forceTalking ||
                !NearlyEqual(left.blinkFrequency, right.blinkFrequency) ||
                !NearlyEqual(left.blinkLength, right.blinkLength) ||
                left.blinkCount != right.blinkCount ||
                !NearlyEqual(left.lookFrequency, right.lookFrequency) ||
                !NearlyEqual(left.lookLength, right.lookLength);
        }

        bool HasExpressionItemChange(
            const ExpressionItemState& left,
            const ExpressionItemState& right) noexcept
        {
            return left.index != right.index ||
                !NearlyEqual(left.weight, right.weight) ||
                left.binary != right.binary ||
                left.mouth != right.mouth ||
                left.blink != right.blink ||
                left.look != right.look;
        }

        const char* AnimationName(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity entity) noexcept
        {
            const auto* name = scene.names.GetComponent(entity);
            if (name == nullptr || name->name.empty())
                return nullptr;
            return name->name.c_str();
        }

        using HumanoidBone = wi::scene::HumanoidComponent::HumanoidBone;

        const std::unordered_map<HumanoidBone, std::vector<std::string>>&
        NativeHumanoidNameMap()
        {
            // Keep this aligned with the pinned Wicked ArmatureWindow. The
            // matching algorithm is intentionally the same case-insensitive
            // substring search so Renegade and Wicked Editor resolve the same
            // VRM/Mixamo/UE-style bone names.
            static const std::unordered_map<HumanoidBone, std::vector<std::string>> map = {
                {HumanoidBone::Hips, {"Hips", "pelvis"}},
                {HumanoidBone::Spine, {"Spine", "spine_01"}},
                {HumanoidBone::Chest, {"Chest", "Spine1", "spine_02"}},
                {HumanoidBone::UpperChest, {"UpperChest", "Spine2", "spine_03"}},
                {HumanoidBone::Neck, {"Neck"}},
                {HumanoidBone::Head, {"Head"}},
                {HumanoidBone::LeftEye, {"LeftEye"}},
                {HumanoidBone::RightEye, {"RightEye"}},
                {HumanoidBone::Jaw, {"Jaw"}},
                {HumanoidBone::LeftUpperLeg, {"LeftUpperLeg", "LeftUpLeg", "thigh_l"}},
                {HumanoidBone::LeftLowerLeg, {"LeftLowerLeg", "LeftLeg", "calf_l"}},
                {HumanoidBone::LeftFoot, {"LeftFoot", "foot_l"}},
                {HumanoidBone::LeftToes, {"LeftToe", "ball_l"}},
                {HumanoidBone::RightUpperLeg, {"RightUpperLeg", "RightUpLeg", "thigh_r"}},
                {HumanoidBone::RightLowerLeg, {"RightLowerLeg", "RightLeg", "calf_r"}},
                {HumanoidBone::RightFoot, {"RightFoot", "foot_r"}},
                {HumanoidBone::RightToes, {"RightToe", "ball_r"}},
                {HumanoidBone::LeftShoulder, {"LeftShoulder", "clavicle_l"}},
                {HumanoidBone::LeftUpperArm, {"LeftUpperArm", "LeftArm", "upperarm_l"}},
                {HumanoidBone::LeftLowerArm, {"LeftLowerArm", "LeftForeArm", "lowerarm_l"}},
                {HumanoidBone::LeftHand, {"LeftHand", "hand_l"}},
                {HumanoidBone::RightShoulder, {"RightShoulder", "clavicle_r"}},
                {HumanoidBone::RightUpperArm, {"RightUpperArm", "RightArm", "upperarm_r"}},
                {HumanoidBone::RightLowerArm, {"RightLowerArm", "RightForeArm", "lowerarm_r"}},
                {HumanoidBone::RightHand, {"RightHand", "hand_r"}},
                {HumanoidBone::LeftThumbMetacarpal, {"LeftThumbMetacarpal", "LeftHandThumb1", "thumb_01_l"}},
                {HumanoidBone::LeftThumbProximal, {"LeftThumbProximal", "LeftHandThumb2", "thumb_02_l"}},
                {HumanoidBone::LeftThumbDistal, {"LeftThumbDistal", "LeftHandThumb3", "thumb_03_l"}},
                {HumanoidBone::LeftIndexProximal, {"LeftIndexProximal", "LeftHandIndex1", "index_01_l"}},
                {HumanoidBone::LeftIndexIntermediate, {"LeftIndexIntermediate", "LeftHandIndex2", "index_02_l"}},
                {HumanoidBone::LeftIndexDistal, {"LeftIndexDistal", "LeftHandIndex3", "index_03_l"}},
                {HumanoidBone::LeftMiddleProximal, {"LeftMiddleProximal", "LeftHandMiddle1", "middle_01_l"}},
                {HumanoidBone::LeftMiddleIntermediate, {"LeftMiddleIntermediate", "LeftHandMiddle2", "middle_02_l"}},
                {HumanoidBone::LeftMiddleDistal, {"LeftMiddleDistal", "LeftHandMiddle3", "middle_03_l"}},
                {HumanoidBone::LeftRingProximal, {"LeftRingProximal", "LeftHandRing1", "ring_01_l"}},
                {HumanoidBone::LeftRingIntermediate, {"LeftRingIntermediate", "LeftHandRing2", "ring_02_l"}},
                {HumanoidBone::LeftRingDistal, {"LeftRingDistal", "LeftHandRing3", "ring_03_l"}},
                {HumanoidBone::LeftLittleProximal, {"LeftLittleProximal", "LeftHandPinky1", "pinky_01_l"}},
                {HumanoidBone::LeftLittleIntermediate, {"LeftLittleIntermediate", "LeftHandPinky2", "pinky_02_l"}},
                {HumanoidBone::LeftLittleDistal, {"LeftLittleDistal", "LeftHandPinky3", "pinky_03_l"}},
                {HumanoidBone::RightThumbMetacarpal, {"RightThumbMetacarpal", "RightHandThumb1", "thumb_01_r"}},
                {HumanoidBone::RightThumbProximal, {"RightThumbProximal", "RightHandThumb2", "thumb_02_r"}},
                {HumanoidBone::RightThumbDistal, {"RightThumbDistal", "RightHandThumb3", "thumb_03_r"}},
                {HumanoidBone::RightIndexProximal, {"RightIndexProximal", "RightHandIndex1", "index_01_r"}},
                {HumanoidBone::RightIndexIntermediate, {"RightIndexIntermediate", "RightHandIndex2", "index_02_r"}},
                {HumanoidBone::RightIndexDistal, {"RightIndexDistal", "RightHandIndex3", "index_03_r"}},
                {HumanoidBone::RightMiddleProximal, {"RightMiddleProximal", "RightHandMiddle1", "middle_01_r"}},
                {HumanoidBone::RightMiddleIntermediate, {"RightMiddleIntermediate", "RightHandMiddle2", "middle_02_r"}},
                {HumanoidBone::RightMiddleDistal, {"RightMiddleDistal", "RightHandMiddle3", "middle_03_r"}},
                {HumanoidBone::RightRingProximal, {"RightRingProximal", "RightHandRing1", "ring_01_r"}},
                {HumanoidBone::RightRingIntermediate, {"RightRingIntermediate", "RightHandRing2", "ring_02_r"}},
                {HumanoidBone::RightRingDistal, {"RightRingDistal", "RightHandRing3", "ring_03_r"}},
                {HumanoidBone::RightLittleProximal, {"RightLittleProximal", "RightHandPinky1", "pinky_01_r"}},
                {HumanoidBone::RightLittleIntermediate, {"RightLittleIntermediate", "RightHandPinky2", "pinky_02_r"}},
                {HumanoidBone::RightLittleDistal, {"RightLittleDistal", "RightHandPinky3", "pinky_03_r"}},
            };
            return map;
        }

        AnimationInterpolation InterpolationOf(
            const wi::scene::AnimationComponent::AnimationSampler::Mode mode) noexcept
        {
            using Mode = wi::scene::AnimationComponent::AnimationSampler::Mode;
            switch (mode)
            {
            case Mode::STEP:
                return AnimationInterpolation::Step;
            case Mode::CUBICSPLINE:
                return AnimationInterpolation::CubicSpline;
            case Mode::LINEAR:
            default:
                return AnimationInterpolation::Linear;
            }
        }

        wi::scene::AnimationComponent::AnimationSampler::Mode NativeInterpolation(
            const AnimationInterpolation mode) noexcept
        {
            using Mode = wi::scene::AnimationComponent::AnimationSampler::Mode;
            switch (mode)
            {
            case AnimationInterpolation::Step:
                return Mode::STEP;
            case AnimationInterpolation::CubicSpline:
                return Mode::CUBICSPLINE;
            case AnimationInterpolation::Linear:
            default:
                return Mode::LINEAR;
            }
        }

        std::size_t BaseValueCount(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity target,
            const wi::scene::AnimationComponent::AnimationChannel& channel,
            std::string& error)
        {
            using Type = wi::scene::AnimationComponent::AnimationChannel::PathDataType;
            switch (channel.GetPathDataType())
            {
            case Type::Event:
                return 0;
            case Type::Float:
                return 1;
            case Type::Float2:
                return 2;
            case Type::Float3:
                return 3;
            case Type::Float4:
                return 4;
            case Type::Weights:
            {
                const auto* mesh = scene.meshes.GetComponent(target);
                if (mesh == nullptr || mesh->morph_targets.empty())
                {
                    error = "Morph-weight animation requires a MeshComponent with morph targets.";
                    return std::numeric_limits<std::size_t>::max();
                }
                return mesh->morph_targets.size();
            }
            default:
                error = "Unsupported native Wicked animation path data type.";
                return std::numeric_limits<std::size_t>::max();
            }
        }

        bool CopyReferencedAnimationData(
            const wi::scene::Scene& scene,
            const wi::scene::AnimationComponent& animation,
            std::vector<std::pair<wi::ecs::Entity,
                wi::scene::AnimationDataComponent>>& output)
        {
            output.clear();
            std::unordered_set<wi::ecs::Entity> seen;
            for (const auto& sampler : animation.samplers)
            {
                if (sampler.data == wi::ecs::INVALID_ENTITY ||
                    !seen.insert(sampler.data).second)
                {
                    continue;
                }
                const auto* data = scene.animation_datas.GetComponent(sampler.data);
                if (data == nullptr)
                    return false;
                output.emplace_back(sampler.data, *data);
            }
            return true;
        }
    }

    AnimationAuthoringState CaptureAnimationAuthoring(
        const wi::scene::AnimationComponent& animation) noexcept
    {
        AnimationAuthoringState result;
        result.start = animation.start;
        result.end = animation.end;
        result.amount = animation.amount;
        result.speed = animation.speed;
        result.playbackMode = PlaybackModeOf(animation);
        result.rootMotion = animation.IsRootMotion();
        result.rootMotionBone = animation.GetRootMotionBone();
        return result;
    }

    AnimationAuthoringState SanitizeAnimationAuthoring(
        const wi::scene::Scene& scene,
        const AnimationAuthoringState& state) noexcept
    {
        AnimationAuthoringState result = state;
        result.start = std::max(0.0f, FiniteOr(result.start, 0.0f));
        result.end = std::max(result.start, FiniteOr(result.end, result.start));
        result.amount = std::clamp(FiniteOr(result.amount, 1.0f), 0.0f, 1.0f);
        result.speed = std::clamp(FiniteOr(result.speed, 1.0f), -8.0f, 8.0f);
        if (!EntityHasTransform(scene, result.rootMotionBone))
            result.rootMotionBone = wi::ecs::INVALID_ENTITY;
        if (result.rootMotionBone == wi::ecs::INVALID_ENTITY)
            result.rootMotion = false;
        return result;
    }

    bool HasAnimationAuthoringChange(
        const AnimationAuthoringState& before,
        const AnimationAuthoringState& after) noexcept
    {
        return HasAnimationAuthoringStateChange(before, after);
    }

    void ApplyAnimationAuthoring(
        wi::scene::Scene& scene,
        wi::scene::AnimationComponent& animation,
        const AnimationAuthoringState& state) noexcept
    {
        const auto safe = SanitizeAnimationAuthoring(scene, state);
        animation.start = safe.start;
        animation.end = safe.end;
        animation.amount = safe.amount;
        animation.speed = safe.speed;
        ApplyPlaybackMode(animation, safe.playbackMode);
        if (safe.rootMotion)
        {
            animation.SetRootMotionBone(safe.rootMotionBone);
            animation.RootMotionOn();
        }
        else
        {
            animation.RootMotionOff();
            animation.SetRootMotionBone(safe.rootMotionBone);
        }
        animation.timer = std::clamp(animation.timer, animation.start, animation.end);
    }

    wi::ecs::Entity ResolveAnimationOwner(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity selected) noexcept
    {
        if (selected == wi::ecs::INVALID_ENTITY)
            return wi::ecs::INVALID_ENTITY;

        wi::ecs::Entity probe = selected;
        while (probe != wi::ecs::INVALID_ENTITY)
        {
            if (HasAnimationBelow(scene, probe))
                return probe;
            const auto* hierarchy = scene.hierarchy.GetComponent(probe);
            if (hierarchy == nullptr || hierarchy->parentID == probe)
                break;
            probe = hierarchy->parentID;
        }
        return wi::ecs::INVALID_ENTITY;
    }

    std::vector<AnimationClipInfo> CollectAnimationClips(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity selected)
    {
        std::vector<AnimationClipInfo> result;
        const wi::ecs::Entity root = ResolveAnimationOwner(scene, selected);
        if (root == wi::ecs::INVALID_ENTITY)
            return result;

        for (std::size_t index = 0; index < scene.animations.GetCount(); ++index)
        {
            const wi::ecs::Entity entity = scene.animations.GetEntity(index);
            if (!IsAnimationDescendantOf(scene, entity, root))
                continue;

            const auto& animation = scene.animations[index];
            AnimationClipInfo info;
            info.entity = entity;
            if (const char* name = AnimationName(scene, entity))
                info.name = name;
            else
                info.name = "Animation " + std::to_string(entity);
            info.authored = CaptureAnimationAuthoring(animation);
            info.playing = animation.IsPlaying();
            info.timer = animation.timer;
            info.length = animation.GetLength();
            info.channelCount = animation.channels.size();
            info.samplerCount = animation.samplers.size();
            result.push_back(std::move(info));
        }
        return result;
    }

    bool PreviewAnimation(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity,
        const AnimationPreviewAction action) noexcept
    {
        auto* animation = scene.animations.GetComponent(animationEntity);
        if (animation == nullptr)
            return false;

        switch (action)
        {
        case AnimationPreviewAction::Play:
            animation->Play();
            break;
        case AnimationPreviewAction::Pause:
            animation->Pause();
            break;
        case AnimationPreviewAction::Stop:
            animation->Pause();
            animation->timer = animation->start;
            break;
        case AnimationPreviewAction::PlayFromStart:
            animation->timer = animation->start;
            if (animation->speed < 0.0f)
                animation->speed = -animation->speed;
            animation->Play();
            break;
        case AnimationPreviewAction::PlayFromEnd:
            animation->timer = animation->end;
            if (animation->speed > 0.0f)
                animation->speed = -animation->speed;
            animation->Play();
            break;
        }
        return true;
    }

    bool PreviewAnimationTimer(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity,
        const float timer) noexcept
    {
        auto* animation = scene.animations.GetComponent(animationEntity);
        if (animation == nullptr || !std::isfinite(timer))
            return false;
        animation->timer = std::clamp(timer, animation->start, animation->end);
        return true;
    }

    SetAnimationAuthoringCommand::SetAnimationAuthoringCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity,
        AnimationAuthoringState state)
        : scene_(&scene)
        , entity_(animationEntity)
        , after_(SanitizeAnimationAuthoring(scene, state))
    {
        if (const auto* animation = scene.animations.GetComponent(animationEntity))
            before_ = CaptureAnimationAuthoring(*animation);
    }

    SetAnimationAuthoringCommand::SetAnimationAuthoringCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity,
        AnimationAuthoringState before,
        AnimationAuthoringState after)
        : scene_(&scene)
        , entity_(animationEntity)
        , before_(SanitizeAnimationAuthoring(scene, before))
        , after_(SanitizeAnimationAuthoring(scene, after))
    {
    }

    bool SetAnimationAuthoringCommand::Execute()
    {
        return HasAnimationAuthoringStateChange(before_, after_) && Apply(after_);
    }

    void SetAnimationAuthoringCommand::Undo()
    {
        Apply(before_);
    }

    bool SetAnimationAuthoringCommand::Apply(
        const AnimationAuthoringState& state) noexcept
    {
        if (scene_ == nullptr)
            return false;
        auto* animation = scene_->animations.GetComponent(entity_);
        if (animation == nullptr)
            return false;
        ApplyAnimationAuthoring(*scene_, *animation, state);
        return true;
    }

    HumanoidMappingState CaptureHumanoidMapping(
        const wi::scene::HumanoidComponent& humanoid) noexcept
    {
        HumanoidMappingState result;
        for (std::size_t index = 0; index < result.bones.size(); ++index)
            result.bones[index] = humanoid.bones[index];
        return result;
    }

    HumanoidLookState CaptureHumanoidLook(
        const wi::scene::HumanoidComponent& humanoid) noexcept
    {
        HumanoidLookState result;
        result.enabled = humanoid.IsLookAtEnabled();
        result.capsuleShadowDisabled = humanoid.IsCapsuleShadowDisabled();
        result.target = humanoid.lookAtEntity;
        result.headHorizontalDegrees = wi::math::RadiansToDegrees(humanoid.head_rotation_max.x);
        result.headVerticalDegrees = wi::math::RadiansToDegrees(humanoid.head_rotation_max.y);
        result.headSpeed = humanoid.head_rotation_speed;
        result.eyeHorizontalDegrees = wi::math::RadiansToDegrees(humanoid.eye_rotation_max.x);
        result.eyeVerticalDegrees = wi::math::RadiansToDegrees(humanoid.eye_rotation_max.y);
        result.eyeSpeed = humanoid.eye_rotation_speed;
        result.armSpacing = humanoid.arm_spacing;
        result.legSpacing = humanoid.leg_spacing;
        return result;
    }

    HumanoidLookState SanitizeHumanoidLook(
        const wi::scene::Scene& scene,
        const HumanoidLookState& state) noexcept
    {
        HumanoidLookState result = state;
        if (!EntityHasTransform(scene, result.target))
            result.target = wi::ecs::INVALID_ENTITY;
        result.headHorizontalDegrees = std::clamp(
            FiniteOr(result.headHorizontalDegrees, 60.0f), 0.0f, 90.0f);
        result.headVerticalDegrees = std::clamp(
            FiniteOr(result.headVerticalDegrees, 30.0f), 0.0f, 60.0f);
        result.headSpeed = std::clamp(FiniteOr(result.headSpeed, 0.1f), 0.01f, 1.0f);
        result.eyeHorizontalDegrees = std::clamp(
            FiniteOr(result.eyeHorizontalDegrees, 9.0f), 0.0f, 40.0f);
        result.eyeVerticalDegrees = std::clamp(
            FiniteOr(result.eyeVerticalDegrees, 9.0f), 0.0f, 30.0f);
        result.eyeSpeed = std::clamp(FiniteOr(result.eyeSpeed, 0.1f), 0.01f, 1.0f);
        result.armSpacing = std::clamp(FiniteOr(result.armSpacing, 0.0f), -1.0f, 1.0f);
        result.legSpacing = std::clamp(FiniteOr(result.legSpacing, 0.0f), -1.0f, 1.0f);
        return result;
    }

    void ApplyHumanoidLook(
        wi::scene::HumanoidComponent& humanoid,
        const HumanoidLookState& state) noexcept
    {
        humanoid.SetLookAtEnabled(state.enabled);
        humanoid.SetCapsuleShadowDisabled(state.capsuleShadowDisabled);
        humanoid.lookAtEntity = state.target;
        humanoid.head_rotation_max.x = wi::math::DegreesToRadians(state.headHorizontalDegrees);
        humanoid.head_rotation_max.y = wi::math::DegreesToRadians(state.headVerticalDegrees);
        humanoid.head_rotation_speed = state.headSpeed;
        humanoid.eye_rotation_max.x = wi::math::DegreesToRadians(state.eyeHorizontalDegrees);
        humanoid.eye_rotation_max.y = wi::math::DegreesToRadians(state.eyeVerticalDegrees);
        humanoid.eye_rotation_speed = state.eyeSpeed;
        humanoid.arm_spacing = state.armSpacing;
        humanoid.leg_spacing = state.legSpacing;
    }

    bool BuildAutomaticHumanoidMapping(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity armatureEntity,
        HumanoidMappingState& mapping,
        std::string& error)
    {
        error.clear();
        mapping = {};
        const auto* armature = scene.armatures.GetComponent(armatureEntity);
        if (armature == nullptr)
        {
            error = "The selected entity does not contain a native Wicked ArmatureComponent.";
            return false;
        }

        bool found = false;
        const auto& nameMap = NativeHumanoidNameMap();
        for (const wi::ecs::Entity bone : armature->boneCollection)
        {
            const auto* name = scene.names.GetComponent(bone);
            if (name == nullptr || name->name.empty())
                continue;
            const std::string upperName = wi::helper::toUpper(name->name);
            for (std::size_t index = 0; index < mapping.bones.size(); ++index)
            {
                if (mapping.bones[index] != wi::ecs::INVALID_ENTITY)
                    continue;
                const auto type = static_cast<HumanoidBone>(index);
                const auto candidates = nameMap.find(type);
                if (candidates == nameMap.end())
                    continue;
                for (const auto& candidate : candidates->second)
                {
                    if (upperName.find(wi::helper::toUpper(candidate)) != std::string::npos)
                    {
                        mapping.bones[index] = bone;
                        found = true;
                        break;
                    }
                }
            }
        }

        if (!found)
        {
            error = "No VRM/Mixamo-compatible humanoid bone names were found in the selected armature.";
            return false;
        }
        return true;
    }

    SetHumanoidMappingCommand::SetHumanoidMappingCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity humanoidEntity,
        HumanoidMappingState mapping)
        : scene_(&scene)
        , entity_(humanoidEntity)
        , after_(std::move(mapping))
    {
        if (const auto* humanoid = scene.humanoids.GetComponent(entity_))
        {
            existedBefore_ = true;
            before_ = CaptureHumanoidMapping(*humanoid);
        }
    }

    bool SetHumanoidMappingCommand::Execute()
    {
        if (existedBefore_ && before_.bones == after_.bones)
            return false;
        return Apply(after_, true);
    }

    void SetHumanoidMappingCommand::Undo()
    {
        if (scene_ == nullptr)
            return;
        if (!existedBefore_)
        {
            scene_->humanoids.Remove(entity_);
            return;
        }
        (void)Apply(before_, true);
    }

    bool SetHumanoidMappingCommand::Apply(
        const HumanoidMappingState& mapping,
        const bool create) noexcept
    {
        if (scene_ == nullptr)
            return false;
        auto* humanoid = scene_->humanoids.GetComponent(entity_);
        if (humanoid == nullptr && create)
            humanoid = &scene_->humanoids.Create(entity_);
        if (humanoid == nullptr)
            return false;
        for (std::size_t index = 0; index < mapping.bones.size(); ++index)
        {
            humanoid->bones[index] = EntityHasTransform(*scene_, mapping.bones[index])
                ? mapping.bones[index]
                : wi::ecs::INVALID_ENTITY;
        }
        humanoid->ragdoll = {};
        return true;
    }

    SetHumanoidBoneCommand::SetHumanoidBoneCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity humanoidEntity,
        const HumanoidBone bone,
        const wi::ecs::Entity target)
        : scene_(&scene)
        , entity_(humanoidEntity)
        , bone_(bone)
        , after_(EntityHasTransform(scene, target)
            ? target
            : wi::ecs::INVALID_ENTITY)
    {
        const auto* humanoid = scene.humanoids.GetComponent(entity_);
        if (humanoid != nullptr)
            before_ = humanoid->bones[static_cast<std::size_t>(bone_)];
    }

    bool SetHumanoidBoneCommand::Execute()
    {
        return before_ != after_ && Apply(after_);
    }

    void SetHumanoidBoneCommand::Undo()
    {
        (void)Apply(before_);
    }

    bool SetHumanoidBoneCommand::Apply(const wi::ecs::Entity target) noexcept
    {
        if (scene_ == nullptr)
            return false;
        auto* humanoid = scene_->humanoids.GetComponent(entity_);
        if (humanoid == nullptr)
            return false;
        humanoid->bones[static_cast<std::size_t>(bone_)] =
            EntityHasTransform(*scene_, target)
                ? target
                : wi::ecs::INVALID_ENTITY;
        humanoid->ragdoll = {};
        return true;
    }

    SetHumanoidLookCommand::SetHumanoidLookCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity humanoidEntity,
        HumanoidLookState state)
        : scene_(&scene)
        , entity_(humanoidEntity)
        , after_(SanitizeHumanoidLook(scene, state))
    {
        if (const auto* humanoid = scene.humanoids.GetComponent(entity_))
            before_ = CaptureHumanoidLook(*humanoid);
    }

    bool SetHumanoidLookCommand::Execute()
    {
        return HasHumanoidLookChange(before_, after_) && Apply(after_);
    }

    void SetHumanoidLookCommand::Undo()
    {
        (void)Apply(before_);
    }

    bool SetHumanoidLookCommand::Apply(const HumanoidLookState& state) noexcept
    {
        if (scene_ == nullptr)
            return false;
        auto* humanoid = scene_->humanoids.GetComponent(entity_);
        if (humanoid == nullptr)
            return false;
        ApplyHumanoidLook(*humanoid, SanitizeHumanoidLook(*scene_, state));
        return true;
    }

    bool ResetHumanoidPosePreview(
        wi::scene::Scene& scene,
        const wi::ecs::Entity humanoidOrArmatureEntity) noexcept
    {
        if (!scene.armatures.Contains(humanoidOrArmatureEntity) &&
            !scene.humanoids.Contains(humanoidOrArmatureEntity))
        {
            return false;
        }
        scene.ResetPose(humanoidOrArmatureEntity);
        return true;
    }

    RetargetAnimationsCommand::RetargetAnimationsCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity humanoidEntity,
        std::string sourcePath)
        : scene_(&scene)
        , humanoid_(humanoidEntity)
        , sourcePath_(std::move(sourcePath))
    {
    }

    bool RetargetAnimationsCommand::Execute()
    {
        lastError_.clear();
        if (scene_ == nullptr ||
            scene_->humanoids.GetComponent(humanoid_) == nullptr ||
            sourcePath_.empty())
        {
            lastError_ = "Retargeting requires a native HumanoidComponent and an animation source file.";
            return false;
        }
        if (!snapshotsReady_)
            return ImportFirstTime();
        return RestoreSnapshots();
    }

    void RetargetAnimationsCommand::Undo()
    {
        if (scene_ == nullptr || animations_.empty())
            return;
        if (!snapshotsReady_)
            CaptureSnapshots();
        for (const auto entity : animations_)
            scene_->Entity_Remove(entity);
        for (const auto entity : animationData_)
            scene_->Entity_Remove(entity);
    }

    const std::vector<wi::ecs::Entity>&
    RetargetAnimationsCommand::CreatedAnimations() const noexcept
    {
        return animations_;
    }

    const std::string& RetargetAnimationsCommand::LastError() const noexcept
    {
        return lastError_;
    }

    bool RetargetAnimationsCommand::ImportFirstTime()
    {
        std::unordered_set<wi::ecs::Entity> dataBefore;
        for (std::size_t index = 0; index < scene_->animation_datas.GetCount(); ++index)
            dataBefore.insert(scene_->animation_datas.GetEntity(index));

        wi::scene::Scene source;
        const std::string extension =
            wi::helper::toUpper(wi::helper::GetExtensionFromFileName(sourcePath_));
        if (extension == "WISCENE")
        {
            wi::scene::LoadModel(source, sourcePath_);
        }
        else if (extension == "GLTF" || extension == "GLB" ||
            extension == "VRM" || extension == "VRMA")
        {
            ImportModel_GLTF(sourcePath_, source);
        }
        else if (extension == "FBX")
        {
            ImportModel_FBX(sourcePath_, source);
        }
        else
        {
            lastError_ = "Unsupported animation source. Use WISCENE, GLTF, GLB, FBX, VRM or VRMA.";
            return false;
        }

        if (source.animations.GetCount() == 0)
        {
            lastError_ = "The selected file imported successfully but contains no native Wicked animations.";
            return false;
        }

        scene_->ResetPose(humanoid_);
        for (std::size_t index = 0; index < source.animations.GetCount(); ++index)
        {
            const wi::ecs::Entity sourceAnimation = source.animations.GetEntity(index);
            const wi::ecs::Entity created = scene_->RetargetAnimation(
                humanoid_, sourceAnimation, true, &source);
            if (created == wi::ecs::INVALID_ENTITY)
                continue;

            std::string name = "Retargeted Animation";
            if (const auto* sourceName = source.names.GetComponent(sourceAnimation))
            {
                if (!sourceName->name.empty())
                    name = sourceName->name;
            }
            scene_->names.Create(created).name = name;
            animations_.push_back(created);
        }

        if (animations_.empty())
        {
            lastError_ = "Wicked could not match any source animation channels to the destination humanoid mapping.";
            return false;
        }

        for (std::size_t index = 0; index < scene_->animation_datas.GetCount(); ++index)
        {
            const auto entity = scene_->animation_datas.GetEntity(index);
            if (dataBefore.find(entity) == dataBefore.end())
                animationData_.push_back(entity);
        }
        return true;
    }

    void RetargetAnimationsCommand::CaptureSnapshots()
    {
        animationSnapshots_.clear();
        dataSnapshots_.clear();
        if (scene_ == nullptr)
            return;

        dataSnapshots_.reserve(animationData_.size());
        for (const auto entity : animationData_)
        {
            wi::Archive archive;
            archive.SetReadModeAndResetPos(false);
            wi::ecs::EntitySerializer serializer;
            scene_->Entity_Serialize(archive, serializer, entity);
            dataSnapshots_.push_back(std::move(archive));
        }

        animationSnapshots_.reserve(animations_.size());
        for (const auto entity : animations_)
        {
            wi::Archive archive;
            archive.SetReadModeAndResetPos(false);
            wi::ecs::EntitySerializer serializer;
            scene_->Entity_Serialize(archive, serializer, entity);
            animationSnapshots_.push_back(std::move(archive));
        }
        snapshotsReady_ = animationSnapshots_.size() == animations_.size() &&
            dataSnapshots_.size() == animationData_.size();
    }

    bool RetargetAnimationsCommand::RestoreSnapshots()
    {
        if (scene_ == nullptr || !snapshotsReady_)
            return false;

        for (std::size_t index = 0; index < dataSnapshots_.size(); ++index)
        {
            auto& archive = dataSnapshots_[index];
            archive.SetReadModeAndResetPos(true);
            wi::ecs::EntitySerializer serializer;
            serializer.allow_remap = false;
            if (scene_->Entity_Serialize(archive, serializer) != animationData_[index])
            {
                lastError_ = "Failed to restore retargeted native animation data during Redo.";
                return false;
            }
        }

        for (std::size_t index = 0; index < animationSnapshots_.size(); ++index)
        {
            auto& archive = animationSnapshots_[index];
            archive.SetReadModeAndResetPos(true);
            wi::ecs::EntitySerializer serializer;
            serializer.allow_remap = false;
            if (scene_->Entity_Serialize(archive, serializer) != animations_[index])
            {
                lastError_ = "Failed to restore retargeted native animation during Redo.";
                return false;
            }
        }
        return true;
    }

    InverseKinematicsState CaptureInverseKinematics(
        const wi::scene::InverseKinematicsComponent& ik) noexcept
    {
        InverseKinematicsState result;
        result.enabled = !ik.IsDisabled();
        result.target = ik.target;
        result.chainLength = ik.chain_length;
        result.iterations = ik.iteration_count;
        return result;
    }

    InverseKinematicsState SanitizeInverseKinematics(
        const wi::scene::Scene& scene,
        const InverseKinematicsState& state) noexcept
    {
        InverseKinematicsState result = state;
        if (!EntityHasTransform(scene, result.target))
            result.target = wi::ecs::INVALID_ENTITY;
        result.chainLength = std::min<std::uint32_t>(result.chainLength, 64u);
        result.iterations = std::clamp<std::uint32_t>(result.iterations, 1u, 64u);
        return result;
    }

    void ApplyInverseKinematics(
        wi::scene::InverseKinematicsComponent& ik,
        const InverseKinematicsState& state) noexcept
    {
        ik.SetDisabled(!state.enabled);
        ik.target = state.target;
        ik.chain_length = state.chainLength;
        ik.iteration_count = state.iterations;
    }

    SetInverseKinematicsCommand::SetInverseKinematicsCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        InverseKinematicsState state,
        const bool createIfMissing)
        : scene_(&scene)
        , entity_(entity)
        , after_(SanitizeInverseKinematics(scene, state))
        , createIfMissing_(createIfMissing)
    {
        if (const auto* ik = scene.inverse_kinematics.GetComponent(entity_))
        {
            existedBefore_ = true;
            before_ = CaptureInverseKinematics(*ik);
        }
    }

    bool SetInverseKinematicsCommand::Execute()
    {
        if (existedBefore_ && !HasIkChange(before_, after_))
            return false;
        return Apply(after_, createIfMissing_);
    }

    void SetInverseKinematicsCommand::Undo()
    {
        if (scene_ == nullptr)
            return;
        if (!existedBefore_)
        {
            scene_->inverse_kinematics.Remove(entity_);
            return;
        }
        (void)Apply(before_, true);
    }

    bool SetInverseKinematicsCommand::Apply(
        const InverseKinematicsState& state,
        const bool create) noexcept
    {
        if (scene_ == nullptr)
            return false;
        auto* ik = scene_->inverse_kinematics.GetComponent(entity_);
        if (ik == nullptr && create)
            ik = &scene_->inverse_kinematics.Create(entity_);
        if (ik == nullptr)
            return false;
        ApplyInverseKinematics(*ik, SanitizeInverseKinematics(*scene_, state));
        return true;
    }

    ExpressionMasterState CaptureExpressionMaster(
        const wi::scene::ExpressionComponent& expressions) noexcept
    {
        ExpressionMasterState state;
        state.forceTalking = expressions.IsForceTalkingEnabled();
        state.blinkFrequency = expressions.blink_frequency;
        state.blinkLength = expressions.blink_length;
        state.blinkCount = expressions.blink_count;
        state.lookFrequency = expressions.look_frequency;
        state.lookLength = expressions.look_length;
        return state;
    }

    ExpressionItemState CaptureExpressionItem(
        const wi::scene::ExpressionComponent& expressions,
        const std::size_t index) noexcept
    {
        ExpressionItemState state;
        state.index = index;
        if (index >= expressions.expressions.size())
            return state;
        const auto& expression = expressions.expressions[index];
        state.weight = expression.weight;
        state.binary = expression.IsBinary();
        state.mouth = expression.override_mouth;
        state.blink = expression.override_blink;
        state.look = expression.override_look;
        return state;
    }

    SetExpressionMasterCommand::SetExpressionMasterCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        ExpressionMasterState state)
        : scene_(&scene)
        , entity_(entity)
        , after_(state)
    {
        after_.blinkFrequency = std::clamp(FiniteOr(after_.blinkFrequency, 0.0f), 0.0f, 1.0f);
        after_.blinkLength = std::clamp(FiniteOr(after_.blinkLength, 0.1f), 0.0f, 1.0f);
        after_.blinkCount = std::clamp(after_.blinkCount, 1, 4);
        after_.lookFrequency = std::clamp(FiniteOr(after_.lookFrequency, 0.0f), 0.0f, 1.0f);
        after_.lookLength = std::clamp(FiniteOr(after_.lookLength, 0.6f), 0.0f, 1.0f);
        if (const auto* expression = scene.expressions.GetComponent(entity_))
            before_ = CaptureExpressionMaster(*expression);
    }

    bool SetExpressionMasterCommand::Execute()
    {
        return HasExpressionMasterChange(before_, after_) && Apply(after_);
    }

    void SetExpressionMasterCommand::Undo()
    {
        (void)Apply(before_);
    }

    bool SetExpressionMasterCommand::Apply(const ExpressionMasterState& state) noexcept
    {
        if (scene_ == nullptr)
            return false;
        auto* expressions = scene_->expressions.GetComponent(entity_);
        if (expressions == nullptr)
            return false;
        expressions->SetForceTalkingEnabled(state.forceTalking);
        expressions->blink_frequency = std::clamp(state.blinkFrequency, 0.0f, 1.0f);
        expressions->blink_length = std::clamp(state.blinkLength, 0.0f, 1.0f);
        expressions->blink_count = std::clamp(state.blinkCount, 1, 4);
        expressions->look_frequency = std::clamp(state.lookFrequency, 0.0f, 1.0f);
        expressions->look_length = std::clamp(state.lookLength, 0.0f, 1.0f);
        return true;
    }

    SetExpressionItemCommand::SetExpressionItemCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        ExpressionItemState state)
        : scene_(&scene)
        , entity_(entity)
        , after_(state)
    {
        after_.weight = std::clamp(FiniteOr(after_.weight, 0.0f), 0.0f, 1.0f);
        if (const auto* expressions = scene.expressions.GetComponent(entity_))
            before_ = CaptureExpressionItem(*expressions, after_.index);
    }

    bool SetExpressionItemCommand::Execute()
    {
        return HasExpressionItemChange(before_, after_) && Apply(after_);
    }

    void SetExpressionItemCommand::Undo()
    {
        (void)Apply(before_);
    }

    bool SetExpressionItemCommand::Apply(const ExpressionItemState& state) noexcept
    {
        if (scene_ == nullptr)
            return false;
        auto* expressions = scene_->expressions.GetComponent(entity_);
        if (expressions == nullptr || state.index >= expressions->expressions.size())
            return false;
        auto& expression = expressions->expressions[state.index];
        expression.weight = std::clamp(FiniteOr(state.weight, 0.0f), 0.0f, 1.0f);
        expression.SetBinary(state.binary);
        expression.override_mouth = state.mouth;
        expression.override_blink = state.blink;
        expression.override_look = state.look;
        expression.SetDirty();
        return true;
    }

    wi::ecs::Entity CreateNativeAnimation(
        wi::scene::Scene& scene,
        const wi::ecs::Entity owner,
        const std::string& name)
    {
        const wi::ecs::Entity entity = wi::ecs::CreateEntity();
        scene.animations.Create(entity);
        scene.names.Create(entity).name = name.empty() ? "Animation" : name;
        if (owner != wi::ecs::INVALID_ENTITY)
            scene.Component_Attach(entity, owner);
        return entity;
    }

    std::vector<TimelineChannelInfo> CollectTimelineChannels(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity)
    {
        std::vector<TimelineChannelInfo> result;
        const auto* animation = scene.animations.GetComponent(animationEntity);
        if (animation == nullptr)
            return result;
        result.reserve(animation->channels.size());
        for (std::size_t index = 0; index < animation->channels.size(); ++index)
        {
            const auto& channel = animation->channels[index];
            TimelineChannelInfo info;
            info.channelIndex = index;
            info.target = channel.target;
            info.path = channel.path;
            if (channel.samplerIndex >= 0 &&
                static_cast<std::size_t>(channel.samplerIndex) < animation->samplers.size())
            {
                const auto& sampler = animation->samplers[channel.samplerIndex];
                info.interpolation = InterpolationOf(sampler.mode);
                if (const auto* data = scene.animation_datas.GetComponent(sampler.data))
                    info.keyCount = data->keyframe_times.size();
            }
            result.push_back(info);
        }
        return result;
    }

    bool RecordAnimationKey(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity,
        const wi::ecs::Entity target,
        const wi::scene::AnimationComponent::AnimationChannel::Path path,
        const AnimationInterpolation interpolation,
        const float time,
        const std::vector<float>& values,
        std::string& error)
    {
        error.clear();
        if (!std::isfinite(time) || time < 0.0f)
        {
            error = "Animation key time must be a finite value at or above zero.";
            return false;
        }
        auto* animation = scene.animations.GetComponent(animationEntity);
        if (animation == nullptr)
        {
            error = "The requested native AnimationComponent does not exist.";
            return false;
        }
        if (target == wi::ecs::INVALID_ENTITY)
        {
            error = "Animation recording requires a valid target entity.";
            return false;
        }

        std::size_t channelIndex = animation->channels.size();
        for (std::size_t index = 0; index < animation->channels.size(); ++index)
        {
            if (animation->channels[index].target == target &&
                animation->channels[index].path == path)
            {
                channelIndex = index;
                break;
            }
        }

        bool createdChannel = false;
        if (channelIndex == animation->channels.size())
        {
            auto& channel = animation->channels.emplace_back();
            channel.target = target;
            channel.path = path;
            channel.samplerIndex = static_cast<int>(animation->samplers.size());
            auto& sampler = animation->samplers.emplace_back();
            sampler.mode = NativeInterpolation(interpolation);
            sampler.data = wi::ecs::CreateEntity();
            scene.animation_datas.Create(sampler.data);
            createdChannel = true;
        }

        auto& channel = animation->channels[channelIndex];
        if (channel.samplerIndex < 0 ||
            static_cast<std::size_t>(channel.samplerIndex) >= animation->samplers.size())
        {
            error = "Animation channel references an invalid native sampler.";
            if (createdChannel)
                animation->channels.pop_back();
            return false;
        }

        auto& sampler = animation->samplers[channel.samplerIndex];
        auto* data = scene.animation_datas.GetComponent(sampler.data);
        if (data == nullptr)
        {
            error = "Animation sampler references missing native AnimationDataComponent.";
            if (createdChannel)
            {
                animation->samplers.pop_back();
                animation->channels.pop_back();
            }
            return false;
        }

        std::string countError;
        const std::size_t baseCount = BaseValueCount(scene, target, channel, countError);
        if (baseCount == std::numeric_limits<std::size_t>::max())
        {
            error = countError;
            return false;
        }
        const bool eventPath = baseCount == 0;
        if (eventPath && interpolation == AnimationInterpolation::CubicSpline)
        {
            error = "Event animation channels do not support cubic-spline interpolation.";
            return false;
        }
        const std::size_t expected = interpolation == AnimationInterpolation::CubicSpline
            ? baseCount * 3u
            : baseCount;
        if (values.size() != expected)
        {
            error = "Animation key value count does not match the native Wicked path data type.";
            return false;
        }

        const std::size_t oldKeyCount = data->keyframe_times.size();
        std::size_t oldStride = expected;
        if (oldKeyCount > 0)
        {
            if (data->keyframe_data.size() % oldKeyCount != 0)
            {
                error = "Existing native animation data has an invalid key/value stride.";
                return false;
            }
            oldStride = data->keyframe_data.size() / oldKeyCount;
            if (oldStride != expected)
            {
                error = "Changing interpolation would invalidate existing native keyframe data. Create a new channel or retain its current sampling mode.";
                return false;
            }
        }

        sampler.mode = NativeInterpolation(interpolation);
        auto position = std::lower_bound(
            data->keyframe_times.begin(), data->keyframe_times.end(), time);
        const std::size_t insertIndex = static_cast<std::size_t>(
            position - data->keyframe_times.begin());
        const bool replace = position != data->keyframe_times.end() &&
            NearlyEqual(*position, time);

        if (replace)
        {
            *position = time;
            if (expected > 0)
            {
                const std::size_t base = insertIndex * expected;
                std::copy(values.begin(), values.end(), data->keyframe_data.begin() + base);
            }
        }
        else
        {
            data->keyframe_times.insert(position, time);
            if (expected > 0)
            {
                data->keyframe_data.insert(
                    data->keyframe_data.begin() + static_cast<std::ptrdiff_t>(insertIndex * expected),
                    values.begin(), values.end());
            }
        }

        if (createdChannel && oldKeyCount == 0)
        {
            animation->start = std::min(animation->start, time);
            animation->end = std::max(animation->end, time);
        }
        else
        {
            animation->start = std::min(animation->start, time);
            animation->end = std::max(animation->end, time);
        }
        animation->timer = std::clamp(animation->timer, animation->start, animation->end);
        return true;
    }

    bool CaptureCurrentAnimationValue(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity target,
        const wi::scene::AnimationComponent::AnimationChannel::Path path,
        std::vector<float>& values,
        std::string& error)
    {
        using Path = wi::scene::AnimationComponent::AnimationChannel::Path;
        values.clear();
        error.clear();

        switch (path)
        {
        case Path::TRANSLATION:
        case Path::ROTATION:
        case Path::SCALE:
        {
            const auto* transform = scene.transforms.GetComponent(target);
            if (transform == nullptr)
            {
                error = "Transform animation requires a TransformComponent target.";
                return false;
            }
            if (path == Path::TRANSLATION)
                values = {transform->translation_local.x, transform->translation_local.y, transform->translation_local.z};
            else if (path == Path::ROTATION)
                values = {transform->rotation_local.x, transform->rotation_local.y, transform->rotation_local.z, transform->rotation_local.w};
            else
                values = {transform->scale_local.x, transform->scale_local.y, transform->scale_local.z};
            return true;
        }
        case Path::WEIGHTS:
        {
            const auto* mesh = scene.meshes.GetComponent(target);
            if (mesh == nullptr || mesh->morph_targets.empty())
            {
                error = "Morph recording requires a MeshComponent with morph targets.";
                return false;
            }
            values.reserve(mesh->morph_targets.size());
            for (const auto& morph : mesh->morph_targets)
                values.push_back(morph.weight);
            return true;
        }
        case Path::LIGHT_COLOR:
        case Path::LIGHT_INTENSITY:
        case Path::LIGHT_RANGE:
        case Path::LIGHT_INNERCONE:
        case Path::LIGHT_OUTERCONE:
        {
            const auto* light = scene.lights.GetComponent(target);
            if (light == nullptr)
            {
                error = "Light animation requires a LightComponent target.";
                return false;
            }
            if (path == Path::LIGHT_COLOR)
                values = {light->color.x, light->color.y, light->color.z};
            else if (path == Path::LIGHT_INTENSITY)
                values = {light->intensity};
            else if (path == Path::LIGHT_RANGE)
                values = {light->range};
            else if (path == Path::LIGHT_INNERCONE)
                values = {light->innerConeAngle};
            else
                values = {light->outerConeAngle};
            return true;
        }
        case Path::SOUND_PLAY:
        case Path::SOUND_STOP:
        case Path::SCRIPT_PLAY:
        case Path::SCRIPT_STOP:
            return true; // native Event paths carry time only
        case Path::SOUND_VOLUME:
        {
            const auto* sound = scene.sounds.GetComponent(target);
            if (sound == nullptr)
            {
                error = "Sound animation requires a SoundComponent target.";
                return false;
            }
            values = {sound->volume};
            return true;
        }
        case Path::EMITTER_EMITCOUNT:
        {
            const auto* emitter = scene.emitters.GetComponent(target);
            if (emitter == nullptr)
            {
                error = "Emitter animation requires an EmittedParticleSystem target.";
                return false;
            }
            values = {emitter->count};
            return true;
        }
        case Path::CAMERA_FOV:
        case Path::CAMERA_FOCAL_LENGTH:
        case Path::CAMERA_APERTURE_SIZE:
        case Path::CAMERA_APERTURE_SHAPE:
        {
            const auto* camera = scene.cameras.GetComponent(target);
            if (camera == nullptr)
            {
                error = "Camera animation requires a CameraComponent target.";
                return false;
            }
            if (path == Path::CAMERA_FOV)
                values = {camera->fov};
            else if (path == Path::CAMERA_FOCAL_LENGTH)
                values = {camera->focal_length};
            else if (path == Path::CAMERA_APERTURE_SIZE)
                values = {camera->aperture_size};
            else
                values = {camera->aperture_shape.x, camera->aperture_shape.y};
            return true;
        }
        case Path::MATERIAL_COLOR:
        case Path::MATERIAL_EMISSIVE:
        case Path::MATERIAL_ROUGHNESS:
        case Path::MATERIAL_METALNESS:
        case Path::MATERIAL_REFLECTANCE:
        case Path::MATERIAL_TEXMULADD:
        {
            const auto* material = scene.materials.GetComponent(target);
            if (material == nullptr)
            {
                error = "Material animation requires a MaterialComponent target.";
                return false;
            }
            if (path == Path::MATERIAL_COLOR)
                values = {material->baseColor.x, material->baseColor.y, material->baseColor.z, material->baseColor.w};
            else if (path == Path::MATERIAL_EMISSIVE)
                values = {material->emissiveColor.x, material->emissiveColor.y, material->emissiveColor.z, material->emissiveColor.w};
            else if (path == Path::MATERIAL_ROUGHNESS)
                values = {material->roughness};
            else if (path == Path::MATERIAL_METALNESS)
                values = {material->metalness};
            else if (path == Path::MATERIAL_REFLECTANCE)
                values = {material->reflectance};
            else
                values = {material->texMulAdd.x, material->texMulAdd.y, material->texMulAdd.z, material->texMulAdd.w};
            return true;
        }
        default:
            error = "The selected native Wicked animation path is not exposed by the Phase 7 timeline recorder.";
            return false;
        }
    }

    RecordAnimationKeyCommand::RecordAnimationKeyCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity,
        const wi::ecs::Entity target,
        const wi::scene::AnimationComponent::AnimationChannel::Path path,
        const AnimationInterpolation interpolation,
        const float time,
        std::vector<float> values)
        : scene_(&scene)
        , animation_(animationEntity)
        , target_(target)
        , path_(path)
        , interpolation_(interpolation)
        , time_(time)
        , values_(std::move(values))
    {
    }

    bool RecordAnimationKeyCommand::Execute()
    {
        lastError_.clear();
        if (scene_ == nullptr)
            return false;
        if (captured_)
            return Restore(afterAnimation_, afterData_);

        CaptureBefore();
        if (!RecordAnimationKey(
                *scene_, animation_, target_, path_, interpolation_, time_, values_, lastError_))
        {
            return false;
        }
        CaptureAfter();
        captured_ = true;
        return true;
    }

    void RecordAnimationKeyCommand::Undo()
    {
        if (scene_ != nullptr && captured_)
            (void)Restore(beforeAnimation_, beforeData_);
    }

    const std::string& RecordAnimationKeyCommand::LastError() const noexcept
    {
        return lastError_;
    }

    void RecordAnimationKeyCommand::CaptureBefore()
    {
        if (scene_ == nullptr)
            return;
        const auto* animation = scene_->animations.GetComponent(animation_);
        if (animation == nullptr)
            return;
        beforeAnimation_ = *animation;
        (void)CopyReferencedAnimationData(*scene_, *animation, beforeData_);
    }

    void RecordAnimationKeyCommand::CaptureAfter()
    {
        if (scene_ == nullptr)
            return;
        const auto* animation = scene_->animations.GetComponent(animation_);
        if (animation == nullptr)
            return;
        afterAnimation_ = *animation;
        (void)CopyReferencedAnimationData(*scene_, *animation, afterData_);
    }

    bool RecordAnimationKeyCommand::Restore(
        const wi::scene::AnimationComponent& animation,
        const std::vector<std::pair<wi::ecs::Entity,
            wi::scene::AnimationDataComponent>>& data) noexcept
    {
        if (scene_ == nullptr)
            return false;
        auto* current = scene_->animations.GetComponent(animation_);
        if (current == nullptr)
            return false;

        std::unordered_set<wi::ecs::Entity> retained;
        for (const auto& item : data)
            retained.insert(item.first);
        for (const auto& sampler : current->samplers)
        {
            if (sampler.data != wi::ecs::INVALID_ENTITY &&
                retained.find(sampler.data) == retained.end())
            {
                scene_->animation_datas.Remove(sampler.data);
            }
        }

        *current = animation;
        for (const auto& item : data)
        {
            auto* component = scene_->animation_datas.GetComponent(item.first);
            if (component == nullptr)
                component = &scene_->animation_datas.Create(item.first);
            *component = item.second;
        }
        return true;
    }
}
