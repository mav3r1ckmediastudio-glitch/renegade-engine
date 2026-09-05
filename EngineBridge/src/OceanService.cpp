#include "renegade/bridge/OceanService.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float Epsilon = 0.00001f;

    bool NearlyEqual(const float left, const float right) noexcept
    {
        return std::abs(left - right) <= Epsilon;
    }

    bool EqualColor(const XMFLOAT4& left, const XMFLOAT4& right) noexcept
    {
        return NearlyEqual(left.x, right.x) &&
            NearlyEqual(left.y, right.y) &&
            NearlyEqual(left.z, right.z) &&
            NearlyEqual(left.w, right.w);
    }

    int SanitizeDimension(const int value) noexcept
    {
        constexpr int dimensions[] = {64, 128, 256, 512, 1024};
        int closest = dimensions[0];
        int distance = std::abs(value - closest);
        for (const int candidate : dimensions)
        {
            const int candidateDistance = std::abs(value - candidate);
            if (candidateDistance < distance)
            {
                closest = candidate;
                distance = candidateDistance;
            }
        }
        return closest;
    }

    bool IsMeaningful(
        const renegade::bridge::OceanState& before,
        const renegade::bridge::OceanState& after) noexcept
    {
        return before.enabled != after.enabled ||
            before.displacementMapDimension !=
                after.displacementMapDimension ||
            !NearlyEqual(before.patchLength, after.patchLength) ||
            !NearlyEqual(before.timeScale, after.timeScale) ||
            !NearlyEqual(before.waveAmplitude, after.waveAmplitude) ||
            !NearlyEqual(
                before.windAzimuthDegrees,
                after.windAzimuthDegrees) ||
            !NearlyEqual(before.windSpeed, after.windSpeed) ||
            !NearlyEqual(before.windDependency, after.windDependency) ||
            !NearlyEqual(before.choppyScale, after.choppyScale) ||
            !EqualColor(before.waterColor, after.waterColor) ||
            !EqualColor(before.extinctionColor, after.extinctionColor) ||
            !NearlyEqual(before.waterHeight, after.waterHeight) ||
            before.surfaceDetail != after.surfaceDetail ||
            !NearlyEqual(
                before.surfaceDisplacementTolerance,
                after.surfaceDisplacementTolerance);
    }
}

namespace renegade::bridge
{
    OceanState CaptureOcean(
        const wi::scene::WeatherComponent& weather) noexcept
    {
        OceanState state;
        const auto& ocean = weather.oceanParameters;
        state.enabled = weather.IsOceanEnabled();
        state.displacementMapDimension = ocean.dmap_dim;
        state.patchLength = ocean.patch_length;
        state.timeScale = ocean.time_scale;
        state.waveAmplitude = ocean.wave_amplitude;
        state.windAzimuthDegrees = std::atan2(
            ocean.wind_dir.y,
            ocean.wind_dir.x) * 180.0f / XM_PI;
        state.windSpeed = ocean.wind_speed;
        state.windDependency = ocean.wind_dependency;
        state.choppyScale = ocean.choppy_scale;
        state.waterColor = ocean.waterColor;
        state.extinctionColor = ocean.extinctionColor;
        state.waterHeight = ocean.waterHeight;
        state.surfaceDetail = ocean.surfaceDetail;
        state.surfaceDisplacementTolerance =
            ocean.surfaceDisplacementTolerance;
        return state;
    }

    void ApplyOcean(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const OceanState& state) noexcept
    {
        auto* weather = scene.weathers.GetComponent(entity);
        if (weather == nullptr)
        {
            return;
        }

        const OceanState before = CaptureOcean(*weather);
        auto& ocean = weather->oceanParameters;
        weather->SetOceanEnabled(state.enabled);
        ocean.dmap_dim = SanitizeDimension(state.displacementMapDimension);
        ocean.patch_length = std::clamp(state.patchLength, 1.0f, 2000.0f);
        ocean.time_scale = std::clamp(state.timeScale, 0.0f, 4.0f);
        ocean.wave_amplitude =
            std::clamp(state.waveAmplitude, 0.0f, 2000.0f);
        const float azimuth = state.windAzimuthDegrees * XM_PI / 180.0f;
        ocean.wind_dir = XMFLOAT2(std::cos(azimuth), std::sin(azimuth));
        ocean.wind_speed = std::clamp(state.windSpeed, 0.0f, 2000.0f);
        ocean.wind_dependency =
            std::clamp(state.windDependency, 0.0f, 1.0f);
        ocean.choppy_scale = std::clamp(state.choppyScale, 0.0f, 10.0f);
        ocean.waterColor = XMFLOAT4(
            std::clamp(state.waterColor.x, 0.0f, 4.0f),
            std::clamp(state.waterColor.y, 0.0f, 4.0f),
            std::clamp(state.waterColor.z, 0.0f, 4.0f),
            std::clamp(state.waterColor.w, 0.0f, 1.0f));
        ocean.extinctionColor = XMFLOAT4(
            std::clamp(state.extinctionColor.x, 0.0f, 4.0f),
            std::clamp(state.extinctionColor.y, 0.0f, 4.0f),
            std::clamp(state.extinctionColor.z, 0.0f, 4.0f),
            1.0f);
        ocean.waterHeight =
            std::clamp(state.waterHeight, -1000.0f, 1000.0f);
        ocean.surfaceDetail = static_cast<std::uint32_t>(
            std::clamp(state.surfaceDetail, 1u, 10u));
        ocean.surfaceDisplacementTolerance = std::clamp(
            state.surfaceDisplacementTolerance,
            1.0f,
            10.0f);

        // Wicked creates the FFT resources lazily. Match its own editor by
        // rebuilding only for spectral/resolution changes; colour, level,
        // choppiness and rendering-detail edits remain cheap live previews.
        const bool recreate = before.enabled != state.enabled ||
            before.displacementMapDimension != ocean.dmap_dim ||
            !NearlyEqual(before.patchLength, ocean.patch_length) ||
            !NearlyEqual(before.waveAmplitude, ocean.wave_amplitude) ||
            !NearlyEqual(before.windAzimuthDegrees, state.windAzimuthDegrees) ||
            !NearlyEqual(before.windSpeed, ocean.wind_speed) ||
            !NearlyEqual(before.windDependency, ocean.wind_dependency);
        if (recreate || !state.enabled)
        {
            scene.ocean = {};
        }
        if (scene.weathers.GetCount() > 0 &&
            scene.weathers.GetEntity(0) == entity)
        {
            scene.weather = *weather;
        }
    }

