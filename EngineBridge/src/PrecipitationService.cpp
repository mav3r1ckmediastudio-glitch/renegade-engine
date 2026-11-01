#include "renegade/bridge/PrecipitationService.h"

#include <wiInitializer.h>

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

    void RefreshPrecipitationVisual(wi::scene::Scene& scene)
    {
        // G4: Native rain remains exclusively Wicked's raindrop simulation.
        // Snow has a separate scene-owned GPU emitter, snowflake asset,
        // emission rate, lifetime, and velocity. Never bind snow artwork to
        // scene.rainMaterial or draw it as a modified rain particle.
        const PrecipitationState state = CapturePrecipitation(scene.weather);
        constexpr const char* SnowEntityName =
            "__renegade_internal_weather_snow";
        wi::ecs::Entity snowEntity = wi::ecs::INVALID_ENTITY;
        for (std::size_t i = 0; i < scene.emitters.GetCount(); ++i)
        {
            const auto candidate = scene.emitters.GetEntity(i);
            const auto* name = scene.names.GetComponent(candidate);
            if (name && name->name == SnowEntityName)
            {
                snowEntity = candidate;
                break;
            }
        }

        if (state.mode != PrecipitationMode::Snow)
        {
            if (snowEntity != wi::ecs::INVALID_ENTITY)
            {
                auto* emitter = scene.emitters.GetComponent(snowEntity);
                if (emitter && emitter->count > 0.0f)
                {
                    emitter->count = 0.0f;
                    emitter->Restart();
                }
            }
            // Snow should never poison the native rain's default gradient
            // even when a legacy scene carries the old snow texture.
            auto& rainMap = scene.rainMaterial.textures[
                wi::scene::MaterialComponent::BASECOLORMAP];
            if (wi::helper::GetFileNameFromPath(rainMap.name) ==
                "snowflake.dds")
                rainMap = {};
            return;
        }

        // Scene/command unit tests run without Wicked GPU initialization.
        // Do not create GPU-backed emitters or load resources until ready.
        if (!wi::initializer::IsInitializeFinished())
            return;

        if (snowEntity == wi::ecs::INVALID_ENTITY)
        {
            snowEntity = scene.Entity_CreateEmitter(
                SnowEntityName, scene.camera.Eye);
        }
        auto* emitter = scene.emitters.GetComponent(snowEntity);
        auto* material = scene.materials.GetComponent(snowEntity);
        auto* transform = scene.transforms.GetComponent(snowEntity);
        if (!emitter || !material || !transform)
            return;

        constexpr std::uint32_t MaxSnowParticles = 56000;
        if (emitter->GetMaxParticleCount() != MaxSnowParticles)
            emitter->SetMaxParticleCount(MaxSnowParticles);
        const auto settings = MakeSnowParticleSettings(state);

        emitter->shaderType = wi::EmittedParticleSystem::SOFT;
        emitter->SetWeatherSnow(true);
        emitter->SetVolumeEnabled(true);
        emitter->SetCollidersDisabled(true);
        emitter->SetDepthCollisionEnabled(false);
        emitter->SetSorted(true);
        emitter->normal_factor = 0.0f;
        emitter->random_factor = std::clamp(state.turbulence * 0.11f, 0.03f, 1.8f);
        emitter->random_life = 0.55f;
        emitter->count = settings.emissionPerSecond;
        emitter->life = settings.lifetimeSeconds;
        emitter->size = settings.sizeMetres;
        emitter->scaleX = 1.0f;
        emitter->scaleY = 1.0f;
        emitter->motionBlurAmount = 0.0f; // independent of rain streaks
        emitter->velocity = settings.velocity;
        emitter->gravity = XMFLOAT3(0.0f, -0.035f, 0.0f);
        emitter->drag = 0.995f;
        emitter->opacityCurveControlPeakStart = 0.15f;
        emitter->opacityCurveControlPeakEnd = 0.72f;

        material->SetUseVertexColors(true);
        material->shaderType =
            wi::scene::MaterialComponent::SHADERTYPE_UNLIT;
        material->userBlendMode = wi::enums::BLENDMODE_ALPHA;
        material->baseColor = XMFLOAT4(
            state.color.x, state.color.y, state.color.z,
            std::clamp(state.color.w, 0.0f, 0.95f));
        const std::string artwork = wi::helper::GetCurrentPath() +
            "/Content/weather/snowflake.dds";
        auto& snowMap = material->textures[
            wi::scene::MaterialComponent::BASECOLORMAP];
        if (snowMap.name != artwork)
        {
            snowMap.name = artwork;
            snowMap.resource = {};
        }
        if (!snowMap.resource.IsValid())
        {
            snowMap.resource = wi::resourcemanager::Load(artwork);
            if (!snowMap.resource.IsValid())
            {
                emitter->count = 0.0f; // missing-art safeguard
                return;
            }
        }

        // Authoring is camera-relative, not fixed at the level's origin:
        // the emitter follows the camera like native weather, but has its
        // own independent particles, simulation, material and source art.
        transform->translation_local = XMFLOAT3(
            scene.camera.Eye.x + scene.camera.At.x * 7.0f,
            scene.camera.Eye.y + scene.camera.At.y * 7.0f + 3.0f,
            scene.camera.Eye.z + scene.camera.At.z * 7.0f);
        transform->scale_local = XMFLOAT3(20.0f, 13.0f, 20.0f);
        transform->SetDirty();
        transform->UpdateTransform();
    }

    SnowParticleSettings MakeSnowParticleSettings(
        const PrecipitationState& state) noexcept
    {
        SnowParticleSettings settings;
        if (state.mode != PrecipitationMode::Snow)
            return settings;
        // Unlike rain's million-particle motion-blur pipeline, each visible
        // flake is a textured sprite with a slow fall and independent drift.
        settings.emissionPerSecond =
            (1150.0f * std::clamp(state.intensity, 0.0f, 1.0f) +
             8300.0f * std::pow(std::clamp(state.intensity, 0.0f, 1.0f), 3.0f));
        settings.lifetimeSeconds = 4.8f;
        settings.sizeMetres =
            std::clamp(state.particleScale * 0.44f, 0.0025f, 0.022f);
        const float a = state.windAzimuthDegrees * XM_PI / 180.0f;
        const float wind = std::clamp(state.windSpeed, 0.0f, 20.0f);
        settings.velocity = XMFLOAT3(
            std::cos(a) * wind * 0.15f,
            -std::clamp(state.fallSpeed, 0.01f, 2.0f) * 3.4f,
            std::sin(a) * wind * 0.15f);
        return settings;
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
            // Turbulence is a GLOBAL wind parameter. Changing particle type
            // must not overwrite authored wind or the foliage will suddenly
            // change its motion when rain is enabled.
            result.streakLength = 0.04f;
            result.splashScale = 0.1f;
            result.color = XMFLOAT4(0.6f, 0.8f, 1.0f, 0.5f);
            break;
        case PrecipitationMode::Snow:
            result.intensity = current.mode == mode
                ? current.intensity
                : 0.50f;
            result.fallSpeed = 0.22f;
            result.particleScale = 0.035f;
            // Snow uses the same global wind; drift can be specialised in a
            // future dedicated precipitation effect, not by mutating wind.
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
        RefreshPrecipitationVisual(*scene_);
        return true;
    }
}
