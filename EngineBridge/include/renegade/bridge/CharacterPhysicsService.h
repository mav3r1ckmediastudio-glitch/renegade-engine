#pragma once

#include <WickedEngine.h>

#include <algorithm>
#include <cmath>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    struct CharacterPhysicsState
    {
        bool enabled = false;
        float maxSlopeAngle = wi::math::DegreesToRadians(50.0f);
        float gravityFactor = 1.0f;
    };

    namespace character_physics_detail
    {
        inline constexpr float Epsilon = 0.00001f;
        inline constexpr float MinimumCapsuleDimension = 0.01f;

        inline float FiniteOr(const float value, const float fallback) noexcept
        {
            return std::isfinite(value) ? value : fallback;
        }

        inline bool NearlyEqual(const float left, const float right) noexcept
        {
            return std::abs(left - right) <= Epsilon;
        }

        inline wi::scene::RigidBodyPhysicsComponent* FindLiveCharacter(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity) noexcept
        {
            auto* body = scene.rigidbodies.GetComponent(entity);
            if (body == nullptr ||
                !body->IsCharacterPhysics() ||
                body->physicsobject == nullptr)
            {
                return nullptr;
            }
            return body;
        }
    }

    [[nodiscard]] inline CharacterPhysicsState CaptureCharacterPhysics(
        const wi::scene::RigidBodyPhysicsComponent& body) noexcept
    {
        CharacterPhysicsState state;
        state.enabled = body.IsCharacterPhysics();
        state.maxSlopeAngle = body.character.maxSlopeAngle;
        state.gravityFactor = body.character.gravityFactor;
        return state;
    }

    [[nodiscard]] inline CharacterPhysicsState SanitizeCharacterPhysicsState(
        const CharacterPhysicsState& state) noexcept
    {
        CharacterPhysicsState result = state;
        result.maxSlopeAngle = std::clamp(
            character_physics_detail::FiniteOr(
                result.maxSlopeAngle,
                wi::math::DegreesToRadians(50.0f)),
            0.0f,
            XM_PIDIV2);
        result.gravityFactor = std::clamp(
            character_physics_detail::FiniteOr(result.gravityFactor, 1.0f),
            0.0f,
            4.0f);
        return result;
    }

    [[nodiscard]] inline bool HasCharacterPhysicsStateChange(
        const CharacterPhysicsState& before,
        const CharacterPhysicsState& after) noexcept
    {
        const auto left = SanitizeCharacterPhysicsState(before);
        const auto right = SanitizeCharacterPhysicsState(after);
        return left.enabled != right.enabled ||
            !character_physics_detail::NearlyEqual(
                left.maxSlopeAngle,
                right.maxSlopeAngle) ||
            !character_physics_detail::NearlyEqual(
                left.gravityFactor,
                right.gravityFactor);
    }

    inline void ApplyCharacterPhysics(
        wi::scene::RigidBodyPhysicsComponent& body,
        const CharacterPhysicsState& state) noexcept
    {
        const auto before = CaptureCharacterPhysics(body);
        const auto safe = SanitizeCharacterPhysicsState(state);
        body.SetCharacterPhysics(safe.enabled);
        body.character.maxSlopeAngle = safe.maxSlopeAngle;
        body.character.gravityFactor = safe.gravityFactor;
        if (HasCharacterPhysicsStateChange(before, safe))
        {
            // Wicked Editor marks all three character controls for a physics
            // parameter refresh. Keep the same live-body recreation path.
            body.SetRefreshParametersNeeded(true);
        }
    }

    // Character and vehicle authoring share Wicked's RigidBodyPhysicsComponent.
    // This low-level bridge intentionally does not erase vehicle settings when
    // character physics is enabled; Physics Lab can present a curated mode
    // selector, while advanced code retains direct access to Wicked state.
    class SetCharacterPhysicsCommand final : public ICommand
    {
    public:
        SetCharacterPhysicsCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const CharacterPhysicsState& state)
            : scene_(&scene)
            , entity_(entity)
            , after_(SanitizeCharacterPhysicsState(state))
        {
            if (const auto* body = scene.rigidbodies.GetComponent(entity))
            {
                before_ = CaptureCharacterPhysics(*body);
            }
        }

        SetCharacterPhysicsCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const CharacterPhysicsState& before,
            const CharacterPhysicsState& after)
            : scene_(&scene)
            , entity_(entity)
            , before_(SanitizeCharacterPhysicsState(before))
            , after_(SanitizeCharacterPhysicsState(after))
        {
        }

        bool Execute() override
        {
            return HasCharacterPhysicsStateChange(before_, after_) && Apply(after_);
        }

        void Undo() override
        {
            Apply(before_);
        }

    private:
        bool Apply(const CharacterPhysicsState& state) noexcept
        {
            if (scene_ == nullptr)
            {
                return false;
            }
            auto* body = scene_->rigidbodies.GetComponent(entity_);
            if (body == nullptr)
            {
                return false;
            }
            ApplyCharacterPhysics(*body, state);
            return true;
        }

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        CharacterPhysicsState before_;
        CharacterPhysicsState after_;
    };

    enum class CharacterGroundState
    {
        OnGround,
        OnSteepGround,
        NotSupported,
        InAir,
    };

    struct CharacterGroundInfo
    {
        XMFLOAT3 position = XMFLOAT3(0, 0, 0);
        XMFLOAT3 normal = XMFLOAT3(0, 1, 0);
        XMFLOAT3 velocity = XMFLOAT3(0, 0, 0);
        CharacterGroundState state = CharacterGroundState::NotSupported;
        bool supported = false;
    };

    [[nodiscard]] inline bool HasLiveCharacterPhysics(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        const auto* body = scene.rigidbodies.GetComponent(entity);
        return body != nullptr &&
            body->IsCharacterPhysics() &&
            body->physicsobject != nullptr;
    }

    [[nodiscard]] inline bool GetCharacterGroundInfo(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        CharacterGroundInfo& info) noexcept
    {
        auto* body = character_physics_detail::FindLiveCharacter(scene, entity);
        if (body == nullptr)
        {
            return false;
        }

        CharacterGroundInfo result;
        result.position = wi::physics::GetCharacterGroundPosition(*body);
        result.normal = wi::physics::GetCharacterGroundNormal(*body);
        result.velocity = wi::physics::GetCharacterGroundVelocity(*body);
        result.supported = wi::physics::IsCharacterGroundSupported(*body);

        switch (wi::physics::GetCharacterGroundState(*body))
        {
        case wi::physics::CharacterGroundStates::OnGround:
            result.state = CharacterGroundState::OnGround;
            break;
        case wi::physics::CharacterGroundStates::OnSteepGround:
            result.state = CharacterGroundState::OnSteepGround;
            break;
        case wi::physics::CharacterGroundStates::InAir:
            result.state = CharacterGroundState::InAir;
            break;
        case wi::physics::CharacterGroundStates::NotSupported:
        default:
            result.state = CharacterGroundState::NotSupported;
            break;
        }

        info = result;
        return true;
    }

    [[nodiscard]] inline bool ChangeCharacterCapsule(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const float radius,
        const float height) noexcept
    {
        auto* body = character_physics_detail::FindLiveCharacter(scene, entity);
        if (body == nullptr)
        {
            return false;
        }

        wi::scene::RigidBodyPhysicsComponent::CapsuleParams capsule;
        capsule.radius = std::max(
            character_physics_detail::MinimumCapsuleDimension,
            character_physics_detail::FiniteOr(radius, 1.0f));
        capsule.height = std::max(
            character_physics_detail::MinimumCapsuleDimension,
            character_physics_detail::FiniteOr(height, 1.0f));
        return wi::physics::ChangeCharacterShape(*body, capsule);
    }

    [[nodiscard]] inline bool MovePhysicsCharacter(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const XMFLOAT3& movementDirection,
        const float movementSpeed,
        const float jump,
        const bool controlMovementDuringJump = true) noexcept
    {
        auto* body = character_physics_detail::FindLiveCharacter(scene, entity);
        if (body == nullptr)
        {
            return false;
        }

        const float safeSpeed = character_physics_detail::FiniteOr(
            movementSpeed,
            0.0f);
        const float safeJump = std::max(
            0.0f,
            character_physics_detail::FiniteOr(jump, 0.0f));
        const XMFLOAT3 safeDirection(
            character_physics_detail::FiniteOr(movementDirection.x, 0.0f),
            character_physics_detail::FiniteOr(movementDirection.y, 0.0f),
            character_physics_detail::FiniteOr(movementDirection.z, 0.0f));

        wi::physics::MoveCharacter(
            *body,
            safeDirection,
            safeSpeed,
            safeJump,
            controlMovementDuringJump);
        return true;
    }
}
