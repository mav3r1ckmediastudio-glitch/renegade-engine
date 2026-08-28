#pragma once

#include <WickedEngine.h>

#include <algorithm>
#include <cmath>
#include <memory>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    struct RagdollPhysicsState
    {
        bool disabled = false;
        bool physicsEnabled = false;
        bool locked2D = false;
        float fatness = 1.0f;
        float headSize = 1.0f;
    };

    namespace ragdoll_physics_detail
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

        inline wi::scene::HumanoidComponent* FindLiveRagdoll(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity) noexcept
        {
            auto* humanoid = scene.humanoids.GetComponent(entity);
            if (humanoid == nullptr ||
                humanoid->IsRagdollDisabled() ||
                humanoid->ragdoll == nullptr)
            {
                return nullptr;
            }
            return humanoid;
        }
    }

    [[nodiscard]] inline RagdollPhysicsState CaptureRagdollPhysics(
        const wi::scene::HumanoidComponent& humanoid) noexcept
    {
        RagdollPhysicsState state;
        state.disabled = humanoid.IsRagdollDisabled();
        state.physicsEnabled = humanoid.IsRagdollPhysicsEnabled();
        state.locked2D = humanoid.IsRagdoll2D();
        state.fatness = humanoid.ragdoll_fatness;
        state.headSize = humanoid.ragdoll_headsize;
        return state;
    }

    [[nodiscard]] inline RagdollPhysicsState SanitizeRagdollPhysicsState(
        const RagdollPhysicsState& state) noexcept
    {
        RagdollPhysicsState result = state;
        result.fatness = std::clamp(
            ragdoll_physics_detail::FiniteOr(result.fatness, 1.0f),
            0.5f,
            2.0f);
        result.headSize = std::clamp(
            ragdoll_physics_detail::FiniteOr(result.headSize, 1.0f),
            0.5f,
            2.0f);
        return result;
    }

    [[nodiscard]] inline bool HasRagdollPhysicsStateChange(
        const RagdollPhysicsState& before,
        const RagdollPhysicsState& after) noexcept
    {
        const auto left = SanitizeRagdollPhysicsState(before);
        const auto right = SanitizeRagdollPhysicsState(after);
        return left.disabled != right.disabled ||
            left.physicsEnabled != right.physicsEnabled ||
            left.locked2D != right.locked2D ||
            !ragdoll_physics_detail::NearlyEqual(left.fatness, right.fatness) ||
            !ragdoll_physics_detail::NearlyEqual(left.headSize, right.headSize);
    }

    inline void ApplyRagdollPhysics(
        wi::scene::HumanoidComponent& humanoid,
        const RagdollPhysicsState& state) noexcept
    {
        const auto before = CaptureRagdollPhysics(humanoid);
        const auto safe = SanitizeRagdollPhysicsState(state);

        const bool recreate = before.locked2D != safe.locked2D ||
            !ragdoll_physics_detail::NearlyEqual(before.fatness, safe.fatness) ||
            !ragdoll_physics_detail::NearlyEqual(before.headSize, safe.headSize);

        humanoid.SetRagdollDisabled(safe.disabled);
        humanoid.SetRagdollPhysicsEnabled(safe.physicsEnabled);
        humanoid.SetRagdoll2D(safe.locked2D);
        humanoid.ragdoll_fatness = safe.fatness;
        humanoid.ragdoll_headsize = safe.headSize;

        // Match Wicked Editor exactly: disabling destroys immediately; 2D,
        // fatness and head-size changes request ragdoll recreation. Re-enabling
        // leaves creation to Wicked's normal physics update.
        if (safe.disabled || recreate)
        {
            humanoid.ragdoll = {};
        }
    }

    class SetRagdollPhysicsCommand final : public ICommand
    {
    public:
        SetRagdollPhysicsCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const RagdollPhysicsState& state)
            : scene_(&scene)
            , entity_(entity)
            , after_(SanitizeRagdollPhysicsState(state))
        {
            if (const auto* humanoid = scene.humanoids.GetComponent(entity))
            {
                before_ = CaptureRagdollPhysics(*humanoid);
            }
        }

        SetRagdollPhysicsCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const RagdollPhysicsState& before,
            const RagdollPhysicsState& after)
            : scene_(&scene)
            , entity_(entity)
            , before_(SanitizeRagdollPhysicsState(before))
            , after_(SanitizeRagdollPhysicsState(after))
        {
        }

        bool Execute() override
        {
            return HasRagdollPhysicsStateChange(before_, after_) && Apply(after_);
        }

        void Undo() override
        {
            Apply(before_);
        }

    private:
        bool Apply(const RagdollPhysicsState& state) noexcept
        {
            if (scene_ == nullptr)
            {
                return false;
            }
            auto* humanoid = scene_->humanoids.GetComponent(entity_);
            if (humanoid == nullptr)
            {
                return false;
            }
            ApplyRagdollPhysics(*humanoid, state);
            return true;
        }

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        RagdollPhysicsState before_;
        RagdollPhysicsState after_;
    };

    [[nodiscard]] inline bool HasLiveRagdollPhysics(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        const auto* humanoid = scene.humanoids.GetComponent(entity);
        return humanoid != nullptr &&
            !humanoid->IsRagdollDisabled() &&
            humanoid->ragdoll != nullptr;
    }

    [[nodiscard]] inline bool ApplyRagdollImpulse(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const wi::scene::HumanoidComponent::HumanoidBone bone,
        const XMFLOAT3& impulse) noexcept
    {
        auto* humanoid = ragdoll_physics_detail::FindLiveRagdoll(scene, entity);
        if (humanoid == nullptr)
        {
            return false;
        }
        wi::physics::ApplyImpulse(*humanoid, bone, impulse);
        return true;
    }

    [[nodiscard]] inline bool ApplyRagdollImpulseAt(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const wi::scene::HumanoidComponent::HumanoidBone bone,
        const XMFLOAT3& impulse,
        const XMFLOAT3& position,
        const bool positionIsLocal = true) noexcept
    {
        auto* humanoid = ragdoll_physics_detail::FindLiveRagdoll(scene, entity);
        if (humanoid == nullptr)
        {
            return false;
        }
        wi::physics::ApplyImpulseAt(
            *humanoid,
            bone,
            impulse,
            position,
            positionIsLocal);
        return true;
    }

    [[nodiscard]] inline bool SetRagdollGhostMode(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const bool ghost) noexcept
    {
        auto* humanoid = ragdoll_physics_detail::FindLiveRagdoll(scene, entity);
        if (humanoid == nullptr)
        {
            return false;
        }
        wi::physics::SetRagdollGhostMode(*humanoid, ghost);
        return true;
    }

    inline void ActivateAllRagdolls(wi::scene::Scene& scene) noexcept
    {
        for (size_t i = 0; i < scene.humanoids.GetCount(); ++i)
        {
            scene.humanoids[i].SetRagdollPhysicsEnabled(true);
        }
    }
}
