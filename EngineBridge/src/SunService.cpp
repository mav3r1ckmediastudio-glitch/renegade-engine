#include "renegade/bridge/SunService.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float Epsilon = 0.00001f;
    constexpr float MaximumSolarElevation = 75.0f;

    float WrapHours(float hours) noexcept
    {
        hours = std::fmod(hours, 24.0f);
        return hours < 0.0f ? hours + 24.0f : hours;
    }

    float WrapDegrees(float degrees) noexcept
    {
        degrees = std::fmod(degrees + 180.0f, 360.0f);
        if (degrees < 0.0f)
        {
            degrees += 360.0f;
        }
        return degrees - 180.0f;
    }

    bool NearlyEqual(const float left, const float right) noexcept
    {
        return std::abs(left - right) <= Epsilon;
    }

    bool IsMeaningful(
        const renegade::bridge::SunState& before,
        const renegade::bridge::SunState& after) noexcept
    {
        return !NearlyEqual(before.direction.x, after.direction.x) ||
            !NearlyEqual(before.direction.y, after.direction.y) ||
            !NearlyEqual(before.direction.z, after.direction.z) ||
            before.lightEntity != after.lightEntity ||
            !NearlyEqual(
                before.lightTransform.rotation.x,
                after.lightTransform.rotation.x) ||
            !NearlyEqual(
                before.lightTransform.rotation.y,
                after.lightTransform.rotation.y) ||
            !NearlyEqual(
                before.lightTransform.rotation.z,
                after.lightTransform.rotation.z) ||
            !NearlyEqual(
                before.lightTransform.rotation.w,
                after.lightTransform.rotation.w);
    }

    void UpdateDerivedState(renegade::bridge::SunState& state) noexcept
    {
        const float azimuth = state.azimuthDegrees * XM_PI / 180.0f;
        const float elevation = state.elevationDegrees * XM_PI / 180.0f;
        const float horizontal = std::cos(elevation);
        state.direction = XMFLOAT3(
            horizontal * std::sin(azimuth),
            std::sin(elevation),
            horizontal * std::cos(azimuth));

        // Wicked directional lights point down at identity and use local +Y
        // as their authored axis. Elevation 90 is therefore pitch 0.
        const float pitch = elevation - XM_PIDIV2;
        const XMVECTOR rotation = XMQuaternionRotationRollPitchYaw(
            pitch,
            azimuth,
            0.0f);
        XMStoreFloat4(&state.lightTransform.rotation, rotation);
    }
}

namespace renegade::bridge
{
    wi::ecs::Entity FindPrimarySunLight(
        const wi::scene::Scene& scene) noexcept
    {
        wi::ecs::Entity fallback = wi::ecs::INVALID_ENTITY;
        for (std::size_t index = 0; index < scene.lights.GetCount(); ++index)
        {
            const auto entity = scene.lights.GetEntity(index);
            const auto* light = scene.lights.GetComponent(entity);
            if (light == nullptr ||
                light->GetType() != wi::scene::LightComponent::DIRECTIONAL)
            {
                continue;
            }
            if (fallback == wi::ecs::INVALID_ENTITY)
            {
                fallback = entity;
            }
            const auto* name = scene.names.GetComponent(entity);
            if (name != nullptr && name->name == "Sun")
            {
                return entity;
            }
        }
        return fallback;
    }

    SunState CaptureSun(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity weatherEntity) noexcept
    {
        SunState state;
        state.lightEntity = FindPrimarySunLight(scene);
        if (const auto* transform =
                scene.transforms.GetComponent(state.lightEntity))
        {
            state.lightTransform = CaptureTransform(*transform);
        }

        if (const auto* weather = scene.weathers.GetComponent(weatherEntity))
        {
            const float length = std::sqrt(
                weather->sunDirection.x * weather->sunDirection.x +
                weather->sunDirection.y * weather->sunDirection.y +
                weather->sunDirection.z * weather->sunDirection.z);
            if (length > Epsilon)
            {
                state.direction = XMFLOAT3(
                    weather->sunDirection.x / length,
                    weather->sunDirection.y / length,
                    weather->sunDirection.z / length);
                state.elevationDegrees = std::asin(std::clamp(
                    state.direction.y,
                    -1.0f,
                    1.0f)) * 180.0f / XM_PI;
                state.azimuthDegrees = std::atan2(
                    state.direction.x,
                    state.direction.z) * 180.0f / XM_PI;
                state.timeHours = WrapHours(
                    12.0f + state.azimuthDegrees / 15.0f);
            }
        }
        return state;
    }

