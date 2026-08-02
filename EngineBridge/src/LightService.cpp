#include "renegade/bridge/LightService.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace
{
    constexpr float Epsilon = 0.00001f;
    constexpr float RadiansToDegrees = 180.0f / XM_PI;
    constexpr float DegreesToRadians = XM_PI / 180.0f;

    bool NearlyEqual(const float left, const float right) noexcept
    {
        return std::abs(left - right) <= Epsilon;
    }

    bool EntityExists(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
    {
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return false;
        }
        wi::unordered_set<wi::ecs::Entity> entities;
        scene.FindAllEntities(entities);
        return entities.count(entity) != 0;
    }

    XMFLOAT4 SanitizeRotation(const XMFLOAT4& rotation) noexcept
    {
        const XMVECTOR value = XMLoadFloat4(&rotation);
        const float lengthSquared = XMVectorGetX(XMVector4LengthSq(value));
        if (!std::isfinite(lengthSquared) || lengthSquared < Epsilon)
        {
            return XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        }
        XMFLOAT4 result;
        XMStoreFloat4(&result, XMQuaternionNormalize(value));
        return result;
    }

    const char* LightTypeName(
        const wi::scene::LightComponent::LightType type) noexcept
    {
        switch (type)
        {
        case wi::scene::LightComponent::DIRECTIONAL:
            return "Directional Light";
        case wi::scene::LightComponent::SPOT:
            return "Spot Light";
        case wi::scene::LightComponent::RECTANGLE:
            return "Rectangle Light";
        case wi::scene::LightComponent::POINT:
        default:
            return "Point Light";
        }
    }
}

namespace renegade::bridge
{
    LightState CaptureLight(
        const wi::scene::LightComponent& light) noexcept
    {
        LightState state;
        state.type = light.GetType();
        state.color = light.color;
        state.intensity = light.intensity;
        state.range = light.range;
        state.outerConeDegrees = light.outerConeAngle * RadiansToDegrees;
        state.innerConeDegrees = light.innerConeAngle * RadiansToDegrees;
        state.radius = light.radius;
        state.length = light.length;
        state.height = light.height;
        state.castShadow = light.IsCastingShadow();
        state.volumetrics = light.IsVolumetricsEnabled();
        state.volumetricBoost = light.volumetric_boost;
        return state;
    }

    LightState SanitizeLightState(const LightState& state) noexcept
    {
        LightState result = state;
        if (result.type < wi::scene::LightComponent::DIRECTIONAL ||
            result.type >= wi::scene::LightComponent::LIGHTTYPE_COUNT)
        {
            result.type = wi::scene::LightComponent::POINT;
        }
        result.color.x = std::clamp(result.color.x, 0.0f, 1.0f);
        result.color.y = std::clamp(result.color.y, 0.0f, 1.0f);
        result.color.z = std::clamp(result.color.z, 0.0f, 1.0f);
        result.intensity = std::clamp(result.intensity, 0.0f, 100000.0f);
        result.range = std::clamp(result.range, 0.0f, 100000.0f);
        result.outerConeDegrees = std::clamp(
            result.outerConeDegrees,
            0.1f,
            89.9f);
        result.innerConeDegrees = std::clamp(
            result.innerConeDegrees,
            0.0f,
            result.outerConeDegrees);
        result.radius = std::clamp(result.radius, 0.0f, 100000.0f);
        result.length = std::clamp(result.length, 0.0f, 100000.0f);
        result.height = std::clamp(result.height, 0.0f, 100000.0f);
        result.volumetricBoost = std::clamp(
            result.volumetricBoost,
            0.0f,
            10.0f);
        return result;
    }

    bool HasLightStateChange(
        const LightState& before,
        const LightState& after) noexcept
    {
        const auto left = SanitizeLightState(before);
        const auto right = SanitizeLightState(after);
        return left.type != right.type ||
            !NearlyEqual(left.color.x, right.color.x) ||
            !NearlyEqual(left.color.y, right.color.y) ||
            !NearlyEqual(left.color.z, right.color.z) ||
            !NearlyEqual(left.intensity, right.intensity) ||
            !NearlyEqual(left.range, right.range) ||
            !NearlyEqual(left.outerConeDegrees, right.outerConeDegrees) ||
            !NearlyEqual(left.innerConeDegrees, right.innerConeDegrees) ||
            !NearlyEqual(left.radius, right.radius) ||
            !NearlyEqual(left.length, right.length) ||
            !NearlyEqual(left.height, right.height) ||
            left.castShadow != right.castShadow ||
            left.volumetrics != right.volumetrics ||
            !NearlyEqual(left.volumetricBoost, right.volumetricBoost);
    }

    void ApplyLight(
        wi::scene::LightComponent& light,
        const LightState& state) noexcept
    {
        const auto safe = SanitizeLightState(state);
        light.SetType(safe.type);
        light.color = safe.color;
        light.intensity = safe.intensity;
        light.range = safe.range;
        light.outerConeAngle = safe.outerConeDegrees * DegreesToRadians;
        light.innerConeAngle = safe.innerConeDegrees * DegreesToRadians;
        light.radius = safe.radius;
        light.length = safe.length;
        light.height = safe.height;
        light.SetCastShadow(safe.castShadow);
        light.SetVolumetricsEnabled(safe.volumetrics);
        light.volumetric_boost = safe.volumetricBoost;
    }

