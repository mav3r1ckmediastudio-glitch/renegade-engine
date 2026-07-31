#include "renegade/bridge/PrecipitationService.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float Epsilon = 0.00001f;

    bool NearlyEqual(const float left, const float right) noexcept
    {
        return std::abs(left - right) <= Epsilon;
    }

    bool IsMeaningful(
        const renegade::bridge::PrecipitationState& before,
        const renegade::bridge::PrecipitationState& after) noexcept
    {
        return before.mode != after.mode ||
            !NearlyEqual(before.intensity, after.intensity) ||
            !NearlyEqual(before.fallSpeed, after.fallSpeed) ||
            !NearlyEqual(before.particleScale, after.particleScale) ||
            !NearlyEqual(
                before.windAzimuthDegrees,
                after.windAzimuthDegrees) ||
            !NearlyEqual(before.windSpeed, after.windSpeed) ||
            !NearlyEqual(before.turbulence, after.turbulence) ||
            !NearlyEqual(before.streakLength, after.streakLength) ||
            !NearlyEqual(before.splashScale, after.splashScale) ||
            !NearlyEqual(before.color.x, after.color.x) ||
            !NearlyEqual(before.color.y, after.color.y) ||
            !NearlyEqual(before.color.z, after.color.z) ||
            !NearlyEqual(before.color.w, after.color.w);
    }

    void RefreshRuntimeWeather(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const wi::scene::WeatherComponent& weather)
    {
        if (scene.weathers.GetCount() > 0 &&
            scene.weathers.GetEntity(0) == entity)
        {
            scene.weather = weather;
        }
    }
}

namespace renegade::bridge
{
    PrecipitationState CapturePrecipitation(
        const wi::scene::WeatherComponent& weather) noexcept
    {
        PrecipitationState state;
        state.intensity = weather.rain_amount;
        state.fallSpeed = weather.rain_speed;
        state.particleScale = weather.rain_scale;
        state.windSpeed = weather.windSpeed;
        state.turbulence = weather.windRandomness;
        state.streakLength = weather.rain_length;
        state.splashScale = weather.rain_splash_scale;
        state.color = weather.rain_color;

        const float horizontalWindLength = std::sqrt(
            weather.windDirection.x * weather.windDirection.x +
            weather.windDirection.z * weather.windDirection.z);
        if (horizontalWindLength > Epsilon)
        {
            state.windAzimuthDegrees =
                std::atan2(
                    weather.windDirection.z,
                    weather.windDirection.x) *
                180.0f / XM_PI;
        }

        if (weather.rain_amount <= Epsilon)
        {
            state.mode = PrecipitationMode::None;
        }
        else if (weather.rain_splash_scale <= 0.001f &&
            weather.rain_length <= 0.005f)
        {
            state.mode = PrecipitationMode::Snow;
        }
        else
        {
            state.mode = PrecipitationMode::Rain;
        }
        return state;
    }

    void ApplyPrecipitation(
        wi::scene::WeatherComponent& weather,
        const PrecipitationState& state) noexcept
    {
        weather.rain_amount = state.mode == PrecipitationMode::None
            ? 0.0f
            : std::clamp(state.intensity, 0.0f, 1.0f);
        weather.rain_speed = std::clamp(state.fallSpeed, 0.01f, 2.0f);
        weather.rain_scale =
            std::clamp(state.particleScale, 0.005f, 0.1f);
        weather.windSpeed = std::clamp(state.windSpeed, 0.0f, 50.0f);
        weather.windRandomness =
            std::clamp(state.turbulence, 0.0f, 20.0f);
        weather.rain_length =
            std::clamp(state.streakLength, 0.0f, 0.1f);
        weather.rain_splash_scale =
            std::clamp(state.splashScale, 0.0f, 1.0f);
        weather.rain_color = state.color;

        const float azimuth =
            state.windAzimuthDegrees * XM_PI / 180.0f;
        weather.windDirection = XMFLOAT3(
            std::cos(azimuth),
            0.0f,
            std::sin(azimuth));

    }

    PrecipitationState MakePrecipitationProfile(
        const PrecipitationState& current,
        const PrecipitationMode mode) noexcept
    {
        PrecipitationState result = current;
        result.mode = mode;
        switch (mode)
        {
        case PrecipitationMode::None:
            result.intensity = 0.0f;
            break;
        case PrecipitationMode::Rain:
            result.intensity = current.mode == mode
                ? current.intensity
                : 0.45f;
            result.fallSpeed = 1.25f;
            result.particleScale = 0.008f;
            result.turbulence = 3.0f;
            result.streakLength = 0.04f;
            result.splashScale = 0.1f;
            result.color = XMFLOAT4(0.6f, 0.8f, 1.0f, 0.5f);
            break;
        case PrecipitationMode::Snow:
            result.intensity = current.mode == mode
                ? current.intensity
                : 0.38f;
            result.fallSpeed = 0.22f;
            result.particleScale = 0.035f;
            result.turbulence = 8.0f;
            result.streakLength = 0.0f;
            result.splashScale = 0.0f;
            result.color = XMFLOAT4(1.0f, 0.98f, 0.95f, 0.88f);
            break;
        }
        return result;
    }

    SetPrecipitationCommand::SetPrecipitationCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const PrecipitationState& precipitation)
        : scene_(&scene)
        , entity_(entity)
        , after_(precipitation)
    {
        const auto* existing = scene.weathers.GetComponent(entity);
        before_ = existing == nullptr
            ? PrecipitationState{}
            : CapturePrecipitation(*existing);
    }

    SetPrecipitationCommand::SetPrecipitationCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const PrecipitationState& before,
        const PrecipitationState& after)
        : scene_(&scene)
        , entity_(entity)
        , before_(before)
        , after_(after)
    {
    }

    bool SetPrecipitationCommand::Execute()
    {
        return IsMeaningful(before_, after_) && Apply(after_);
    }

    void SetPrecipitationCommand::Undo()
    {
        Apply(before_);
    }

    bool SetPrecipitationCommand::Apply(
        const PrecipitationState& state)
    {
        if (scene_ == nullptr)
        {
            return false;
        }
        auto* weather = scene_->weathers.GetComponent(entity_);
        if (weather == nullptr)
        {
            return false;
        }
        ApplyPrecipitation(*weather, state);
        RefreshRuntimeWeather(*scene_, entity_, *weather);
        return true;
    }
}