    void SetSunTime(SunState& state, const float timeHours) noexcept
    {
        state.timeHours = WrapHours(timeHours);
        state.azimuthDegrees = WrapDegrees(
            (state.timeHours - 12.0f) * 15.0f);
        state.elevationDegrees = MaximumSolarElevation * std::sin(
            (state.timeHours - 6.0f) * XM_PI / 12.0f);
        UpdateDerivedState(state);
    }

    void SetSunAzimuth(
        SunState& state,
        const float azimuthDegrees) noexcept
    {
        state.azimuthDegrees = WrapDegrees(azimuthDegrees);
        state.timeHours = WrapHours(
            12.0f + state.azimuthDegrees / 15.0f);
        UpdateDerivedState(state);
    }

    void SetSunElevation(
        SunState& state,
        const float elevationDegrees) noexcept
    {
        state.elevationDegrees = std::clamp(
            elevationDegrees,
            -90.0f,
            90.0f);
        UpdateDerivedState(state);
    }

    SunState MakeSunPreset(
        const SunState& current,
        const SunPreset preset) noexcept
    {
        SunState result = current;
        switch (preset)
        {
        case SunPreset::Dawn:
            SetSunTime(result, 6.5f);
            break;
        case SunPreset::Midday:
            SetSunTime(result, 12.0f);
            break;
        case SunPreset::GoldenHour:
            SetSunTime(result, 17.0f);
            break;
        case SunPreset::Dusk:
            SetSunTime(result, 18.5f);
            break;
        case SunPreset::Midnight:
            SetSunTime(result, 0.0f);
            break;
        }
        return result;
    }

    bool ApplySun(
        wi::scene::Scene& scene,
        const wi::ecs::Entity weatherEntity,
        const SunState& state) noexcept
    {
        auto* weather = scene.weathers.GetComponent(weatherEntity);
        if (weather == nullptr)
        {
            return false;
        }
        weather->sunDirection = state.direction;
        if (scene.weathers.GetCount() > 0 &&
            scene.weathers.GetEntity(0) == weatherEntity)
        {
            scene.weather = *weather;
        }

        if (auto* transform =
                scene.transforms.GetComponent(state.lightEntity))
        {
            transform->translation_local = state.lightTransform.translation;
            transform->rotation_local = state.lightTransform.rotation;
            transform->scale_local = state.lightTransform.scale;
            transform->SetDirty();
            transform->UpdateTransform();
        }
        if (auto* light = scene.lights.GetComponent(state.lightEntity))
        {
            light->direction = XMFLOAT3(
                -state.direction.x,
                -state.direction.y,
                -state.direction.z);
        }
        return true;
    }

    SetSunCommand::SetSunCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity weatherEntity,
        const SunState& sun)
        : scene_(&scene)
        , weatherEntity_(weatherEntity)
        , before_(CaptureSun(scene, weatherEntity))
        , after_(sun)
    {
    }

    SetSunCommand::SetSunCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity weatherEntity,
        const SunState& before,
        const SunState& after)
        : scene_(&scene)
        , weatherEntity_(weatherEntity)
        , before_(before)
        , after_(after)
    {
    }

    bool SetSunCommand::Execute()
    {
        return scene_ != nullptr && IsMeaningful(before_, after_) &&
            ApplySun(*scene_, weatherEntity_, after_);
    }

    void SetSunCommand::Undo()
    {
        if (scene_ != nullptr)
        {
            ApplySun(*scene_, weatherEntity_, before_);
        }
    }
}
