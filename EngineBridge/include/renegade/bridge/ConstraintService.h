#pragma once

#include <WickedEngine.h>

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    // JP01 creator-facing constraint contract. It mirrors the complete
    // PhysicsConstraintComponent surface that Wicked Editor currently exposes,
    // while Wicked's native component remains the serialized source of truth.
    struct ConstraintState
    {
        using Type = wi::scene::PhysicsConstraintComponent::Type;

        Type type = Type::Fixed;
        wi::ecs::Entity bodyA = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity bodyB = wi::ecs::INVALID_ENTITY;
        bool disableSelfCollision = false;
        float breakDistance = FLT_MAX;

        float distanceMin = 0.0f;
        float distanceMax = 0.0f;

        float hingeMinAngle = -XM_PI;
        float hingeMaxAngle = XM_PI;
        float hingeTargetAngularVelocity = 0.0f;

        float coneHalfAngle = 0.0f;

        XMFLOAT3 sixDofMinTranslation = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        XMFLOAT3 sixDofMaxTranslation = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
        XMFLOAT3 sixDofMinRotation = XMFLOAT3(-XM_PI, -XM_PI, -XM_PI);
        XMFLOAT3 sixDofMaxRotation = XMFLOAT3(XM_PI, XM_PI, XM_PI);

        float swingTwistNormalHalfConeAngle = 0.0f;
        float swingTwistPlaneHalfConeAngle = 0.0f;
        float swingTwistMinAngle = 0.0f;
        float swingTwistMaxAngle = 0.0f;

        float sliderMinLimit = -FLT_MAX;
        float sliderMaxLimit = FLT_MAX;
        float sliderTargetVelocity = 0.0f;
        float sliderMaxForce = 0.0f;
    };

    namespace constraint_detail
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

        inline bool NearlyEqual(const XMFLOAT3& left, const XMFLOAT3& right) noexcept
        {
            return NearlyEqual(left.x, right.x) &&
                NearlyEqual(left.y, right.y) &&
                NearlyEqual(left.z, right.z);
        }

        inline bool IsSupportedType(
            const wi::scene::PhysicsConstraintComponent::Type type) noexcept
        {
            using Type = wi::scene::PhysicsConstraintComponent::Type;
            switch (type)
            {
            case Type::Fixed:
            case Type::Point:
            case Type::Distance:
            case Type::Hinge:
            case Type::Cone:
            case Type::SixDOF:
            case Type::SwingTwist:
            case Type::Slider:
                return true;
            default:
                return false;
            }
        }

        inline bool RequiresNativeRebind(
            const ConstraintState& before,
            const ConstraintState& after) noexcept
        {
            return before.type != after.type ||
                before.bodyA != after.bodyA ||
                before.bodyB != after.bodyB ||
                before.disableSelfCollision != after.disableSelfCollision;
        }
    }

    [[nodiscard]] inline ConstraintState CaptureConstraint(
        const wi::scene::PhysicsConstraintComponent& constraint) noexcept
    {
        ConstraintState state;
        state.type = constraint.type;
        state.bodyA = constraint.bodyA;
        state.bodyB = constraint.bodyB;
        state.disableSelfCollision = constraint.IsDisableSelfCollision();
        state.breakDistance = constraint.break_distance;

        state.distanceMin = constraint.distance_constraint.min_distance;
        state.distanceMax = constraint.distance_constraint.max_distance;

        state.hingeMinAngle = constraint.hinge_constraint.min_angle;
        state.hingeMaxAngle = constraint.hinge_constraint.max_angle;
        state.hingeTargetAngularVelocity =
            constraint.hinge_constraint.target_angular_velocity;

        state.coneHalfAngle = constraint.cone_constraint.half_cone_angle;

        state.sixDofMinTranslation = constraint.six_dof.minTranslationAxes;
        state.sixDofMaxTranslation = constraint.six_dof.maxTranslationAxes;
        state.sixDofMinRotation = constraint.six_dof.minRotationAxes;
        state.sixDofMaxRotation = constraint.six_dof.maxRotationAxes;

        state.swingTwistNormalHalfConeAngle =
            constraint.swing_twist.normal_half_cone_angle;
        state.swingTwistPlaneHalfConeAngle =
            constraint.swing_twist.plane_half_cone_angle;
        state.swingTwistMinAngle = constraint.swing_twist.min_twist_angle;
        state.swingTwistMaxAngle = constraint.swing_twist.max_twist_angle;

        state.sliderMinLimit = constraint.slider_constraint.min_limit;
        state.sliderMaxLimit = constraint.slider_constraint.max_limit;
        state.sliderTargetVelocity = constraint.slider_constraint.target_velocity;
        state.sliderMaxForce = constraint.slider_constraint.max_force;
        return state;
    }

    [[nodiscard]] inline ConstraintState SanitizeConstraintState(
        const ConstraintState& state) noexcept
    {
        using Type = wi::scene::PhysicsConstraintComponent::Type;
        ConstraintState result = state;
        if (!constraint_detail::IsSupportedType(result.type))
        {
            result.type = Type::Fixed;
        }

        result.breakDistance = std::max(
            0.0f,
            constraint_detail::FiniteOr(result.breakDistance, FLT_MAX));
        result.distanceMin = std::max(
            0.0f,
            constraint_detail::FiniteOr(result.distanceMin, 0.0f));
        result.distanceMax = std::max(
            0.0f,
            constraint_detail::FiniteOr(result.distanceMax, 0.0f));

        result.hingeMinAngle = std::clamp(
            constraint_detail::FiniteOr(result.hingeMinAngle, -XM_PI),
            -XM_PI,
            XM_PI);
        result.hingeMaxAngle = std::clamp(
            constraint_detail::FiniteOr(result.hingeMaxAngle, XM_PI),
            -XM_PI,
            XM_PI);
        result.hingeTargetAngularVelocity = constraint_detail::FiniteOr(
            result.hingeTargetAngularVelocity,
            0.0f);

        result.coneHalfAngle = std::clamp(
            constraint_detail::FiniteOr(result.coneHalfAngle, 0.0f),
            0.0f,
            XM_PIDIV2);

        auto sanitizeTranslation = [](const XMFLOAT3& value, const XMFLOAT3& fallback) {
            return XMFLOAT3(
                constraint_detail::FiniteOr(value.x, fallback.x),
                constraint_detail::FiniteOr(value.y, fallback.y),
                constraint_detail::FiniteOr(value.z, fallback.z));
        };
        auto sanitizeRotation = [](const XMFLOAT3& value, const XMFLOAT3& fallback) {
            return XMFLOAT3(
                std::clamp(constraint_detail::FiniteOr(value.x, fallback.x), -XM_PI, XM_PI),
                std::clamp(constraint_detail::FiniteOr(value.y, fallback.y), -XM_PI, XM_PI),
                std::clamp(constraint_detail::FiniteOr(value.z, fallback.z), -XM_PI, XM_PI));
        };

        result.sixDofMinTranslation = sanitizeTranslation(
            result.sixDofMinTranslation,
            XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX));
        result.sixDofMaxTranslation = sanitizeTranslation(
            result.sixDofMaxTranslation,
            XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX));
        result.sixDofMinRotation = sanitizeRotation(
            result.sixDofMinRotation,
            XMFLOAT3(-XM_PI, -XM_PI, -XM_PI));
        result.sixDofMaxRotation = sanitizeRotation(
            result.sixDofMaxRotation,
            XMFLOAT3(XM_PI, XM_PI, XM_PI));

        result.swingTwistNormalHalfConeAngle = std::clamp(
            constraint_detail::FiniteOr(
                result.swingTwistNormalHalfConeAngle,
                0.0f),
            0.0f,
            XM_PIDIV2);
        result.swingTwistPlaneHalfConeAngle = std::clamp(
            constraint_detail::FiniteOr(
                result.swingTwistPlaneHalfConeAngle,
                0.0f),
            0.0f,
            XM_PIDIV2);
        result.swingTwistMinAngle = std::clamp(
            constraint_detail::FiniteOr(result.swingTwistMinAngle, 0.0f),
            -XM_PI,
            XM_PI);
        result.swingTwistMaxAngle = std::clamp(
            constraint_detail::FiniteOr(result.swingTwistMaxAngle, 0.0f),
            -XM_PI,
            XM_PI);

        result.sliderMinLimit = constraint_detail::FiniteOr(
            result.sliderMinLimit,
            -FLT_MAX);
        result.sliderMaxLimit = constraint_detail::FiniteOr(
            result.sliderMaxLimit,
            FLT_MAX);
        result.sliderTargetVelocity = constraint_detail::FiniteOr(
            result.sliderTargetVelocity,
            0.0f);
        result.sliderMaxForce = std::max(
            0.0f,
            constraint_detail::FiniteOr(result.sliderMaxForce, 0.0f));
        return result;
    }

    [[nodiscard]] inline bool HasConstraintStateChange(
        const ConstraintState& before,
        const ConstraintState& after) noexcept
    {
        const auto left = SanitizeConstraintState(before);
        const auto right = SanitizeConstraintState(after);
        return left.type != right.type ||
            left.bodyA != right.bodyA ||
            left.bodyB != right.bodyB ||
            left.disableSelfCollision != right.disableSelfCollision ||
            !constraint_detail::NearlyEqual(left.breakDistance, right.breakDistance) ||
            !constraint_detail::NearlyEqual(left.distanceMin, right.distanceMin) ||
            !constraint_detail::NearlyEqual(left.distanceMax, right.distanceMax) ||
            !constraint_detail::NearlyEqual(left.hingeMinAngle, right.hingeMinAngle) ||
            !constraint_detail::NearlyEqual(left.hingeMaxAngle, right.hingeMaxAngle) ||
            !constraint_detail::NearlyEqual(
                left.hingeTargetAngularVelocity,
                right.hingeTargetAngularVelocity) ||
            !constraint_detail::NearlyEqual(left.coneHalfAngle, right.coneHalfAngle) ||
            !constraint_detail::NearlyEqual(
                left.sixDofMinTranslation,
                right.sixDofMinTranslation) ||
            !constraint_detail::NearlyEqual(
                left.sixDofMaxTranslation,
                right.sixDofMaxTranslation) ||
            !constraint_detail::NearlyEqual(
                left.sixDofMinRotation,
                right.sixDofMinRotation) ||
            !constraint_detail::NearlyEqual(
                left.sixDofMaxRotation,
                right.sixDofMaxRotation) ||
            !constraint_detail::NearlyEqual(
                left.swingTwistNormalHalfConeAngle,
                right.swingTwistNormalHalfConeAngle) ||
            !constraint_detail::NearlyEqual(
                left.swingTwistPlaneHalfConeAngle,
                right.swingTwistPlaneHalfConeAngle) ||
            !constraint_detail::NearlyEqual(
                left.swingTwistMinAngle,
                right.swingTwistMinAngle) ||
            !constraint_detail::NearlyEqual(
                left.swingTwistMaxAngle,
                right.swingTwistMaxAngle) ||
            !constraint_detail::NearlyEqual(left.sliderMinLimit, right.sliderMinLimit) ||
            !constraint_detail::NearlyEqual(left.sliderMaxLimit, right.sliderMaxLimit) ||
            !constraint_detail::NearlyEqual(
                left.sliderTargetVelocity,
                right.sliderTargetVelocity) ||
            !constraint_detail::NearlyEqual(left.sliderMaxForce, right.sliderMaxForce);
    }

    inline void ApplyConstraint(
        wi::scene::PhysicsConstraintComponent& constraint,
        const ConstraintState& state) noexcept
    {
        const auto before = CaptureConstraint(constraint);
        const auto safe = SanitizeConstraintState(state);

        // Match Wicked Editor semantics: type/body/self-collision changes alter
        // binding topology and must recreate the native Jolt constraint.
        if (constraint_detail::RequiresNativeRebind(before, safe))
        {
            constraint.physicsobject.reset();
        }

        constraint.type = safe.type;
        constraint.bodyA = safe.bodyA;
        constraint.bodyB = safe.bodyB;
        constraint.SetDisableSelfCollision(safe.disableSelfCollision);
        constraint.break_distance = safe.breakDistance;

        constraint.distance_constraint.min_distance = safe.distanceMin;
        constraint.distance_constraint.max_distance = safe.distanceMax;

        constraint.hinge_constraint.min_angle = safe.hingeMinAngle;
        constraint.hinge_constraint.max_angle = safe.hingeMaxAngle;
        constraint.hinge_constraint.target_angular_velocity =
            safe.hingeTargetAngularVelocity;

        constraint.cone_constraint.half_cone_angle = safe.coneHalfAngle;

        constraint.six_dof.minTranslationAxes = safe.sixDofMinTranslation;
        constraint.six_dof.maxTranslationAxes = safe.sixDofMaxTranslation;
        constraint.six_dof.minRotationAxes = safe.sixDofMinRotation;
        constraint.six_dof.maxRotationAxes = safe.sixDofMaxRotation;

        constraint.swing_twist.normal_half_cone_angle =
            safe.swingTwistNormalHalfConeAngle;
        constraint.swing_twist.plane_half_cone_angle =
            safe.swingTwistPlaneHalfConeAngle;
        constraint.swing_twist.min_twist_angle = safe.swingTwistMinAngle;
        constraint.swing_twist.max_twist_angle = safe.swingTwistMaxAngle;

        constraint.slider_constraint.min_limit = safe.sliderMinLimit;
        constraint.slider_constraint.max_limit = safe.sliderMaxLimit;
        constraint.slider_constraint.target_velocity = safe.sliderTargetVelocity;
        constraint.slider_constraint.max_force = safe.sliderMaxForce;

        if (HasConstraintStateChange(before, safe))
        {
            constraint.SetRefreshParametersNeeded(true);
        }
    }

    [[nodiscard]] inline bool RebindConstraint(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        if (!scene.transforms.Contains(entity))
        {
            return false;
        }
        auto* constraint = scene.constraints.GetComponent(entity);
        if (constraint == nullptr)
        {
            return false;
        }
        constraint->physicsobject.reset();
        constraint->SetRefreshParametersNeeded(true);
        return true;
    }

    class CreateConstraintCommand final : public ICommand
    {
    public:
        CreateConstraintCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const ConstraintState& initial = {})
            : scene_(&scene)
            , entity_(entity)
            , initial_(SanitizeConstraintState(initial))
        {
        }

        bool Execute() override
        {
            if (scene_ == nullptr ||
                entity_ == wi::ecs::INVALID_ENTITY ||
                scene_->constraints.Contains(entity_) ||
                !scene_->transforms.Contains(entity_))
            {
                return false;
            }
            auto& constraint = scene_->constraints.Create(entity_);
            ApplyConstraint(constraint, initial_);
            return true;
        }

        void Undo() override
        {
            if (scene_ != nullptr)
            {
                scene_->constraints.Remove(entity_);
            }
        }

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        ConstraintState initial_;
    };

    class SetConstraintCommand final : public ICommand
    {
    public:
        SetConstraintCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const ConstraintState& constraint)
            : scene_(&scene)
            , entity_(entity)
            , after_(SanitizeConstraintState(constraint))
        {
            if (const auto* existing = scene.constraints.GetComponent(entity))
            {
                before_ = CaptureConstraint(*existing);
            }
        }

        SetConstraintCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const ConstraintState& before,
            const ConstraintState& after)
            : scene_(&scene)
            , entity_(entity)
            , before_(SanitizeConstraintState(before))
            , after_(SanitizeConstraintState(after))
        {
        }

        bool Execute() override
        {
            return HasConstraintStateChange(before_, after_) && Apply(after_);
        }

        void Undo() override
        {
            Apply(before_);
        }

    private:
        bool Apply(const ConstraintState& state) noexcept
        {
            if (scene_ == nullptr || !scene_->transforms.Contains(entity_))
            {
                return false;
            }
            auto* constraint = scene_->constraints.GetComponent(entity_);
            if (constraint == nullptr)
            {
                return false;
            }
            ApplyConstraint(*constraint, state);
            return true;
        }

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        ConstraintState before_;
        ConstraintState after_;
    };

    class RemoveConstraintCommand final : public ICommand
    {
    public:
        RemoveConstraintCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity)
            : scene_(&scene)
            , entity_(entity)
        {
        }

        bool Execute() override
        {
            if (scene_ == nullptr)
            {
                return false;
            }
            const auto* existing = scene_->constraints.GetComponent(entity_);
            if (existing == nullptr)
            {
                return false;
            }

            removedNative_ = *existing;
            removedNative_.physicsobject.reset();
            removedNative_.SetRefreshParametersNeeded(true);
            hasRemoved_ = true;
            scene_->constraints.Remove(entity_);
            return true;
        }

        void Undo() override
        {
            if (scene_ == nullptr ||
                !hasRemoved_ ||
                scene_->constraints.Contains(entity_))
            {
                return;
            }
            auto& constraint = scene_->constraints.Create(entity_);
            constraint = removedNative_;
            constraint.physicsobject.reset();
            constraint.SetRefreshParametersNeeded(true);
        }

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::scene::PhysicsConstraintComponent removedNative_;
        bool hasRemoved_ = false;
    };
}
