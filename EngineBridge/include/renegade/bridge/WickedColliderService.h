#pragma once

#include <WickedEngine.h>

#include <algorithm>
#include <cmath>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    // Wicked ColliderComponent is deliberately separate from Jolt. It provides
    // lightweight CPU/GPU collision primitives consumed by springs, emitters,
    // hair and related Wicked systems.
    struct WickedColliderState
    {
        using Shape = wi::scene::ColliderComponent::Shape;

        Shape shape = Shape::Sphere;
        bool cpuEnabled = true;
        bool gpuEnabled = true;
        float radius = 1.0f;
        XMFLOAT3 offset = XMFLOAT3(0, 0, 0);
        XMFLOAT3 tail = XMFLOAT3(0, 1, 0);
    };

    namespace wicked_collider_detail
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

        inline bool IsSupportedShape(const wi::scene::ColliderComponent::Shape shape) noexcept
        {
            using Shape = wi::scene::ColliderComponent::Shape;
            switch (shape)
            {
            case Shape::Sphere:
            case Shape::Capsule:
            case Shape::Plane:
                return true;
            default:
                return false;
            }
        }

        inline XMFLOAT3 ClampVector(const XMFLOAT3& value) noexcept
        {
            return XMFLOAT3(
                std::clamp(FiniteOr(value.x, 0.0f), -10.0f, 10.0f),
                std::clamp(FiniteOr(value.y, 0.0f), -10.0f, 10.0f),
                std::clamp(FiniteOr(value.z, 0.0f), -10.0f, 10.0f));
        }
    }

    [[nodiscard]] inline WickedColliderState CaptureWickedCollider(
        const wi::scene::ColliderComponent& collider) noexcept
    {
        WickedColliderState state;
        state.shape = collider.shape;
        state.cpuEnabled = collider.IsCPUEnabled();
        state.gpuEnabled = collider.IsGPUEnabled();
        state.radius = collider.radius;
        state.offset = collider.offset;
        state.tail = collider.tail;
        return state;
    }

    [[nodiscard]] inline WickedColliderState SanitizeWickedColliderState(
        const WickedColliderState& state) noexcept
    {
        WickedColliderState result = state;
        if (!wicked_collider_detail::IsSupportedShape(result.shape))
        {
            result.shape = wi::scene::ColliderComponent::Shape::Sphere;
        }
        result.radius = std::clamp(
            wicked_collider_detail::FiniteOr(result.radius, 0.0f),
            0.0f,
            10.0f);
        result.offset = wicked_collider_detail::ClampVector(result.offset);
        result.tail = wicked_collider_detail::ClampVector(result.tail);
        return result;
    }

    [[nodiscard]] inline bool HasWickedColliderStateChange(
        const WickedColliderState& before,
        const WickedColliderState& after) noexcept
    {
        const auto left = SanitizeWickedColliderState(before);
        const auto right = SanitizeWickedColliderState(after);
        return left.shape != right.shape ||
            left.cpuEnabled != right.cpuEnabled ||
            left.gpuEnabled != right.gpuEnabled ||
            !wicked_collider_detail::NearlyEqual(left.radius, right.radius) ||
            !wicked_collider_detail::NearlyEqual(left.offset, right.offset) ||
            !wicked_collider_detail::NearlyEqual(left.tail, right.tail);
    }

    inline void ApplyWickedCollider(
        wi::scene::ColliderComponent& collider,
        const WickedColliderState& state) noexcept
    {
        const auto safe = SanitizeWickedColliderState(state);
        collider.shape = safe.shape;
        collider.SetCPUEnabled(safe.cpuEnabled);
        collider.SetGPUEnabled(safe.gpuEnabled);
        collider.radius = safe.radius;
        collider.offset = safe.offset;
        collider.tail = safe.tail;
    }

    class CreateWickedColliderCommand final : public ICommand
    {
    public:
        CreateWickedColliderCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const WickedColliderState& state = {})
            : scene_(&scene)
            , entity_(entity)
            , state_(SanitizeWickedColliderState(state))
        {
        }

        bool Execute() override
        {
            if (scene_ == nullptr || entity_ == wi::ecs::INVALID_ENTITY ||
                scene_->colliders.Contains(entity_))
            {
                return false;
            }
            auto& collider = scene_->colliders.Create(entity_);
            ApplyWickedCollider(collider, state_);
            return true;
        }

        void Undo() override
        {
            if (scene_ != nullptr)
            {
                scene_->colliders.Remove(entity_);
            }
        }

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        WickedColliderState state_;
    };

    class SetWickedColliderCommand final : public ICommand
    {
    public:
        SetWickedColliderCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const WickedColliderState& state)
            : scene_(&scene)
            , entity_(entity)
            , after_(SanitizeWickedColliderState(state))
        {
            if (const auto* collider = scene.colliders.GetComponent(entity))
            {
                before_ = CaptureWickedCollider(*collider);
            }
        }

        SetWickedColliderCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const WickedColliderState& before,
            const WickedColliderState& after)
            : scene_(&scene)
            , entity_(entity)
            , before_(SanitizeWickedColliderState(before))
            , after_(SanitizeWickedColliderState(after))
        {
        }

        bool Execute() override
        {
            return HasWickedColliderStateChange(before_, after_) && Apply(after_);
        }

        void Undo() override
        {
            Apply(before_);
        }

    private:
        bool Apply(const WickedColliderState& state) noexcept
        {
            if (scene_ == nullptr)
            {
                return false;
            }
            auto* collider = scene_->colliders.GetComponent(entity_);
            if (collider == nullptr)
            {
                return false;
            }
            ApplyWickedCollider(*collider, state);
            return true;
        }

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        WickedColliderState before_;
        WickedColliderState after_;
    };

    class RemoveWickedColliderCommand final : public ICommand
    {
    public:
        RemoveWickedColliderCommand(
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
            const auto* collider = scene_->colliders.GetComponent(entity_);
            if (collider == nullptr)
            {
                return false;
            }
            removed_ = *collider;
            hasRemoved_ = true;
            scene_->colliders.Remove(entity_);
            return true;
        }

        void Undo() override
        {
            if (scene_ == nullptr || !hasRemoved_ || scene_->colliders.Contains(entity_))
            {
                return;
            }
            scene_->colliders.Create(entity_) = removed_;
        }

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::scene::ColliderComponent removed_;
        bool hasRemoved_ = false;
    };
}
