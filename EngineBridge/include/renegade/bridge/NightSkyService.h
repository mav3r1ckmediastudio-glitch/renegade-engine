#pragma once
// Renegade-owned Night Sky authoring on an existing serialized Environment entity.
// Engine shading is transient; versioned MetadataComponent is source-of-truth.
#include "renegade/bridge/CommandService.h"
#include <WickedEngine.h>
#include <algorithm>
#include <cmath>

namespace renegade::bridge
{
    inline constexpr const char* NightSkySchemaKey = "renegade.night_sky.schema";
    inline constexpr const char* NightSkyGradientKey = "renegade.night_sky.gradient";
    inline constexpr const char* NightSkyAuroraKey = "renegade.night_sky.aurora";
    inline constexpr const char* NightSkySpeedKey = "renegade.night_sky.aurora_speed";
    inline constexpr const char* NightSkyZenithRKey = "renegade.night_sky.zenith_r";
    inline constexpr const char* NightSkyZenithGKey = "renegade.night_sky.zenith_g";
    inline constexpr const char* NightSkyZenithBKey = "renegade.night_sky.zenith_b";
    inline constexpr const char* NightSkyHorizonRKey = "renegade.night_sky.horizon_r";
    inline constexpr const char* NightSkyHorizonGKey = "renegade.night_sky.horizon_g";
    inline constexpr const char* NightSkyHorizonBKey = "renegade.night_sky.horizon_b";
    inline constexpr const char* NightSkyHorizonGlowKey = "renegade.night_sky.horizon_glow";
    inline constexpr const char* NightSkyCoverageKey = "renegade.night_sky.aurora_coverage";

    inline constexpr const char* NightSkyMilkyWayKey = "renegade.night_sky.milky_way";