    LightState MakeNewLightState(
        const wi::scene::LightComponent::LightType type) noexcept
    {
        LightState state;
        state.type = type;
        state.color = XMFLOAT3(1.0f, 1.0f, 1.0f);
        state.range = 60.0f;
        state.outerConeDegrees = 45.0f;
        state.innerConeDegrees = 0.0f;
        state.radius = 0.025f;

        switch (type)
        {
        case wi::scene::LightComponent::DIRECTIONAL:
            state.intensity = 10.0f;
            state.radius = 0.25f;
            break;
        case wi::scene::LightComponent::SPOT:
            state.intensity = 100.0f;
            break;
        case wi::scene::LightComponent::RECTANGLE:
            state.intensity = 20.0f;
            state.length = 2.0f;
            state.height = 2.0f;
            break;
        case wi::scene::LightComponent::POINT:
        default:
            state.type = wi::scene::LightComponent::POINT;
            state.intensity = 20.0f;
            break;
        }
        return state;
    }

    CreateLightCommand::CreateLightCommand(
        wi::scene::Scene& scene,
        const wi::scene::LightComponent::LightType type,
        const XMFLOAT3& position,
        const XMFLOAT4& rotation)
        : scene_(&scene)
        , type_(MakeNewLightState(type).type)
        , position_(position)
        , rotation_(SanitizeRotation(rotation))
    {
    }

    bool CreateLightCommand::Execute()
    {
        if (scene_ == nullptr)
        {
            return false;
        }

        if (hasSnapshot_)
        {
            if (EntityExists(*scene_, entity_))
            {
                return false;
            }
            snapshot_.SetReadModeAndResetPos(true);
            wi::ecs::EntitySerializer serializer;
            serializer.allow_remap = false;
            return scene_->Entity_Serialize(snapshot_, serializer) == entity_;
        }

        const auto state = MakeNewLightState(type_);
        entity_ = scene_->Entity_CreateLight(
            MakeUniqueName(),
            position_,
            state.color,
            state.intensity,
            state.range,
            state.type,
            state.outerConeDegrees * DegreesToRadians,
            state.innerConeDegrees * DegreesToRadians);
        auto* light = scene_->lights.GetComponent(entity_);
        if (entity_ == wi::ecs::INVALID_ENTITY || light == nullptr)
        {
            entity_ = wi::ecs::INVALID_ENTITY;
            return false;
        }
        ApplyLight(*light, state);
        auto* transform = scene_->transforms.GetComponent(entity_);
        if (transform == nullptr)
        {
            scene_->Entity_Remove(entity_);
            entity_ = wi::ecs::INVALID_ENTITY;
            return false;
        }
        transform->rotation_local = rotation_;
        transform->SetDirty();
        transform->UpdateTransform();

        snapshot_.SetReadModeAndResetPos(false);
        wi::ecs::EntitySerializer serializer;
        scene_->Entity_Serialize(snapshot_, serializer, entity_);
        hasSnapshot_ = true;
        return true;
    }

    void CreateLightCommand::Undo()
    {
        if (scene_ != nullptr && EntityExists(*scene_, entity_))
        {
            scene_->Entity_Remove(entity_);
        }
    }

    wi::ecs::Entity CreateLightCommand::CreatedEntity() const noexcept
    {
        return entity_;
    }

    std::string CreateLightCommand::MakeUniqueName() const
    {
        const std::string base = LightTypeName(type_);
        std::string candidate = base;
        int suffix = 2;
        bool collision = true;
        while (collision)
        {
            collision = false;
            for (std::size_t index = 0; index < scene_->names.GetCount(); ++index)
            {
                if (scene_->names[index].name == candidate)
                {
                    collision = true;
                    candidate = base + " " + std::to_string(suffix++);
                    break;
                }
            }
        }
        return candidate;
    }

    SetLightCommand::SetLightCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const LightState& light)
        : scene_(&scene)
        , entity_(entity)
        , after_(SanitizeLightState(light))
    {
        if (const auto* existing = scene.lights.GetComponent(entity))
        {
            before_ = CaptureLight(*existing);
        }
    }

    SetLightCommand::SetLightCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const LightState& before,
        const LightState& after)
        : scene_(&scene)
        , entity_(entity)
        , before_(SanitizeLightState(before))
        , after_(SanitizeLightState(after))
    {
    }

    bool SetLightCommand::Execute()
    {
        return HasLightStateChange(before_, after_) && Apply(after_);
    }

    void SetLightCommand::Undo()
    {
        Apply(before_);
    }

    bool SetLightCommand::Apply(const LightState& state) noexcept
    {
        if (scene_ == nullptr)
        {
            return false;
        }
        auto* light = scene_->lights.GetComponent(entity_);
        if (light == nullptr)
        {
            return false;
        }
        ApplyLight(*light, state);
        return true;
    }
}