    OceanState MakeOceanPreset(
        const OceanState& current,
        const OceanPreset preset) noexcept
    {
        OceanState result = current;
        result.enabled = true;
        result.displacementMapDimension = 512;
        result.surfaceDetail = 4;
        result.surfaceDisplacementTolerance = 2.0f;
        switch (preset)
        {
        case OceanPreset::Calm:
            result.patchLength = 250.0f;
            result.timeScale = 0.12f;
            result.waveAmplitude = 150.0f;
            result.windAzimuthDegrees = 20.0f;
            result.windSpeed = 120.0f;
            result.windDependency = 0.15f;
            result.choppyScale = 0.45f;
            result.waterColor = XMFLOAT4(0.01f, 0.05f, 0.08f, 0.72f);
            result.extinctionColor = XMFLOAT4(0.04f, 0.55f, 0.72f, 1.0f);
            break;
        case OceanPreset::Coastal:
            result.patchLength = 100.0f;
            result.timeScale = 0.30f;
            result.waveAmplitude = 450.0f;
            result.windAzimuthDegrees = 35.0f;
            result.windSpeed = 350.0f;
            result.windDependency = 0.08f;
            result.choppyScale = 1.2f;
            result.waterColor = XMFLOAT4(0.0f, 0.04f, 0.07f, 0.65f);
            result.extinctionColor = XMFLOAT4(0.0f, 0.72f, 0.78f, 1.0f);
            break;
        case OceanPreset::Storm:
            result.patchLength = 180.0f;
            result.timeScale = 0.9f;
            result.waveAmplitude = 1000.0f;
            result.windAzimuthDegrees = -35.0f;
            result.windSpeed = 900.0f;
            result.windDependency = 0.03f;
            result.choppyScale = 2.5f;
            result.waterColor = XMFLOAT4(0.005f, 0.012f, 0.018f, 0.78f);
            result.extinctionColor = XMFLOAT4(0.12f, 0.25f, 0.32f, 1.0f);
            result.surfaceDetail = 6;
            result.surfaceDisplacementTolerance = 4.0f;
            break;
        case OceanPreset::Alien:
            result.patchLength = 80.0f;
            result.timeScale = 0.45f;
            result.waveAmplitude = 650.0f;
            result.windAzimuthDegrees = 110.0f;
            result.windSpeed = 500.0f;
            result.windDependency = 0.12f;
            result.choppyScale = 3.0f;
            result.waterColor = XMFLOAT4(0.08f, 0.0f, 0.12f, 0.68f);
            result.extinctionColor = XMFLOAT4(0.2f, 1.0f, 0.62f, 1.0f);
            result.surfaceDetail = 5;
            break;
        }
        return result;
    }

    SetOceanCommand::SetOceanCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const OceanState& ocean)
        : scene_(&scene)
        , entity_(entity)
        , after_(ocean)
    {
        const auto* weather = scene.weathers.GetComponent(entity);
        before_ = weather == nullptr ? OceanState{} : CaptureOcean(*weather);
    }

    SetOceanCommand::SetOceanCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const OceanState& before,
        const OceanState& after)
        : scene_(&scene)
        , entity_(entity)
        , before_(before)
        , after_(after)
    {
    }

    bool SetOceanCommand::Execute()
    {
        return IsMeaningful(before_, after_) && Apply(after_);
    }

    void SetOceanCommand::Undo()
    {
        Apply(before_);
    }

    bool SetOceanCommand::Apply(const OceanState& state)
    {
        if (scene_ == nullptr ||
            scene_->weathers.GetComponent(entity_) == nullptr)
        {
            return false;
        }
        ApplyOcean(*scene_, entity_, state);
        return true;
    }
}
