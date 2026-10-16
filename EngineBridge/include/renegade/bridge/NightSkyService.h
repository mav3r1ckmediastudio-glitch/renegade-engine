#pragma once
// Scene-persistent authoring data for the Renegade Night Sky renderer prototype.
// No editor-specific state; the Environment weather entity owns the metadata.
#include <WickedEngine.h>
#include <algorithm>
#include <cmath>

namespace renegade::bridge
{
    inline constexpr const char* NightSkySchemaKey = "renegade.night_sky.schema";
    inline constexpr const char* NightSkyGradientKey = "renegade.night_sky.gradient";
    inline constexpr const char* NightSkyAuroraKey = "renegade.night_sky.aurora";
    inline constexpr const char* NightSkySpeedKey = "renegade.night_sky.aurora_speed";

    struct NightSkyState
    {
        float gradient = 0.0f;  // zero preserves the pre-feature sky
        float aurora = 0.0f;
        float auroraSpeed = 0.35f;
    };

    inline float SanitizeNightSkyValue(float value, float fallback, float maximum) noexcept
    {
        return std::isfinite(value) ? std::clamp(value, 0.0f, maximum) : fallback;
    }

    inline NightSkyState SanitizeNightSky(const NightSkyState& input) noexcept
    {
        return {
            SanitizeNightSkyValue(input.gradient, 0.0f, 1.0f),
            SanitizeNightSkyValue(input.aurora, 0.0f, 1.0f),
            SanitizeNightSkyValue(input.auroraSpeed, 0.35f, 2.0f)
        };
    }

    // Reads only v1, keeping missing/legacy metadata opt-in and visually unchanged.
    inline NightSkyState CaptureNightSky(
        const wi::scene::Scene& scene, wi::ecs::Entity weatherEntity) noexcept
    {
        NightSkyState result;
        if (!scene.weathers.Contains(weatherEntity)) return result;
        const auto* meta = scene.metadatas.GetComponent(weatherEntity);
        if (meta == nullptr || !meta->int_values.has(NightSkySchemaKey) ||
            meta->int_values.get(NightSkySchemaKey) != 1) return result;
        if (meta->float_values.has(NightSkyGradientKey))
            result.gradient = meta->float_values.get(NightSkyGradientKey);
        if (meta->float_values.has(NightSkyAuroraKey))
            result.aurora = meta->float_values.get(NightSkyAuroraKey);
        if (meta->float_values.has(NightSkySpeedKey))
            result.auroraSpeed = meta->float_values.get(NightSkySpeedKey);
        return SanitizeNightSky(result);
    }

    inline void ApplyNightSkyRuntime(
        wi::scene::Scene& scene, wi::ecs::Entity weatherEntity,
        const NightSkyState& requested) noexcept
    {
        auto* weather = scene.weathers.GetComponent(weatherEntity);
        if (weather == nullptr) return;
        const auto state = SanitizeNightSky(requested);
        weather->night_sky_gradient = state.gradient;
        weather->aurora_intensity = state.aurora;
        weather->aurora_speed = state.auroraSpeed;
        // The resolved copy is used until the next native WeatherUpdateSystem.
        if (scene.weathers.GetCount() > 0 &&
            scene.weathers.GetEntity(0) == weatherEntity)
        {
            scene.weather.night_sky_gradient = state.gradient;
            scene.weather.aurora_intensity = state.aurora;
            scene.weather.aurora_speed = state.auroraSpeed;
        }
    }

    // Command-backed editor transactions should call this for commit/undo/redo.
    inline bool StoreNightSky(
        wi::scene::Scene& scene, wi::ecs::Entity weatherEntity,
        const NightSkyState& requested) noexcept
    {
        if (!scene.weathers.Contains(weatherEntity)) return false;
        const auto state = SanitizeNightSky(requested);
        auto* meta = scene.metadatas.GetComponent(weatherEntity);
        if (meta == nullptr) meta = &scene.metadatas.Create(weatherEntity);
        meta->int_values.set(NightSkySchemaKey, 1);
        meta->float_values.set(NightSkyGradientKey, state.gradient);
        meta->float_values.set(NightSkyAuroraKey, state.aurora);
        meta->float_values.set(NightSkySpeedKey, state.auroraSpeed);
        ApplyNightSkyRuntime(scene, weatherEntity, state);
        return true;
    }

    // Call after every scene load/merge, including Studio document adoption.
    inline void RestoreNightSky(
        wi::scene::Scene& scene) noexcept
    {
        for (std::size_t i = 0; i < scene.weathers.GetCount(); ++i)
        {
            const auto entity = scene.weathers.GetEntity(i);
            ApplyNightSkyRuntime(scene, entity, CaptureNightSky(scene, entity));
        }
    }
}