    struct NightSkyState
    {
        float gradient = 0.0f;           // 0 preserves legacy sky
        float aurora = 0.0f;             // off by default
        float auroraSpeed = 0.35f;
        XMFLOAT3 zenith = XMFLOAT3(0.0018f, 0.0030f, 0.0085f); // linear HDR radiance
        XMFLOAT3 horizon = XMFLOAT3(0.0130f, 0.0120f, 0.0220f);
        float horizonGlow = 1.0f;
        float auroraCoverage = 0.4f;
        float milkyWay = 0.0f; // optional; missing legacy key remains off
    };
    inline float SafeNight(float v, float fallback, float max) noexcept
    {
        return std::isfinite(v) ? std::clamp(v, 0.0f, max) : fallback;
    }
    inline NightSkyState SanitizeNightSky(const NightSkyState& input) noexcept
    {
        NightSkyState s = input;
        s.gradient = SafeNight(s.gradient, 0, 1);
        s.aurora = SafeNight(s.aurora, 0, 1);
        s.auroraSpeed = SafeNight(s.auroraSpeed, 0.35f, 2);
        s.zenith.x = SafeNight(s.zenith.x, 0.0018f, 0.1f);
        s.zenith.y = SafeNight(s.zenith.y, 0.0030f, 0.1f);
        s.zenith.z = SafeNight(s.zenith.z, 0.0085f, 0.1f);
        s.horizon.x = SafeNight(s.horizon.x, 0.0130f, 0.1f);
        s.horizon.y = SafeNight(s.horizon.y, 0.0120f, 0.1f);
        s.horizon.z = SafeNight(s.horizon.z, 0.0220f, 0.1f);
        s.horizonGlow = SafeNight(s.horizonGlow, 1, 2);
        s.auroraCoverage = SafeNight(s.auroraCoverage, 0.4f, 1);
        s.milkyWay = SafeNight(s.milkyWay, 0, 1);
        return s;
    }
    inline bool HasNightSky(const wi::scene::Scene& scene, wi::ecs::Entity entity) noexcept
    {
        if (!scene.weathers.Contains(entity)) return false;
        const auto* m = scene.metadatas.GetComponent(entity);
        return m && m->int_values.has(NightSkySchemaKey) && m->int_values.get(NightSkySchemaKey) == 1;
    }
    inline NightSkyState CaptureNightSky(const wi::scene::Scene& scene, wi::ecs::Entity entity) noexcept
    {
        NightSkyState s;
        if (!HasNightSky(scene, entity)) return s;
        const auto& m = *scene.metadatas.GetComponent(entity);
#define REN_NIGHT_READ(KEY, DST) if(m.float_values.has(KEY)) (DST) = m.float_values.get(KEY)
        REN_NIGHT_READ(NightSkyGradientKey, s.gradient);
        REN_NIGHT_READ(NightSkyAuroraKey, s.aurora);
        REN_NIGHT_READ(NightSkySpeedKey, s.auroraSpeed);
        REN_NIGHT_READ(NightSkyZenithRKey, s.zenith.x);
        REN_NIGHT_READ(NightSkyZenithGKey, s.zenith.y);
        REN_NIGHT_READ(NightSkyZenithBKey, s.zenith.z);
        REN_NIGHT_READ(NightSkyHorizonRKey, s.horizon.x);
        REN_NIGHT_READ(NightSkyHorizonGKey, s.horizon.y);
        REN_NIGHT_READ(NightSkyHorizonBKey, s.horizon.z);
        REN_NIGHT_READ(NightSkyHorizonGlowKey, s.horizonGlow);
        REN_NIGHT_READ(NightSkyCoverageKey, s.auroraCoverage);
        REN_NIGHT_READ(NightSkyMilkyWayKey, s.milkyWay);
#undef REN_NIGHT_READ
        return SanitizeNightSky(s);
    }
    inline void ApplyNightSkyRuntime(wi::scene::Scene& scene, wi::ecs::Entity entity,
                                     const NightSkyState& requested) noexcept
    {
        auto* w = scene.weathers.GetComponent(entity);
        if (!w) return;
        const auto s = SanitizeNightSky(requested);
        w->night_sky_gradient = s.gradient;
        w->aurora_intensity = s.aurora;
        w->aurora_speed = s.auroraSpeed;
        // Existing reserved alpha carries Milky Way intensity; GPU offsets do not change.
        w->night_zenith = XMFLOAT4(s.zenith.x, s.zenith.y, s.zenith.z, s.milkyWay);
        w->night_horizon = XMFLOAT4(s.horizon.x, s.horizon.y, s.horizon.z, s.horizonGlow);
        w->aurora_coverage = s.auroraCoverage;
        if (scene.weathers.GetCount() && scene.weathers.GetEntity(0) == entity)
        {
            scene.weather = *w;
        }
    }
    inline bool StoreNightSky(wi::scene::Scene& scene, wi::ecs::Entity entity,
                              const NightSkyState& requested) noexcept
    {
        if (!scene.weathers.Contains(entity)) return false;
        const auto s = SanitizeNightSky(requested);
        auto* m = scene.metadatas.GetComponent(entity);
        if (!m) m = &scene.metadatas.Create(entity);
        m->int_values.set(NightSkySchemaKey, 1);
        m->float_values.set(NightSkyGradientKey, s.gradient);
        m->float_values.set(NightSkyAuroraKey, s.aurora);
        m->float_values.set(NightSkySpeedKey, s.auroraSpeed);
        m->float_values.set(NightSkyZenithRKey, s.zenith.x);
        m->float_values.set(NightSkyZenithGKey, s.zenith.y);
        m->float_values.set(NightSkyZenithBKey, s.zenith.z);
        m->float_values.set(NightSkyHorizonRKey, s.horizon.x);
        m->float_values.set(NightSkyHorizonGKey, s.horizon.y);
        m->float_values.set(NightSkyHorizonBKey, s.horizon.z);
        m->float_values.set(NightSkyHorizonGlowKey, s.horizonGlow);
        m->float_values.set(NightSkyCoverageKey, s.auroraCoverage);
        m->float_values.set(NightSkyMilkyWayKey, s.milkyWay);
        ApplyNightSkyRuntime(scene, entity, s);
        return true;
    }
    inline void ClearNightSky(wi::scene::Scene& scene, wi::ecs::Entity entity) noexcept
    {
        auto* m = scene.metadatas.GetComponent(entity);
        if (m)
        {
            m->int_values.erase(NightSkySchemaKey);
            for (const char* key : {
                NightSkyGradientKey, NightSkyAuroraKey, NightSkySpeedKey,
                NightSkyZenithRKey, NightSkyZenithGKey, NightSkyZenithBKey,
                NightSkyHorizonRKey, NightSkyHorizonGKey, NightSkyHorizonBKey,
                NightSkyHorizonGlowKey, NightSkyCoverageKey, NightSkyMilkyWayKey
            }) m->float_values.erase(key);
        }
        ApplyNightSkyRuntime(scene, entity, NightSkyState{});
    }
    inline void RestoreNightSky(wi::scene::Scene& scene) noexcept
    {
        for (std::size_t i = 0; i < scene.weathers.GetCount(); ++i)
        {
            const auto entity = scene.weathers.GetEntity(i);
            ApplyNightSkyRuntime(scene, entity, CaptureNightSky(scene, entity));
        }
    }
    inline bool SameNightSky(const NightSkyState& a, const NightSkyState& b) noexcept
    {
        const auto nearlyEqual = [](float x, float y) noexcept { return std::abs(x - y) < 0.00001f; };
        return nearlyEqual(a.gradient,b.gradient) && nearlyEqual(a.aurora,b.aurora) &&
            nearlyEqual(a.auroraSpeed,b.auroraSpeed) && nearlyEqual(a.auroraCoverage,b.auroraCoverage) &&
            nearlyEqual(a.zenith.x,b.zenith.x) && nearlyEqual(a.zenith.y,b.zenith.y) &&
            nearlyEqual(a.zenith.z,b.zenith.z) && nearlyEqual(a.horizon.x,b.horizon.x) &&
            nearlyEqual(a.horizon.y,b.horizon.y) && nearlyEqual(a.horizon.z,b.horizon.z) &&
            nearlyEqual(a.horizonGlow,b.horizonGlow) && nearlyEqual(a.milkyWay,b.milkyWay);
    }
    class SetNightSkyCommand final : public ICommand
    {
    public:
        SetNightSkyCommand(wi::scene::Scene& scene, wi::ecs::Entity entity, NightSkyState next) noexcept
            : scene_(&scene), entity_(entity), before_(CaptureNightSky(scene, entity)),
              beforeExisted_(HasNightSky(scene, entity)), after_(SanitizeNightSky(next)) {}
        bool Execute() override
        {
            if (!scene_ || !scene_->weathers.Contains(entity_)) return false;
            if (SameNightSky(before_, after_)) return false;
            return StoreNightSky(*scene_, entity_, after_);
        }
        void Undo() override
        {
            if (!scene_) return;
            if (beforeExisted_) StoreNightSky(*scene_, entity_, before_);
            else ClearNightSky(*scene_, entity_);
        }
    private:
        wi::scene::Scene* scene_;
        wi::ecs::Entity entity_;
        NightSkyState before_, after_;
        bool beforeExisted_;
    };
}
